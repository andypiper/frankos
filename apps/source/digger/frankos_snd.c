/*
 * frankos_snd.c - FRANK OS Sound Backend for Digger
 *
 * Replaces rp2350_snd.c. Uses FRANK OS pcm_init/pcm_write for I2S audio.
 * Audio is initialized in frankos_main.c; this file handles per-frame
 * sample generation and submission.
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "m-os-api.h"
#include "frankos-app.h"

#undef switch
#undef inline
#undef __force_inline
#undef abs

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "def.h"
#include "device.h"
#include "newsnd.h"
#include "frankos_app.h"

bool wave_device_available = false;

/* Global app state */
extern digger_app_t *g_app;

/*
 * setsounddevice - Initialize audio (PCM already set up in main).
 */
bool setsounddevice(uint16_t samprate, uint16_t bufsize) {
    (void)samprate;
    (void)bufsize;
    if (g_app) {
        g_app->audio_initialized = true;
        wave_device_available = true;
    }
    return true;
}

/*
 * initsounddevice - Stub (init done in main).
 */
bool initsounddevice(void) {
    return true;
}

/*
 * pausesounddevice - Enable/disable audio output.
 */
void pausesounddevice(bool p) {
    if (g_app)
        g_app->audio_paused = p;
}

/*
 * audio_fill_and_submit - Generate audio samples and submit to PCM.
 *
 * Called once per game frame from the main loop.
 * Generates DIGGER_AUDIO_SAMPLES_PER_FRAME mono samples via getsample(),
 * duplicates to stereo, and submits via pcm_write() which blocks for pacing.
 */
void audio_fill_and_submit(void) {
    if (!g_app || !g_app->audio_initialized || !g_app->audio_buf)
        return;

    if (g_app->audio_paused) {
        /* Fill silence and still submit for pacing */
        memset(g_app->audio_buf, 0,
               DIGGER_AUDIO_SAMPLES_PER_FRAME * 2 * sizeof(int16_t));
    } else {
        for (int i = 0; i < DIGGER_AUDIO_SAMPLES_PER_FRAME; i++) {
            int16_t s = getsample();
            g_app->audio_buf[i * 2] = s;      /* Left */
            g_app->audio_buf[i * 2 + 1] = s;  /* Right (mono->stereo) */
        }
    }

    pcm_write(g_app->audio_buf, DIGGER_AUDIO_SAMPLES_PER_FRAME);
}
