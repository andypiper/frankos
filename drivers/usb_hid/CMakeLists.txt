add_library(usb_hid_driver STATIC
    usb_hid.c
)

target_include_directories(usb_hid_driver PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(usb_hid_driver
    pico_stdlib
    hardware_gpio
    tinyusb_host
    tinyusb_board
    FreeRTOS-Kernel-Heap4
)
