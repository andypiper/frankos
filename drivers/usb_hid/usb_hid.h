/*
 * FRANK OS — USB HID host input driver (Adafruit Fruit Jam).
 *
 * Replaces the PS/2 driver on the M2 board. Receives keyboard and mouse
 * reports from TinyUSB and converts them to the same key_event_t and
 * mouse-delta types that the existing input_task consumes.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>
#include <stdbool.h>
#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise internal state. Called once before the scheduler starts. */
void usb_hid_init(void);

/*
 * Consume the next queued keyboard event.
 * Returns true and fills *out if an event was waiting; false if queue empty.
 * Safe to call from input_task (FreeRTOS context).
 */
bool usb_hid_get_key_event(key_event_t *out);

/*
 * Consume accumulated mouse movement since the last call.
 * Returns true if the mouse state changed (moved or button transition).
 * dx/dy are relative pixel deltas; wheel is scroll delta; buttons is a
 * bitmask (bit 0 = left, bit 1 = right, bit 2 = middle).
 */
bool usb_hid_get_mouse_delta(int16_t *dx, int16_t *dy,
                              int8_t *wheel, uint8_t *buttons);

#ifdef __cplusplus
}
#endif

#endif /* USB_HID_H */
