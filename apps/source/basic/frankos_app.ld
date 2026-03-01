/*
 * frankos_app.ld -- Minimal linker script for Frank OS relocatable (-r) apps
 *
 * Frank OS apps are built as relocatable ELF files; there are no fixed memory
 * regions.  The OS allocates memory and resolves all symbol addresses at load
 * time.  This file overrides the pico-sdk default linker script so that its
 * RAM/FLASH region limits are not applied during the -r link step.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
SECTIONS {}
