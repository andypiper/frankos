/*
 * frankos_kbd.c - FRANK OS Keyboard Backend for Digger
 *
 * Replaces rp2350_kbd.c. Receives key events from FRANK OS WM_KEYDOWN/WM_KEYUP
 * messages and provides the same GetAsyncKeyState/kbhit/getkey interface.
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "m-os-api.h"
#include "frankos-app.h"

#undef switch
#undef inline
#undef __force_inline
#undef abs

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "def.h"
#include "input.h"
#include "frankos_app.h"

/* Global app state */
extern digger_app_t *g_app;

/* HID keycode constants (matching ps2kbd/hid_codes.h values) */
#define HID_KEY_A             0x04
#define HID_KEY_N             0x11
#define HID_KEY_S             0x16
#define HID_KEY_T             0x17
#define HID_KEY_W             0x1A
#define HID_KEY_Z             0x1D
#define HID_KEY_1             0x1E
#define HID_KEY_9             0x26
#define HID_KEY_0             0x27
#define HID_KEY_ENTER         0x28
#define HID_KEY_BACKSPACE     0x2A
#define HID_KEY_TAB           0x2B
#define HID_KEY_SPACE         0x2C
#define HID_KEY_DELETE        0x4C
#define HID_KEY_F1            0x3A
#define HID_KEY_F7            0x40
#define HID_KEY_F8            0x41
#define HID_KEY_F9            0x42
#define HID_KEY_F10           0x43
#define HID_KEY_ARROW_RIGHT   0x4F
#define HID_KEY_ARROW_LEFT    0x50
#define HID_KEY_ARROW_DOWN    0x51
#define HID_KEY_ARROW_UP      0x52
#define HID_KEY_KEYPAD_ADD       0x57
#define HID_KEY_KEYPAD_SUBTRACT  0x56

/*
 * Key mappings using HID keycodes.
 * keycodes[NKEYS][5]: up to 5 alternative keys per function.
 * -2 = unused slot.
 */
int keycodes[NKEYS][5] = {
    {HID_KEY_ARROW_RIGHT, -2, -2, -2, -2},  /* P1 Right */
    {HID_KEY_ARROW_UP,    -2, -2, -2, -2},  /* P1 Up */
    {HID_KEY_ARROW_LEFT,  -2, -2, -2, -2},  /* P1 Left */
    {HID_KEY_ARROW_DOWN,  -2, -2, -2, -2},  /* P1 Down */
    {HID_KEY_F1,          -2, -2, -2, -2},  /* P1 Fire */
    {HID_KEY_S,           -2, -2, -2, -2},  /* P2 Right */
    {HID_KEY_W,           -2, -2, -2, -2},  /* P2 Up */
    {HID_KEY_A,           -2, -2, -2, -2},  /* P2 Left */
    {HID_KEY_Z,           -2, -2, -2, -2},  /* P2 Down */
    {HID_KEY_TAB,         -2, -2, -2, -2},  /* P2 Fire */
    {HID_KEY_T,           -2, -2, -2, -2},  /* Cheat */
    {HID_KEY_KEYPAD_ADD,  -2, -2, -2, -2},  /* Accelerate */
    {HID_KEY_KEYPAD_SUBTRACT, -2, -2, -2, -2},  /* Brake */
    {HID_KEY_F7,          -2, -2, -2, -2},  /* Music toggle */
    {HID_KEY_F9,          -2, -2, -2, -2},  /* Sound toggle */
    {HID_KEY_F10,         -2, -2, -2, -2},  /* Exit */
    {HID_KEY_SPACE,       -2, -2, -2, -2},  /* Pause */
    {HID_KEY_N,           -2, -2, -2, -2},  /* Change mode */
    {HID_KEY_F8,          -2, -2, -2, -2},  /* Save DRF */
};

/*
 * GetAsyncKeyState - Check if a specific HID key is currently held.
 * Reads from the volatile key_state array set by WM_KEYDOWN/WM_KEYUP events.
 */
bool GetAsyncKeyState(int key) {
    if (key < 0 || key >= 256 || !g_app)
        return false;
    return g_app->key_state[key] != 0;
}

/*
 * initkeyb - Initialize keyboard state.
 */
void initkeyb(void) {
    if (!g_app)
        return;
    memset((void *)g_app->key_state, 0, sizeof(g_app->key_state));
    g_app->key_fifo_len = 0;
}

/*
 * restorekeyb - No-op on FRANK OS.
 */
void restorekeyb(void) {
}

/*
 * hid_to_ascii - Convert HID keycode to ASCII character.
 * Returns 0 for unmapped keys.
 */
static int16_t hid_to_ascii(int16_t hid) {
    if (hid >= HID_KEY_A && hid <= HID_KEY_A + 25)
        return 'A' + (hid - HID_KEY_A);
    if (hid >= HID_KEY_1 && hid <= HID_KEY_9)
        return '1' + (hid - HID_KEY_1);
    if (hid == HID_KEY_0)
        return '0';
    if (hid == HID_KEY_ENTER)
        return 13;
    if (hid == HID_KEY_BACKSPACE)
        return 8;
    if (hid == HID_KEY_DELETE)
        return 127;
    if (hid == HID_KEY_SPACE)
        return ' ';
    return 0;
}

/* Forward declarations */
void gethrt(bool);
bool kbhit(void);

/*
 * getkey - Block until a key is pressed.
 * If scancode=true, return raw HID scancode.
 * If scancode=false, return ASCII character.
 */
int16_t getkey(bool scancode) {
    int16_t result;

    while (!kbhit()) {
        if (g_app && g_app->closing)
            return HID_KEY_F10;  /* Return exit key if window closing */
        gethrt(true);
    }

    result = g_app->key_fifo[0];
    g_app->key_fifo_len--;
    if (g_app->key_fifo_len > 0)
        memmove(g_app->key_fifo, g_app->key_fifo + 1,
                g_app->key_fifo_len * sizeof(int16_t));

    if (!scancode)
        result = hid_to_ascii(result);

    return result;
}

/*
 * kbhit - Check if any key event is available.
 */
bool kbhit(void) {
    if (!g_app)
        return false;
    return g_app->key_fifo_len > 0;
}
