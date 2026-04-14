/*
 * Manul - HTTP/1.0 Client
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>
#include <stdbool.h>

#define HTTP_MAX_URL 512

#define HTTP_STATE_IDLE         0
#define HTTP_STATE_CONNECTING   1
#define HTTP_STATE_SENDING      2
#define HTTP_STATE_RECV_STATUS  3
#define HTTP_STATE_RECV_HEADERS 4
#define HTTP_STATE_RECV_BODY    5
#define HTTP_STATE_DONE         6
#define HTTP_STATE_ERROR        7

typedef struct {
    uint16_t status_code;
    int32_t  content_length;
    int32_t  body_received;
    bool     chunked;
    char     content_type[64];
    char     location[HTTP_MAX_URL];
} http_response_t;

typedef void (*http_body_cb_t)(const uint8_t *data, uint16_t len, void *ctx);
typedef void (*http_done_cb_t)(void *ctx);

void http_init(void);
void http_poll(void);
bool http_get(const char *url_str, http_body_cb_t body_cb, http_done_cb_t done_cb, void *ctx);
int http_get_state(void);
const http_response_t *http_get_response(void);
void http_abort(void);

#endif
