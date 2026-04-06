/*
 * MIDI file parser — adapted from Chocolate Doom / murmdoom
 * Copyright(C) 2005-2014 Simon Howard
 * Copyright(C) 2021-2022 Graham Sanderson
 *
 * Stripped of Doom dependencies.  Uses FatFS for initial file load.
 * The entire file is loaded into RAM so that event parsing during
 * playback never touches the SD card (avoids periodic GC stalls).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdlib.h>
#include <string.h>
#include "opl_alloc.h"
#include "ff.h"
#include "midifile.h"

/* Byte-swap helpers (MIDI is big-endian) */
static inline uint16_t swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}
static inline uint32_t swap32(uint32_t v) {
    return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
           ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000u);
}

#define HEADER_CHUNK_ID "MThd"
#define TRACK_CHUNK_ID  "MTrk"

#pragma pack(push, 1)
typedef struct {
    uint8_t  chunk_id[4];
    uint32_t chunk_size;
} chunk_header_t;

typedef struct {
    chunk_header_t chunk_header;
    uint16_t format_type;
    uint16_t num_tracks;
    uint16_t time_division;
} midi_header_t;
#pragma pack(pop)

/* ------------------------------------------------------------------ */
/* In-memory cursor — replaces FatFS streaming during playback        */
/* ------------------------------------------------------------------ */

typedef struct {
    const uint8_t *data;
    uint32_t       size;
    uint32_t       pos;
} mem_cursor_t;

static inline bool mc_read(mem_cursor_t *mc, void *dst, uint32_t n) {
    if (mc->pos + n > mc->size) return false;
    memcpy(dst, mc->data + mc->pos, n);
    mc->pos += n;
    return true;
}

static inline bool mc_read_byte(mem_cursor_t *mc, uint8_t *out) {
    if (mc->pos >= mc->size) return false;
    *out = mc->data[mc->pos++];
    return true;
}

static inline void mc_seek(mem_cursor_t *mc, uint32_t pos) {
    mc->pos = pos;
}

static inline uint32_t mc_tell(mem_cursor_t *mc) {
    return mc->pos;
}

/* ------------------------------------------------------------------ */
/* Track / file structures                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned int data_len;

    /* Ping-pong event buffers: events[next_idx] = lookahead,
     * events[next_idx ^ 1] = last returned (still valid for caller) */
    midi_event_t  events[2];
    uint8_t       next_idx;     /* index of the lookahead event */
    bool          has_next;

    /* Memory-cursor streaming state (offsets into file->buf) */
    uint32_t     file_pos;
    uint32_t     initial_file_pos;
    unsigned int last_event_type;
    bool         end_of_track;
} midi_track_t;

struct midi_file_s {
    midi_header_t header;
    midi_track_t *tracks;
    unsigned int  num_tracks;

    /* Entire MIDI file loaded into RAM */
    uint8_t      *buf;
    uint32_t      buf_size;
};

struct midi_track_iter_s {
    midi_track_t *track;
    midi_file_t  *file;
    unsigned int  track_num;
};

/* ------------------------------------------------------------------ */
/* Memory-based byte reading helpers                                   */
/* ------------------------------------------------------------------ */

static bool ReadByte(uint8_t *result, mem_cursor_t *mc) {
    return mc_read_byte(mc, result);
}

static bool ReadVariableLength(uint32_t *result, mem_cursor_t *mc) {
    uint8_t b;
    *result = 0;
    for (int i = 0; i < 4; i++) {
        if (!ReadByte(&b, mc))
            return false;
        *result = (*result << 7) | (b & 0x7f);
        if ((b & 0x80) == 0)
            return true;
    }
    return false;
}

static void *ReadByteSequence(unsigned int num_bytes, mem_cursor_t *mc) {
    uint8_t *result = malloc(num_bytes + 1);
    if (!result) return NULL;
    if (!mc_read(mc, result, num_bytes)) {
        free(result);
        return NULL;
    }
    return result;
}

/* ------------------------------------------------------------------ */
/* Event readers                                                       */
/* ------------------------------------------------------------------ */

static bool ReadChannelEvent(midi_event_t *event, uint8_t event_type,
                             bool two_param, mem_cursor_t *mc) {
    uint8_t b;
    event->event_type = (midi_event_type_t)(event_type & 0xf0);
    event->data.channel.channel = event_type & 0x0f;
    if (!ReadByte(&b, mc)) return false;
    event->data.channel.param1 = b;
    if (two_param) {
        if (!ReadByte(&b, mc)) return false;
        event->data.channel.param2 = b;
    } else {
        event->data.channel.param2 = 0;
    }
    return true;
}

