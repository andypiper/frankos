/*
 * frankos_libc.c - C library function wrappers for FRANK OS
 *
 * Provides implementations of standard C library functions (malloc, free,
 * sprintf, strlen, etc.) that route through the FRANK OS sys_table.
 * Needed by game logic files (soundgen.c, spinlock.c, drawing.c, etc.)
 * that use standard libc but don't include m-os-api.h.
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Access FRANK OS sys_table directly. We cannot include m-os-api.h because
 * it defines inline static versions of malloc/free/strlen/etc. that would
 * conflict with the extern definitions we need to provide here.
 */
#define M_OS_API_SYS_TABLE_BASE ((void*)(0x10000000ul + (16 << 20) - (4 << 10)))
static const unsigned long * const _sys_table_ptrs =
    (const unsigned long * const)M_OS_API_SYS_TABLE_BASE;

/* --- Memory allocation --- */

void *malloc(size_t size) {
    typedef void *(*fn)(size_t);
    return ((fn)_sys_table_ptrs[32])(size);  /* pvPortMalloc */
}

void free(void *ptr) {
    typedef void (*fn)(void *);
    ((fn)_sys_table_ptrs[33])(ptr);  /* vPortFree */
}

void *calloc(size_t nmemb, size_t size) {
    typedef void *(*fn)(size_t, size_t);
    return ((fn)_sys_table_ptrs[166])(nmemb, size);  /* pvPortCalloc */
}

/* --- String functions --- */

size_t strlen(const char *s) {
    typedef size_t (*fn)(const char *);
    return ((fn)_sys_table_ptrs[62])(s);
}

char *strncpy(char *dst, const char *src, size_t n) {
    typedef char *(*fn)(char *, const char *, size_t);
    return ((fn)_sys_table_ptrs[63])(dst, src, n);
}

char *stpcpy(char *dst, const char *src) {
    typedef char *(*fn_strcpy)(char *, const char *);
    ((fn_strcpy)_sys_table_ptrs[60])(dst, src);  /* strcpy */
    typedef size_t (*fn_strlen)(const char *);
    return dst + ((fn_strlen)_sys_table_ptrs[62])(dst);  /* strlen */
}

/* --- Formatted output --- */

int sprintf(char *buf, const char *fmt, ...) {
    typedef int (*fn)(char *, size_t, const char *, __builtin_va_list);
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int ret = ((fn)_sys_table_ptrs[67])(buf, 4096, fmt, ap);  /* vsnprintf */
    __builtin_va_end(ap);
    return ret;
}

int printf(const char *fmt, ...) {
    char buf[256];
    typedef int (*vsnfn)(char *, size_t, const char *, __builtin_va_list);
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int ret = ((vsnfn)_sys_table_ptrs[67])(buf, sizeof(buf), fmt, ap);
    __builtin_va_end(ap);
    typedef void (*gfn)(const char *, ...);
    ((gfn)_sys_table_ptrs[438])("%s", buf);  /* serial_printf */
    return ret;
}

/* --- Conversion functions --- */

long atol(const char *s) {
    long result = 0;
    int neg = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return neg ? -result : result;
}

/* --- Environment / system stubs --- */

char *getenv(const char *name) {
    (void)name;
    return (char *)0;  /* No environment on FRANK OS */
}

void abort(void) {
    typedef void (*fn)(const char *, ...);
    ((fn)_sys_table_ptrs[438])("[digger] abort() called\n");
    for (;;) {}
}

/* --- Character classification table for newlib <ctype.h> --- */

/*
 * Newlib's ctype macros use: (_ctype_ + 1)[c] & mask
 * Bit flags: _U=0x01, _L=0x02, _N=0x04, _S=0x08, _P=0x10, _C=0x20,
 *            _X=0x40, _B=0x80
 *
 * Table layout: [0]=EOF, [1..256]=chars 0x00..0xFF
 */
#define _U  0x01
#define _L  0x02
#define _N  0x04
#define _S  0x08
#define _P  0x10
#define _C  0x20
#define _X  0x40
#define _B  0x80

const char _ctype_[1 + 256] = {
    0,                                                      /* EOF */
    /* 0x00-0x07 */ _C, _C, _C, _C, _C, _C, _C, _C,
    /* 0x08-0x0F */ _C, _S|_B, _S, _S, _S, _S, _C, _C,
    /* 0x10-0x17 */ _C, _C, _C, _C, _C, _C, _C, _C,
    /* 0x18-0x1F */ _C, _C, _C, _C, _C, _C, _C, _C,
    /* 0x20-0x27 */ _S|_B, _P, _P, _P, _P, _P, _P, _P,
    /* 0x28-0x2F */ _P, _P, _P, _P, _P, _P, _P, _P,
    /* 0x30-0x37 */ _N, _N, _N, _N, _N, _N, _N, _N,
    /* 0x38-0x3F */ _N, _N, _P, _P, _P, _P, _P, _P,
    /* 0x40-0x47 */ _P, _U|_X, _U|_X, _U|_X, _U|_X, _U|_X, _U|_X, _U,
    /* 0x48-0x4F */ _U, _U, _U, _U, _U, _U, _U, _U,
    /* 0x50-0x57 */ _U, _U, _U, _U, _U, _U, _U, _U,
    /* 0x58-0x5F */ _U, _U, _U, _P, _P, _P, _P, _P,
    /* 0x60-0x67 */ _P, _L|_X, _L|_X, _L|_X, _L|_X, _L|_X, _L|_X, _L,
    /* 0x68-0x6F */ _L, _L, _L, _L, _L, _L, _L, _L,
    /* 0x70-0x77 */ _L, _L, _L, _L, _L, _L, _L, _L,
    /* 0x78-0x7F */ _L, _L, _L, _P, _P, _P, _P, _C,
    /* 0x80-0xFF: non-ASCII, all zero */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
