/*
 * psx_str.h — PSX STR/BS video + XA-ADPCM audio decoder (single header)
 *
 * Decodes PlayStation 1 .STR streaming video files:
 *   - STR container demuxer (2336-byte sectors, interleaved A/V)
 *   - BS (bitstream) video decoder (MDEC DCT intra-frame codec)
 *   - XA-ADPCM audio decoder (4-bit ADPCM → PCM int16_t)
 *
 * Output: YCbCr 4:2:0 planes (same layout as pl_mpeg) + PCM audio.
 * Every frame is an I-frame — no reference buffers needed.
 *
 * Usage:
 *   #define PSX_STR_IMPLEMENTATION
 *   #include "psx_str.h"
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PSX_STR_H
#define PSX_STR_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifndef PSX_MALLOC
#define PSX_MALLOC(sz)  malloc(sz)
#endif
#ifndef PSX_FREE
#define PSX_FREE(p)     free(p)
#endif

/* ======================================================================
 * Public types
 * ====================================================================== */

typedef struct {
    unsigned int width;
    unsigned int height;
    uint8_t *data;
} psx_plane_t;

typedef struct {
    unsigned int width;
    unsigned int height;
    psx_plane_t y;
    psx_plane_t cb;
    psx_plane_t cr;
} psx_frame_t;

typedef struct {
    int16_t samples[4032];  /* max: 18 groups × 4 units × 28 × 2ch interleaved */
    int count;              /* sample frames (stereo pairs for stereo) */
    int channels;           /* 1 or 2 */
    int sample_rate;        /* 18900 or 37800 */
} psx_audio_t;

/* Opaque decoder state */
typedef struct psx_str_t psx_str_t;

/* Callbacks */
typedef void (*psx_video_cb)(psx_str_t *str, psx_frame_t *frame, void *user);
typedef void (*psx_audio_cb)(psx_str_t *str, psx_audio_t *audio, void *user);

/* File I/O callbacks (same pattern as pl_mpeg) */
typedef int  (*psx_read_cb)(void *buf, uint32_t size, void *user);
typedef void (*psx_seek_cb)(uint32_t offset, void *user);
typedef uint32_t (*psx_tell_cb)(void *user);

/* ======================================================================
 * Public API
 * ====================================================================== */

psx_str_t *psx_str_create(psx_read_cb read, psx_seek_cb seek,
                           psx_tell_cb tell, void *user);
void       psx_str_destroy(psx_str_t *s);

void psx_str_set_video_callback(psx_str_t *s, psx_video_cb cb, void *user);
void psx_str_set_audio_callback(psx_str_t *s, psx_audio_cb cb, void *user);

/* Decode next sector. Returns 1 if ok, 0 if EOF. */
int  psx_str_decode_sector(psx_str_t *s);

/* Get video dimensions (available after first video sector). */
int  psx_str_get_width(psx_str_t *s);
int  psx_str_get_height(psx_str_t *s);

/* Get frame rate — STR is fixed 15fps (150 sectors/sec, ~10 sectors/frame) */
int  psx_str_get_fps(psx_str_t *s);

#endif /* PSX_STR_H */


/* ======================================================================
 * Implementation
 * ====================================================================== */

#ifdef PSX_STR_IMPLEMENTATION

/* ---------------- STR sector format ---------------- */

/* Each 2336-byte raw sector:
 *   [0..7]   CD-XA subheader (file, channel, submode, coding) × 2
 *   [8..39]  STR video header (for video sectors) — 32 bytes
 *   [40..2335] BS frame data (2296 bytes) or padding
 * For audio sectors:
 *   [8..2335] XA-ADPCM sound groups (18 × 128 = 2304 bytes + 24 padding) */

#define STR_SECTOR_RAW    2336
#define STR_SUBHDR_SIZE   8       /* CD-XA subheader (repeated pair) */
#define STR_VID_HDR_SIZE  32      /* video sector header */
#define STR_VID_DATA      2016    /* BS data per video sector (conservative) */
#define STR_VIDEO_ID      0x0160

/* CD-XA submode bits (byte 2 of subheader) */
#define XA_SUBMODE_VIDEO  0x02
#define XA_SUBMODE_AUDIO  0x04
#define XA_SUBMODE_DATA   0x08
#define XA_SUBMODE_EOF    0x80

