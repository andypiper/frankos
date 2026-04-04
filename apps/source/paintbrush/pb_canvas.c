/*
 * FRANK OS — Paintbrush: canvas operations & drawing primitives
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pb.h"

/*==========================================================================
 * Canvas pixel operations
 *=========================================================================*/

void canvas_set(int16_t x, int16_t y, uint8_t c) {
    if (x >= 0 && x < pb.canvas_w && y >= 0 && y < pb.canvas_h)
        pb.canvas[(int32_t)y * pb.canvas_w + x] = c;
}

uint8_t canvas_get(int16_t x, int16_t y) {
    if (x >= 0 && x < pb.canvas_w && y >= 0 && y < pb.canvas_h)
        return pb.canvas[(int32_t)y * pb.canvas_w + x];
    return 0;
}

/*==========================================================================
 * Brush stamps
 *=========================================================================*/

void stamp_brush_circle(int16_t cx, int16_t cy, uint8_t color, uint8_t radius) {
    if (radius <= 1) {
        canvas_set(cx, cy, color);
        return;
    }
    int16_t r = radius / 2;
    for (int16_t dy = -r; dy <= r; dy++)
        for (int16_t dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r)
                canvas_set(cx + dx, cy + dy, color);
}

void stamp_brush_square(int16_t cx, int16_t cy, uint8_t color, uint8_t size) {
    int16_t r = size / 2;
    for (int16_t dy = -r; dy <= r; dy++)
        for (int16_t dx = -r; dx <= r; dx++)
            canvas_set(cx + dx, cy + dy, color);
}

/* Eraser: square block (jspaint style) */
void stamp_eraser(int16_t cx, int16_t cy, uint8_t color, uint8_t size) {
    int16_t half = size / 2;
    for (int16_t dy = 0; dy < size; dy++)
        for (int16_t dx = 0; dx < size; dx++)
            canvas_set(cx - half + dx, cy - half + dy, color);
}

/*==========================================================================
 * Safe Bresenham line — no abs(), no function pointers, iteration cap.
 * Stamps brush circle at each pixel along the line.
 *=========================================================================*/

void draw_line_bresenham(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         uint8_t color, uint8_t width) {
    int16_t adx = x1 - x0, ady = y1 - y0;
    int16_t asx = 1, asy = 1;
    if (adx < 0) { adx = -adx; asx = -1; }
    if (ady < 0) { ady = -ady; asy = -1; }
    int16_t er = adx - ady;
    int lim = adx + ady + 1;
    if (lim > 800) lim = 800;
    while (lim-- > 0) {
        stamp_brush_circle(x0, y0, color, width);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * er;
        if (e2 > -ady) { er -= ady; x0 += asx; }
        if (e2 < adx)  { er += adx; y0 += asy; }
    }
}

/*==========================================================================
 * Rectangle
 *=========================================================================*/

void draw_rect_outline(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        uint8_t color, uint8_t width) {
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    draw_line_bresenham(x0, y0, x1, y0, color, width);
    draw_line_bresenham(x1, y0, x1, y1, color, width);
    draw_line_bresenham(x1, y1, x0, y1, color, width);
    draw_line_bresenham(x0, y1, x0, y0, color, width);
}

void draw_rect_filled(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       uint8_t color) {
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    for (int16_t y = y0; y <= y1; y++)
        for (int16_t x = x0; x <= x1; x++)
            canvas_set(x, y, color);
}

/*==========================================================================
 * Ellipse — midpoint algorithm
 *=========================================================================*/

