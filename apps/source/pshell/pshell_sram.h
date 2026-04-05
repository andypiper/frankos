/*
 * pshell_sram.h — SRAM-first allocator for pshell
 *
 * Uses pvPortMalloc (sys_table[529]) to allocate from FreeRTOS SRAM heap
 * directly, bypassing the default PSRAM-first strategy.  Code buffers MUST
 * live in SRAM for fast ARM execution; terminal buffers benefit from the
 * speed of SRAM as well.  Falls back to the default allocator (which tries
 * PSRAM first) when SRAM is full.
 *
 * __free (sys_table[33]) handles both SRAM and PSRAM pointers — psram_free
 * checks the address range internally.
 */

#ifndef PSHELL_SRAM_H
#define PSHELL_SRAM_H

#include <stddef.h>

/* _sys_table_ptrs is declared in m-os-api.h (included before this header).
 * We cast entries to function pointers to call pvPortMalloc / __malloc / __free. */

static inline void *sram_malloc(size_t sz) {
    typedef void *(*fn_t)(size_t);
    void *p = ((fn_t)(void*)_sys_table_ptrs[529])(sz);  /* pvPortMalloc (SRAM) */
    if (!p) p = ((fn_t)(void*)_sys_table_ptrs[32])(sz);  /* __malloc (PSRAM fallback) */
    return p;
}

static inline void sram_free(void *p) {
    typedef void (*fn_t)(void *);
    ((fn_t)(void*)_sys_table_ptrs[33])(p);               /* __free (handles both) */
}

#endif /* PSHELL_SRAM_H */