static bool ReadSysExEvent(midi_event_t *event, int event_type,
                           mem_cursor_t *mc) {
    uint32_t length;
    event->event_type = (midi_event_type_t)event_type;
    if (!ReadVariableLength(&length, mc)) return false;
    event->data.sysex.length = length;
    event->data.sysex.data = NULL;
    if (length > 0)
        mc_seek(mc, mc_tell(mc) + length);
    return true;
}

static bool ReadMetaEvent(midi_event_t *event, mem_cursor_t *mc) {
    uint8_t b;
    uint32_t length;
    event->event_type = MIDI_EVENT_META;
    if (!ReadByte(&b, mc)) return false;
    event->data.meta.type = b;
    if (!ReadVariableLength(&length, mc)) return false;
    event->data.meta.length = length;
    /* Only store data for SET_TEMPO events */
    if (b == MIDI_META_SET_TEMPO && length == 3) {
        event->data.meta.data = ReadByteSequence(length, mc);
        if (!event->data.meta.data) return false;
    } else {
        event->data.meta.data = NULL;
        if (length > 0)
            mc_seek(mc, mc_tell(mc) + length);
    }
    return true;
}

static bool ReadEvent(midi_event_t *event, unsigned int *last_event_type,
                      mem_cursor_t *mc) {
    uint8_t event_type;
    if (!ReadVariableLength(&event->delta_time, mc)) return false;
    if (!ReadByte(&event_type, mc)) return false;

    if ((event_type & 0x80) == 0) {
        event_type = (uint8_t)*last_event_type;
        mc_seek(mc, mc_tell(mc) - 1);
    } else {
        *last_event_type = event_type;
    }

    switch (event_type & 0xf0) {
        case MIDI_EVENT_NOTE_OFF:
        case MIDI_EVENT_NOTE_ON:
        case MIDI_EVENT_AFTERTOUCH:
        case MIDI_EVENT_CONTROLLER:
        case MIDI_EVENT_PITCH_BEND:
            return ReadChannelEvent(event, event_type, true, mc);
        case MIDI_EVENT_PROGRAM_CHANGE:
        case MIDI_EVENT_CHAN_AFTERTOUCH:
            return ReadChannelEvent(event, event_type, false, mc);
        default:
            break;
    }
    switch (event_type) {
        case MIDI_EVENT_SYSEX:
        case MIDI_EVENT_SYSEX_SPLIT:
            return ReadSysExEvent(event, event_type, mc);
        case MIDI_EVENT_META:
            return ReadMetaEvent(event, mc);
        default:
            break;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Free meta/sysex data from a single event                            */
/* ------------------------------------------------------------------ */

static void FreeEventData(midi_event_t *event) {
    if (event->event_type == MIDI_EVENT_META) {
        free(event->data.meta.data);
        event->data.meta.data = NULL;
    } else if (event->event_type == MIDI_EVENT_SYSEX ||
               event->event_type == MIDI_EVENT_SYSEX_SPLIT) {
        free(event->data.sysex.data);
        event->data.sysex.data = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Read the next event for a track into the lookahead slot             */
/* ------------------------------------------------------------------ */

static void ReadAheadEvent(midi_file_t *file, midi_track_t *track) {
    if (track->end_of_track || !file->buf) {
        track->has_next = false;
        return;
    }

    midi_event_t *ev = &track->events[track->next_idx];

    /* Free any previous data in this slot */
    FreeEventData(ev);

    /* Set up memory cursor at this track's read position */
    mem_cursor_t mc = { file->buf, file->buf_size, track->file_pos };

    memset(ev, 0, sizeof(*ev));
    if (!ReadEvent(ev, &track->last_event_type, &mc)) {
        track->has_next = false;
        track->end_of_track = true;
        return;
    }

    /* Save cursor position for next read */
    track->file_pos = mc_tell(&mc);
    track->has_next = true;

    if (ev->event_type == MIDI_EVENT_META &&
        ev->data.meta.type == MIDI_META_END_OF_TRACK) {
        track->end_of_track = true;
    }
}

/* ------------------------------------------------------------------ */
/* Reset a track's streaming state                                     */
/* ------------------------------------------------------------------ */

static void ResetTrack(midi_file_t *file, midi_track_t *track) {
    FreeEventData(&track->events[0]);
    FreeEventData(&track->events[1]);
    memset(track->events, 0, sizeof(track->events));
    track->next_idx = 0;
    track->has_next = false;
    track->file_pos = track->initial_file_pos;
    track->last_event_type = 0;
    track->end_of_track = false;

    /* Read first event into lookahead */
    ReadAheadEvent(file, track);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

midi_file_t *MIDI_LoadFile(const char *filename) {
    midi_file_t *file = calloc(1, sizeof(midi_file_t));
    if (!file) return NULL;

    FIL fil;
    if (f_open(&fil, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        free(file);
        return NULL;
    }

    /* Load entire file into RAM — MIDI files are small (typically < 100 KB) */
    file->buf_size = (uint32_t)f_size(&fil);
    file->buf = malloc(file->buf_size);
    if (!file->buf) {
        f_close(&fil);
        free(file);
        return NULL;
    }

    UINT br;
    if (f_read(&fil, file->buf, file->buf_size, &br) != FR_OK ||
        br != file->buf_size) {
        f_close(&fil);
        free(file->buf);
        free(file);
        return NULL;
    }
    f_close(&fil);

    /* Parse header from the in-memory buffer */
    mem_cursor_t mc = { file->buf, file->buf_size, 0 };

    if (!mc_read(&mc, &file->header, sizeof(midi_header_t)))
        goto fail;
    if (memcmp(file->header.chunk_header.chunk_id, HEADER_CHUNK_ID, 4) != 0)
        goto fail;

    file->num_tracks = swap16(file->header.num_tracks);
    if (file->num_tracks == 0)
        goto fail;

    file->tracks = calloc(file->num_tracks, sizeof(midi_track_t));
    if (!file->tracks) goto fail;

    /* Read each track header and record buffer offset */
    for (unsigned int i = 0; i < file->num_tracks; i++) {
        midi_track_t *track = &file->tracks[i];
        chunk_header_t hdr;
        if (!mc_read(&mc, &hdr, sizeof(hdr)))
            goto fail;
        if (memcmp(hdr.chunk_id, TRACK_CHUNK_ID, 4) != 0)
            goto fail;
        track->data_len = swap32(hdr.chunk_size);
        track->initial_file_pos = mc_tell(&mc);
        track->file_pos = track->initial_file_pos;
        /* Skip track data */
        mc_seek(&mc, mc_tell(&mc) + track->data_len);
    }

    return file;

fail:
    if (file->tracks) free(file->tracks);
    free(file->buf);
    free(file);
    return NULL;
}

void MIDI_FreeFile(midi_file_t *file) {
    if (!file) return;
    if (file->tracks) {
        for (unsigned int i = 0; i < file->num_tracks; i++) {
            FreeEventData(&file->tracks[i].events[0]);
            FreeEventData(&file->tracks[i].events[1]);
        }
        free(file->tracks);
    }
    free(file->buf);
    free(file);
}

unsigned int MIDI_GetFileTimeDivision(midi_file_t *file) {
    return swap16(file->header.time_division);
}

unsigned int MIDI_NumTracks(midi_file_t *file) {
    return file->num_tracks;
}

midi_track_iter_t *MIDI_IterateTrack(midi_file_t *file, unsigned int track_num) {
    if (track_num >= file->num_tracks) return NULL;
    midi_track_iter_t *iter = calloc(1, sizeof(midi_track_iter_t));
    if (!iter) return NULL;
    iter->track = &file->tracks[track_num];
    iter->file = file;
    iter->track_num = track_num;

    /* Reset and read first event */
    ResetTrack(file, iter->track);

    return iter;
}

void MIDI_FreeIterator(midi_track_iter_t *iter) {
    free(iter);
}

unsigned int MIDI_GetDeltaTime(midi_track_iter_t *iter) {
    if (!iter->track->has_next)
        return 0;
    return iter->track->events[iter->track->next_idx].delta_time;
}

int MIDI_GetNextEvent(midi_track_iter_t *iter, midi_event_t **event) {
    midi_track_t *track = iter->track;
    if (!track->has_next)
        return 0;

    /* Return pointer to current lookahead event */
    uint8_t cur_idx = track->next_idx;
    *event = &track->events[cur_idx];

    /* Toggle to the other slot for the next lookahead */
    track->next_idx ^= 1;

    /* Read ahead the next event into the new slot */
    if (!track->end_of_track) {
        ReadAheadEvent(iter->file, track);
    } else {
        track->has_next = false;
    }

    return 1;
}

void MIDI_RestartIterator(midi_track_iter_t *iter) {
    ResetTrack(iter->file, iter->track);
}
