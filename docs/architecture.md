# FRANK OS Architecture

## Overview

FRANK OS is a FreeRTOS-based desktop operating system for the RP2350 microcontroller. It runs on a single RP2350B chip with 520 KB SRAM, 4 MB flash, optional QSPI PSRAM, and DVI output via the HSTX peripheral.

## Core Architecture

```
 Core 0 (FreeRTOS)                    Core 1 (bare metal)
 +--------------------------+         +-------------------+
 | USB task (highest prio)  |         | DispHSTX DVI      |
 | Input task (PS/2 poll)   |         | scanline renderer  |
 | Compositor task          |         +-------------------+
 | Heartbeat task           |
 | Shell tasks (per window) |
 | App tasks (ELF binaries) |
 +--------------------------+
```

### Task Priorities

| Task | Priority | Description |
|------|----------|-------------|
| **USB** | Highest | TinyUSB CDC serial console |
| **Input** | 3 | PS/2 keyboard and mouse polling, event routing |
| **Compositor** | 2 | Window manager event dispatch and screen compositing |
| **Heartbeat** | 1 | LED blink (system alive indicator) |
| **Shell** | Per-terminal | Command-line interpreter |
| **App** | Per-app | ELF binary execution context |

## Memory Map

### Flash (4 MB, 0x10000000 - 0x103FFFFF)

```
0x10000000  +-----------------------+
            |  Pico SDK boot stage  |
            +-----------------------+
            |  FRANK OS firmware    |
            |  (code + rodata)      |
            +-----------------------+
0x10EFF000  |  ZERO_BLOCK           |
0x10F00000  |  OS code top region   |
0x10FFF000  |  sys_table (4 KB)     |  <-- MOS2 API entry point
0x10FFFFFF  +-----------------------+
```

### SRAM (520 KB)

```
+---------------------------+
|  FreeRTOS heap (Heap4)    |
|  Task stacks              |
|  Framebuffer (150 KB)     |
|    640x480 @ 4bpp         |
|    2 pixels/byte          |
|    stride = 320 bytes     |
|  Window manager state     |
|  Terminal buffers          |
|  Static BSS               |
+---------------------------+
```

### PSRAM (optional, up to 16 MB)

Used for ELF application memory. Accessed via QSPI on CS1, auto-detected at boot. If not present, the allocator returns NULL and apps fall back to internal SRAM.

## Display Pipeline

```
 App code
   |
   v
 display_set_pixel() / display_hline_fast() / display_blit_glyph_8wide()
   |
   v
 Framebuffer (uint8_t[], 4bpp packed, 320 bytes/row)
   |
   v
 DispHSTX library (Core 1)
   |
   v
 HSTX peripheral -> DVI TMDS output -> Monitor
```

- **Resolution:** 640x480 @ 60 Hz
- **Color depth:** 4-bit paletted (16-color CGA/EGA palette)
- **Pixel packing:** 2 pixels per byte (high nibble = even x, low nibble = odd x)
- **Palette:** RGB565 converted from RGB888 at init, passed to DispHSTX

## Window Manager

The window manager (`window.c`, `window_event.c`, `window_draw.c`) implements a Win95-style overlapping window system:

- Up to 16 simultaneous windows
- Z-order managed by `z_order` field
- Each window has: outer frame rect, flags, title, event/paint callbacks
- Client area computed from frame minus decorations (title bar, borders, menu bar)

### Compositing

The compositor runs in its own FreeRTOS task and:
1. Dispatches queued input events to focused window
2. Repaints dirty windows back-to-front
3. Draws window decorations (title bar, borders, buttons)
4. Renders the taskbar
5. Stamps the mouse cursor overlay onto the show buffer

### Event Flow

```
PS/2 interrupt -> Input task -> Event queue -> Compositor task
                                                    |
                                         window event_handler()
                                                    |
                                         App-specific logic
                                                    |
                                         wm_invalidate() -> repaint
```

## Application Model

### ELF Loading

FRANK OS loads standalone ARM ELF binaries from the SD card. Apps are compiled against `apps/api/frankos-app.h` which provides inline wrappers that call through the MOS2 sys_table at `0x10FFF000`.

### sys_table

The sys_table is a fixed-address array of function pointers. Each OS API function has a reserved index. Apps call OS functions by reading the pointer at a known index and calling through it. This decouples app binaries from OS binary layout.

Key index ranges:
- **0-100**: MOS2 core (filesystem, memory, tasks)
- **404-433**: FRANK OS GUI (window management, drawing)
- **439-440**: File dialogs
- **441-442**: Buttons, direct framebuffer access
- **451-454**: Clipboard
- **455-459**: Scrollbar control
- **460-474**: Textarea control
- **475-481**: File save dialog, find/replace
- **494-505**: File associations, desktop shortcuts, deferred launch
- **506-511**: MIDI/OPL audio

### App Lifecycle

1. User launches app (shell, file manager, or file association)
2. OS creates a FreeRTOS task, loads ELF into PSRAM
3. App's `main()` runs, creates a window via `wm_create_window()`
4. App handles events in its `event_handler` callback
5. App paints its client area in its `paint_handler` callback
6. On close: `wm_destroy_window()`, task exits, memory freed

### App Exports

Apps can export special symbols:
- `__app_flags` — returns `APPFLAG_*` bitfield (background, singleton)

## File System

- **FAT32** via SPI-connected SD card
- **FatFs** library for filesystem operations
- **SD card layout:**
  ```
  /fos/           FRANK OS apps (.elf binaries, .inf metadata, .ico icons)
  /mos2/          MOS2 compatibility utilities
  ```

## Audio

- **I2S output** via PIO on GPIO 20-21
- **PWM beeper** on GPIO 22
- **Codecs:** Helix MP3, HxCMod tracker, OPL FM synth (MIDI)
- Audio runs in the app task context; rendering fills I2S DMA buffers

## Source Organization

```
src/                    FRANK OS kernel and UI
  main.c                Entry point, task creation
  display.c/h           Framebuffer and DispHSTX interface
  gfx.c/h               Drawing primitives
  window.c/h             Window data structures and management
  window_event.c/h      Input routing and drag/resize
  window_draw.c/h       Clipped drawing context for windows
  window_theme.h        Win95-style metrics and hit testing
  cursor.c/h            Mouse cursor with save-under overlay
  terminal.c/h          VT100 terminal emulator
  shell.c/h             Built-in command interpreter
  app.c/h               ELF loader and app task management
  sys_table.c/h         MOS2 API function table
  filemanager.c/h       Graphical file browser
  taskbar.c/h           Taskbar with clock and window buttons
  startmenu.c/h         Start menu popup
  menu.c/h              Generic menu rendering
  controls.c/h          Reusable UI widgets (textfield, scrollbar, etc.)
  settings.c/h          Persistent settings (SD card backed)

drivers/                Hardware abstraction
  ps2/                  PS/2 keyboard and mouse (PIO-based)
  fatfs/                FatFs filesystem library
  sdcard/               SD card SPI driver (PIO-based)
  psram/                QSPI PSRAM allocator
  audio/                I2S PIO program

lib/                    Third-party libraries (git submodules)
  DispHSTX/             DVI display driver
  FreeRTOS-Kernel/      Real-time OS kernel
  helix/                MP3 decoder
  hxcmod/               MOD tracker player
  opl/                  OPL FM synthesizer

apps/                   Standalone applications
  api/frankos-app.h     App development header
  source/               App source code (8 apps)
  build_apps.sh         Build script
```
