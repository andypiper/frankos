/*
 * Manul - URL Parser (FRANK OS port)
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 *
 * Ported from frank-lynx url.c for FRANK OS app environment.
 * Uses manul_ prefixed string helpers; no switch statements.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "manul.h"
#include "url.h"

bool url_parse(const char *url_str, url_t *out) {
    memset(out, 0, sizeof(*out));

    const char *p = url_str;

    /* Parse scheme */
    if (strncmp(p, "https://", 8) == 0) {
        strncpy(out->scheme, "https", sizeof(out->scheme) - 1);
        out->port = 443;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        strncpy(out->scheme, "http", sizeof(out->scheme) - 1);
        out->port = 80;
        p += 7;
    } else {
        /* Default to http if no scheme */
        strncpy(out->scheme, "http", sizeof(out->scheme) - 1);
        out->port = 80;
    }

    /* Parse host and optional port */
    const char *host_start = p;

    /* Find end of host: either ':', '/', or end of string */
    while (*p && *p != ':' && *p != '/')
        p++;

    size_t host_len = p - host_start;
    if (host_len == 0 || host_len >= sizeof(out->host))
        return false;

    memcpy(out->host, host_start, host_len);
    out->host[host_len] = '\0';

    /* Parse optional port */
    if (*p == ':') {
        p++;
        uint16_t port = 0;
        while (*p >= '0' && *p <= '9') {
            port = port * 10 + (*p - '0');
            p++;
        }
        if (port > 0)
            out->port = port;
    }

    /* Parse path (everything from '/' onwards) */
    if (*p == '/') {
        strncpy(out->path, p, sizeof(out->path) - 1);
    } else {
        strncpy(out->path, "/", sizeof(out->path) - 1);
    }

    return true;
}

void url_resolve(const url_t *base, const char *relative, url_t *out) {
    /* Absolute URL? */
    if (strncmp(relative, "http://", 7) == 0 ||
        strncmp(relative, "https://", 8) == 0) {
        url_parse(relative, out);
        return;
    }

    /* Copy base scheme, host, port */
    memcpy(out, base, sizeof(*out));

    if (relative[0] == '/') {
        /* Absolute path */
        strncpy(out->path, relative, sizeof(out->path) - 1);
        out->path[sizeof(out->path) - 1] = '\0';
    } else {
        /* Relative path: resolve against base path */
        /* Find last '/' in base path */
        char *last_slash = manul_strrchr(out->path, '/');
        if (last_slash) {
            size_t base_dir_len = last_slash - out->path + 1;
            if (base_dir_len + strlen(relative) < sizeof(out->path)) {
                strncpy(out->path + base_dir_len, relative,
                        sizeof(out->path) - base_dir_len - 1);
                out->path[sizeof(out->path) - 1] = '\0';
            }
        } else {
            snprintf(out->path, sizeof(out->path), "/%s", relative);
        }
    }

    /* Resolve ".." and "." segments using manual tokenization (no strtok) */
    char temp[URL_MAX];
    strncpy(temp, out->path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *segments[64];
    int seg_count = 0;

    char *tp = temp;
    /* Skip leading slash */
    if (*tp == '/')
        tp++;

    while (*tp && seg_count < 64) {
        /* Start of a segment */
        char *seg_start = tp;
        /* Find the next '/' or end of string */
        while (*tp && *tp != '/')
            tp++;
        /* Null-terminate this segment */
        if (*tp == '/') {
            *tp = '\0';
            tp++;
        }
        /* Process segment */
        if (strcmp(seg_start, ".") == 0) {
            /* Skip current-dir marker */
        } else if (strcmp(seg_start, "..") == 0) {
            if (seg_count > 0)
                seg_count--;
        } else if (seg_start[0] != '\0') {
            segments[seg_count++] = seg_start;
        }
    }

    char resolved[URL_MAX];
    resolved[0] = '/';
    resolved[1] = '\0';
    for (int i = 0; i < seg_count; i++) {
        if (i > 0)
            manul_strncat(resolved, "/",
                          sizeof(resolved) - strlen(resolved) - 1);
        manul_strncat(resolved, segments[i],
                      sizeof(resolved) - strlen(resolved) - 1);
    }

    strncpy(out->path, resolved, sizeof(out->path) - 1);
    out->path[sizeof(out->path) - 1] = '\0';
}

void url_to_string(const url_t *url, char *out, size_t out_sz) {
    bool default_port = false;
    if (strcmp(url->scheme, "http") == 0 && url->port == 80)
        default_port = true;
    else if (strcmp(url->scheme, "https") == 0 && url->port == 443)
        default_port = true;

    if (default_port)
        snprintf(out, out_sz, "%s://%s%s",
                 url->scheme, url->host, url->path);
    else
        snprintf(out, out_sz, "%s://%s:%u%s",
                 url->scheme, url->host, url->port, url->path);
}
