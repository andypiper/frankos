/*
 * frankos/class/cdc/cdc_device.h — TinyUSB CDC stub for Frank OS
 *
 * PicoMite.c includes class/cdc/cdc_device.h (without USBKEYBOARD) to use
 * USB CDC serial output. On Frank OS there is no CDC device; all CDC calls
 * return neutral values so the paths are never taken at runtime.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FRANKOS_CDC_DEVICE_H
#define FRANKOS_CDC_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

/* Minimal CDC device API declarations (matching TinyUSB signatures). */
static inline bool     tud_cdc_connected(void)                             { return false; }
static inline uint32_t tud_cdc_available(void)                             { return 0; }
static inline uint32_t tud_cdc_read(void *buffer, uint32_t bufsize)        { (void)buffer; (void)bufsize; return 0; }
static inline uint16_t tud_cdc_get_line_state(void)                        { return 0; }
static inline uint32_t tud_cdc_write_available(void)                       { return 0; }
static inline uint32_t tud_cdc_write_char(char ch)                         { (void)ch; return 1; }
static inline uint32_t tud_cdc_write_flush(void)                           { return 0; }

/* Additional CDC queries sometimes referenced. */
static inline uint32_t tud_cdc_read_flush(void)                            { return 0; }
static inline uint32_t tud_cdc_write(const void *buf, uint32_t bufsize)    { (void)buf; (void)bufsize; return 0; }

#endif /* FRANKOS_CDC_DEVICE_H */
