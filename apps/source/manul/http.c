/*
 * Manul - HTTP/1.0 Client (FRANK OS port)
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 *
 * Streaming HTTP/1.0 client using netcard sys_table wrappers.
 * Ported from frank-lynx http.c for FRANK OS app environment.
 * Uses socket 0 for all HTTP connections.
 * No switch statements; uses manul_ prefixed string helpers.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "manul.h"
#include "http.h"
#include "url.h"

/* Socket used for HTTP connections */
#define HTTP_SOCKET_ID  0

/* Maximum number of redirect hops */
#define MAX_REDIRECTS  5

/* Receive buffer for header accumulation */
#define HTTP_RECV_BUF_SIZE  (16 * 1024)  /* 16KB in PSRAM — large sites have big headers */

/* User-Agent string */
#define HTTP_USER_AGENT  "frank-netcard/1.01"

/* ---- Chunked transfer encoding states ---- */
#define CHUNK_ST_SIZE_LINE   0
#define CHUNK_ST_DATA        1
#define CHUNK_ST_DATA_CRLF   2
#define CHUNK_ST_DONE        3

/* ---- Internal state ---- */

static int              state;
static http_response_t  response;
static url_t            current_url;

static http_body_cb_t   body_cb;
static http_done_cb_t   done_cb;
static void            *user_ctx;

static uint8_t  *recv_buf;      /* allocated from PSRAM in http_init */
static uint16_t recv_len;

static int      redirect_count;

/* Deferred redirect -- set inside data callback, executed from main loop */
static bool     redirect_pending;
static url_t    redirect_url;

/* Data reception timestamp for timeout detection */
static uint32_t last_data_tick;

/* Chunked transfer encoding state */
static int      chunk_state;
static int32_t  chunk_remaining;

/* ---- Forward declarations ---- */

static void http_on_data(uint8_t socket_id, const uint8_t *data, uint16_t len);
static void http_on_close(uint8_t socket_id);
static bool http_start_request(void);
static void http_finish(int end_state);
static void http_parse_status_line(const char *line);
static void http_parse_header_line(const char *line);
static void http_process_headers(void);
static void http_deliver_body(const uint8_t *data, uint16_t len);
static void http_deliver_body_chunked(const uint8_t *data, uint16_t len);
static bool http_is_redirect(uint16_t code);

/* ---- Public API ---- */

void http_init(void) {
    state = HTTP_STATE_IDLE;
    memset(&response, 0, sizeof(response));
    if (!recv_buf) {
        recv_buf = (uint8_t *)psram_alloc(HTTP_RECV_BUF_SIZE);
        if (!recv_buf)
            recv_buf = (uint8_t *)malloc(HTTP_RECV_BUF_SIZE);
    }
    recv_len = 0;
    redirect_count = 0;
    redirect_pending = false;
    body_cb = NULL;
    done_cb = NULL;
    user_ctx = NULL;
}

void http_poll(void) {
    /* Data reception timeout: if we're receiving body and haven't
     * gotten any data for 3 seconds, assume the transfer is complete.
     * This handles servers that use HTTP/1.1 keep-alive and don't
     * close the connection, or when the ESP-01 loses the +SCLOSED event. */
    if (state == HTTP_STATE_RECV_BODY && response.body_received > 0 &&
        !redirect_pending) {
        uint32_t elapsed = xTaskGetTickCount() - last_data_tick;
        if (elapsed >= pdMS_TO_TICKS(1000)) {
            dbg_printf("[HTTP] timeout after %ld bytes, finishing\n",
                       response.body_received);
            netcard_socket_close(HTTP_SOCKET_ID);
            http_finish(HTTP_STATE_DONE);
            return;
        }
    }

    if (!redirect_pending)
        return;
    redirect_pending = false;

    /* Close the current socket (safe here -- not inside netcard callback) */
    netcard_socket_close(HTTP_SOCKET_ID);

    /* Reset state so http_start_request() can proceed */
    state = HTTP_STATE_IDLE;

    /* Follow the redirect */
    memcpy(&current_url, &redirect_url, sizeof(url_t));
    dbg_printf("[HTTP] following redirect to %s://%s%s\n",
               current_url.scheme, current_url.host, current_url.path);
    if (!http_start_request()) {
        http_finish(HTTP_STATE_ERROR);
    }
}

bool http_get(const char *url_str, http_body_cb_t bcb,
              http_done_cb_t dcb, void *ctx) {
    if (state != HTTP_STATE_IDLE && state != HTTP_STATE_DONE &&
        state != HTTP_STATE_ERROR)
        return false;

    if (!url_parse(url_str, &current_url))
        return false;

    body_cb = bcb;
    done_cb = dcb;
    user_ctx = ctx;
    redirect_count = 0;

    return http_start_request();
}

