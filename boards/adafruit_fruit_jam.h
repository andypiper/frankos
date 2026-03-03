/*
 * FRANK OS
 * Copyright (c) 2025 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * FRANK OS board definition for the Adafruit Fruit Jam.
 *
 * The Fruit Jam is an RP2350B-based board sharing much of the same
 * peripheral set as the FRANK M2:
 *   - Same RP2350B MCU (48 GPIOs, PSRAM on QSPI CS1 pin 47)
 *   - Same DVI output via HSTX (GPIO 12-19) — no changes needed
 *   - Same 16 MB flash
 *   - 8 MB PSRAM
 *
 * Key differences from the M2:
 *   - Input:    USB HID keyboard + mouse via CH334F USB hub
 *               (PIO USB host on GPIO 1/2; USB host 5V enable on GPIO 11)
 *               instead of PS/2 on GPIO 0-3
 *   - SD card:  GPIO 34-39 (SPI mode) instead of GPIO 4-7
 *   - Audio:    TLV320DAC3100 I2S DAC (I2C config on GPIO 20/21,
 *               I2S data on GPIO 24-27) instead of direct PIO I2S on GPIO 9-11
 *   - LED:      GPIO 29 (red LED) instead of GPIO 25
 *   - Buttons:  GPIO 0 (BOOT), GPIO 4, GPIO 5
 *   - NeoPixels: 5× WS2812 on GPIO 32 (bonus feature)
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

#ifndef _BOARDS_ADAFRUIT_FRUIT_JAM_H
#define _BOARDS_ADAFRUIT_FRUIT_JAM_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)

// --- RP2350 VARIANT ---
// Fruit Jam uses RP2350B (QFN-80, 48 GPIOs) — same as M2
#define PICO_RP2350A 0

// --- LED ---
// Red LED on GPIO 29 (GPIO 25 is not present on the header)
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 29
#endif

// --- FLASH ---
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (16 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

#endif // _BOARDS_ADAFRUIT_FRUIT_JAM_H
