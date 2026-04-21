# FRANK OS — USB HID Host Driver
# Provides keyboard and mouse input via USB Host
#
# Enable with: cmake -DUSB_HID_ENABLED=1
# NOTE: USB CDC stdio is DISABLED when USB HID is enabled.
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(usbhid INTERFACE)

if(USB_HID_ENABLED)
    message(STATUS "USB HID Host enabled")
    message(STATUS "  NOTE: USB CDC stdio will be DISABLED when USB HID is enabled!")

    target_sources(usbhid INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/hid_app.c
    )

    target_include_directories(usbhid INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_compile_definitions(usbhid INTERFACE
        USB_HID_ENABLED=1
    )

    target_link_libraries(usbhid INTERFACE
        tinyusb_host
        tinyusb_board
    )
else()
    message(STATUS "USB HID Host disabled (use -DUSB_HID_ENABLED=1 to enable)")
endif()
