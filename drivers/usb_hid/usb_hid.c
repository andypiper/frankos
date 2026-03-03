/*
 * FRANK OS — USB HID Host driver for Adafruit Fruit Jam
 * Copyright (c) 2025 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * USB HID host driver for the Fruit Jam board.
 *
 * Hardware:  CH334F 2-port USB hub connected to RP2350B PIO USB on
 *            GPIO 1 (D-) and GPIO 2 (D+).  5 V power controlled by GPIO 11.
 *
 * The driver uses TinyUSB in host mode (CFG_TUH_ENABLED=1) with the
 * pio_usb transport on the Fruit Jam USB host pins.  The RP2350's built-in
 * USB controller keeps running in device mode for CDC stdio via Type-C.
 *
 * Keyboard events are queued as synthetic "scancode" bytes:
 *   bit 7 = 0 → press,   bits 6-0 = HID keycode
 *   bit 7 = 1 → release, bits 6-0 = HID keycode
 *
 * keyboard.c reads these with usb_hid_kbd_get_byte() — the same call-site
 * as ps2_kbd_get_byte() — and feeds them through its existing HID→ASCII and
 * MOS2 conversion paths.  The PS/2→HID scancode translation is skipped
 * because TinyUSB already delivers native HID keycodes.
 */

#include "usb_hid.h"
#include "board_config.h"

#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"

/* pio_usb transport header — supplied by the tinyusb_host + pio_usb targets */
#include "pio_usb.h"

/*==========================================================================
 * Keyboard event ring buffer
 * Each entry encodes one key event:
 *   bit 7 = release flag,  bits 6-0 = HID keycode
 *==========================================================================*/

#define KBD_FIFO_SIZE 64

static uint8_t  kbd_fifo[KBD_FIFO_SIZE];
static uint32_t kbd_head = 0;
static uint32_t kbd_tail = 0;

static inline bool kbd_fifo_push(uint8_t b) {
    uint32_t next = (kbd_head + 1) % KBD_FIFO_SIZE;
    if (next == kbd_tail) return false; /* full */
    kbd_fifo[kbd_head] = b;
    kbd_head = next;
    return true;
}

static inline int kbd_fifo_pop(void) {
    if (kbd_head == kbd_tail) return -1;
    uint8_t b = kbd_fifo[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_FIFO_SIZE;
    return (int)(unsigned int)b;
}

/*==========================================================================
 * Previous keyboard report — used to detect press/release transitions
 *==========================================================================*/

static usb_hid_kbd_report_t prev_kbd  = {0};
static usb_hid_kbd_report_t latest_kbd = {0};
static bool                 kbd_report_ready = false;

/*==========================================================================
 * Mouse accumulated state
 *==========================================================================*/

static volatile int16_t  mouse_dx      = 0;
static volatile int16_t  mouse_dy      = 0;
static volatile int8_t   mouse_wheel   = 0;
static volatile uint8_t  mouse_buttons = 0;
static volatile bool     mouse_moved   = false;

/*==========================================================================
 * usb_hid_init
 *==========================================================================*/
bool usb_hid_init(void) {
    /* Enable USB host 5 V power */
    gpio_init(USB_HOST_5V_PIN);
    gpio_set_dir(USB_HOST_5V_PIN, GPIO_OUT);
    gpio_put(USB_HOST_5V_PIN, 1);

    /* Allow 5 V rail to settle before enumerating */
    sleep_ms(100);

    /*
     * Configure pio_usb transport.
     * The PIO USB host uses one PIO state machine and runs on Core 0 here;
     * it can also run on Core 1 if DVI output causes timing issues.
     */
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = USB_HOST_DP_PIN;  /* D+ pin; D- = DP-1 = GPIO 1 */

    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    if (!tuh_init(BOARD_TUH_RHPORT)) {
        printf("[usb_hid] tuh_init failed\n");
        return false;
    }

    printf("[usb_hid] PIO USB host initialised (D+=%d D-=%d 5V=%d)\n",
           USB_HOST_DP_PIN, USB_HOST_DM_PIN, USB_HOST_5V_PIN);
    return true;
}

/*==========================================================================
 * usb_hid_task — call regularly from FreeRTOS task
 *==========================================================================*/
void usb_hid_task(void) {
    tuh_task();
}

/*==========================================================================
 * Keyboard public API
 *==========================================================================*/

int usb_hid_kbd_get_byte(void) {
    return kbd_fifo_pop();
}

bool usb_hid_kbd_get_report(usb_hid_kbd_report_t *out) {
    if (!kbd_report_ready) return false;
    *out = latest_kbd;
    kbd_report_ready = false;
    return true;
}

/*==========================================================================
 * Mouse public API
 *==========================================================================*/

bool usb_hid_mouse_get_state(int16_t *dx, int16_t *dy,
                              int8_t *wheel, uint8_t *buttons) {
    if (!mouse_moved && mouse_buttons == 0) return false;

    uint32_t saved = save_and_disable_interrupts();
    *dx      = mouse_dx;
    *dy      = mouse_dy;
    *wheel   = mouse_wheel;
    *buttons = mouse_buttons;
    mouse_dx    = 0;
    mouse_dy    = 0;
    mouse_wheel = 0;
    mouse_moved = false;
    restore_interrupts(saved);

    return true;
}

/*==========================================================================
 * TinyUSB HID host callbacks
 *
 * These are called by tuh_task() when HID reports arrive.
 *==========================================================================*/

/* Called when a HID device is mounted */
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *desc_report, uint16_t desc_len) {
    (void)desc_report; (void)desc_len;
    uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);
    printf("[usb_hid] HID device %u.%u mounted (protocol=%u)\n",
           dev_addr, instance, proto);

    /* Start receiving reports */
    tuh_hid_receive_report(dev_addr, instance);
}