/* XA-ADPCM coding byte bits (byte 3 of subheader) */
#define XA_STEREO      0x01
#define XA_37800HZ     0x04
#define XA_8BIT        0x10

/* ---------------- BS bitstream reader ---------------- */

typedef struct {
    const uint16_t *data;   /* BS data as 16-bit words (little-endian) */
    int             len;    /* total 16-bit words */
    int             pos;    /* current word index */
    uint32_t        bits;   /* bit accumulator */
    int             nbits;  /* bits remaining in accumulator */
} bs_reader_t;

/* ---------------- MDEC VLC tables ---------------- */

/* ---------------- Decoder state ---------------- */

#define PSX_MAX_WIDTH   320
#define PSX_MAX_HEIGHT  240
#define PSX_MAX_MBS_X   (PSX_MAX_WIDTH / 16)
#define PSX_MAX_MBS_Y   (PSX_MAX_HEIGHT / 16)
#define PSX_FRAME_BUF   (128 * 1024)  /* max BS frame data */

struct psx_str_t {
    /* I/O */
    psx_read_cb  read;
    psx_seek_cb  seek;
    psx_tell_cb  tell;
    void        *io_user;

    /* Callbacks */
    psx_video_cb video_cb;
    void        *video_user;
    psx_audio_cb audio_cb;
    void        *audio_user;

    /* Video state */
    int      width, height;
    int      fps;
    uint32_t cur_frame;         /* frame number being assembled */
    int      sectors_received;  /* sectors collected for current frame */
    int      sectors_needed;    /* total sectors for current frame */
    uint8_t *frame_buf;         /* reassembly buffer for BS data */
    int      frame_buf_len;     /* bytes accumulated */
    int      quant_scale;
    int      bs_version;

    /* Decoded YCbCr output */
    psx_frame_t frame;

    /* DCT block workspace */
    int32_t block[64];

    /* Audio state */
    psx_audio_t audio;
    int16_t     xa_prev[2][2];  /* [channel][prev0, prev1] */

    /* Sector read buffer */
    uint8_t sector[STR_SECTOR_RAW];
};

/* ---- Helpers ---- */

static uint16_t psx_le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t psx_le32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/* ---- MDEC quantization matrix (standard JPEG zigzag order) ---- */

static const uint8_t psx_quant_matrix[64] = {
     2, 16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83
};

static const uint8_t psx_zigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/* ---- BS bitstream reader ---- */

static void bs_init(bs_reader_t *r, const uint16_t *data, int words) {
    r->data  = data;
    r->len   = words;
    r->pos   = 0;
    r->bits  = 0;
    r->nbits = 0;
}

static void bs_fill(bs_reader_t *r) {
    while (r->nbits <= 16 && r->pos < r->len) {
        r->bits |= (uint32_t)r->data[r->pos++] << (16 - r->nbits);
        r->nbits += 16;
    }
}

static uint32_t bs_peek(bs_reader_t *r, int n) {
    if (r->nbits < n) bs_fill(r);
    return (r->bits >> (32 - n)) & ((1u << n) - 1);
}

static void bs_skip(bs_reader_t *r, int n) {
    if (r->nbits < n) bs_fill(r);
    r->bits <<= n;
    r->nbits -= n;
}

static uint32_t bs_read(bs_reader_t *r, int n) {
    uint32_t v = bs_peek(r, n);
    bs_skip(r, n);
    return v;
}

static int32_t bs_read_signed(bs_reader_t *r, int n) {
    int32_t v = (int32_t)bs_read(r, n);
    if (v >= (1 << (n - 1)))
        v -= (1 << n);
    return v;
}

/* ---- MDEC AC VLC decoding ----
 * 10-bit lookup table for O(1) decode of most common codes (2-10 bits).
 * Longer codes (12-16 bits) fall through to a small linear scan.
 * Each LUT entry: bits[15:12]=code_len, bits[11:6]=run, bits[5:0]=|level|.
 * len=0 means "not in LUT, need longer decode". */

#define VLC_LUT_BITS 10
#define VLC_LUT_SIZE (1 << VLC_LUT_BITS)
#define VLC_PACK(len, run, lev) (((len)<<12)|((run)<<6)|(lev))

static uint16_t psx_vlc_lut[VLC_LUT_SIZE];
static int psx_vlc_lut_ready = 0;

