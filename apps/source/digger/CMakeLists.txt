set(PICO_BOARD pico2 CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(digger C CXX ASM)
pico_sdk_init()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_C_STANDARD 11)

set(COMPILED_DIR "${CMAKE_SOURCE_DIR}/../../../sdcard/fos")

# Game logic sources (platform-independent)
set(GAME_SOURCES
    main.c
    game.c
    digger.c
    monster.c
    bags.c
    bullet_obj.c
    drawing.c
    sprite.c
    cgagrafx.c
    input.c
    keyboard.c
    sound.c
    soundgen.c
    newsnd.c
    record.c
    spinlock.c
    scores.c
    ini.c
    alpha.c
    title_gz.c
    digger_obj.c
    monster_obj.c
)

# FRANK OS platform sources (replace rp2350_*.c)
set(FRANKOS_SOURCES
    frankos_main.c
    frankos_vid.c
    frankos_kbd.c
    frankos_snd.c
    frankos_timer.c
    frankos_math.c
    frankos_libc.c
)

add_executable(${PROJECT_NAME}
    ${GAME_SOURCES}
    ${FRANKOS_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../api/m-os-api-sdtfn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../api/m-os-api-math.c
)

target_include_directories(${PROJECT_NAME} BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../api
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../api/FreeRTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../api
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_options(${PROJECT_NAME} PRIVATE
    -Xlinker
    --print-memory-usage
    -r
)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
target_compile_options(${PROJECT_NAME} PUBLIC
    -O2
    -fno-builtin-printf
    -fno-jump-tables
    -Werror=implicit-function-declaration
    -include _ansi.h
)
endif()

# Read version from version.txt ("major minor" format)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../../version.txt" _VERSION_LINE)
list(GET _VERSION_LINE 0 _VERSION_LINE)
string(REPLACE " " ";" _VERSION_PARTS "${_VERSION_LINE}")
list(GET _VERSION_PARTS 0 FRANK_VER_MAJOR)
list(GET _VERSION_PARTS 1 FRANK_VER_MINOR)
if(FRANK_VER_MINOR LESS 10)
    set(FRANK_VERSION_STR "${FRANK_VER_MAJOR}.0${FRANK_VER_MINOR}")
else()
    set(FRANK_VERSION_STR "${FRANK_VER_MAJOR}.${FRANK_VER_MINOR}")
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    configSTACK_DEPTH_TYPE=uint32_t
    FRANK_VERSION_STR="${FRANK_VERSION_STR}"
    _RP2350
    _FRANKOS
)

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 0)

# Generate digger.inf (text-only: app name)
set(GEN_INF "${CMAKE_CURRENT_SOURCE_DIR}/gen_inf.py")

add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/digger.inf"
    COMMAND ${Python3_EXECUTABLE} "${GEN_INF}" "Digger"
        "${CMAKE_BINARY_DIR}/digger.inf"
    DEPENDS "${GEN_INF}"
    COMMENT "Generating digger.inf"
)
add_custom_target(digger_inf ALL
    DEPENDS "${CMAKE_BINARY_DIR}/digger.inf"
)
add_dependencies(${PROJECT_NAME} digger_inf)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>
        ${COMPILED_DIR}/digger
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_BINARY_DIR}/digger.inf"
        ${COMPILED_DIR}/digger.inf
)