int http_get_state(void) {
    return state;
}

const http_response_t *http_get_response(void) {
    return &response;
}

void http_abort(void) {
    if (state != HTTP_STATE_IDLE && state != HTTP_STATE_DONE &&
        state != HTTP_STATE_ERROR) {
        netcard_socket_close(HTTP_SOCKET_ID);
        http_finish(HTTP_STATE_ERROR);
    }
}

/* ---- Internal implementation ---- */

static bool http_start_request(void) {
    /* Reset response and receive buffer */
    memset(&response, 0, sizeof(response));
    response.content_length = -1;
    recv_len = 0;
    chunk_state = CHUNK_ST_SIZE_LINE;
    chunk_remaining = 0;
    last_data_tick = xTaskGetTickCount();

    /* Install our callbacks */
    netcard_set_data_callback(http_on_data);
    netcard_set_close_callback(http_on_close);

    /* Open socket: TLS for HTTPS, plain TCP for HTTP */
    bool tls = (strcmp(current_url.scheme, "https") == 0);
    state = HTTP_STATE_CONNECTING;

    dbg_printf("[HTTP] %s %s://%s:%u%s\n",
               tls ? "HTTPS" : "HTTP",
               current_url.scheme, current_url.host,
               current_url.port, current_url.path);

    if (!netcard_socket_open(HTTP_SOCKET_ID, tls,
                             current_url.host, current_url.port)) {
        dbg_printf("[HTTP] socket_open failed\n");
        http_finish(HTTP_STATE_ERROR);
        return false;
    }

    /* AT+SOPEN blocks until TCP/TLS handshake completes.
     * If it returned OK the socket is already connected. */
    dbg_printf("[HTTP] connected, sending request\n");

    /* Build and send GET request */
    state = HTTP_STATE_SENDING;

    char request[768];
    int req_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "User-Agent: %s\r\n"
        "\r\n",
        current_url.path,
        current_url.host,
        HTTP_USER_AGENT);

    dbg_printf("[HTTP] GET %s Host: %s (%d bytes)\n",
               current_url.path, current_url.host, req_len);

    if (req_len < 0 || req_len >= (int)sizeof(request)) {
        dbg_printf("[HTTP] request too long\n");
        netcard_socket_close(HTTP_SOCKET_ID);
        http_finish(HTTP_STATE_ERROR);
        return false;
    }

    if (!netcard_socket_send(HTTP_SOCKET_ID,
                             (const uint8_t *)request, (uint16_t)req_len)) {
        dbg_printf("[HTTP] send failed\n");
        netcard_socket_close(HTTP_SOCKET_ID);
        http_finish(HTTP_STATE_ERROR);
        return false;
    }

    /* Only transition to RECV_STATUS if the state machine hasn't already
     * advanced past it.  The response may have arrived during the blocking
     * netcard_socket_send() — callbacks run inside nc_wait_response(). */
    if (state == HTTP_STATE_SENDING)
        state = HTTP_STATE_RECV_STATUS;

    dbg_printf("[HTTP] send done, state=%d\n", state);
    return true;
}

static void http_finish(int end_state) {
    state = end_state;
    if (done_cb)
        done_cb(user_ctx);
}

static bool http_is_redirect(uint16_t code) {
    if (code == 301) return true;
    if (code == 302) return true;
    if (code == 307) return true;
    if (code == 308) return true;
    return false;
}

/*
 * Netcard data callback: receives raw TCP data from socket 0.
 * Routes to header accumulation or body delivery depending on state.
 */
