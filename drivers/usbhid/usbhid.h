/*
 * FRANK OS — USB HID Host Driver
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 *
 * Provides keyboard and mouse input via USB Host.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef USBHID_H
#define USBHID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t dx;
    int16_t dy;
    int8_t  wheel;
    uint8_t buttons;    /* bit 0=left, 1=right, 2=middle */
    int     has_motion;
} usbhid_mouse_state_t;

void usbhid_init(void);
void usbhid_task(void);

int usbhid_keyboard_connected(void);
int usbhid_mouse_connected(void);

/*
 * Get next keyboard action from queue.
 * keycode: HID usage code (0x04=A, 0xE0=LCtrl, etc.)
 * down: 1=pressed, 0=released
 * Returns 1 if an action was available.
 */
int usbhid_get_key_action(uint8_t *keycode, int *down);

/*
 * Get accumulated mouse state and reset deltas.
 */
void usbhid_get_mouse_state(usbhid_mouse_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
