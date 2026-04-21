/*
 * FRANK OS — USB HID host input driver (Adafruit Fruit Jam).
 *
 * Receives TinyUSB HID keyboard and mouse reports via callbacks and
 * converts them to FRANK OS key_event_t / mouse-delta format, matching
 * exactly what the PS/2 driver produced on the M2 board.
 *
 * Keyboard: boot-protocol 8-byte report → HID keycode + modifier mask.
 * ASCII translation is handled by the existing keyboard.c layer (unchanged).
 *
 * Mouse: boot-protocol 3-byte report → relative dx/dy/wheel/buttons.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "usb_hid.h"
#include "keyboard.h"
#include "tusb.h"
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * Key event ring buffer (keyboard → input_task)
 * -------------------------------------------------------------------------- */

#define KEY_QUEUE_SIZE 32

static key_event_t  key_queue[KEY_QUEUE_SIZE];
static volatile int key_head = 0;
static volatile int key_tail = 0;

static void key_enqueue(const key_event_t *ev) {
    int next = (key_head + 1) % KEY_QUEUE_SIZE;
    if (next == key_tail) return; /* overflow — drop */
    key_queue[key_head] = *ev;
    key_head = next;
}

bool usb_hid_get_key_event(key_event_t *out) {
    if (key_tail == key_head) return false;
    *out = key_queue[key_tail];
    key_tail = (key_tail + 1) % KEY_QUEUE_SIZE;
    return true;
}

/* --------------------------------------------------------------------------
 * HID modifier byte → FRANK OS modifier mask
 * -------------------------------------------------------------------------- */

static uint8_t hid_mod_to_frank(uint8_t hid_mod) {
    uint8_t m = 0;
    if (hid_mod & 0x01) m |= KBD_MOD_LCTRL;
    if (hid_mod & 0x02) m |= KBD_MOD_LSHIFT;
    if (hid_mod & 0x04) m |= KBD_MOD_LALT;
    if (hid_mod & 0x08) m |= KBD_MOD_LGUI;
    if (hid_mod & 0x10) m |= KBD_MOD_RCTRL;
    if (hid_mod & 0x20) m |= KBD_MOD_RSHIFT;
    if (hid_mod & 0x40) m |= KBD_MOD_RALT;
    if (hid_mod & 0x80) m |= KBD_MOD_RGUI;
    return m;
}

/* --------------------------------------------------------------------------
 * HID keycode → ASCII (mirrors keyboard.c translation table)
 *
 * USB HID boot-protocol reports deliver raw HID keycodes.  We replicate the
 * same ASCII mapping that keyboard.c uses for PS/2 scancodes so the rest of
 * the OS sees identical events regardless of input device.
 * -------------------------------------------------------------------------- */

static const char hid_to_ascii_lower[128] = {
    /* 0x00 */ 0,0,0,0,
    /* 0x04 a-z */ 'a','b','c','d','e','f','g','h','i','j','k','l','m',
                   'n','o','p','q','r','s','t','u','v','w','x','y','z',
    /* 0x1e 1-0 */ '1','2','3','4','5','6','7','8','9','0',
    /* 0x28 */'\r','\033','\b','\t',' ','-','=','[',']','\\',0,';','\'','`',',','.','/',
    /* 0x39 caps */ 0,
    /* 0x3a-0x45 F1-F12 */ 0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x47 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x54 numpad */ '/','*','-','+','\r',
    /* 0x59 kp1-kp9 */ '1','2','3','4','5','6','7','8','9','0','.', 0,
    /* 0x65 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x7f */ 0
};

static const char hid_to_ascii_upper[128] = {
    /* 0x00 */ 0,0,0,0,
    /* 0x04 A-Z */ 'A','B','C','D','E','F','G','H','I','J','K','L','M',
                   'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    /* 0x1e !@#$% */  '!','@','#','$','%','^','&','*','(',')',
    /* 0x28 */ '\r','\033','\b','\t',' ','_','+','{','}','|',0,':','"','~','<','>','?',
    /* 0x39 */ 0,
    /* 0x3a-0x45 */ 0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x47 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x54 */ '/','*','-','+','\r',
    /* 0x59 */ '1','2','3','4','5','6','7','8','9','0','.', 0,
    /* 0x65 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x7f */ 0
};

static char keycode_to_ascii(uint8_t keycode, uint8_t modifiers) {
    if (keycode >= 128) return 0;
    bool shifted = (modifiers & (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)) != 0;
    char c = shifted ? hid_to_ascii_upper[keycode] : hid_to_ascii_lower[keycode];
    return c;
}

/* --------------------------------------------------------------------------
 * Previous keyboard state (to detect key-up events from boot report)
 * -------------------------------------------------------------------------- */

#define MAX_ROLLOVER 6

static uint8_t prev_modifiers = 0;
static uint8_t prev_keycodes[MAX_ROLLOVER] = {0};

/* --------------------------------------------------------------------------
 * Mouse state
 * -------------------------------------------------------------------------- */

