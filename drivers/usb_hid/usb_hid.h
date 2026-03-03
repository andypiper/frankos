/*
 * FRANK OS — USB HID Host driver for Adafruit Fruit Jam
 * Copyright (c) 2025 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * USB HID host input driver for the Fruit Jam board.
 *
 * The Fruit Jam has a CH334F 2-port USB hub connected to the RP2350B via
 * PIO USB on GPIO 1 (D-) and GPIO 2 (D+).  Standard USB keyboards and mice
 * plug into the Type-A ports of that hub.
 *
 * This driver runs TinyUSB in host mode over PIO USB and exposes the same
 * interface that the rest of FRANK OS expects from the PS/2 driver:
 *
 *   - usb_hid_init()               — initialise PIO USB host
 *   - usb_hid_task()               — must be called regularly (replaces tuh_task)
 *   - usb_hid_kbd_get_byte()       — raw HID keycode byte (for keyboard.c)
 *   - usb_hid_kbd_get_report()     — latest full keyboard HID report
 *   - usb_hid_mouse_get_state()    — accumulated mouse deltas + buttons
 */

#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------
 * Keyboard report — mirrors TinyUSB hid_keyboard_report_t
 *-------------------------------------------------------------------------*/
typedef struct {
    uint8_t modifier;   /* HID modifier byte (shift/ctrl/alt/gui bits) */
    uint8_t keycode[6]; /* Up to 6 simultaneous HID keycodes */
} usb_hid_kbd_report_t;

/*---------------------------------------------------------------------------
 * Initialisation
 *
 * Configures PIO USB host on USB_HOST_DM_PIN / USB_HOST_DP_PIN,
 * enables USB host 5 V via USB_HOST_5V_PIN, and starts TinyUSB.
 *-------------------------------------------------------------------------*/
bool usb_hid_init(void);

/*---------------------------------------------------------------------------
 * Periodic service — call from a FreeRTOS task (replaces tuh_task()).
 * Also pumps the keyboard event ring buffer.
 *-------------------------------------------------------------------------*/
void usb_hid_task(void);

/*---------------------------------------------------------------------------
 * Keyboard — raw HID keycode stream (one byte per event, sign bit = release)
 *
 * Returns a synthetic "scancode" byte compatible with the upper layers:
 *   bit 7 = 0  → key press,   bits 6-0 = HID keycode
 *   bit 7 = 1  → key release, bits 6-0 = HID keycode
 * Returns -1 when the queue is empty.
 *
 * keyboard.c reads these via usb_hid_kbd_get_byte() the same way it reads
 * ps2_kbd_get_byte() on M2, but skips the PS/2→HID conversion step.
 *-------------------------------------------------------------------------*/
int  usb_hid_kbd_get_byte(void);

/*---------------------------------------------------------------------------
 * Keyboard — latest full HID report (for modifier tracking)
 *-------------------------------------------------------------------------*/
bool usb_hid_kbd_get_report(usb_hid_kbd_report_t *out);

/*---------------------------------------------------------------------------
 * Mouse — accumulated state since last call (resets deltas on read)
 *
 * Compatible with ps2_mouse_get_state() so input_task needs no changes.
 *-------------------------------------------------------------------------*/
bool usb_hid_mouse_get_state(int16_t *dx, int16_t *dy,
                              int8_t  *wheel, uint8_t *buttons);

#ifdef __cplusplus
}
#endif

#endif /* USB_HID_H */