static void psx_vlc_init_lut(void) {
    if (psx_vlc_lut_ready) return;
    memset(psx_vlc_lut, 0, sizeof(psx_vlc_lut));

    static const struct { uint16_t pat; uint8_t len, run, lev; } codes[] = {
        {0x03,2,0,1},{0x03,3,1,1},{0x04,4,0,2},{0x05,4,2,1},
        {0x05,5,0,3},{0x06,5,4,1},{0x07,5,3,1},
        {0x04,6,7,1},{0x05,6,6,1},{0x06,6,1,2},{0x07,6,5,1},
        {0x04,7,2,2},{0x05,7,9,1},{0x06,7,0,4},{0x07,7,8,1},
        {0x20,8,13,1},{0x21,8,0,6},{0x22,8,12,1},{0x23,8,11,1},
        {0x24,8,3,2},{0x25,8,1,3},{0x26,8,0,5},{0x27,8,10,1},
        {0x08,10,16,1},{0x09,10,5,2},{0x0A,10,0,7},{0x0B,10,2,3},
        {0x0C,10,1,4},{0x0D,10,15,1},{0x0E,10,14,1},{0x0F,10,4,2},
    };

    for (int i = 0; i < (int)(sizeof(codes)/sizeof(codes[0])); i++) {
        int len = codes[i].len;
        uint16_t pat = codes[i].pat;
        uint16_t entry = VLC_PACK(len, codes[i].run, codes[i].lev);
        /* Fill all LUT entries that start with this code pattern */
        int shift = VLC_LUT_BITS - len;
        int base = pat << shift;
        int count = 1 << shift;
        for (int j = 0; j < count; j++)
            psx_vlc_lut[base + j] = entry;
    }
    psx_vlc_lut_ready = 1;
}

/* Longer codes (12-16 bit) — rare, small linear scan */
static const struct { uint16_t pat; uint8_t len, run, lev; } vlc_long[] = {
    {0x10,12,0,11},{0x11,12,8,2},{0x12,12,4,3},{0x13,12,0,10},
    {0x14,12,2,4},{0x15,12,7,2},{0x16,12,21,1},{0x17,12,20,1},
    {0x18,12,0,9},{0x19,12,19,1},{0x1A,12,18,1},{0x1B,12,1,5},
    {0x1C,12,3,3},{0x1D,12,0,8},{0x1E,12,6,2},{0x1F,12,17,1},
    {0x10,13,10,2},{0x11,13,9,2},{0x12,13,5,3},{0x13,13,3,4},
    {0x14,13,2,5},{0x15,13,1,7},{0x16,13,1,6},{0x17,13,0,15},
    {0x18,13,0,14},{0x19,13,0,13},{0x1A,13,0,12},{0x1B,13,26,1},
    {0x1C,13,25,1},{0x1D,13,24,1},{0x1E,13,23,1},{0x1F,13,22,1},
    {0x10,14,0,31},{0x11,14,0,30},{0x12,14,0,29},{0x13,14,0,28},
    {0x14,14,0,27},{0x15,14,0,26},{0x16,14,0,25},{0x17,14,0,24},
    {0x18,14,0,23},{0x19,14,0,22},{0x1A,14,0,21},{0x1B,14,0,20},
    {0x1C,14,0,19},{0x1D,14,0,18},{0x1E,14,0,17},{0x1F,14,0,16},
    {0x10,15,0,40},{0x11,15,0,39},{0x12,15,0,38},{0x13,15,0,37},
    {0x14,15,0,36},{0x15,15,0,35},{0x16,15,0,34},{0x17,15,0,33},
    {0x18,15,0,32},{0x19,15,1,14},{0x1A,15,1,13},{0x1B,15,1,12},
    {0x1C,15,1,11},{0x1D,15,1,10},{0x1E,15,1,9},{0x1F,15,1,8},
    {0x10,16,1,18},{0x11,16,1,17},{0x12,16,1,16},{0x13,16,1,15},
    {0x14,16,6,3},{0x15,16,16,2},{0x16,16,15,2},{0x17,16,14,2},
    {0x18,16,13,2},{0x19,16,12,2},{0x1A,16,11,2},{0x1B,16,31,1},
    {0x1C,16,30,1},{0x1D,16,29,1},{0x1E,16,28,1},{0x1F,16,27,1},
};