static void http_on_data(uint8_t socket_id, const uint8_t *data, uint16_t len) {
    if (socket_id != HTTP_SOCKET_ID)
        return;

    last_data_tick = xTaskGetTickCount();

    if (state == HTTP_STATE_CONNECTING) {
        state = HTTP_STATE_RECV_STATUS;
    }

    /* If data arrives while we're still in SENDING state (response came
     * before SEND OK was processed), treat it as RECV_STATUS. */
    if (state == HTTP_STATE_SENDING) {
        state = HTTP_STATE_RECV_STATUS;
    }

    if (state == HTTP_STATE_RECV_STATUS || state == HTTP_STATE_RECV_HEADERS) {
        /* Accumulate into recv_buf for header parsing */
        uint16_t space = HTTP_RECV_BUF_SIZE - recv_len;
        uint16_t copy = (len < space) ? len : space;
        if (copy > 0) {
            memcpy(recv_buf + recv_len, data, copy);
            recv_len += copy;
        }

        /* Look for end of headers: \r\n\r\n */
        char *hdr_end = NULL;
        uint16_t i;
        for (i = 3; i < recv_len; i++) {
            if (recv_buf[i-3] == '\r' && recv_buf[i-2] == '\n' &&
                recv_buf[i-1] == '\r' && recv_buf[i]   == '\n') {
                hdr_end = (char *)&recv_buf[i+1];
                break;
            }
        }

        if (!hdr_end)
            return;  /* Need more data */

        /* Null-terminate headers for string parsing */
        uint16_t hdr_len = (uint16_t)(hdr_end - (char *)recv_buf);
        /* Place null terminator at end of header block (overwrite first body byte
         * temporarily -- we saved the data and will deliver it below) */
        uint8_t saved_byte = 0;
        if (hdr_len < recv_len)
            saved_byte = recv_buf[hdr_len];
        recv_buf[hdr_len] = '\0';

        /* Parse status line and headers */
        http_process_headers();

        /* Restore byte */
        if (hdr_len < recv_len)
            recv_buf[hdr_len] = saved_byte;

        /* Handle redirects -- defer to avoid recursive netcard_poll.
         * The actual close/reopen happens in http_poll(). */
        if (http_is_redirect(response.status_code) && response.location[0]) {
            redirect_count++;
            if (redirect_count > MAX_REDIRECTS) {
                dbg_printf("[HTTP] too many redirects\n");
                state = HTTP_STATE_ERROR;
                return;
            }

            dbg_printf("[HTTP] redirect %d -> %s\n", response.status_code,
                       response.location);

            url_resolve(&current_url, response.location, &redirect_url);
            redirect_pending = true;
            return;
        }

        state = HTTP_STATE_RECV_BODY;

        /* Deliver any body data already in the buffer */
        uint16_t body_in_buf = recv_len - hdr_len;
        if (body_in_buf > 0) {
            http_deliver_body(recv_buf + hdr_len, body_in_buf);
        }

        /* Also deliver the overflow data that didn't fit in recv_buf */
        if (copy < len) {
            http_deliver_body(data + copy, len - copy);
        }
    } else if (state == HTTP_STATE_RECV_BODY) {
        /* Direct body delivery */
        http_deliver_body(data, len);
    }
}

/*
 * Netcard close callback: server closed the connection.
 * For HTTP/1.0 with Connection: close, this signals end of response.
 */
static void http_on_close(uint8_t socket_id) {
    dbg_printf("[HTTP] on_close: sock=%u st=%d redir=%d\n",
               socket_id, state, redirect_pending);

    if (socket_id != HTTP_SOCKET_ID)
        return;

    /* If a redirect is pending, the close is expected — the server
     * sent a 301/302 and hung up.  Don't signal "done" yet;
     * http_poll() will follow the redirect. */
    if (redirect_pending) {
        dbg_printf("[HTTP] close during redirect (expected)\n");
        return;
    }

    if (state == HTTP_STATE_CONNECTING ||
        state == HTTP_STATE_SENDING) {
        /* Connection failed or closed before response */
        dbg_printf("[HTTP] connection closed early\n");
        http_finish(HTTP_STATE_ERROR);
    } else if (state == HTTP_STATE_RECV_BODY ||
               state == HTTP_STATE_RECV_STATUS ||
               state == HTTP_STATE_RECV_HEADERS) {
        /* Server closed connection -- transfer complete */
        dbg_printf("[HTTP] close -> done (%ld bytes)\n",
                   response.body_received);
        http_finish(HTTP_STATE_DONE);
    } else {
        dbg_printf("[HTTP] close ignored, state=%d\n", state);
    }
}

/*
 * Parse the accumulated header block.
 * Format: "HTTP/1.x NNN reason\r\nHeader: Value\r\n..."
 */
static void http_process_headers(void) {
    char *p = (char *)recv_buf;

    /* Parse status line: find first \r\n */
    char *line_end = manul_strstr(p, "\r\n");
    if (!line_end) {
        state = HTTP_STATE_ERROR;
        return;
    }
    *line_end = '\0';
    http_parse_status_line(p);
    p = line_end + 2;

    state = HTTP_STATE_RECV_HEADERS;

    /* Parse each header line */
    while (*p && !(p[0] == '\r' && p[1] == '\n')) {
        line_end = manul_strstr(p, "\r\n");
        if (!line_end)
            break;
        *line_end = '\0';
        http_parse_header_line(p);
        p = line_end + 2;
    }
}

/*
 * Parse "HTTP/1.x NNN reasonphrase"
 */
static void http_parse_status_line(const char *line) {
    /* Skip "HTTP/1.x " */
    const char *p = line;
    while (*p && *p != ' ')
        p++;
    while (*p == ' ')
        p++;

    /* Parse status code */
    response.status_code = 0;
    while (*p >= '0' && *p <= '9') {
        response.status_code = response.status_code * 10 + (*p - '0');
        p++;
    }

    dbg_printf("[HTTP] status %u\n", response.status_code);
}

/*
 * Parse a single "Header-Name: value" line (case-insensitive).
 */
