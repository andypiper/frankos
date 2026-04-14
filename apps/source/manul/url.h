/*
 * Manul - URL Parser
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef URL_H
#define URL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define URL_MAX 512

typedef struct {
    char     scheme[8];
    char     host[128];
    uint16_t port;
    char     path[URL_MAX];
} url_t;

bool url_parse(const char *url_str, url_t *out);
void url_resolve(const url_t *base, const char *relative, url_t *out);
void url_to_string(const url_t *url, char *out, size_t out_sz);

#endif