static int bs_decode_ac(bs_reader_t *r, int *run, int *level) {
    if (r->nbits < 17) bs_fill(r);
    uint32_t top = r->bits >> 16;

    /* Escape code: 000001 (6 bits) */
    if ((top >> 10) == 0x01) {
        bs_skip(r, 6);
        *run = (int)bs_read(r, 6);
        *level = (int)bs_read_signed(r, 10);
        return 0;
    }

    /* Fast LUT for codes ≤ 10 bits */
    uint16_t entry = psx_vlc_lut[top >> (16 - VLC_LUT_BITS)];
    if (entry) {
        int len = entry >> 12;
        bs_skip(r, len);
        int lv = entry & 0x3F;
        if (bs_read(r, 1)) lv = -lv;
        *run = (entry >> 6) & 0x3F;
        *level = lv;
        return 0;
    }

    /* Longer codes (12-16 bits) — linear scan over small table */
    int nlong = (int)(sizeof(vlc_long) / sizeof(vlc_long[0]));
    for (int i = 0; i < nlong; i++) {
        int len = vlc_long[i].len;
        if ((top >> (16 - len)) == vlc_long[i].pat) {
            bs_skip(r, len);
            int lv = vlc_long[i].lev;
            if (bs_read(r, 1)) lv = -lv;
            *run = vlc_long[i].run;
            *level = lv;
            return 0;
        }
    }

    bs_skip(r, 1);
    *run = 0; *level = 0;
    return -1;
}

/* ---- IDCT (simple integer, row-column) ---- */

/* AAN fast IDCT — same algorithm as pl_mpeg. Only 2 multiplies per 1D pass.
 * Adapted from pl_mpeg's plm_video_idct for int32_t block. */

static void psx_idct(int32_t *block) {
    int b1, b3, b4, b6, b7, tmp1, tmp2, m0,
        x0, x1, x2, x3, x4, y3, y4, y5, y6, y7;

    /* Columns first */
    for (int i = 0; i < 8; i++) {
        if (!block[1*8+i] && !block[2*8+i] && !block[3*8+i] &&
            !block[4*8+i] && !block[5*8+i] && !block[6*8+i] && !block[7*8+i]) {
            int dc = block[i];
            block[0*8+i]=dc; block[1*8+i]=dc; block[2*8+i]=dc; block[3*8+i]=dc;
            block[4*8+i]=dc; block[5*8+i]=dc; block[6*8+i]=dc; block[7*8+i]=dc;
            continue;
        }
        b1 = block[4*8+i];
        b3 = block[2*8+i] + block[6*8+i];
        b4 = block[5*8+i] - block[3*8+i];
        tmp1 = block[1*8+i] + block[7*8+i];
        tmp2 = block[3*8+i] + block[5*8+i];
        b6 = block[1*8+i] - block[7*8+i];
        b7 = tmp1 + tmp2;
        m0 = block[0*8+i];
        x4 = ((b6*473 - b4*196 + 128) >> 8) - b7;
        x0 = x4 - (((tmp1 - tmp2)*362 + 128) >> 8);
        x1 = m0 - b1;
        x2 = (((block[2*8+i] - block[6*8+i])*362 + 128) >> 8) - b3;
        x3 = m0 + b1;
        y3 = x1 + x2; y4 = x3 + b3; y5 = x1 - x2; y6 = x3 - b3;
        y7 = -x0 - ((b4*473 + b6*196 + 128) >> 8);
        block[0*8+i] = b7+y4; block[1*8+i] = x4+y3;
        block[2*8+i] = y5-x0; block[3*8+i] = y6-y7;
        block[4*8+i] = y6+y7; block[5*8+i] = x0+y5;
        block[6*8+i] = y3-x4; block[7*8+i] = y4-b7;
    }

    /* Then rows */
    for (int i = 0; i < 64; i += 8) {
        if (!block[1+i] && !block[2+i] && !block[3+i] &&
            !block[4+i] && !block[5+i] && !block[6+i] && !block[7+i]) {
            int dc = (block[i] + 128) >> 8;
            block[0+i]=dc; block[1+i]=dc; block[2+i]=dc; block[3+i]=dc;
            block[4+i]=dc; block[5+i]=dc; block[6+i]=dc; block[7+i]=dc;
            continue;
        }
        b1 = block[4+i];
        b3 = block[2+i] + block[6+i];
        b4 = block[5+i] - block[3+i];
        tmp1 = block[1+i] + block[7+i];
        tmp2 = block[3+i] + block[5+i];
        b6 = block[1+i] - block[7+i];
        b7 = tmp1 + tmp2;
        m0 = block[0+i];
        x4 = ((b6*473 - b4*196 + 128) >> 8) - b7;
        x0 = x4 - (((tmp1 - tmp2)*362 + 128) >> 8);
        x1 = m0 - b1;
        x2 = (((block[2+i] - block[6+i])*362 + 128) >> 8) - b3;
        x3 = m0 + b1;
        y3 = x1 + x2; y4 = x3 + b3; y5 = x1 - x2; y6 = x3 - b3;
        y7 = -x0 - ((b4*473 + b6*196 + 128) >> 8);
        block[0+i] = (b7+y4+128)>>8; block[1+i] = (x4+y3+128)>>8;
        block[2+i] = (y5-x0+128)>>8; block[3+i] = (y6-y7+128)>>8;
        block[4+i] = (y6+y7+128)>>8; block[5+i] = (x0+y5+128)>>8;
        block[6+i] = (y3-x4+128)>>8; block[7+i] = (y4-b7+128)>>8;
    }
}

