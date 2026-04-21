/*
 * FRANK OS — ESP32-C6 WiFi co-processor driver (Adafruit Fruit Jam).
 *
 * The Fruit Jam's ESP32-C6 runs Adafruit Nina-FW firmware and exposes a
 * SPI-based command interface (the "Airlift" protocol).  This driver
 * implements the minimal subset needed for FRANK OS:
 *
 *   - Join a WPA2 network
 *   - Query connection status and IP address
 *   - HTTP GET a URL to a caller-supplied buffer
 *   - Disconnect
 *
 * The SPI bus (GPIO 28/30/31) is shared with the SD card driver.  Access
 * is serialised via a FreeRTOS mutex; callers must NOT hold the mutex when
 * calling these functions (they acquire it internally).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WIFI_ESP32_H
#define WIFI_ESP32_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Connection status codes returned by wifi_esp32_status() */
typedef enum {
    WIFI_STATUS_IDLE        = 0,
    WIFI_STATUS_NO_SSID     = 1,
    WIFI_STATUS_SCAN_DONE   = 2,
    WIFI_STATUS_CONNECTED   = 3,
    WIFI_STATUS_CONNECT_FAIL= 4,
    WIFI_STATUS_CONN_LOST   = 5,
    WIFI_STATUS_DISCONNECTED= 6,
} wifi_status_t;

/*
 * Initialise the SPI bus and reset the ESP32-C6.
 * Must be called once before the FreeRTOS scheduler starts (or with the
 * SPI mutex unheld).  Returns true if the ESP32 responds to a handshake.
 */
bool wifi_esp32_init(SemaphoreHandle_t spi_mutex);

/*
 * Join a WPA2 network.  Blocks up to ~10 s waiting for connection.
 * Returns true if WIFI_STATUS_CONNECTED is reached within the timeout.
 */
bool wifi_esp32_join(const char *ssid, const char *passphrase);

/* Return current connection status. */
wifi_status_t wifi_esp32_status(void);

/*
 * Get the assigned IPv4 address as a dotted-decimal string.
 * buf must be at least 16 bytes.
 */
void wifi_esp32_get_ip(char *buf, size_t len);

/*
 * Perform an HTTP GET.
 * url     — full URL (http://... only; HTTPS not supported by Nina-FW over SPI)
 * out_buf — caller-supplied buffer for response body
 * buf_len — size of out_buf
 * Returns number of bytes written to out_buf, or -1 on error.
 */
int wifi_esp32_http_get(const char *url, uint8_t *out_buf, size_t buf_len);

/* Disconnect from the current network. */
void wifi_esp32_disconnect(void);

/* FreeRTOS task body — processes async responses; stack ~2 KB */
void wifi_esp32_task(void *params);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_ESP32_H */
