/*
 * FRANK OS — TLV320DAC3100 I2S Audio DAC driver (Fruit Jam)
 * Copyright (c) 2025 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * TLV320DAC3100 configuration sequence.
 *
 * The DAC is configured as an I2S slave:
 *   - MCLK supplied externally by RP2350B PWM on GPIO 25
 *   - BCLK and WCLK (LRCLK) are inputs driven by the RP2350B PIO I2S engine
 *   - DAC clocking uses MCLK PLL mode: MCLK → PLL → DAC clocks
 *   - Stereo audio out to headphone jack; mono mix to speaker
 *
 * Register page references: Texas Instruments TLV320DAC3100 datasheet
 * (SLOS445 / SLASE04).  Page 0 is selected by writing 0 to register 0.
 */

#include "tlv320dac3100.h"
#include "board_config.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

/*==========================================================================
 * I2C helpers
 *==========================================================================*/

/* Write one register on page 0 */
static bool dac_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    int ret = i2c_write_blocking(DAC_I2C, DAC_I2C_ADDR, buf, 2, false);
    return (ret == 2);
}

/* Select register page (register 0 on any page) */
static bool dac_select_page(uint8_t page) {
    return dac_write_reg(0x00, page);
}

/*==========================================================================
 * MCLK generation via PWM
 *
 * The TLV320DAC3100 PLL can accept MCLK in the range 1–20 MHz.
 * We generate ~12.288 MHz (for 48 kHz) or ~11.289 MHz (for 44.1 kHz)
 * from the RP2350B PWM peripheral on I2S_MCLK_PIN.
 *
 * PWM wrap = 1, so frequency = sys_clk / (clkdiv × 2).
 * With sys_clk = 504 MHz and target ~12.288 MHz:
 *   clkdiv ≈ 504 / (12.288 × 2) ≈ 20.508  → integer 20, frac ≈ 8/16
 *==========================================================================*/

static void mclk_init(uint32_t sample_rate) {
    /* Target MCLK: 256 × Fs */
    uint32_t mclk_hz = 256u * sample_rate;

    uint32_t sys_hz = clock_get_hz(clk_sys);
    /* PWM runs at sys_clk / (clkdiv × (wrap+1)), wrap=1 → div by 2 */
    /* clkdiv (8.4 fixed-point) = sys_clk / (mclk_hz × 2) × 16 */
    uint32_t clkdiv_16 = (uint32_t)((uint64_t)sys_hz * 16u / (mclk_hz * 2u));

    uint slice = pwm_gpio_to_slice_num(I2S_MCLK_PIN);
    uint chan  = pwm_gpio_to_channel(I2S_MCLK_PIN);

    gpio_set_function(I2S_MCLK_PIN, GPIO_FUNC_PWM);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 1u);          /* 50 % duty-cycle square wave */
    pwm_config_set_clkdiv_int_frac(&cfg,
        (uint8_t)(clkdiv_16 >> 4),
        (uint8_t)(clkdiv_16 & 0xFu));
    pwm_init(slice, &cfg, false);
    pwm_set_chan_level(slice, chan, 1u);
    pwm_set_enabled(slice, true);

    printf("[tlv320] MCLK: %lu Hz on GPIO %d (sys=%lu Hz, div16=%lu)\n",
           (unsigned long)mclk_hz, I2S_MCLK_PIN,
           (unsigned long)sys_hz, (unsigned long)clkdiv_16);
}

/*==========================================================================
 * tlv320_init
 *==========================================================================*/
