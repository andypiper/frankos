/*
 * FRANK OS — Startup Sound
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STARTUP_SOUND_H
#define STARTUP_SOUND_H

/* Spawn a background FreeRTOS task that plays the embedded startup WAV
 * via the I2S driver, then cleans up and deletes itself. */
void startup_sound_start(void);

#endif /* STARTUP_SOUND_H */
