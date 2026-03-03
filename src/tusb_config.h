/*
 * FRANK OS — TinyUSB configuration
 * Copyright (c) 2025 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * TinyUSB configuration for FRANK OS.
 *
 * M2 board:    USB device mode only (CDC stdio via Type-C / USB-C).
 * Fruit Jam:   USB device mode (CDC stdio, RP2350B built-in USB, Type-C)
 *              + USB host mode (PIO USB on GPIO 1/2 → CH334F hub →
 *                keyboard + mouse).
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------
 * Common controller settings
 *------------------------------------------------------------------*/
#ifndef CFG_TUSB_MCU
#  define CFG_TUSB_MCU OPT_MCU_RP2350
#endif

#ifndef CFG_TUSB_OS
#  define CFG_TUSB_OS OPT_OS_FREERTOS
#endif

/*------------------------------------------------------------------
 * Device mode — always enabled for CDC stdio
 *------------------------------------------------------------------*/
#define CFG_TUD_ENABLED     1
#define CFG_TUD_RHPORT      0   /* RP2350 built-in USB on rhport 0 */

/* CDC (serial) — one instance for stdio */
#define CFG_TUD_CDC         1
#define CFG_TUD_CDC_RX_BUFSIZE  256
#define CFG_TUD_CDC_TX_BUFSIZE  256

/* No other device classes needed */
#define CFG_TUD_MSC         0
#define CFG_TUD_HID         0
#define CFG_TUD_MIDI        0
#define CFG_TUD_VENDOR      0

/*------------------------------------------------------------------
 * Host mode — Fruit Jam only (PIO USB on GPIO 1/2)
 *------------------------------------------------------------------*/
#ifdef FRANK_BOARD_FRUIT_JAM

#define CFG_TUH_ENABLED     1
#define BOARD_TUH_RHPORT    1   /* pio_usb runs on rhport 1 */

/* Support up to 4 USB devices (hub + 2 ports + extra) */
#define CFG_TUH_DEVICE_MAX  4

/* Hub support: the CH334F is a 2-port hub */
#define CFG_TUH_HUB         1

/* HID: keyboard + mouse (2 interfaces minimum) */
#define CFG_TUH_HID         4

#else /* BOARD_M2 */

#define CFG_TUH_ENABLED     0

#endif /* FRANK_BOARD_FRUIT_JAM */

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
