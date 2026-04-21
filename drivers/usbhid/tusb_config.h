/*
 * FRANK OS — TinyUSB Host Configuration for USB HID (Keyboard/Mouse)
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 *
 * Only included when USB_HID_ENABLED is defined in CMake.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT 0
#endif

#define BOARD_TUH_MAX_SPEED   OPT_MODE_FULL_SPEED

#define CFG_TUH_ENABLED       1
#define CFG_TUD_ENABLED       0

#define CFG_TUH_MAX_SPEED     BOARD_TUH_MAX_SPEED

#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_DEVICE_MAX   5
#define CFG_TUH_HUB          1
#define CFG_TUH_HID          8

#define CFG_TUH_CDC          0
#define CFG_TUH_VENDOR       0
#define CFG_TUH_MSC          0

#define CFG_TUH_HID_EPIN_BUFSIZE  64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64

#ifdef __cplusplus
}
#endif

#endif