/* ---- BS video frame decoder ---- */

/* Decode one 8×8 DCT block from bitstream.
 * v2: DC is 10-bit signed, EOB = 10 (binary)
 * v3: DC is 10-bit signed in quant*DC form, EOB = 01 (binary) */
static int psx_decode_block(bs_reader_t *r, int32_t *blk, int qt) {
    memset(blk, 0, 64 * sizeof(int32_t));

    /* DC coefficient: 10-bit signed.
     * pl_mpeg's AAN IDCT has total gain 1/256 (>>8 in row pass).
     * DC dequant = dc * matrix[0] (compensates for 8× fdct / 8).
     * Scale by 32 for IDCT: dc * matrix[0] * 32 = dc << 6. */
    int32_t dc = bs_read_signed(r, 10);
    blk[0] = dc << 6;

    int idx = 0;

    for (;;) {
        if (r->nbits < 2) bs_fill(r);
        if ((r->bits >> 30) == 0x2) { bs_skip(r, 2); break; }

        int run, level;
        if (bs_decode_ac(r, &run, &level) < 0) break;

        idx += run + 1;
        if (idx >= 64) break;

        /* Dequant for AAN IDCT: level * matrix * qscale * 4
         * = (level * matrix * qscale / 8) * 32, combining /8 (fdct comp) and *32 (IDCT scale) */
        int natural = psx_zigzag[idx];
        blk[natural] = (int32_t)level * (int32_t)psx_quant_matrix[natural] * qt * 4;
    }

    psx_idct(blk);
    return 0;
}