static void http_parse_header_line(const char *line) {
    const char *colon = manul_strchr(line, ':');
    if (!colon)
        return;

    size_t name_len = colon - line;

    /* Skip colon and leading whitespace in value */
    const char *value = colon + 1;
    while (*value == ' ' || *value == '\t')
        value++;

    if (name_len == 14 &&
        manul_strncasecmp(line, "content-length", 14) == 0) {
        response.content_length = (int32_t)manul_strtol(value, NULL, 10);
    } else if (name_len == 12 &&
               manul_strncasecmp(line, "content-type", 12) == 0) {
        strncpy(response.content_type, value,
                sizeof(response.content_type) - 1);
        response.content_type[sizeof(response.content_type) - 1] = '\0';
    } else if (name_len == 8 &&
               manul_strncasecmp(line, "location", 8) == 0) {
        strncpy(response.location, value,
                sizeof(response.location) - 1);
        response.location[sizeof(response.location) - 1] = '\0';
    } else if (name_len == 17 &&
               manul_strncasecmp(line, "transfer-encoding", 17) == 0) {
        if (manul_strncasecmp(value, "chunked", 7) == 0) {
            response.chunked = true;
            chunk_state = CHUNK_ST_SIZE_LINE;
            chunk_remaining = 0;
        }
    }
}

/*
 * Deliver body data to the user callback, either directly or
 * through chunked decoding.  Detects transfer completion so we
 * don't rely solely on the TCP close event.
 */
static void http_deliver_body(const uint8_t *data, uint16_t len) {
    if (len == 0)
        return;

    if (response.chunked) {
        http_deliver_body_chunked(data, len);
        /* Chunked: complete when final zero-chunk is parsed */
        if (chunk_state == CHUNK_ST_DONE) {
            dbg_printf("[HTTP] chunked complete (%ld bytes)\n",
                       response.body_received);
            /* Don't call netcard_socket_close here — we're inside the
             * netcard data callback.  Just signal done; the server will
             * close (Connection: close) or http_poll timeout will clean up. */
            http_finish(HTTP_STATE_DONE);
        }
    } else {
        /* Plain body: deliver directly */
        if (body_cb)
            body_cb(data, len, user_ctx);
        response.body_received += len;
        /* Content-Length: complete when all bytes received */
        if (response.content_length >= 0 &&
            response.body_received >= response.content_length) {
            dbg_printf("[HTTP] content-length complete (%ld bytes)\n",
                       response.body_received);
            http_finish(HTTP_STATE_DONE);
        }
    }
}

/*
 * Chunked transfer encoding decoder.
 * Format: hex-size\r\n...data...\r\n  (repeated; final chunk has size 0)
 * All control flow uses if/else (no switch).
 */
static void http_deliver_body_chunked(const uint8_t *data, uint16_t len) {
    uint16_t pos = 0;

    while (pos < len && chunk_state != CHUNK_ST_DONE) {

        if (chunk_state == CHUNK_ST_SIZE_LINE) {
            /* Parse hex chunk size, terminated by \r\n */
            while (pos < len) {
                uint8_t c = data[pos++];
                if (c == '\r') {
                    /* Expect \n next */
                    continue;
                }
                if (c == '\n') {
                    if (chunk_remaining == 0) {
                        chunk_state = CHUNK_ST_DONE;
                    } else {
                        chunk_state = CHUNK_ST_DATA;
                    }
                    break;
                }
                /* Parse hex digit */
                int digit = -1;
                if (c >= '0' && c <= '9')      digit = c - '0';
                else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                if (digit >= 0)
                    chunk_remaining = chunk_remaining * 16 + digit;
                /* Ignore chunk extensions (;...) */
            }

        } else if (chunk_state == CHUNK_ST_DATA) {
            /* Deliver up to chunk_remaining bytes */
            uint16_t avail = len - pos;
            uint16_t deliver = (avail < (uint16_t)chunk_remaining)
                                ? avail : (uint16_t)chunk_remaining;
            if (deliver > 0) {
                if (body_cb)
                    body_cb(data + pos, deliver, user_ctx);
                response.body_received += deliver;
                chunk_remaining -= deliver;
                pos += deliver;
            }
            if (chunk_remaining == 0)
                chunk_state = CHUNK_ST_DATA_CRLF;

        } else if (chunk_state == CHUNK_ST_DATA_CRLF) {
            /* Consume trailing \r\n after chunk data */
            uint8_t c = data[pos++];
            if (c == '\n') {
                /* End of CRLF -- start next chunk */
                chunk_state = CHUNK_ST_SIZE_LINE;
                chunk_remaining = 0;
            }
            /* Skip \r silently */

        } else {
            /* CHUNK_ST_DONE */
            return;
        }
    }
}
