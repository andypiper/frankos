/*
 * FRANK OS — TinyUSB host-mode configuration for Adafruit Fruit Jam.
 *
 * The Fruit Jam has an on-board USB hub connected to the RP2350 USB
 * peripheral. RP2350 is the USB host; the two USB-A ports accept
 * standard HID keyboard and mouse devices.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

/* RP2350 has one USB port; run it as host */
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_HOST

/* No device-side CDC/MSC/HID */
#define CFG_TUD_ENABLED         0

/* Host HID: up to 4 devices (keyboard + mouse + 2 spare) */
#define CFG_TUH_ENABLED         1
#define CFG_TUH_HID             4

/* Enumeration buffer — generous to handle complex USB descriptors */
#define CFG_TUH_ENUMERATION_BUFSIZE 256

/* Use FreeRTOS-aware malloc if available */
#define CFG_TUSB_OS             OPT_OS_FREERTOS
#define CFG_TUSB_OS_INC_PATH    FreeRTOS.h

/* Debug level (0 = off) */
#define CFG_TUSB_DEBUG          0

/* Stack-allocated control buffer per device */
#define CFG_TUH_DEVICE_MAX      4

#endif /* _TUSB_CONFIG_H_ */