/* Decode a complete BS frame: Cr, Cb, Y0, Y1, Y2, Y3 per macroblock */
static void psx_decode_bs_frame(psx_str_t *s) {
    int mb_w = (s->width + 15) / 16;
    int mb_h = (s->height + 15) / 16;
    int qt   = s->quant_scale;

    bs_reader_t reader;
    bs_init(&reader, (const uint16_t *)s->frame_buf,
            s->frame_buf_len / 2);

    /* Skip BS header: 2 words (demux_size in bits + magic/id) */
    bs_read(&reader, 16);  /* number of 16-bit words */
    bs_read(&reader, 16);  /* magic 0x3800 */
    bs_read(&reader, 16);  /* quant_scale */
    bs_read(&reader, 16);  /* version */

    int yw = (int)s->frame.y.width;
    int cw = (int)s->frame.cb.width;

    for (int mbx = 0; mbx < mb_w; mbx++) {
        for (int mby = 0; mby < mb_h; mby++) {
            int32_t *blk = s->block;

            /* Cr block — DC-only (chroma detail invisible on 216-color palette).
             * DC dequant = dc * matrix[0] = dc * 2. IDCT DC gain = 1/8.
             * Net: dc * 2 / 8 = dc / 4. */
            {
                int32_t dc = bs_read_signed(&reader, 10);
                int v = ((dc + 2) >> 2) + 128;  /* dc/4 + 128, rounded */
                if (v < 0) v = 0; if (v > 255) v = 255;
                /* Skip AC coefficients in bitstream */
                for (;;) {
                    if (reader.nbits < 2) bs_fill(&reader);
                    if ((reader.bits >> 30) == 0x2) { bs_skip(&reader, 2); break; }
                    int run, level;
                    if (bs_decode_ac(&reader, &run, &level) < 0) break;
                }
                uint8_t *dst = s->frame.cr.data + mby*8*cw + mbx*8;
                memset(dst, (uint8_t)v, 8);
                for (int y = 1; y < 8; y++) memcpy(dst + y*cw, dst, 8);
            }

            /* Cb block — DC-only */
            {
                int32_t dc = bs_read_signed(&reader, 10);
                int v = ((dc + 2) >> 2) + 128;
                if (v < 0) v = 0; if (v > 255) v = 255;
                for (;;) {
                    if (reader.nbits < 2) bs_fill(&reader);
                    if ((reader.bits >> 30) == 0x2) { bs_skip(&reader, 2); break; }
                    int run, level;
                    if (bs_decode_ac(&reader, &run, &level) < 0) break;
                }
                uint8_t *dst = s->frame.cb.data + mby*8*cw + mbx*8;
                memset(dst, (uint8_t)v, 8);
                for (int y = 1; y < 8; y++) memcpy(dst + y*cw, dst, 8);
            }

            /* Y blocks: TL, TR, BL, BR — full IDCT */
            int yoff[4][2] = {{0,0},{8,0},{0,8},{8,8}};
            for (int b = 0; b < 4; b++) {
                psx_decode_block(&reader, blk, qt);
                int bx = mbx*16 + yoff[b][0];
                int by = mby*16 + yoff[b][1];
                uint8_t *dst = s->frame.y.data + by*yw + bx;
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++) {
                        int v = blk[y*8+x] + 128;
                        if (v < 0) v = 0; if (v > 255) v = 255;
                        dst[y*yw+x] = (uint8_t)v;
                    }
            }
        }
    }
}

/* ---- XA-ADPCM audio decoder ---- */

/* XA-ADPCM filter coefficients (K0, K1) for filter indices 0-3 */
static const int xa_k0[4] = { 0, 60, 115, 98 };
static const int xa_k1[4] = { 0,  0, -52, -55 };

/* Decode XA-ADPCM audio from a sector.
 * coding_byte: byte 3 of CD-XA subheader (stereo/rate/depth flags)
 * sound_data:  pointer to 2304 bytes of sound groups (18 × 128 bytes) */
static void psx_decode_xa_sector(psx_str_t *s, uint8_t coding_byte,
                                  const uint8_t *sound_data) {
    int stereo   = (coding_byte & XA_STEREO) ? 1 : 0;
    int rate     = (coding_byte & XA_37800HZ) ? 37800 : 18900;
    int is8bit   = (coding_byte & XA_8BIT) ? 1 : 0;
    int channels = stereo ? 2 : 1;

    s->audio.channels    = channels;
    s->audio.sample_rate = rate;

    if (is8bit) {
        s->audio.count = 0;
        return;
    }

    /* 4-bit ADPCM: 18 sound groups, 128 bytes each.
     * Stereo: 8 sound units/group (even=L, odd=R), 28 samples each.
     * Mono:   4 sound units/group, 28 samples each.
     * Max stereo output: 18 × 4 × 28 = 2016 stereo sample pairs. */
    int16_t *out = s->audio.samples;
    int out_idx = 0;
    int units_per_group = stereo ? 8 : 4;

    for (int grp = 0; grp < 18; grp++) {
        const uint8_t *sg = sound_data + grp * 128;

        /* Sound group: bytes 0-15 = params for 8 units, bytes 16-127 = samples.
         * Param layout: bytes 0,1,2,3 hold params for units 0-3 (low nibble = range,
         * high nibble = filter). Bytes 4,5,6,7 duplicate for units 4-7.
         * For 4-bit: param byte pairs map to sound unit indices. */

        for (int unit = 0; unit < units_per_group; unit++) {
            int ch = stereo ? (unit & 1) : 0;

            /* Parameter byte: units 0-3 use bytes 0-3, units 4-7 use bytes 4-7.
             * But they're interleaved: byte 0=unit0, byte 1=unit1, byte 2=unit2, byte 3=unit3
             * byte 4=unit0 dup, ... Actually the layout per the XA spec:
             * Byte i (0-15): filter/range for sound unit (i % 8), repeated every 8 bytes.
             * For 4-bit stereo: param for unit N is at sg[N + (N >= 4 ? 4 : 0)]...
             * Simplification: param byte for unit N is at sg[N] (first 8 bytes are params,
             * next 8 bytes are copies). */
            uint8_t p = sg[unit + (unit < 4 ? 0 : 4)];
            int range  = p & 0x0F;
            int filter = (p >> 4) & 0x03;
            int k0 = xa_k0[filter];
            int k1 = xa_k1[filter];

            int32_t prev0 = s->xa_prev[ch][0];
            int32_t prev1 = s->xa_prev[ch][1];

            for (int j = 0; j < 28; j++) {
                /* Sample nibbles at bytes 16 + j*4 + (unit/2).
                 * Even units = low nibble, odd units = high nibble. */
                uint8_t byte = sg[16 + j * 4 + (unit >> 1)];
                int nibble;
                if (unit & 1)
                    nibble = (byte >> 4) & 0x0F;
                else
                    nibble = byte & 0x0F;

                /* Sign-extend 4-bit → signed */
                if (nibble >= 8) nibble -= 16;

                /* Decode: shift by (12-range), apply filter */
                int32_t sample;
                if (range <= 12)
                    sample = (nibble << (12 - range));
                else
                    sample = (nibble >> (range - 12));  /* protect against bad range */
                sample += (prev0 * k0 + prev1 * k1 + 32) >> 6;

                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;

                prev1 = prev0;
                prev0 = sample;

                if (stereo) {
                    out[out_idx * 2 + ch] = (int16_t)sample;
                    if (ch == 1) out_idx++;
                } else {
                    out[out_idx++] = (int16_t)sample;
                }
            }

            s->xa_prev[ch][0] = prev0;
            s->xa_prev[ch][1] = prev1;
        }
    }

    s->audio.count = out_idx;
}

