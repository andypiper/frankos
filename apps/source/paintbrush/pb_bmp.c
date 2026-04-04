/*
 * FRANK OS — Paintbrush: BMP file I/O (4-bit)
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pb.h"

/*==========================================================================
 * Little-endian helpers (alignment-safe)
 *=========================================================================*/

static inline uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline int32_t rd32s(const uint8_t *p) {
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}
static inline void wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static inline void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/*==========================================================================
 * CGA palette data
 *=========================================================================*/

const uint8_t bmp_pal[16][4] = {
    {0x00,0x00,0x00,0x00},  /*  0 Black          */
    {0xAA,0x00,0x00,0x00},  /*  1 Blue           */
    {0x00,0xAA,0x00,0x00},  /*  2 Green          */
    {0xAA,0xAA,0x00,0x00},  /*  3 Cyan           */
    {0x00,0x00,0xAA,0x00},  /*  4 Red            */
    {0xAA,0x00,0xAA,0x00},  /*  5 Magenta        */
    {0x00,0x55,0xAA,0x00},  /*  6 Brown          */
    {0xAA,0xAA,0xAA,0x00},  /*  7 Light Gray     */
    {0x55,0x55,0x55,0x00},  /*  8 Dark Gray      */
    {0xFF,0x55,0x55,0x00},  /*  9 Light Blue     */
    {0x55,0xFF,0x55,0x00},  /* 10 Light Green    */
    {0xFF,0xFF,0x55,0x00},  /* 11 Light Cyan     */
    {0x55,0x55,0xFF,0x00},  /* 12 Light Red      */
    {0xFF,0x55,0xFF,0x00},  /* 13 Light Magenta  */
    {0x55,0xFF,0xFF,0x00},  /* 14 Yellow         */
    {0xFF,0xFF,0xFF,0x00},  /* 15 White          */
};

const uint8_t cga_rgb[16][3] = {
    {0x00,0x00,0x00}, {0x00,0x00,0xAA}, {0x00,0xAA,0x00}, {0x00,0xAA,0xAA},
    {0xAA,0x00,0x00}, {0xAA,0x00,0xAA}, {0xAA,0x55,0x00}, {0xAA,0xAA,0xAA},
    {0x55,0x55,0x55}, {0x55,0x55,0xFF}, {0x55,0xFF,0x55}, {0x55,0xFF,0xFF},
    {0xFF,0x55,0x55}, {0xFF,0x55,0xFF}, {0xFF,0xFF,0x55}, {0xFF,0xFF,0xFF},
};

static uint8_t nearest_cga(uint8_t b, uint8_t g, uint8_t r) {
    int best = 0;
    int32_t best_d = 0x7FFFFFFF;
    for (int i = 0; i < 16; i++) {
        int32_t dr = (int32_t)r - cga_rgb[i][0];
        int32_t dg = (int32_t)g - cga_rgb[i][1];
        int32_t db = (int32_t)b - cga_rgb[i][2];
        int32_t d = dr * dr + dg * dg + db * db;
        if (d < best_d) { best_d = d; best = i; }
    }
    return (uint8_t)best;
}

/*==========================================================================
 * Save BMP
 *=========================================================================*/

