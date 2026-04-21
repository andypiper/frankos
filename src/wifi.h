/*
 * FRANK OS — WiFi integration layer (Adafruit Fruit Jam).
 *
 * Thin wrapper over the ESP32-C6 driver that exposes WiFi to the rest
 * of FRANK OS: shell commands, status queries, and a shared SPI mutex
 * that serialises access between the SD card and the WiFi co-processor.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WIFI_H
#define WIFI_H

#ifdef BOARD_FRUIT_JAM

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialise WiFi subsystem.
 * Creates the shared SPI mutex, resets and probes the ESP32-C6.
 * Call once from main() before the scheduler starts.
 * Returns true if the ESP32 is responsive.
 */
bool wifi_init(void);

/*
 * Shared SPI bus mutex — held by whoever is using GPIO 28/30/31.
 * Both the SD card driver and the WiFi driver must take this before
 * driving their respective CS pins.
 */
SemaphoreHandle_t wifi_get_spi_mutex(void);

/* Shell command entry point — called from shell.c with argc/argv */
void wifi_shell_cmd(int argc, char **argv);

/* Quick status string for display (e.g. taskbar tooltip) */
const char *wifi_status_str(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_FRUIT_JAM */
#endif /* WIFI_H */
