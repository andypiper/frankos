/*
 * FRANK OS
 * Copyright (c) 2025 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "hardware/vreg.h"

/*
 * Board Configuration for FRANK OS
 *
 * Supported boards (select via CMake -DBOARD_TARGET=<name>):
 *
 * BOARD_M2 (default) GPIO layout:
 *   PS/2 Mouse:  CLK=0, DATA=1
 *   PS/2 Kbd:    CLK=2, DATA=3
 *   SD Card:     MISO=4, CSn=5, SCK=6, MOSI=7
 *   PSRAM:       CS=8 (RP2350A) / 47 (RP2350B)
 *   I2S Audio:   DATA=9, BCLK=10, LRCLK=11
 *   HDMI (HSTX): CLK-=12, CLK+=13, D0-=14, D0+=15,
 *                D1-=16, D1+=17, D2-=18, D2+=19
 *
 * BOARD_FRUIT_JAM GPIO layout:
 *   HDMI (HSTX): CLK-=12, CLK+=13, D0-=14, D0+=15,   ← IDENTICAL to M2
 *                D1-=16, D1+=17, D2-=18, D2+=19
 *   PSRAM:       CS=47 (RP2350B only)                 ← IDENTICAL to M2
 *   USB Host:    DP=1, DM=2, 5V_EN=11 (on-board hub)
 *   I2C (DAC):   SDA=20, SCL=21 (TLV320DAC3100 @ 0x18)
 *   I2S Audio:   DATA=24, BCLK=25, LRCLK=26
 *   SD Card:     MISO=28, SCK=30, MOSI=31, CSn=39
 *   ESP32-C6:    MISO=28, SCK=30, MOSI=31, CS=46 (shared SPI bus)
 *                IRQ=23, BUSY=3, RESET=22
 *   NeoPixel:    GPIO=32
 *   Buttons:     BOOT=0, BTN2=4, BTN3=5
 */

#if !defined(BOARD_M2) && !defined(BOARD_FRUIT_JAM)
#define BOARD_M2
#endif

//=============================================================================
// M2 board
//=============================================================================
#ifdef BOARD_M2

#ifndef CPU_CLOCK_MHZ
#define CPU_CLOCK_MHZ 504
#endif
#ifndef CPU_VOLTAGE
#define CPU_VOLTAGE VREG_VOLTAGE_1_65
#endif

#define PS2_PIN_CLK    2
#define PS2_PIN_DATA   3
#define PS2_MOUSE_CLK  0
#define PS2_MOUSE_DATA 1

/* SD Card (SPI0, GPIO 4-7) */
#define SDCARD_PIN_CLK  6
#define SDCARD_PIN_CMD  7
#define SDCARD_PIN_D0   4
#define SDCARD_PIN_D3   5

/* PSRAM — auto-detect RP2350A vs B */
#define PSRAM_PIN_RP2350A 8
#define PSRAM_PIN_RP2350B 47

/* I2S Audio (PIO1, GPIO 9-11) */
#define I2S_DATA_PIN        9
#define I2S_CLOCK_PIN_BASE  10

#endif /* BOARD_M2 */

//=============================================================================
// Fruit Jam board
//=============================================================================
#ifdef BOARD_FRUIT_JAM

/* Run at 378 MHz — same safe overclock as M2 fallback; DVI needs ≥252 MHz */
#ifndef CPU_CLOCK_MHZ
#define CPU_CLOCK_MHZ 378
#endif
#ifndef CPU_VOLTAGE
#define CPU_VOLTAGE VREG_VOLTAGE_1_65
#endif

/* No PS/2 — input is USB HID Host */

/* SD Card (SPI1 shared bus, GPIO 28/30/31/39) */
#define SDCARD_PIN_CLK  30
#define SDCARD_PIN_CMD  31
#define SDCARD_PIN_D0   28
#define SDCARD_PIN_D3   39

/* PSRAM — Fruit Jam is always RP2350B, GPIO 47 */
#define PSRAM_PIN_RP2350A 47    /* unused but keep define consistent */
#define PSRAM_PIN_RP2350B 47

/* I2S Audio (PIO1, GPIO 24-26) */
#define I2S_DATA_PIN        24
#define I2S_CLOCK_PIN_BASE  25  /* BCLK=25, LRCLK=26 */

/* TLV320DAC3100 audio DAC (I2C0, GPIO 20/21) */
#define TLV320_I2C_INST  i2c0
#define TLV320_I2C_SDA   20
#define TLV320_I2C_SCL   21
#define TLV320_I2C_ADDR  0x18

/* ESP32-C6 WiFi co-processor (shared SPI1 bus) */
#define WIFI_SPI_INST    spi1
#define WIFI_SPI_SCK     30
#define WIFI_SPI_MOSI    31
#define WIFI_SPI_MISO    28
#define WIFI_SPI_CS      46
#define WIFI_IRQ_PIN     23
#define WIFI_BUSY_PIN     3
#define WIFI_RESET_PIN   22

/* NeoPixels and buttons */
#define NEOPIXEL_PIN     32
#define BTN_BOOT_PIN      0
#define BTN2_PIN          4
#define BTN3_PIN          5

#endif /* BOARD_FRUIT_JAM */

//=============================================================================
// PSRAM pin helper — both boards are RP2350B so this always returns 47.
// Keep the runtime detection for future-proofing.
//=============================================================================
#if PICO_RP2350
#include "hardware/structs/sysinfo.h"
static inline uint get_psram_pin(void) {
    uint32_t package_sel = *((io_ro_32 *)(SYSINFO_BASE + SYSINFO_PACKAGE_SEL_OFFSET));
    if (package_sel & 1)
        return PSRAM_PIN_RP2350A;
    else
        return PSRAM_PIN_RP2350B;
}
#endif

#endif // BOARD_CONFIG_H