/* Called when a HID device is unmounted */
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    printf("[usb_hid] HID device %u.%u unmounted\n", dev_addr, instance);
}

/*--------------------------------------------------------------------------
 * Keyboard report processing
 *
 * Compares the new report against the previous one to generate
 * press/release events and push them to the FIFO.
 *--------------------------------------------------------------------------*/
static void process_kbd_report(hid_keyboard_report_t const *report) {
    usb_hid_kbd_report_t new_rep;
    new_rep.modifier = report->modifier;
    memcpy(new_rep.keycode, report->keycode, 6);

    /* Detect newly-released keys (were in prev, not in new) */
    for (int i = 0; i < 6; i++) {
        uint8_t kc = prev_kbd.keycode[i];
        if (kc == HID_KEY_NONE) continue;
        bool still_held = false;
        for (int j = 0; j < 6; j++) {
            if (new_rep.keycode[j] == kc) { still_held = true; break; }
        }
        if (!still_held) {
            /* Key released: bit 7 set */
            kbd_fifo_push(0x80u | (kc & 0x7Fu));
        }
    }

    /* Detect newly-pressed keys (in new, not in prev) */
    for (int i = 0; i < 6; i++) {
        uint8_t kc = new_rep.keycode[i];
        if (kc == HID_KEY_NONE) continue;
        bool was_held = false;
        for (int j = 0; j < 6; j++) {
            if (prev_kbd.keycode[j] == kc) { was_held = true; break; }
        }
        if (!was_held) {
            /* Key pressed: bit 7 clear */
            kbd_fifo_push(kc & 0x7Fu);
        }
    }

    /* Handle modifier transitions as synthetic keycodes (0x70-0x77) */
    uint8_t mod_changed = prev_kbd.modifier ^ new_rep.modifier;
    if (mod_changed) {
        /* Map modifier bits → HID modifier keycodes */
        static const uint8_t mod_kc[8] = {
            HID_KEY_CONTROL_LEFT,  HID_KEY_SHIFT_LEFT,
            HID_KEY_ALT_LEFT,      HID_KEY_GUI_LEFT,
            HID_KEY_CONTROL_RIGHT, HID_KEY_SHIFT_RIGHT,
            HID_KEY_ALT_RIGHT,     HID_KEY_GUI_RIGHT,
        };
        for (int b = 0; b < 8; b++) {
            if (!(mod_changed & (1u << b))) continue;
            uint8_t kc = mod_kc[b];
            if (new_rep.modifier & (1u << b)) {
                kbd_fifo_push(kc & 0x7Fu);       /* press */
            } else {
                kbd_fifo_push(0x80u | (kc & 0x7Fu)); /* release */
            }
        }
    }

    prev_kbd = new_rep;
    latest_kbd = new_rep;
    kbd_report_ready = true;
}

/* Called when a HID report is received */
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                 uint8_t const *report, uint16_t len) {
    uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);

    if (proto == HID_ITF_PROTOCOL_KEYBOARD && len >= sizeof(hid_keyboard_report_t)) {
        process_kbd_report((hid_keyboard_report_t const *)report);
    } else if (proto == HID_ITF_PROTOCOL_MOUSE && len >= sizeof(hid_mouse_report_t)) {
        hid_mouse_report_t const *m = (hid_mouse_report_t const *)report;
        uint32_t saved = save_and_disable_interrupts();
        mouse_dx      += m->x;
        mouse_dy      += m->y;
        mouse_wheel   += m->wheel;
        mouse_buttons  = m->buttons;
        mouse_moved    = true;
        restore_interrupts(saved);
    }

    /* Re-arm for the next report */
    tuh_hid_receive_report(dev_addr, instance);
}
