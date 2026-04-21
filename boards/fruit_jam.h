/*
 * FRANK OS — Adafruit Fruit Jam board definition.
 *
 * Based on frank_m2.h. Same RP2350B chip, same 16 MB flash, same HSTX
 * DVI pinout (GPIO 12-19). Differences: USB HID host input (replaces PS/2),
 * SD card on SPI1 (GPIO 28/30/31/39), I2S audio on GPIO 24-27 with
 * TLV320DAC3100 DAC (I2C GPIO 20/21), and ESP32-C6 WiFi co-processor
 * on shared SPI bus (GPIO 28/30/31, CS GPIO 46).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

#ifndef _BOARDS_FRUIT_JAM_H
#define _BOARDS_FRUIT_JAM_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)

// --- RP2350 VARIANT (B — 48 GPIO, needed for GPIO 47 PSRAM CS1) ---
#define PICO_RP2350A 0

// --- LED (on-board LED via GPIO 25, same as Pico 2) ---
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

// --- FLASH (16 MB W25Q128, same capacity as M2) ---
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

#endif // _BOARDS_FRUIT_JAM_H
