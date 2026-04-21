/*
 * FRANK OS — ESP32-C6 WiFi co-processor driver (Adafruit Fruit Jam).
 *
 * Implements the Adafruit Nina-FW / Airlift SPI command protocol.
 * Protocol reference: https://github.com/adafruit/nina-fw (protocol.md)
 *
 * Frame format (SPI):
 *   Host→ESP: START_CMD | cmd | n_params | [len | data]* | END_CMD
 *   ESP→Host: START_CMD | cmd | n_resp   | [len | data]* | END_CMD
 *
 * Control signals:
 *   CS    — active-low chip select
 *   BUSY  — ESP32 asserts high while it needs the bus / is processing
 *   IRQ   — ESP32 asserts high when a response is ready
 *   RESET — active-low hardware reset
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "wifi_esp32.h"
#include "board_config.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 * Nina-FW command codes (subset)
 * ------------------------------------------------------------------------- */
#define CMD_SET_NET          0x10   /* open unencrypted network */
#define CMD_SET_PASSPHRASE   0x11   /* WPA2 join */
#define CMD_DISCONNECT       0x30
#define CMD_GET_CONN_STATUS  0x20
#define CMD_GET_IPADDR       0x21
#define CMD_REQ_HOST_BY_NAME 0x34
#define CMD_GET_SOCKET       0x3F
#define CMD_START_CLIENT_TCP 0x2D
#define CMD_SEND_DATA_TCP    0x44
#define CMD_GET_DATA_TCP     0x45
#define CMD_AVAIL_DATA_TCP   0x2B
#define CMD_STOP_CLIENT      0x2E

#define START_CMD            0xE0
#define END_CMD              0xEE
#define ERR_CMD              0xEF
#define REPLY_FLAG           0x40
#define DUMMY_DATA           0xFF

/* -------------------------------------------------------------------------
 * SPI helpers
 * ------------------------------------------------------------------------- */

static SemaphoreHandle_t g_spi_mutex = NULL;

static inline void cs_low(void)  { gpio_put(WIFI_SPI_CS, 0); }
static inline void cs_high(void) { gpio_put(WIFI_SPI_CS, 1); }

static void wait_for_esp_ready(void) {
    /* BUSY high means ESP is not ready — wait for it to go low */
    uint32_t deadline = time_us_32() + 10000000; /* 10 s */
    while (gpio_get(WIFI_BUSY_PIN) && time_us_32() < deadline)
        sleep_us(100);
}

static void wait_for_irq(void) {
    /* IRQ high means ESP has a response ready */
    uint32_t deadline = time_us_32() + 5000000; /* 5 s */
    while (!gpio_get(WIFI_IRQ_PIN) && time_us_32() < deadline)
        sleep_us(200);
}

static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_write_read_blocking(WIFI_SPI_INST, &byte, &rx, 1);
    return rx;
}

static uint8_t read_byte_noack(void) {
    return spi_transfer(DUMMY_DATA);
}

/* -------------------------------------------------------------------------
 * Command frame builder / parser
 * ------------------------------------------------------------------------- */

static void send_cmd(uint8_t cmd, uint8_t n_params,
                     const uint8_t **params, const uint8_t *param_lens) {
    wait_for_esp_ready();
    cs_low();
    sleep_us(2);

    spi_transfer(START_CMD);
    spi_transfer(cmd & ~REPLY_FLAG);
    spi_transfer(n_params);

    for (int i = 0; i < n_params; i++) {
        spi_transfer(param_lens[i]);
        for (int j = 0; j < param_lens[i]; j++)
            spi_transfer(params[i][j]);
    }

    spi_transfer(END_CMD);
    sleep_us(2);
    cs_high();
}

/* Read a response frame.  Returns the number of response params,
 * fills resp_buf[0] with the first param (up to resp_buf_len bytes). */
