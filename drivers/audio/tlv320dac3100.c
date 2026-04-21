/*
 * FRANK OS — TLV320DAC3100 audio DAC driver (Adafruit Fruit Jam).
 *
 * Configures the TI TLV320DAC3100 via I2C so the existing I2S PIO path
 * (audio.c / snd.c) can stream 44100 Hz 16-bit stereo audio to the
 * 3.5 mm headphone jack and mono speaker output.
 *
 * Register map follows the TLV320DAC3100 datasheet (SLAS865).
 * All writes use the page-register (reg 0x00) to select page 0 or 1.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tlv320dac3100.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <string.h>

/* Write a single register on the current page */
static bool tlv_write(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_write_blocking(i2c, addr, buf, 2, false) == 2;
}

/* Select register page (0 or 1) */
static bool tlv_page(i2c_inst_t *i2c, uint8_t addr, uint8_t page) {
    return tlv_write(i2c, addr, 0x00, page);
}

bool tlv320_init(i2c_inst_t *i2c, uint sda_pin, uint scl_pin, uint8_t i2c_addr) {
    /* Initialise I2C bus at 400 kHz */
    i2c_init(i2c, 400 * 1000);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    sleep_ms(10);

    /* Software reset (page 0, reg 1) */
    if (!tlv_page(i2c, i2c_addr, 0)) {
        printf("TLV320: I2C NAK — DAC not responding at 0x%02x\n", i2c_addr);
        return false;
    }
    tlv_write(i2c, i2c_addr, 0x01, 0x01);  /* software reset */
    sleep_ms(5);
    tlv_page(i2c, i2c_addr, 0);

    /*
     * Clock configuration for 44100 Hz, 16-bit, I2S slave.
     *
     * MCLK = 12 MHz (Fruit Jam crystal)
     * Target CODEC_CLKIN = 112.896 MHz = 44100 * 256 * 10
     *
     * PLL: PLLCLK = MCLK * R * J.D / P
     *   P=1, R=1, J=9, D=3200 → 12 * 1 * 9.3200 = 111.840 MHz (close)
     *
     * Use NADC=2, MADC=4 → ADC fs = 111.84M/(2*4*256) ≈ 54.6k (not used)
     * Use NDAC=2, MDAC=4 → DAC fs = 111.84M/(2*4*256) ≈ 54.6k (not used)
     *
     * Simpler: use MCLK directly, NDAC=1, MDAC=21 for ~44.1 kHz.
     * CODEC_CLKIN = MCLK = 12 MHz
     * DAC_MOD_CLK = 12M / (NDAC * MDAC) = 12M / (1*21) = 571428 Hz
     * That's too low.  Use PLL:
     *   J=7, D=5264, P=1, R=1 → PLLCLK = 12 * 7.5264 = 90.317 MHz
     *   NDAC=2, MDAC=2 → DAC_CLK = 90.317M/4 = 22.58 MHz
     *   BCLK divider to 64fs → BCLK = 44100*64 = 2.8224 MHz ✓
     *
     * For simplicity we configure the PLL for ≈44.1 kHz via the
     * standard Adafruit / TI example values.
     */

    /* Page 0: clock / interface config */
    tlv_page(i2c, i2c_addr, 0);

    /* Reg 4: PLL clock source = MCLK */
    tlv_write(i2c, i2c_addr, 0x04, 0x03);  /* CODEC_CLKIN = PLL_CLK; PLL_CLK = MCLK */

    /* Reg 5: PLL P=1, R=1 */
    tlv_write(i2c, i2c_addr, 0x05, 0x91);  /* PLL powered, P=1, R=1 */

    /* Reg 6: PLL J=7 */
    tlv_write(i2c, i2c_addr, 0x06, 0x07);

    /* Reg 7/8: PLL D = 5264 (0x1490) */
    tlv_write(i2c, i2c_addr, 0x07, 0x14);
    tlv_write(i2c, i2c_addr, 0x08, 0x90);

    /* Reg 11: NDAC=2, powered */
    tlv_write(i2c, i2c_addr, 0x0B, 0x82);

    /* Reg 12: MDAC=2, powered */
    tlv_write(i2c, i2c_addr, 0x0C, 0x82);

    /* Reg 13/14: DOSR = 128 (0x0080) */
    tlv_write(i2c, i2c_addr, 0x0D, 0x00);
    tlv_write(i2c, i2c_addr, 0x0E, 0x80);

    /* Reg 27: I2S, 16-bit, slave mode (BCLK & WCLK are inputs) */
    tlv_write(i2c, i2c_addr, 0x1B, 0x00);

    /* Reg 60: DAC signal processing — DAC on, volume control enabled */
    tlv_write(i2c, i2c_addr, 0x3C, 0x00);

    /* Page 1: output routing and power */
    tlv_page(i2c, i2c_addr, 1);

    /* Reg 1: disable weak AVDD (powered externally on Fruit Jam) */
    tlv_write(i2c, i2c_addr, 0x01, 0x08);

    /* Reg 2: enable analog blocks */
    tlv_write(i2c, i2c_addr, 0x02, 0x00);

    /* Reg 9: HPL routed from DAC_L */
    tlv_write(i2c, i2c_addr, 0x09, 0x0C);

    /* Reg 10: HPR routed from DAC_R */
    tlv_write(i2c, i2c_addr, 0x0A, 0x0C);

    /* Reg 12: Class-D SPK_L from DAC_L (mono speaker) */
    tlv_write(i2c, i2c_addr, 0x0C, 0x08);

    /* Reg 17: HPL PGA = 0 dB, not muted */
    tlv_write(i2c, i2c_addr, 0x11, 0x00);

    /* Reg 18: HPR PGA = 0 dB, not muted */
    tlv_write(i2c, i2c_addr, 0x12, 0x00);

    /* Reg 19: Speaker driver gain = 6 dB */
    tlv_write(i2c, i2c_addr, 0x13, 0x04);

    /* Reg 20: power HP and speaker output */
    tlv_write(i2c, i2c_addr, 0x14, 0x00);

    /* Reg 31: HPL powered, short-circuit protection enabled */
    tlv_write(i2c, i2c_addr, 0x1F, 0xC4);

    /* Reg 32: HPR powered */
    tlv_write(i2c, i2c_addr, 0x20, 0xC4);

    /* Reg 33: Speaker driver powered */
    tlv_write(i2c, i2c_addr, 0x21, 0xC6);

    /* Back to page 0: power DAC channels */
    tlv_page(i2c, i2c_addr, 0);

    /* Reg 63: DAC_L and DAC_R powered, left data to both channels */
    tlv_write(i2c, i2c_addr, 0x3F, 0xD4);

    /* Reg 65: DAC_L digital volume = 0 dB (not muted) */
    tlv_write(i2c, i2c_addr, 0x41, 0x00);

    /* Reg 66: DAC_R digital volume = 0 dB */
    tlv_write(i2c, i2c_addr, 0x42, 0x00);

    sleep_ms(10);
    printf("TLV320: DAC initialised (44100 Hz, 16-bit I2S)\n");
    return true;
}

void tlv320_set_volume(i2c_inst_t *i2c, uint8_t i2c_addr, uint8_t vol) {
    /* DAC digital volume: 0x00 = 0 dB, 0x80 = mute.
     * vol 0-127 maps to 0x80 (mute) down to 0x00 (max). */
    uint8_t reg_val = (vol == 0) ? 0x80 : (uint8_t)((127 - vol));
    tlv_page(i2c, i2c_addr, 0);
    tlv_write(i2c, i2c_addr, 0x41, reg_val);
    tlv_write(i2c, i2c_addr, 0x42, reg_val);
}
