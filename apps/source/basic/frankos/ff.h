/*
 * frankos/ff.h — Frank OS override for PicoMite's ff.h (FatFS)
 *
 * Strategy: include the original basic/ff.h unchanged for type definitions;
 * all function implementations (f_open, f_read…) are provided in frankos_ff.c
 * via the Frank OS sys_table.  Do NOT include m-os-api.h here — that would
 * conflict with <stdio.h> already pulled in by pico/stdlib.h.
 *
 * f_mount / f_getcwd / f_chdir are not in the sys_table; they are stubbed in
 * frankos_ff.c as no-ops / always-success.
 */
#ifndef _FRANKOS_FF_H
#define _FRANKOS_FF_H

/* Include the original FatFS ff.h from the basic/ source tree for types */
#include_next "ff.h"

#endif /* _FRANKOS_FF_H */
