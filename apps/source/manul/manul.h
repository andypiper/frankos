/*
 * Manul — Text Web Browser for FRANK OS
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * Shared types and sys_table wrappers for networking.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MANUL_H
#define MANUL_H

#include "m-os-api.h"
#include "frankos-app.h"

/* ========================================================================
 * sys_table wrappers for netcard API
 * ======================================================================== */

/* Callback types matching netcard.h */
typedef void (*nc_data_cb_t)(uint8_t socket_id, const uint8_t *data, uint16_t len);
typedef void (*nc_close_cb_t)(uint8_t socket_id);

static inline bool netcard_wifi_connected(void) {
    typedef bool (*fn_t)(void);
    return ((fn_t)_sys_table_ptrs[538])();
}

static inline bool netcard_socket_open(uint8_t id, bool tls,
                                        const char *host, uint16_t port) {
    typedef bool (*fn_t)(uint8_t, bool, const char *, uint16_t);
    return ((fn_t)_sys_table_ptrs[540])(id, tls, host, port);
}

static inline bool netcard_socket_send(uint8_t id, const uint8_t *data,
                                        uint16_t len) {
    typedef bool (*fn_t)(uint8_t, const uint8_t *, uint16_t);
    return ((fn_t)_sys_table_ptrs[541])(id, data, len);
}

static inline void netcard_socket_close(uint8_t id) {
    typedef void (*fn_t)(uint8_t);
    ((fn_t)_sys_table_ptrs[542])(id);
}

static inline void netcard_set_data_callback(nc_data_cb_t cb) {
    typedef void (*fn_t)(nc_data_cb_t);
    ((fn_t)_sys_table_ptrs[543])(cb);
}

static inline void netcard_set_close_callback(nc_close_cb_t cb) {
    typedef void (*fn_t)(nc_close_cb_t);
    ((fn_t)_sys_table_ptrs[544])(cb);
}

static inline bool netcard_resolve(const char *hostname, char *ip_out,
                                    int ip_out_size) {
    typedef bool (*fn_t)(const char *, char *, int);
    return ((fn_t)_sys_table_ptrs[549])(hostname, ip_out, ip_out_size);
}

#define dbg_printf(...) ((int(*)(const char*,...))_sys_table_ptrs[438])(__VA_ARGS__)

/* ========================================================================
 * Utility functions missing from m-os-api
 * ======================================================================== */

static inline char *manul_strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == 0) return (char *)s;
    return 0;
}

static inline char *manul_strrchr(const char *s, int c) {
    const char *last = 0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == 0) return (char *)s;
    return (char *)last;
}

static inline char *manul_strstr(const char *hay, const char *needle) {
    if (!needle[0]) return (char *)hay;
    uint16_t nlen = (uint16_t)strlen(needle);
    while (*hay) {
        if (*hay == *needle && strncmp(hay, needle, nlen) == 0)
            return (char *)hay;
        hay++;
    }
    return 0;
}

static inline char *manul_strncat(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (*d) d++;
    while (n-- > 0 && *src) *d++ = *src++;
    *d = '\0';
    return dst;
}

static inline long manul_strtol(const char *s, char **endp, int base) {
    long val = 0;
    bool neg = false;
    while (*s == ' ') s++;
    if (*s == '-') { neg = true; s++; }
    else if (*s == '+') s++;
    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16; s += 2;
        } else if (s[0] == '0') {
            base = 8; s++;
        } else {
            base = 10;
        }
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    while (*s) {
        int d = -1;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        if (d < 0 || d >= base) break;
        val = val * base + d;
        s++;
    }
    if (endp) *endp = (char *)s;
    return neg ? -val : val;
}

static inline int manul_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static inline int manul_strncasecmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (!a[i] && !b[i]) return 0;
        if (!a[i]) return -1;
        if (!b[i]) return 1;
        char ca = (a[i] >= 'A' && a[i] <= 'Z') ? a[i] + 32 : a[i];
        char cb = (b[i] >= 'A' && b[i] <= 'Z') ? b[i] + 32 : b[i];
        if (ca != cb) return ca - cb;
    }
    return 0;
}

static inline char manul_tolower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

#endif /* MANUL_H */