void draw_ellipse(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint8_t color, bool filled) {
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    int16_t cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    int16_t rx = (x1 - x0) / 2, ry = (y1 - y0) / 2;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    int32_t rx2 = (int32_t)rx * rx, ry2 = (int32_t)ry * ry;
    int32_t lim = rx2 * ry2;

    for (int16_t dy = -ry; dy <= ry; dy++) {
        for (int16_t dx = -rx; dx <= rx; dx++) {
            int32_t d = (int32_t)dx * dx * ry2 + (int32_t)dy * dy * rx2;
            if (d <= lim) {
                if (filled) {
                    canvas_set(cx + dx, cy + dy, color);
                } else {
                    bool edge = false;
                    if ((int32_t)(dx-1)*(dx-1)*ry2 + (int32_t)dy*dy*rx2 > lim) edge = true;
                    if ((int32_t)(dx+1)*(dx+1)*ry2 + (int32_t)dy*dy*rx2 > lim) edge = true;
                    if ((int32_t)dx*dx*ry2 + (int32_t)(dy-1)*(dy-1)*rx2 > lim) edge = true;
                    if ((int32_t)dx*dx*ry2 + (int32_t)(dy+1)*(dy+1)*rx2 > lim) edge = true;
                    if (edge) canvas_set(cx + dx, cy + dy, color);
                }
            }
        }
    }
}

/*==========================================================================
 * Rounded rectangle — arc corners via trig sampling (jspaint approach)
 * Stub: draws regular rectangle for now
 *=========================================================================*/

void draw_rounded_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        uint8_t color, uint8_t width, bool filled) {
    /* TODO: implement corner arcs */
    if (filled)
        draw_rect_filled(x0, y0, x1, y1, color);
    else
        draw_rect_outline(x0, y0, x1, y1, color, width);
}

/*==========================================================================
 * Flood fill — scanline stack algorithm (jspaint approach)
 *=========================================================================*/

void flood_fill(int16_t sx, int16_t sy, uint8_t nc) {
    int16_t w = pb.canvas_w, h = pb.canvas_h;
    if (sx < 0 || sx >= w || sy < 0 || sy >= h) return;
    uint8_t oc = pb.canvas[(int32_t)sy * w + sx];
    if (oc == nc) return;

    #define FS 4096
    int32_t *stk = (int32_t *)malloc(FS * sizeof(int32_t));
    if (!stk) return;
    int top = 0;

    stk[top++] = (sx & 0xFFFF) | (((uint32_t)sy & 0xFFFF) << 16);

    while (top > 0) {
        int32_t p = stk[--top];
        int16_t px = (int16_t)(p & 0xFFFF);
        int16_t py = (int16_t)((uint32_t)p >> 16);

        if (py < 0 || py >= h) continue;
        if (pb.canvas[(int32_t)py * w + px] != oc) continue;

        /* Scan left */
        int16_t lx = px;
        while (lx > 0 && pb.canvas[(int32_t)py * w + lx - 1] == oc) lx--;

        /* Scan right and fill */
        int16_t rx = lx;
        bool above = false, below = false;
        while (rx < w && pb.canvas[(int32_t)py * w + rx] == oc) {
            pb.canvas[(int32_t)py * w + rx] = nc;

            if (py > 0) {
                if (pb.canvas[(int32_t)(py - 1) * w + rx] == oc) {
                    if (!above && top < FS) {
                        stk[top++] = (rx & 0xFFFF) | (((uint32_t)(py - 1) & 0xFFFF) << 16);
                        above = true;
                    }
                } else {
                    above = false;
                }
            }
            if (py < h - 1) {
                if (pb.canvas[(int32_t)(py + 1) * w + rx] == oc) {
                    if (!below && top < FS) {
                        stk[top++] = (rx & 0xFFFF) | (((uint32_t)(py + 1) & 0xFFFF) << 16);
                        below = true;
                    }
                } else {
                    below = false;
                }
            }
            rx++;
        }
    }

    free(stk);
    #undef FS
}

/*==========================================================================
 * Airbrush — random dot spray in circle (jspaint approach)
 * rejection sampling: random point in square, accept if inside circle
 *=========================================================================*/

/* Simple PRNG (xorshift32) — no stdlib rand on embedded */
static uint32_t rng_state = 12345;
static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

