#
# FRANK OS — USB HID host driver (Fruit Jam board support)
# Copyright (c) 2025 Mikhail Matveev <xtreme@rh1.tech>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

if(NOT TARGET usb_hid_host)
    add_library(usb_hid_host INTERFACE)

    target_sources(usb_hid_host INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/usb_hid.c
    )

    target_include_directories(usb_hid_host INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    # TinyUSB host + PIO USB transport
    target_link_libraries(usb_hid_host INTERFACE
        pico_stdlib
        hardware_gpio
        tinyusb_host
        tinyusb_board
    )
endif()
