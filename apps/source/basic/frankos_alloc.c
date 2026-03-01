/*
 * frankos_alloc.c — Thin wrappers around Frank OS memory allocation API.
 *
 * PURPOSE:
 *   frankos_platform.c cannot include m-os-api.h because that header
 *   defines macros (fclose, FILE, fopen, …) and FatFS typedefs that
 *   conflict with MMBasic's <stdlib.h> / ffs.h includes.
 *
 *   At the same time, bare `extern void *pvPortMalloc(size_t)` declarations
 *   in frankos_platform.c create UNDEFINED ELF symbols (SHN_UNDEF, value=0).
 *   The Frank OS loader recognises SHN_UNDEF as an unsupported link target
 *   ("Unsupported link from STRTAB record #N to section #0 (UNDEF): …") and
 *   aborts loading ANY section that references them.  Those function sections
 *   are then mapped to address 0, and the first call crashes the task.
 *
 *   The correct way to reach OS services from an app is via _sys_table_ptrs[]:
 *
 *     pvPortMalloc  ← _sys_table_ptrs[32]  (__malloc  — PSRAM first, SRAM fallback)
 *     vPortFree     ← _sys_table_ptrs[33]  (__free    — frees PSRAM or SRAM block)
 *     pvPortCalloc  ← _sys_table_ptrs[166] (__calloc  — PSRAM first, SRAM fallback)
 *     __realloc     ← _sys_table_ptrs[303] (__realloc — PSRAM-aware grow)
 *
 *   _sys_table_ptrs is a pointer to the fixed address 0x10FFF000 (never an
 *   undefined symbol), so sections compiled here have NO SHN_UNDEF entries
 *   and load successfully.
 *
 * USAGE:
 *   frankos_platform.c and frankos_main.c declare:
 *     extern void *frankos_malloc (size_t sz);
 *     extern void *frankos_calloc (size_t n, size_t sz);
 *     extern void  frankos_free   (void *p);
 *     extern void *frankos_realloc(void *p, size_t sz);
 *   and call them in place of pvPortMalloc / pvPortCalloc / vPortFree.
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Include ONLY m-os-api.h here — no MMBasic headers, no standard FILE/fclose.
 * m-os-api.h defines pvPortMalloc/pvPortCalloc/vPortFree as inline functions
 * that call _sys_table_ptrs[N], dereference the fixed address 0x10FFF000, and
 * never produce SHN_UNDEF relocation entries.
 */

/* m-os-api.h defines:
 *   #define switch   DO_NOT_USE_SWITCH
 *   #define inline   __force_inline
 * Undefine them after the include so they don't leak into later headers. */
#include "m-os-api.h"
#undef switch
#undef inline
#undef __force_inline

#include <string.h>  /* memcpy — provided by pico-sdk, no conflicting macros */

/* ── frankos_malloc ─────────────────────────────────────────────────────── */
void *frankos_malloc(size_t sz)
{
    return pvPortMalloc(sz);
}

/* ── frankos_calloc ─────────────────────────────────────────────────────── */
void *frankos_calloc(size_t n, size_t sz)
{
    return pvPortCalloc(n, sz);
}

/* ── frankos_free ───────────────────────────────────────────────────────── */
void frankos_free(void *p)
{
    vPortFree(p);
}

/* ── frankos_realloc ────────────────────────────────────────────────────── */
/*
 * The OS provides __realloc at sys_table[303].  It is PSRAM-aware: for blocks
 * from psram_alloc it reads the size header at (p - 2*sizeof(size_t)) and
 * copies correctly.  For FreeRTOS SRAM blocks it calls pvPortRealloc.
 * Delegate to it directly rather than re-implementing the logic here.
 */
void *frankos_realloc(void *p, size_t sz)
{
    typedef void *(*realloc_fn_t)(void *, size_t);
    return ((realloc_fn_t)_sys_table_ptrs[303])(p, sz);
}