void airbrush_spray(int16_t cx, int16_t cy, uint8_t color, uint8_t diameter) {
    int16_t r = diameter / 2;
    int dots = 6 + r / 5;
    for (int i = 0; i < dots; i++) {
        int16_t rx = (int16_t)((rng_next() % (2 * r + 1)) - r);
        int16_t ry = (int16_t)((rng_next() % (2 * r + 1)) - r);
        if (rx * rx + ry * ry <= r * r)
            canvas_set(cx + rx, cy + ry, color);
    }
}

/*==========================================================================
 * Undo
 *=========================================================================*/

void save_undo(void) {
    int32_t sz = (int32_t)pb.canvas_w * pb.canvas_h;
    memcpy(pb.undo_buf, pb.canvas, sz);
    pb.has_undo = true;
}

void restore_undo(void) {
    int32_t sz = (int32_t)pb.canvas_w * pb.canvas_h;
    memcpy(pb.canvas, pb.undo_buf, sz);
}

/*==========================================================================
 * Minimal 6x8 bitmap font for text tool (ASCII 32-127)
 * Each character is 6 pixels wide, 8 pixels tall, 1 byte per row.
 *=========================================================================*/

static const uint8_t font6x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 32 ' ' */
    {0x20,0x20,0x20,0x20,0x20,0x00,0x20,0x00}, /* 33 '!' */
    {0x50,0x50,0x00,0x00,0x00,0x00,0x00,0x00}, /* 34 '"' */
    {0x50,0xF8,0x50,0x50,0xF8,0x50,0x00,0x00}, /* 35 '#' */
    {0x20,0x78,0xA0,0x70,0x28,0xF0,0x20,0x00}, /* 36 '$' */
    {0xC8,0xC8,0x10,0x20,0x40,0x98,0x98,0x00}, /* 37 '%' */
    {0x40,0xA0,0xA0,0x40,0xA8,0x90,0x68,0x00}, /* 38 '&' */
    {0x20,0x20,0x00,0x00,0x00,0x00,0x00,0x00}, /* 39 ''' */
    {0x10,0x20,0x40,0x40,0x40,0x20,0x10,0x00}, /* 40 '(' */
    {0x40,0x20,0x10,0x10,0x10,0x20,0x40,0x00}, /* 41 ')' */
    {0x00,0x20,0xA8,0x70,0xA8,0x20,0x00,0x00}, /* 42 '*' */
    {0x00,0x20,0x20,0xF8,0x20,0x20,0x00,0x00}, /* 43 '+' */
    {0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x40}, /* 44 ',' */
    {0x00,0x00,0x00,0xF8,0x00,0x00,0x00,0x00}, /* 45 '-' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00}, /* 46 '.' */
    {0x08,0x08,0x10,0x20,0x40,0x80,0x80,0x00}, /* 47 '/' */
    {0x70,0x88,0x98,0xA8,0xC8,0x88,0x70,0x00}, /* 48 '0' */
    {0x20,0x60,0x20,0x20,0x20,0x20,0x70,0x00}, /* 49 '1' */
    {0x70,0x88,0x08,0x10,0x20,0x40,0xF8,0x00}, /* 50 '2' */
    {0x70,0x88,0x08,0x30,0x08,0x88,0x70,0x00}, /* 51 '3' */
    {0x10,0x30,0x50,0x90,0xF8,0x10,0x10,0x00}, /* 52 '4' */
    {0xF8,0x80,0xF0,0x08,0x08,0x88,0x70,0x00}, /* 53 '5' */
    {0x30,0x40,0x80,0xF0,0x88,0x88,0x70,0x00}, /* 54 '6' */
    {0xF8,0x08,0x10,0x20,0x40,0x40,0x40,0x00}, /* 55 '7' */
    {0x70,0x88,0x88,0x70,0x88,0x88,0x70,0x00}, /* 56 '8' */
    {0x70,0x88,0x88,0x78,0x08,0x10,0x60,0x00}, /* 57 '9' */
    {0x00,0x00,0x20,0x00,0x00,0x20,0x00,0x00}, /* 58 ':' */
    {0x00,0x00,0x20,0x00,0x00,0x20,0x20,0x40}, /* 59 ';' */
    {0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x00}, /* 60 '<' */
    {0x00,0x00,0xF8,0x00,0xF8,0x00,0x00,0x00}, /* 61 '=' */
    {0x80,0x40,0x20,0x10,0x20,0x40,0x80,0x00}, /* 62 '>' */
    {0x70,0x88,0x08,0x10,0x20,0x00,0x20,0x00}, /* 63 '?' */
    {0x70,0x88,0xB8,0xA8,0xB8,0x80,0x70,0x00}, /* 64 '@' */
    {0x70,0x88,0x88,0xF8,0x88,0x88,0x88,0x00}, /* 65 'A' */
    {0xF0,0x88,0x88,0xF0,0x88,0x88,0xF0,0x00}, /* 66 'B' */
    {0x70,0x88,0x80,0x80,0x80,0x88,0x70,0x00}, /* 67 'C' */
    {0xF0,0x88,0x88,0x88,0x88,0x88,0xF0,0x00}, /* 68 'D' */
    {0xF8,0x80,0x80,0xF0,0x80,0x80,0xF8,0x00}, /* 69 'E' */
    {0xF8,0x80,0x80,0xF0,0x80,0x80,0x80,0x00}, /* 70 'F' */
    {0x70,0x88,0x80,0xB8,0x88,0x88,0x70,0x00}, /* 71 'G' */
    {0x88,0x88,0x88,0xF8,0x88,0x88,0x88,0x00}, /* 72 'H' */
    {0x70,0x20,0x20,0x20,0x20,0x20,0x70,0x00}, /* 73 'I' */
    {0x38,0x10,0x10,0x10,0x10,0x90,0x60,0x00}, /* 74 'J' */
    {0x88,0x90,0xA0,0xC0,0xA0,0x90,0x88,0x00}, /* 75 'K' */
    {0x80,0x80,0x80,0x80,0x80,0x80,0xF8,0x00}, /* 76 'L' */
    {0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88,0x00}, /* 77 'M' */
    {0x88,0xC8,0xA8,0x98,0x88,0x88,0x88,0x00}, /* 78 'N' */
    {0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00}, /* 79 'O' */
    {0xF0,0x88,0x88,0xF0,0x80,0x80,0x80,0x00}, /* 80 'P' */
    {0x70,0x88,0x88,0x88,0xA8,0x90,0x68,0x00}, /* 81 'Q' */
    {0xF0,0x88,0x88,0xF0,0xA0,0x90,0x88,0x00}, /* 82 'R' */
    {0x70,0x88,0x80,0x70,0x08,0x88,0x70,0x00}, /* 83 'S' */
    {0xF8,0x20,0x20,0x20,0x20,0x20,0x20,0x00}, /* 84 'T' */
    {0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00}, /* 85 'U' */
    {0x88,0x88,0x88,0x88,0x50,0x50,0x20,0x00}, /* 86 'V' */
    {0x88,0x88,0x88,0xA8,0xA8,0xD8,0x88,0x00}, /* 87 'W' */
    {0x88,0x88,0x50,0x20,0x50,0x88,0x88,0x00}, /* 88 'X' */
    {0x88,0x88,0x50,0x20,0x20,0x20,0x20,0x00}, /* 89 'Y' */
    {0xF8,0x08,0x10,0x20,0x40,0x80,0xF8,0x00}, /* 90 'Z' */
    {0x70,0x40,0x40,0x40,0x40,0x40,0x70,0x00}, /* 91 '[' */
    {0x80,0x80,0x40,0x20,0x10,0x08,0x08,0x00}, /* 92 '\' */
    {0x70,0x10,0x10,0x10,0x10,0x10,0x70,0x00}, /* 93 ']' */
    {0x20,0x50,0x88,0x00,0x00,0x00,0x00,0x00}, /* 94 '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x00}, /* 95 '_' */
    {0x40,0x20,0x00,0x00,0x00,0x00,0x00,0x00}, /* 96 '`' */
    {0x00,0x00,0x70,0x08,0x78,0x88,0x78,0x00}, /* 97 'a' */
    {0x80,0x80,0xF0,0x88,0x88,0x88,0xF0,0x00}, /* 98 'b' */
    {0x00,0x00,0x70,0x80,0x80,0x80,0x70,0x00}, /* 99 'c' */
    {0x08,0x08,0x78,0x88,0x88,0x88,0x78,0x00}, /*100 'd' */
    {0x00,0x00,0x70,0x88,0xF8,0x80,0x70,0x00}, /*101 'e' */
    {0x30,0x48,0x40,0xE0,0x40,0x40,0x40,0x00}, /*102 'f' */
    {0x00,0x00,0x78,0x88,0x88,0x78,0x08,0x70}, /*103 'g' */
    {0x80,0x80,0xF0,0x88,0x88,0x88,0x88,0x00}, /*104 'h' */
    {0x20,0x00,0x60,0x20,0x20,0x20,0x70,0x00}, /*105 'i' */
    {0x10,0x00,0x30,0x10,0x10,0x10,0x90,0x60}, /*106 'j' */
    {0x80,0x80,0x90,0xA0,0xC0,0xA0,0x90,0x00}, /*107 'k' */
    {0x60,0x20,0x20,0x20,0x20,0x20,0x70,0x00}, /*108 'l' */
    {0x00,0x00,0xD0,0xA8,0xA8,0xA8,0xA8,0x00}, /*109 'm' */
    {0x00,0x00,0xB0,0xC8,0x88,0x88,0x88,0x00}, /*110 'n' */
    {0x00,0x00,0x70,0x88,0x88,0x88,0x70,0x00}, /*111 'o' */
    {0x00,0x00,0xF0,0x88,0x88,0xF0,0x80,0x80}, /*112 'p' */
    {0x00,0x00,0x78,0x88,0x88,0x78,0x08,0x08}, /*113 'q' */
    {0x00,0x00,0xB0,0xC8,0x80,0x80,0x80,0x00}, /*114 'r' */
    {0x00,0x00,0x78,0x80,0x70,0x08,0xF0,0x00}, /*115 's' */
    {0x40,0x40,0xE0,0x40,0x40,0x48,0x30,0x00}, /*116 't' */
    {0x00,0x00,0x88,0x88,0x88,0x98,0x68,0x00}, /*117 'u' */
    {0x00,0x00,0x88,0x88,0x88,0x50,0x20,0x00}, /*118 'v' */
    {0x00,0x00,0x88,0xA8,0xA8,0xA8,0x50,0x00}, /*119 'w' */
    {0x00,0x00,0x88,0x50,0x20,0x50,0x88,0x00}, /*120 'x' */
    {0x00,0x00,0x88,0x88,0x78,0x08,0x88,0x70}, /*121 'y' */
    {0x00,0x00,0xF8,0x10,0x20,0x40,0xF8,0x00}, /*122 'z' */
    {0x18,0x20,0x20,0xC0,0x20,0x20,0x18,0x00}, /*123 '{' */
    {0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00}, /*124 '|' */
    {0xC0,0x20,0x20,0x18,0x20,0x20,0xC0,0x00}, /*125 '}' */
    {0x48,0xA8,0x90,0x00,0x00,0x00,0x00,0x00}, /*126 '~' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /*127 DEL */
};

void canvas_text(int16_t x, int16_t y, const char *str, uint8_t color) {
    int16_t cx = x;
    while (*str) {
        uint8_t ch = (uint8_t)*str;
        if (ch >= 32 && ch < 128) {
            const uint8_t *glyph = font6x8[ch - 32];
            int row, col;
            for (row = 0; row < 8; row++)
                for (col = 0; col < 6; col++)
                    if (glyph[row] & (0x80 >> col))
                        canvas_set(cx + col, y + row, color);
        }
        cx += 6;
        str++;
    }
}