static volatile int16_t  mouse_dx       = 0;
static volatile int16_t  mouse_dy       = 0;
static volatile int8_t   mouse_wheel    = 0;
static volatile uint8_t  mouse_buttons  = 0;
static volatile bool     mouse_changed  = false;

bool usb_hid_get_mouse_delta(int16_t *dx, int16_t *dy,
                              int8_t *wheel, uint8_t *buttons) {
    if (!mouse_changed) return false;
    /* Atomic read — disable IRQ briefly (we're on Cortex-M33) */
    uint32_t saved = save_and_disable_interrupts();
    *dx       = mouse_dx;
    *dy       = mouse_dy;
    *wheel    = mouse_wheel;
    *buttons  = mouse_buttons;
    mouse_dx  = 0;
    mouse_dy  = 0;
    mouse_wheel  = 0;
    mouse_changed = false;
    restore_interrupts(saved);
    return true;
}

/* --------------------------------------------------------------------------
 * Public init
 * -------------------------------------------------------------------------- */

void usb_hid_init(void) {
    memset(prev_keycodes, 0, sizeof(prev_keycodes));
    prev_modifiers = 0;
    mouse_changed  = false;
}

/* --------------------------------------------------------------------------
 * TinyUSB host callbacks
 * -------------------------------------------------------------------------- */

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *desc_report, uint16_t desc_len) {
    (void)desc_report; (void)desc_len;
    /* Request boot-protocol reports for simplicity */
    tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);
    tuh_hid_receive_report(dev_addr, instance);
    printf("USB HID: device %u instance %u mounted\n", dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)dev_addr; (void)instance;
    printf("USB HID: device %u instance %u unmounted\n", dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                 uint8_t const *report, uint16_t len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD && len >= 8) {
        /* Boot keyboard report: [modifier, reserved, keycode0..keycode5] */
        uint8_t modifiers = hid_mod_to_frank(report[0]);
        uint8_t keycodes[MAX_ROLLOVER];
        memcpy(keycodes, report + 2, MAX_ROLLOVER);

        /* Detect modifier key-up / key-down transitions */
        static const struct { uint8_t bit; uint8_t hid; } mod_map[] = {
            { KBD_MOD_LCTRL,  0xE0 }, { KBD_MOD_LSHIFT, 0xE1 },
            { KBD_MOD_LALT,   0xE2 }, { KBD_MOD_LGUI,   0xE3 },
            { KBD_MOD_RCTRL,  0xE4 }, { KBD_MOD_RSHIFT, 0xE5 },
            { KBD_MOD_RALT,   0xE6 }, { KBD_MOD_RGUI,   0xE7 },
        };
        for (int i = 0; i < 8; i++) {
            bool was = (prev_modifiers & mod_map[i].bit) != 0;
            bool now = (modifiers      & mod_map[i].bit) != 0;
            if (was != now) {
                key_event_t ev = {
                    .hid_code  = mod_map[i].hid,
                    .ascii     = 0,
                    .modifiers = modifiers,
                    .pressed   = now,
                };
                key_enqueue(&ev);
            }
        }

        /* Detect regular key releases (in prev but not in current) */
        for (int p = 0; p < MAX_ROLLOVER; p++) {
            uint8_t kc = prev_keycodes[p];
            if (kc == 0) continue;
            bool still_held = false;
            for (int c = 0; c < MAX_ROLLOVER; c++) {
                if (keycodes[c] == kc) { still_held = true; break; }
            }
            if (!still_held) {
                key_event_t ev = {
                    .hid_code  = kc,
                    .ascii     = 0,
                    .modifiers = modifiers,
                    .pressed   = false,
                };
                key_enqueue(&ev);
            }
        }

        /* Detect new key presses (in current but not in prev) */
        for (int c = 0; c < MAX_ROLLOVER; c++) {
            uint8_t kc = keycodes[c];
            if (kc == 0) continue;
            bool is_new = true;
            for (int p = 0; p < MAX_ROLLOVER; p++) {
                if (prev_keycodes[p] == kc) { is_new = false; break; }
            }
            if (is_new) {
                key_event_t ev = {
                    .hid_code  = kc,
                    .ascii     = keycode_to_ascii(kc, modifiers),
                    .modifiers = modifiers,
                    .pressed   = true,
                };
                key_enqueue(&ev);
            }
        }

        prev_modifiers = modifiers;
        memcpy(prev_keycodes, keycodes, MAX_ROLLOVER);

    } else if (itf_protocol == HID_ITF_PROTOCOL_MOUSE && len >= 3) {
        /* Boot mouse report: [buttons, dx, dy] (dy: up = negative in HID) */
        uint32_t saved = save_and_disable_interrupts();
        mouse_buttons = report[0] & 0x07;
        mouse_dx     += (int8_t)report[1];
        mouse_dy     += (int8_t)report[2];
        if (len >= 4) mouse_wheel += (int8_t)report[3];
        mouse_changed = true;
        restore_interrupts(saved);
    }

    /* Re-arm for next report */
    tuh_hid_receive_report(dev_addr, instance);
}
