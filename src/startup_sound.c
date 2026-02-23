/*
 * FRANK OS — Startup Sound
 * Copyright (c) 2025 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * Plays the embedded WAV file (8-bit unsigned PCM, mono, 22050 Hz) through
 * the PIO1 I2S driver at boot.  Runs as a one-shot FreeRTOS task that
 * streams audio via i2s_dma_write(), then cleans up and deletes itself.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "startup_sound.h"
#include "board_config.h"
#include "audio.h"

#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "FreeRTOS.h"
#include "task.h"

/* Embedded WAV (placed in .rodata / flash by startup_wav.S) */
extern const uint8_t  startup_wav_data[];
extern const uint32_t startup_wav_size;

/* Number of stereo frames per DMA chunk (~20 ms at 22050 Hz) */
#define CHUNK_FRAMES 441

/*--------------------------------------------------------------------------
 * Find the "data" sub-chunk inside a RIFF/WAVE file.
 *--------------------------------------------------------------------------*/
static const uint8_t *wav_find_data(const uint8_t *wav, uint32_t wav_len,
                                    uint32_t *out_len) {
    if (wav_len < 44) return NULL;
    if (memcmp(wav, "RIFF", 4) != 0) return NULL;
    if (memcmp(wav + 8, "WAVE", 4) != 0) return NULL;

    uint32_t pos = 12;
    while (pos + 8 <= wav_len) {
        uint32_t chunk_size = (uint32_t)wav[pos + 4]
                            | ((uint32_t)wav[pos + 5] << 8)
                            | ((uint32_t)wav[pos + 6] << 16)
                            | ((uint32_t)wav[pos + 7] << 24);
        if (memcmp(wav + pos, "data", 4) == 0) {
            uint32_t data_start = pos + 8;
            uint32_t data_len = chunk_size;
            if (data_start + data_len > wav_len)
                data_len = wav_len - data_start;
            *out_len = data_len;
            return wav + data_start;
        }
        pos += 8 + chunk_size;
        if (pos & 1) pos++;
    }
    return NULL;
}

/*--------------------------------------------------------------------------
 * Startup sound task — I2S playback via PIO1 + DMA
 *
 * Initialises its own I2S instance (PIO1), streams converted audio, then
 * tears down I2S so the PCM API can re-init later for app use.
 *--------------------------------------------------------------------------*/
static void startup_sound_task(void *params) {
    (void)params;

    /* Locate PCM data in the embedded WAV */
    uint32_t pcm_len = 0;
    const uint8_t *pcm = wav_find_data(startup_wav_data, startup_wav_size,
                                       &pcm_len);
    if (!pcm || pcm_len == 0) {
        vTaskDelete(NULL);
        return;
    }

    /* Configure I2S for 22050 Hz playback (mono WAV → stereo I2S) */
    i2s_config_t cfg = {
        .sample_freq     = 22050,
        .channel_count   = 2,
        .data_pin        = I2S_DATA_PIN,
        .clock_pin_base  = I2S_CLOCK_PIN_BASE,
        .pio             = pio1,
        .sm              = 0,
        .dma_channel     = 0,
        .dma_buf         = NULL,
        .dma_trans_count = CHUNK_FRAMES,
        .volume          = 0,
    };

    i2s_init(&cfg);

    /* Stream: convert 8-bit unsigned mono → 16-bit signed stereo in chunks,
     * then push each chunk to the I2S DMA pipeline. */
    int16_t chunk[CHUNK_FRAMES * 2];  /* L+R interleaved */
    uint32_t pos = 0;

    while (pos < pcm_len) {
        uint32_t frames = CHUNK_FRAMES;
        if (pos + frames > pcm_len)
            frames = pcm_len - pos;

        /* 8-bit unsigned (0..255) → 16-bit signed (-32768..32767), mono→stereo */
        for (uint32_t i = 0; i < frames; i++) {
            int16_t s = ((int16_t)pcm[pos + i] - 128) << 8;
            chunk[i * 2]     = s;
            chunk[i * 2 + 1] = s;
        }
        /* Pad remainder of last chunk with silence */
        for (uint32_t i = frames; i < CHUNK_FRAMES; i++) {
            chunk[i * 2]     = 0;
            chunk[i * 2 + 1] = 0;
        }

        i2s_dma_write(&cfg, chunk);
        pos += frames;
    }

    /* Flush: push silence through both DMA buffers so the last real audio
     * makes it all the way out before we tear down the hardware. */
    memset(chunk, 0, sizeof(chunk));
    i2s_dma_write(&cfg, chunk);
    i2s_dma_write(&cfg, chunk);

    i2s_deinit(&cfg);

    vTaskDelete(NULL);
}

void startup_sound_start(void) {
    xTaskCreate(startup_sound_task, "startsnd", 1024, NULL, 1, NULL);
}