/* ---- STR demuxer ---- */

static int psx_read_sector(psx_str_t *s) {
    int n = s->read(s->sector, STR_SECTOR_RAW, s->io_user);
    return (n == STR_SECTOR_RAW) ? 1 : 0;
}

static void psx_emit_video_frame(psx_str_t *s) {
    if (!s->video_cb) return;
    psx_decode_bs_frame(s);
    s->video_cb(s, &s->frame, s->video_user);
}

/* ---- Public API ---- */

psx_str_t *psx_str_create(psx_read_cb read, psx_seek_cb seek,
                           psx_tell_cb tell, void *user) {
    psx_str_t *s = (psx_str_t *)PSX_MALLOC(sizeof(psx_str_t));
    if (!s) return NULL;
    memset(s, 0, sizeof(psx_str_t));

    s->read    = read;
    s->seek    = seek;
    s->tell    = tell;
    s->io_user = user;
    s->fps     = 15;
    psx_vlc_init_lut();

    s->frame_buf = (uint8_t *)PSX_MALLOC(PSX_FRAME_BUF);
    if (!s->frame_buf) { PSX_FREE(s); return NULL; }

    return s;
}

void psx_str_destroy(psx_str_t *s) {
    if (!s) return;
    if (s->frame.y.data)  PSX_FREE(s->frame.y.data);
    if (s->frame.cb.data) PSX_FREE(s->frame.cb.data);
    if (s->frame.cr.data) PSX_FREE(s->frame.cr.data);
    if (s->frame_buf)     PSX_FREE(s->frame_buf);
    PSX_FREE(s);
}

void psx_str_set_video_callback(psx_str_t *s, psx_video_cb cb, void *user) {
    s->video_cb   = cb;
    s->video_user = user;
}

void psx_str_set_audio_callback(psx_str_t *s, psx_audio_cb cb, void *user) {
    s->audio_cb   = cb;
    s->audio_user = user;
}

int psx_str_get_width(psx_str_t *s)  { return s->width; }
int psx_str_get_height(psx_str_t *s) { return s->height; }
int psx_str_get_fps(psx_str_t *s)    { return s->fps; }

