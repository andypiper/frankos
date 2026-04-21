/*
 * FRANK OS — WiFi integration layer (Adafruit Fruit Jam).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef BOARD_FRUIT_JAM

#include "wifi.h"
#include "wifi_esp32.h"
#include "terminal.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>
#include <stdio.h>

static SemaphoreHandle_t s_spi_mutex = NULL;
static bool              s_ready     = false;

bool wifi_init(void) {
    s_spi_mutex = xSemaphoreCreateMutex();
    if (!s_spi_mutex) return false;

    s_ready = wifi_esp32_init(s_spi_mutex);
    return s_ready;
}

SemaphoreHandle_t wifi_get_spi_mutex(void) {
    return s_spi_mutex;
}

const char *wifi_status_str(void) {
    if (!s_ready) return "no WiFi";
    switch (wifi_esp32_status()) {
        case WIFI_STATUS_CONNECTED:    return "connected";
        case WIFI_STATUS_CONNECT_FAIL: return "failed";
        case WIFI_STATUS_CONN_LOST:    return "lost";
        case WIFI_STATUS_DISCONNECTED: return "disconnected";
        default:                       return "idle";
    }
}

/*
 * wifi <subcommand> [args...]
 *
 *   wifi join <ssid> <password>   — join WPA2 network
 *   wifi status                   — show connection info
 *   wifi get <url> <local-path>   — HTTP GET to SD card file
 *   wifi disconnect               — leave current network
 */
void wifi_shell_cmd(int argc, char **argv) {
    terminal_t *t = terminal_get_active();

    if (!s_ready) {
        terminal_puts(t, "wifi: ESP32-C6 not available\n");
        return;
    }

    if (argc < 2) {
        terminal_puts(t,
            "Usage: wifi <command> [args]\n"
            "  join <ssid> <password>  join WPA2 network\n"
            "  status                  show IP and signal\n"
            "  get <url> <path>        HTTP GET to file on SD\n"
            "  disconnect              leave current network\n");
        return;
    }

    if (strcmp(argv[1], "join") == 0) {
        if (argc < 4) {
            terminal_puts(t, "Usage: wifi join <ssid> <password>\n");
            return;
        }
        terminal_printf(t, "Joining %s...\n", argv[2]);
        if (wifi_esp32_join(argv[2], argv[3])) {
            char ip[16];
            wifi_esp32_get_ip(ip, sizeof(ip));
            terminal_printf(t, "Connected. IP: %s\n", ip);
        } else {
            terminal_puts(t, "Connection failed.\n");
        }

    } else if (strcmp(argv[1], "status") == 0) {
        wifi_status_t st = wifi_esp32_status();
        terminal_printf(t, "Status: %s\n", wifi_status_str());
        if (st == WIFI_STATUS_CONNECTED) {
            char ip[16];
            wifi_esp32_get_ip(ip, sizeof(ip));
            terminal_printf(t, "IP: %s\n", ip);
        }

    } else if (strcmp(argv[1], "get") == 0) {
        if (argc < 4) {
            terminal_puts(t, "Usage: wifi get <url> <local-path>\n");
            return;
        }
        const char *url   = argv[2];
        const char *fpath = argv[3];

        /* Allocate a 64 KB receive buffer from heap */
        const size_t BUF_SZ = 65536;
        uint8_t *buf = pvPortMalloc(BUF_SZ);
        if (!buf) {
            terminal_puts(t, "wifi get: out of memory\n");
            return;
        }

        terminal_printf(t, "Fetching %s...\n", url);
        int bytes = wifi_esp32_http_get(url, buf, BUF_SZ);
        if (bytes < 0) {
            terminal_puts(t, "wifi get: HTTP request failed\n");
        } else {
            /* Write to SD card via FatFS */
            FIL f;
            FRESULT fr = f_open(&f, fpath, FA_CREATE_ALWAYS | FA_WRITE);
            if (fr != FR_OK) {
                terminal_printf(t, "wifi get: cannot open %s (err %d)\n",
                                fpath, (int)fr);
            } else {
                UINT written;
                f_write(&f, buf, (UINT)bytes, &written);
                f_close(&f);
                terminal_printf(t, "Saved %u bytes to %s\n",
                                (unsigned)written, fpath);
            }
        }
        vPortFree(buf);

    } else if (strcmp(argv[1], "disconnect") == 0) {
        wifi_esp32_disconnect();
        terminal_puts(t, "Disconnected.\n");

    } else {
        terminal_printf(t, "wifi: unknown command '%s'\n", argv[1]);
    }
}

#endif /* BOARD_FRUIT_JAM */
