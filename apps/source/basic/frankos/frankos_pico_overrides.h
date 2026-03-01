/*
 * frankos_pico_overrides.h — Forced-include header for Frank OS ELF apps
 *
 * Included via -include BEFORE all other headers (including pico SDK).
 * Overrides pico SDK macros that place code in .time_critical sections.
 *
 * Why: ELF apps run entirely from PSRAM.  There is no flash-vs-RAM
 * distinction.  The .time_critical sections produce 170+ tiny separate
 * ELF sections that slow down loading and get placed at address 0
 * (invalid for PSRAM execution).  Making them no-ops puts all code
 * in .text where the OS loader handles it correctly.
 */
#ifndef _FRANKOS_PICO_OVERRIDES_H
#define _FRANKOS_PICO_OVERRIDES_H

/* Override BEFORE pico/platform/sections.h is included. */
#define __not_in_flash(group)
#define __not_in_flash_func(func_name) func_name
#define __time_critical_func(func_name) func_name
#define __no_inline_not_in_flash_func(func_name) __attribute__((noinline)) func_name

/* Prevent pico SDK from redefining these. */
#define _PICO_PLATFORM_SECTIONS_H

/*
 * Hardware functions that must be no-ops for PSRAM-loaded ELF apps.
 * Normally defined in FileIO.c, these access QMI PSRAM controller
 * registers and SPI flash — both fatal from userspace.
 *
 * We declare them here as static-inline no-ops.  Since this header
 * is force-included BEFORE FileIO.c, the preprocessor definitions
 * below replace the function CALLS (not definitions).
 *
 * The actual definitions in FileIO.c are guarded with #ifdef _FRANKOS.
 */

#endif /* _FRANKOS_PICO_OVERRIDES_H */