bool pb_save_bmp(const char *path) {
    FIL fil;
    if (f_open(&fil, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return false;

    int16_t w = pb.canvas_w, h = pb.canvas_h;
    int row_bytes = (w + 1) / 2;
    int row_stride = (row_bytes + 3) & ~3;
    uint32_t pixel_size = (uint32_t)row_stride * h;
    uint32_t data_offset = 14 + 40 + 64;
    uint32_t file_size = data_offset + pixel_size;

    UINT bw;

    /* BMP file header (14 bytes) */
    uint8_t fh[14];
    memset(fh, 0, sizeof(fh));
    fh[0] = 'B'; fh[1] = 'M';
    wr32(&fh[2], file_size);
    wr32(&fh[10], data_offset);
    f_write(&fil, fh, 14, &bw);

    /* DIB header (40 bytes) */
    uint8_t ih[40];
    memset(ih, 0, sizeof(ih));
    wr32(&ih[0], 40);
    wr32(&ih[4], (uint32_t)w);
    wr32(&ih[8], (uint32_t)h);
    wr16(&ih[12], 1);
    wr16(&ih[14], 4);
    wr32(&ih[32], 16);
    wr32(&ih[36], 16);
    f_write(&fil, ih, 40, &bw);

    /* Color table */
    f_write(&fil, bmp_pal, 64, &bw);

    /* Pixel data — bottom-up, 4bpp packed */
    uint8_t *row_buf = (uint8_t *)malloc(row_stride);
    if (!row_buf) { f_close(&fil); return false; }

    for (int y = h - 1; y >= 0; y--) {
        memset(row_buf, 0, row_stride);
        for (int x = 0; x < w; x++) {
            uint8_t c = pb.canvas[(int32_t)y * w + x] & 0x0F;
            if (x & 1)
                row_buf[x / 2] |= c;
            else
                row_buf[x / 2] |= (uint8_t)(c << 4);
        }
        f_write(&fil, row_buf, row_stride, &bw);
    }

    free(row_buf);
    f_close(&fil);
    return true;
}

/*==========================================================================
 * Load BMP
 *=========================================================================*/

bool pb_load_bmp(const char *path) {
    FIL fil;
    if (f_open(&fil, path, FA_READ) != FR_OK)
        return false;

    UINT br;
    uint8_t header[54];
    if (f_read(&fil, header, 54, &br) != FR_OK || br < 54) {
        f_close(&fil);
        return false;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        f_close(&fil);
        dialog_show(pb.hwnd, "Paintbrush", "Not a valid BMP file.",
                    DLG_ICON_ERROR, DLG_BTN_OK);
        return false;
    }

    uint32_t data_offset = (uint32_t)rd32s(&header[10]);
    uint32_t dib_size = (uint32_t)rd32s(&header[14]);
    int32_t bmp_w = rd32s(&header[18]);
    int32_t bmp_h = rd32s(&header[22]);
    uint16_t bpp = rd16(&header[28]);

    bool bottom_up = (bmp_h > 0);
    if (bmp_h < 0) bmp_h = -bmp_h;

    if (bpp != 4 || bmp_w <= 0 || bmp_h <= 0 || bmp_w > 2048 || bmp_h > 2048) {
        f_close(&fil);
        dialog_show(pb.hwnd, "Paintbrush",
                    "Unsupported BMP format.\n"
                    "Only 4-bit BMP supported.",
                    DLG_ICON_ERROR, DLG_BTN_OK);
        return false;
    }

    /* Read palette */
    uint8_t palette[64];
    f_lseek(&fil, 14 + dib_size);
    f_read(&fil, palette, 64, &br);

    uint8_t color_map[16];
    for (int i = 0; i < 16; i++)
        color_map[i] = nearest_cga(palette[i * 4], palette[i * 4 + 1], palette[i * 4 + 2]);

    /* Resize canvas if needed */
    if (bmp_w != pb.canvas_w || bmp_h != pb.canvas_h) {
        int32_t new_sz = bmp_w * bmp_h;
        uint8_t *nc = (uint8_t *)calloc(new_sz, 1);
        uint8_t *nu = (uint8_t *)calloc(new_sz, 1);
        if (!nc || !nu) {
            free(nc); free(nu);
            f_close(&fil);
            dialog_show(pb.hwnd, "Paintbrush", "Not enough memory.",
                        DLG_ICON_ERROR, DLG_BTN_OK);
            return false;
        }
        free(pb.canvas);
        free(pb.undo_buf);
        pb.canvas = nc;
        pb.undo_buf = nu;
        pb.canvas_w = (int16_t)bmp_w;
        pb.canvas_h = (int16_t)bmp_h;
    }

    /* Read pixel data */
    f_lseek(&fil, data_offset);
    int row_bytes = ((int)bmp_w + 1) / 2;
    int row_stride = (row_bytes + 3) & ~3;
    uint8_t *row_buf = (uint8_t *)malloc(row_stride);
    if (!row_buf) { f_close(&fil); return false; }

    for (int row = 0; row < (int)bmp_h; row++) {
        int y = bottom_up ? ((int)bmp_h - 1 - row) : row;
        f_read(&fil, row_buf, row_stride, &br);
        for (int x = 0; x < (int)bmp_w; x++) {
            uint8_t c;
            if (x & 1)
                c = row_buf[x / 2] & 0x0F;
            else
                c = (row_buf[x / 2] >> 4) & 0x0F;
            pb.canvas[(int32_t)y * bmp_w + x] = color_map[c];
        }
    }

    free(row_buf);
    f_close(&fil);
    return true;
}