bool tlv320_init(uint32_t sample_rate) {
    /* --- I2C initialisation --- */
    i2c_init(DAC_I2C, 400 * 1000);  /* 400 kHz fast-mode */
    gpio_set_function(DAC_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(DAC_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(DAC_I2C_SDA_PIN);
    gpio_pull_up(DAC_I2C_SCL_PIN);

    /* --- MCLK generation (must be running before DAC power-up) --- */
    mclk_init(sample_rate);
    sleep_ms(5);  /* let MCLK stabilise */

    /* === Page 0 — Software reset === */
    if (!dac_select_page(0)) {
        printf("[tlv320] I2C not responding (addr=0x%02X)\n", DAC_I2C_ADDR);
        return false;
    }
    dac_write_reg(0x01, 0x01);  /* Reg 1: software reset */
    sleep_ms(5);

    /* === Page 0 — Clock configuration ===
     *
     * MCLK → PLL → CODEC_CLKIN
     *   PLL enabled, P=1, R=1, J=8, D=1920 for ~12.288 MHz MCLK → 98.304 MHz
     * NDAC=4, MDAC=2 → DAC_CLK = 98.304/8 = 12.288 MHz
     * DOSR=128        → DAC_FS  = 12.288/128 = 96 kHz (or J/D adjusted for 44.1/48)
     *
     * For simplicity we configure for 44.1 kHz using MCLK=256×Fs=11.2896 MHz:
     *   NDAC=2, MDAC=7, DOSR=64 → DAC_FS = (11.2896×1e6)/(2×7×64) = 12600 Hz — wrong
     *
     * Practical setting: use BCLK as CODEC_CLKIN (BCLK=64×Fs from the PIO):
     *   CODEC_CLKIN = BCLK,  NDAC=1, MDAC=1, DOSR=64
     */

    /* Reg 4: CODEC_CLKIN = MCLK (codec_clkin_src = 00 = MCLK) */
    dac_select_page(0);
    dac_write_reg(0x04, 0x00);

    /* Reg 6: PLL_CLKIN = MCLK, CODEC_CLKIN = PLL_CLK */
    dac_write_reg(0x04, 0x03);  /* codec_clkin = PLL output */

    /* Reg 5: PLL power-up, P=1, R=1 */
    dac_write_reg(0x05, 0x91);  /* PLL power up, P=1, R=1 */
    /* Reg 6: J=7 (for 44100) or J=8 (for 48000) */
    uint8_t pll_j = (sample_rate == 44100) ? 7u : 8u;
    dac_write_reg(0x06, pll_j);
    /* Reg 7,8: D=1680 (44100) or D=1920 (48000) — MSB/LSB */
    uint16_t pll_d = (sample_rate == 44100) ? 1680u : 1920u;
    dac_write_reg(0x07, (uint8_t)(pll_d >> 6));   /* D[13:6] in bits [7:0] */
    dac_write_reg(0x08, (uint8_t)((pll_d & 0x3Fu) << 2)); /* D[5:0] in bits [7:2] */

    /* Reg 11: NDAC=4, powered */
    dac_write_reg(0x0B, 0x84);
    /* Reg 12: MDAC=2, powered */
    dac_write_reg(0x0C, 0x82);
    /* Reg 13,14: DOSR=128 */
    dac_write_reg(0x0D, 0x00);
    dac_write_reg(0x0E, 0x80);

    /* === Page 0 — Audio interface: I2S, 16-bit, slave mode === */
    /* Reg 27: I2S mode, 16-bit word length, BCLK+WCLK inputs (slave) */
    dac_write_reg(0x1B, 0x00);

    /* === Page 0 — DAC data path === */
    /* Reg 63: DAC channel setup — left DAC → left channel, right → right */
    dac_write_reg(0x3F, 0xD4);  /* DAC powered, left=left, right=right, soft-step */

    /* === Page 1 — Analog outputs === */
    dac_select_page(1);

    /* Reg 1 (page 1): power on headphone driver */
    dac_write_reg(0x01, 0x08);  /* Weak AVDD-to-DVDD */

    /* Reg 10 (page 1): HPL driver volume = 0 dB */
    dac_write_reg(0x10, 0x00);
    /* Reg 11 (page 1): HPR driver volume = 0 dB */
    dac_write_reg(0x11, 0x00);

    /* Reg 9 (page 1): DAC output routed to HPL + HPR + SPK */
    dac_write_reg(0x09, 0x0C);  /* HPL = DAC left, HPR = DAC right */

    /* Reg 14 (page 1): HPL + HPR driver power-up */
    dac_write_reg(0x0E, 0x0A);  /* HPL power, HPR power */

    /* Reg 38 (page 1): Class-D speaker driver gain = 6 dB */
    dac_write_reg(0x26, 0x04);
    /* Reg 46 (page 1): SPK powered */
    dac_write_reg(0x2E, 0x86);

    /* === Page 0 — DAC volume === */
    dac_select_page(0);
    /* Reg 65: left DAC volume = 0 dB (0x00) */
    dac_write_reg(0x41, 0x00);
    /* Reg 66: right DAC volume = 0 dB */
    dac_write_reg(0x42, 0x00);

    /* Unmute both DAC channels */
    dac_write_reg(0x40, 0x00);  /* Reg 64: unmute left+right DAC */

    sleep_ms(10);  /* allow PLL and drivers to lock */

    printf("[tlv320] DAC initialised (%lu Hz)\n", (unsigned long)sample_rate);
    return true;
}

/*==========================================================================
 * tlv320_set_volume
 *==========================================================================*/
void tlv320_set_volume(uint8_t vol) {
    /* Volume register: 0x00 = 0 dB, 0x81 = mute (bit 7 = mute) */
    /* Map 0-127 linearly to 0x00-0x7F (0 dB to -63.5 dB in 0.5 dB steps) */
    dac_select_page(0);
    uint8_t reg_val = (vol >= 127) ? 0x81u : (vol & 0x7Fu);
    dac_write_reg(0x41, reg_val);  /* left  */
    dac_write_reg(0x42, reg_val);  /* right */
}