static int recv_resp(uint8_t expected_cmd, uint8_t *resp_buf, uint8_t resp_buf_len) {
    wait_for_irq();
    cs_low();
    sleep_us(2);

    /* Read until START_CMD (skip padding) */
    uint8_t b;
    uint32_t deadline = time_us_32() + 2000000;
    do {
        b = read_byte_noack();
        if (time_us_32() > deadline) { cs_high(); return -1; }
    } while (b != START_CMD);

    uint8_t cmd     = read_byte_noack();
    uint8_t n_resp  = read_byte_noack();

    if (cmd != (expected_cmd | REPLY_FLAG) || n_resp == 0) {
        cs_high();
        return -1;
    }

    int total_written = 0;
    for (int i = 0; i < n_resp; i++) {
        uint8_t len = read_byte_noack();
        for (int j = 0; j < len; j++) {
            uint8_t data = read_byte_noack();
            if (i == 0 && j < resp_buf_len) {
                resp_buf[j] = data;
                total_written++;
            }
        }
    }

    read_byte_noack(); /* END_CMD */
    sleep_us(2);
    cs_high();
    return total_written;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

static bool g_initialised = false;

bool wifi_esp32_init(SemaphoreHandle_t spi_mutex) {
    g_spi_mutex = spi_mutex;

    /* GPIO setup */
    gpio_init(WIFI_SPI_CS);
    gpio_set_dir(WIFI_SPI_CS, GPIO_OUT);
    gpio_put(WIFI_SPI_CS, 1);

    gpio_init(WIFI_RESET_PIN);
    gpio_set_dir(WIFI_RESET_PIN, GPIO_OUT);
    gpio_put(WIFI_RESET_PIN, 1);

    gpio_init(WIFI_BUSY_PIN);
    gpio_set_dir(WIFI_BUSY_PIN, GPIO_IN);
    gpio_pull_up(WIFI_BUSY_PIN);

    gpio_init(WIFI_IRQ_PIN);
    gpio_set_dir(WIFI_IRQ_PIN, GPIO_IN);
    gpio_pull_up(WIFI_IRQ_PIN);

    /* SPI bus — shared with SD card, already initialised by sdcard driver.
     * We use the same instance and frequency. */
    gpio_set_function(WIFI_SPI_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(WIFI_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(WIFI_SPI_MISO, GPIO_FUNC_SPI);

    /* Hardware reset */
    gpio_put(WIFI_RESET_PIN, 0);
    sleep_ms(10);
    gpio_put(WIFI_RESET_PIN, 1);
    sleep_ms(750); /* Nina-FW boot time */

    /* Handshake: GET_CONN_STATUS should return IDLE */
    uint8_t resp[1] = {0xFF};
    send_cmd(CMD_GET_CONN_STATUS, 0, NULL, NULL);
    int r = recv_resp(CMD_GET_CONN_STATUS, resp, sizeof(resp));
    if (r <= 0) {
        printf("WiFi ESP32: no response from ESP32-C6\n");
        return false;
    }

    printf("WiFi ESP32: ESP32-C6 ready (status=%u)\n", resp[0]);
    g_initialised = true;
    return true;
}

bool wifi_esp32_join(const char *ssid, const char *passphrase) {
    if (!g_initialised) return false;

    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);

    const uint8_t *params[2] = {
        (const uint8_t *)ssid,
        (const uint8_t *)passphrase,
    };
    uint8_t lens[2] = {
        (uint8_t)strlen(ssid),
        (uint8_t)strlen(passphrase),
    };
    send_cmd(CMD_SET_PASSPHRASE, 2, params, lens);

    uint8_t resp[1];
    recv_resp(CMD_SET_PASSPHRASE, resp, sizeof(resp));

    xSemaphoreGive(g_spi_mutex);

    /* Poll for connection, up to 10 s */
    for (int i = 0; i < 50; i++) {
        vTaskDelay(pdMS_TO_TICKS(200));
        wifi_status_t s = wifi_esp32_status();
        if (s == WIFI_STATUS_CONNECTED) {
            printf("WiFi: connected to %s\n", ssid);
            return true;
        }
        if (s == WIFI_STATUS_CONNECT_FAIL) break;
    }
    printf("WiFi: failed to connect to %s\n", ssid);
    return false;
}

wifi_status_t wifi_esp32_status(void) {
    if (!g_initialised) return WIFI_STATUS_IDLE;

    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    send_cmd(CMD_GET_CONN_STATUS, 0, NULL, NULL);
    uint8_t resp[1] = {0};
    recv_resp(CMD_GET_CONN_STATUS, resp, sizeof(resp));
    xSemaphoreGive(g_spi_mutex);

    return (wifi_status_t)resp[0];
}

void wifi_esp32_get_ip(char *buf, size_t len) {
    if (!g_initialised || !buf || len < 16) return;

    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);

    /* CMD_GET_IPADDR returns: [local_ip(4), subnet(4), gateway(4)] */
    uint8_t dummy = 0xFF;
    const uint8_t *p = &dummy;
    uint8_t plen = 1;
    send_cmd(CMD_GET_IPADDR, 1, &p, &plen);

    uint8_t resp[12] = {0};
    recv_resp(CMD_GET_IPADDR, resp, sizeof(resp));

    xSemaphoreGive(g_spi_mutex);

    snprintf(buf, len, "%u.%u.%u.%u",
             resp[0], resp[1], resp[2], resp[3]);
}

void wifi_esp32_disconnect(void) {
    if (!g_initialised) return;
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    send_cmd(CMD_DISCONNECT, 0, NULL, NULL);
    uint8_t resp[1];
    recv_resp(CMD_DISCONNECT, resp, sizeof(resp));
    xSemaphoreGive(g_spi_mutex);
}

/*
 * Minimal HTTP GET using Nina-FW TCP socket primitives.
 * Opens a TCP socket to host:80, sends a GET request, reads the body.
 * Only handles http:// URLs (no TLS).
 */
int wifi_esp32_http_get(const char *url, uint8_t *out_buf, size_t buf_len) {
    if (!g_initialised || !url || !out_buf) return -1;

    /* Parse URL: http://host[:port]/path */
    if (strncmp(url, "http://", 7) != 0) {
        printf("WiFi HTTP: only http:// supported\n");
        return -1;
    }
    const char *host_start = url + 7;
    const char *path_start = strchr(host_start, '/');
    if (!path_start) path_start = host_start + strlen(host_start);

    char host[128];
    size_t host_len = (size_t)(path_start - host_start);
    if (host_len >= sizeof(host)) return -1;
    memcpy(host, host_start, host_len);
    host[host_len] = '\0';

    uint16_t port = 80;
    char *colon = strchr(host, ':');
    if (colon) {
        port = (uint16_t)atoi(colon + 1);
        *colon = '\0';
    }
    const char *path = (*path_start == '/') ? path_start : "/";

    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);

    /* Resolve hostname */
    const uint8_t *rp[1] = { (const uint8_t *)host };
    uint8_t rl[1] = { (uint8_t)strlen(host) };
    send_cmd(CMD_REQ_HOST_BY_NAME, 1, rp, rl);
    uint8_t resolved[1];
    recv_resp(CMD_REQ_HOST_BY_NAME, resolved, sizeof(resolved));
    if (!resolved[0]) {
        xSemaphoreGive(g_spi_mutex);
        printf("WiFi HTTP: DNS failed for %s\n", host);
        return -1;
    }

    /* Allocate socket */
    send_cmd(CMD_GET_SOCKET, 0, NULL, NULL);
    uint8_t sock_resp[1] = {0xFF};
    recv_resp(CMD_GET_SOCKET, sock_resp, sizeof(sock_resp));
    uint8_t sock = sock_resp[0];
    if (sock == 0xFF) {
        xSemaphoreGive(g_spi_mutex);
        return -1;
    }

    /* Connect: [host, port_hi, port_lo, socket, tcp=0] */
    uint8_t port_hi = (uint8_t)(port >> 8);
    uint8_t port_lo = (uint8_t)(port & 0xFF);
    uint8_t proto   = 0; /* TCP */
    const uint8_t *cp[5] = {
        (const uint8_t *)host,
        &port_hi, &port_lo, &sock, &proto,
    };
    uint8_t cl[5] = { (uint8_t)strlen(host), 1, 1, 1, 1 };
    send_cmd(CMD_START_CLIENT_TCP, 5, cp, cl);
    uint8_t conn_resp[1];
    recv_resp(CMD_START_CLIENT_TCP, conn_resp, sizeof(conn_resp));

    /* Wait for connection */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Build HTTP request */
    char req[256];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, host);

    /* Send data */
    uint8_t req_len8 = (uint8_t)(req_len & 0xFF);
    const uint8_t *sp[3] = { &sock, (const uint8_t *)req, &req_len8 };
    uint8_t sl[3] = { 1, (uint8_t)req_len, 1 };
    send_cmd(CMD_SEND_DATA_TCP, 3, sp, sl);
    uint8_t send_resp[1];
    recv_resp(CMD_SEND_DATA_TCP, send_resp, sizeof(send_resp));

    /* Read response */
    int total = 0;
    bool in_body = false;
    uint8_t chunk[256];
    uint32_t deadline = time_us_32() + 10000000; /* 10 s */

    while ((size_t)total < buf_len && time_us_32() < deadline) {
        /* Check available bytes */
        const uint8_t *ap[1] = { &sock };
        uint8_t al[1] = { 1 };
        send_cmd(CMD_AVAIL_DATA_TCP, 1, ap, al);
        uint8_t avail_resp[2] = {0, 0};
        recv_resp(CMD_AVAIL_DATA_TCP, avail_resp, sizeof(avail_resp));
        uint16_t avail = ((uint16_t)avail_resp[0] << 8) | avail_resp[1];

        if (avail == 0) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        uint8_t to_read = (avail > sizeof(chunk)) ? sizeof(chunk) : (uint8_t)avail;
        uint8_t sz_hi   = (uint8_t)(to_read >> 8);
        uint8_t sz_lo   = (uint8_t)(to_read & 0xFF);
        const uint8_t *gp[3] = { &sock, &sz_hi, &sz_lo };
        uint8_t gl[3] = { 1, 1, 1 };
        send_cmd(CMD_GET_DATA_TCP, 3, gp, gl);
        int got = recv_resp(CMD_GET_DATA_TCP, chunk, sizeof(chunk));
        if (got <= 0) break;

        if (!in_body) {
            /* Skip HTTP headers — look for \r\n\r\n */
            uint8_t *sep = (uint8_t *)"\r\n\r\n";
            uint8_t *found = NULL;
            for (int i = 0; i <= got - 4; i++) {
                if (memcmp(chunk + i, sep, 4) == 0) {
                    found = chunk + i + 4;
                    break;
                }
            }
            if (found) {
                in_body = true;
                int body_bytes = (int)(chunk + got - found);
                int copy = (body_bytes < (int)(buf_len - total))
                           ? body_bytes : (int)(buf_len - total);
                memcpy(out_buf + total, found, copy);
                total += copy;
            }
        } else {
            int copy = (got < (int)(buf_len - total)) ? got : (int)(buf_len - total);
            memcpy(out_buf + total, chunk, copy);
            total += copy;
        }
    }

    /* Close socket */
    const uint8_t *closep[1] = { &sock };
    uint8_t closel[1] = { 1 };
    send_cmd(CMD_STOP_CLIENT, 1, closep, closel);
    uint8_t close_resp[1];
    recv_resp(CMD_STOP_CLIENT, close_resp, sizeof(close_resp));

    xSemaphoreGive(g_spi_mutex);
    return total;
}

void wifi_esp32_task(void *params) {
    (void)params;
    /* Placeholder task — keeps the scheduler happy.
     * Async response handling can be added here as needed. */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