static void psx_alloc_planes(psx_str_t *s, int w, int h) {
    if (s->frame.y.data && (int)s->frame.width == w && (int)s->frame.height == h)
        return;

    int yw = (w + 15) & ~15;
    int yh = (h + 15) & ~15;
    int cw = yw / 2;
    int ch = yh / 2;

    if (s->frame.y.data)  PSX_FREE(s->frame.y.data);
    if (s->frame.cb.data) PSX_FREE(s->frame.cb.data);
    if (s->frame.cr.data) PSX_FREE(s->frame.cr.data);

    s->frame.width  = (unsigned)w;
    s->frame.height = (unsigned)h;
    s->frame.y.width  = (unsigned)yw;
    s->frame.y.height = (unsigned)yh;
    s->frame.y.data   = (uint8_t *)PSX_MALLOC(yw * yh);
    s->frame.cb.width  = (unsigned)cw;
    s->frame.cb.height = (unsigned)ch;
    s->frame.cb.data   = (uint8_t *)PSX_MALLOC(cw * ch);
    s->frame.cr.width  = (unsigned)cw;
    s->frame.cr.height = (unsigned)ch;
    s->frame.cr.data   = (uint8_t *)PSX_MALLOC(cw * ch);
}

/* Decode one 2336-byte sector from the STR stream.
 * Layout: [0..7] CD-XA subheader, [8..] payload.
 * Video: submode has DATA bit, payload[0..1] == 0x0160.
 * Audio: submode has AUDIO bit, payload = 18 sound groups. */
int psx_str_decode_sector(psx_str_t *s) {
    if (!psx_read_sector(s))
        return 0;

    const uint8_t *raw = s->sector;

    /* CD-XA subheader: bytes 0-3 (file, channel, submode, coding), repeated at 4-7 */
    uint8_t submode = raw[2];
    uint8_t coding  = raw[3];

    /* Payload starts after the 8-byte subheader */
    const uint8_t *payload = raw + STR_SUBHDR_SIZE;

    if (submode & XA_SUBMODE_AUDIO) {
        /* ---- Audio sector ---- */
        if (s->audio_cb) {
            psx_decode_xa_sector(s, coding, payload);
            if (s->audio.count > 0)
                s->audio_cb(s, &s->audio, s->audio_user);
        }
    }
    else if ((submode & (XA_SUBMODE_DATA | XA_SUBMODE_VIDEO)) &&
             psx_le16(payload) == STR_VIDEO_ID) {
        /* ---- Video sector ---- */
        /* STR video header at payload[0..31] */
        uint16_t sector_num   = psx_le16(payload + 4);
        uint16_t sector_count = psx_le16(payload + 6);
        uint32_t frame_num    = psx_le32(payload + 8);
        uint32_t demux_size   = psx_le32(payload + 12);
        int      w            = (int)psx_le16(payload + 16);
        int      h            = (int)psx_le16(payload + 18);
        uint16_t qscale       = psx_le16(payload + 24);
        uint16_t version      = psx_le16(payload + 26);

        if (w > 0 && h > 0 && w <= PSX_MAX_WIDTH && h <= PSX_MAX_HEIGHT) {
            s->width = w;
            s->height = h;
            psx_alloc_planes(s, w, h);
        }

        /* New frame? */
        if (frame_num != s->cur_frame || sector_num == 0) {
            if (s->sectors_received > 0 && s->sectors_received >= s->sectors_needed)
                psx_emit_video_frame(s);
            s->cur_frame = frame_num;
            s->sectors_needed = sector_count;
            s->sectors_received = 0;
            s->frame_buf_len = 0;
            s->quant_scale = qscale > 0 ? qscale : 1;
            s->bs_version  = (version == 3) ? 3 : 2;
        }

        /* Append BS data (after 32-byte STR header) to frame buffer */
        const uint8_t *bs_data = payload + STR_VID_HDR_SIZE;
        int data_len = STR_VID_DATA;
        if ((int)demux_size > 0 && s->frame_buf_len + data_len > (int)demux_size)
            data_len = (int)demux_size - s->frame_buf_len;
        if (data_len > 0 && s->frame_buf_len + data_len <= PSX_FRAME_BUF) {
            memcpy(s->frame_buf + s->frame_buf_len, bs_data, data_len);
            s->frame_buf_len += data_len;
        }
        s->sectors_received++;

        /* Complete frame? */
        if (s->sectors_received >= s->sectors_needed && s->frame_buf_len > 0) {
            psx_emit_video_frame(s);
            s->sectors_received = 0;
            s->frame_buf_len = 0;
        }
    }

    return 1;
}

#endif /* PSX_STR_IMPLEMENTATION */
