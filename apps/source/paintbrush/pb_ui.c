/*
 * FRANK OS — Paintbrush: UI rendering (toolbar, palette, sub-options, canvas blit)
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pb.h"

/*==========================================================================
 * Layout computation
 *=========================================================================*/

void compute_layout(void) {
    rect_t cr = wm_get_client_rect(pb.hwnd);
    int16_t cw = cr.w, ch = cr.h;

    pb.view_x = TOOL_PANEL_W;
    pb.view_y = 0;
    pb.view_w = cw - TOOL_PANEL_W - SCROLLBAR_WIDTH;
    pb.view_h = ch - PALETTE_H - SCROLLBAR_WIDTH;
    if (pb.view_w < 1) pb.view_w = 1;
    if (pb.view_h < 1) pb.view_h = 1;

    bool need_h = (pb.canvas_w > pb.view_w);
    bool need_v = (pb.canvas_h > pb.view_h);
    if (!need_h) pb.view_h = ch - PALETTE_H;
    if (!need_v) pb.view_w = cw - TOOL_PANEL_W;
    if (!need_h && pb.canvas_w > pb.view_w) { need_h = true; pb.view_h = ch - PALETTE_H - SCROLLBAR_WIDTH; }
    if (!need_v && pb.canvas_h > pb.view_h) { need_v = true; pb.view_w = cw - TOOL_PANEL_W - SCROLLBAR_WIDTH; }

    /* Scrollbar geometry */
    pb.hscroll.x = TOOL_PANEL_W;
    pb.hscroll.y = ch - PALETTE_H - SCROLLBAR_WIDTH;
    pb.hscroll.w = pb.view_w;
    pb.hscroll.h = SCROLLBAR_WIDTH;
    pb.hscroll.visible = need_h;
    scrollbar_set_range(&pb.hscroll, pb.canvas_w, pb.view_w);

    pb.vscroll.x = cw - SCROLLBAR_WIDTH;
    pb.vscroll.y = 0;
    pb.vscroll.w = SCROLLBAR_WIDTH;
    pb.vscroll.h = ch - PALETTE_H - (need_h ? SCROLLBAR_WIDTH : 0);
    pb.vscroll.visible = need_v;
    scrollbar_set_range(&pb.vscroll, pb.canvas_h, pb.view_h);

    /* Clamp scroll */
    int16_t max_sx = pb.canvas_w - pb.view_w;
    int16_t max_sy = pb.canvas_h - pb.view_h;
    if (max_sx < 0) max_sx = 0;
    if (max_sy < 0) max_sy = 0;
    if (pb.scroll_x > max_sx) pb.scroll_x = max_sx;
    if (pb.scroll_y > max_sy) pb.scroll_y = max_sy;
    if (pb.scroll_x < 0) pb.scroll_x = 0;
    if (pb.scroll_y < 0) pb.scroll_y = 0;

    scrollbar_set_pos(&pb.hscroll, pb.scroll_x);
    scrollbar_set_pos(&pb.vscroll, pb.scroll_y);
}

/*==========================================================================
 * Coordinate conversion
 *=========================================================================*/

bool mouse_to_canvas(int16_t mx, int16_t my, int16_t *cx, int16_t *cy) {
    if (mx < pb.view_x || mx >= pb.view_x + pb.view_w) return false;
    if (my < pb.view_y || my >= pb.view_y + pb.view_h) return false;
    *cx = (mx - pb.view_x) + pb.scroll_x;
    *cy = (my - pb.view_y) + pb.scroll_y;
    if (*cx < 0 || *cx >= pb.canvas_w) return false;
    if (*cy < 0 || *cy >= pb.canvas_h) return false;
    return true;
}

/*==========================================================================
 * Tool icon rendering (16x16, color 7 = transparent)
 *=========================================================================*/

static void draw_tool_icon(int16_t x, int16_t y, uint8_t tool) {
    if (tool >= TOOL_COUNT) return;
    const uint8_t *bmp = tool_icons[tool];
    for (int iy = 0; iy < 16; iy++)
        for (int ix = 0; ix < 16; ix++) {
            uint8_t c = bmp[iy * 16 + ix];
            if (c != 7) wd_pixel(x + ix, y + iy, c);
        }
}

/*==========================================================================
 * Tool panel — 2x8 grid (MS Paint 95 layout)
 *=========================================================================*/

void paint_tool_panel(void) {
    rect_t cr = wm_get_client_rect(pb.hwnd);
    int16_t panel_h = cr.h - PALETTE_H;

    /* Panel background */
    wd_fill_rect(0, 0, TOOL_PANEL_W, panel_h, THEME_BUTTON_FACE);

    /* Right separator */
    /* No separator line — clean look */

    /* Tool buttons (2 columns x 8 rows) */
    for (int i = 0; i < TOOL_COUNT; i++) {
        int col = i % TOOL_COLS;
        int row = i / TOOL_COLS;
        int16_t bx = 1 + col * TOOL_BTN_SIZE;
        int16_t by = 1 + row * TOOL_BTN_SIZE;

        if (i == pb.tool) {
            /* Sunken (selected) */
            wd_hline(bx, by, TOOL_BTN_SIZE, COLOR_DARK_GRAY);
            wd_vline(bx, by, TOOL_BTN_SIZE, COLOR_DARK_GRAY);
            wd_hline(bx, by + TOOL_BTN_SIZE - 1, TOOL_BTN_SIZE, COLOR_WHITE);
            wd_vline(bx + TOOL_BTN_SIZE - 1, by, TOOL_BTN_SIZE, COLOR_WHITE);
            wd_fill_rect(bx + 1, by + 1, TOOL_BTN_SIZE - 2, TOOL_BTN_SIZE - 2,
                         THEME_BUTTON_FACE);
        } else {
            /* Raised */
            wd_bevel_rect(bx, by, TOOL_BTN_SIZE, TOOL_BTN_SIZE,
                          COLOR_WHITE, COLOR_DARK_GRAY, THEME_BUTTON_FACE);
        }
        draw_tool_icon(bx + ICON_PAD, by + ICON_PAD, (uint8_t)i);
    }

    /* Separator below tools */
    int16_t sep_y = SUBOPTS_Y;
    wd_hline(1, sep_y, TOOL_PANEL_W - 2, COLOR_DARK_GRAY);
    wd_hline(1, sep_y + 1, TOOL_PANEL_W - 2, COLOR_WHITE);
}

/*==========================================================================
 * Sub-options panel — content varies by active tool
 *=========================================================================*/

static void draw_subopts_line_widths(int16_t oy) {
    /* 5 line width options */
    int16_t row_h = SUBOPTS_H / LINE_WIDTH_COUNT;
    for (int i = 0; i < LINE_WIDTH_COUNT; i++) {
        int16_t ry = oy + i * row_h;
        int16_t bw = TOOL_PANEL_W - 4;

        if (i == pb.line_width_idx) {
            wd_hline(1, ry, bw + 2, COLOR_DARK_GRAY);
            wd_vline(1, ry, row_h, COLOR_DARK_GRAY);
            wd_hline(1, ry + row_h - 1, bw + 2, COLOR_WHITE);
            wd_vline(bw + 2, ry, row_h, COLOR_WHITE);
            wd_fill_rect(2, ry + 1, bw, row_h - 2, THEME_BUTTON_FACE);
        } else {
            wd_fill_rect(1, ry, bw + 2, row_h, THEME_BUTTON_FACE);
        }

        /* Draw line sample */
        uint8_t w = line_widths[i];
        int16_t cx = TOOL_PANEL_W / 2;
        int16_t cy = ry + row_h / 2;
        if (w == 1) {
            wd_hline(cx - 8, cy, 16, COLOR_BLACK);
        } else {
            int16_t r = w / 2;
            wd_fill_rect(cx - 8, cy - r, 16, w, COLOR_BLACK);
        }
    }
}

static void draw_subopts_eraser_sizes(int16_t oy) {
    int16_t row_h = SUBOPTS_H / ERASER_SIZE_COUNT;
    for (int i = 0; i < ERASER_SIZE_COUNT; i++) {
        int16_t ry = oy + i * row_h;
        int16_t bw = TOOL_PANEL_W - 4;

        if (i == pb.eraser_size_idx) {
            wd_hline(1, ry, bw + 2, COLOR_DARK_GRAY);
            wd_vline(1, ry, row_h, COLOR_DARK_GRAY);
            wd_hline(1, ry + row_h - 1, bw + 2, COLOR_WHITE);
            wd_vline(bw + 2, ry, row_h, COLOR_WHITE);
            wd_fill_rect(2, ry + 1, bw, row_h - 2, THEME_BUTTON_FACE);
        } else {
            wd_fill_rect(1, ry, bw + 2, row_h, THEME_BUTTON_FACE);
        }

        /* Draw square block sample */
        uint8_t sz = eraser_sizes[i];
        int16_t cx = TOOL_PANEL_W / 2 - sz / 2;
        int16_t cy = ry + row_h / 2 - sz / 2;
        wd_fill_rect(cx, cy, sz, sz, COLOR_BLACK);
    }
}

static void draw_subopts_brush_shapes(int16_t oy) {
    /* 3 size options for current brush shape */
    int16_t row_h = SUBOPTS_H / BRUSH_SIZE_COUNT;
    for (int i = 0; i < BRUSH_SIZE_COUNT; i++) {
        int16_t ry = oy + i * row_h;
        int16_t bw = TOOL_PANEL_W - 4;

        if (i == pb.brush_size_idx) {
            wd_hline(1, ry, bw + 2, COLOR_DARK_GRAY);
            wd_vline(1, ry, row_h, COLOR_DARK_GRAY);
            wd_hline(1, ry + row_h - 1, bw + 2, COLOR_WHITE);
            wd_vline(bw + 2, ry, row_h, COLOR_WHITE);
            wd_fill_rect(2, ry + 1, bw, row_h - 2, THEME_BUTTON_FACE);
        } else {
            wd_fill_rect(1, ry, bw + 2, row_h, THEME_BUTTON_FACE);
        }

        /* Draw brush dot sample */
        uint8_t sz = brush_sizes[i];
        int16_t cx = TOOL_PANEL_W / 2;
        int16_t cy = ry + row_h / 2;
        if (pb.brush_shape == BRUSH_SQUARE) {
            int16_t half = sz / 2;
            wd_fill_rect(cx - half, cy - half, sz, sz, COLOR_BLACK);
        } else {
            /* Circle (default) */
            int16_t r = sz / 2;
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++)
                    if (dx * dx + dy * dy <= r * r)
                        wd_pixel(cx + dx, cy + dy, COLOR_BLACK);
        }
    }
}

static void draw_subopts_airbrush_sizes(int16_t oy) {
    int16_t row_h = SUBOPTS_H / AIRBRUSH_SIZE_COUNT;
    for (int i = 0; i < AIRBRUSH_SIZE_COUNT; i++) {
        int16_t ry = oy + i * row_h;
        int16_t bw = TOOL_PANEL_W - 4;

        if (i == pb.airbrush_size_idx) {
            wd_hline(1, ry, bw + 2, COLOR_DARK_GRAY);
            wd_vline(1, ry, row_h, COLOR_DARK_GRAY);
            wd_hline(1, ry + row_h - 1, bw + 2, COLOR_WHITE);
            wd_vline(bw + 2, ry, row_h, COLOR_WHITE);
            wd_fill_rect(2, ry + 1, bw, row_h - 2, THEME_BUTTON_FACE);
        } else {
            wd_fill_rect(1, ry, bw + 2, row_h, THEME_BUTTON_FACE);
        }

        /* Draw dot cluster sample */
        uint8_t sz = airbrush_sizes[i];
        int16_t cx = TOOL_PANEL_W / 2;
        int16_t cy = ry + row_h / 2;
        int16_t r = sz / 4;  /* scaled down for preview */
        if (r < 1) r = 1;
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (dx * dx + dy * dy <= r * r)
                    wd_pixel(cx + dx, cy + dy, COLOR_BLACK);
    }
}

static void draw_subopts_fill_modes(int16_t oy) {
    /* 3 fill mode options: outline, outline+fill, fill */
    int16_t row_h = SUBOPTS_H / FILL_MODE_COUNT;
    for (int i = 0; i < FILL_MODE_COUNT; i++) {
        int16_t ry = oy + i * row_h;
        int16_t bw = TOOL_PANEL_W - 4;

        if (i == pb.fill_mode) {
            wd_hline(1, ry, bw + 2, COLOR_DARK_GRAY);
            wd_vline(1, ry, row_h, COLOR_DARK_GRAY);
            wd_hline(1, ry + row_h - 1, bw + 2, COLOR_WHITE);
            wd_vline(bw + 2, ry, row_h, COLOR_WHITE);
            wd_fill_rect(2, ry + 1, bw, row_h - 2, THEME_BUTTON_FACE);
        } else {
            wd_fill_rect(1, ry, bw + 2, row_h, THEME_BUTTON_FACE);
        }

        /* Draw mini rect showing fill mode */
        int16_t rx = 6, ry2 = ry + 2;
        int16_t rw = TOOL_PANEL_W - 14, rh = row_h - 4;
        if (i == FILL_OUTLINE) {
            /* Outline only: black border, white inside */
            wd_fill_rect(rx + 1, ry2 + 1, rw - 2, rh - 2, COLOR_WHITE);
            wd_rect(rx, ry2, rw, rh, COLOR_BLACK);
        } else if (i == FILL_OUTLINE_FILL) {
            /* Outline + fill: black border, cyan fill */
            wd_fill_rect(rx + 1, ry2 + 1, rw - 2, rh - 2, COLOR_CYAN);
            wd_rect(rx, ry2, rw, rh, COLOR_BLACK);
        } else {
            /* Fill only: cyan fill, no border */
            wd_fill_rect(rx, ry2, rw, rh, COLOR_CYAN);
        }
    }
}

void paint_sub_options(void) {
    int16_t oy = SUBOPTS_Y + 3;
    uint8_t t = pb.tool;

    /* Clear sub-options area */
    wd_fill_rect(0, oy, TOOL_PANEL_W - 1, SUBOPTS_H, THEME_BUTTON_FACE);

    if (t == TOOL_LINE) {
        draw_subopts_line_widths(oy);
    } else if (t == TOOL_ERASER) {
        draw_subopts_eraser_sizes(oy);
    } else if (t == TOOL_BRUSH) {
        draw_subopts_brush_shapes(oy);
    } else if (t == TOOL_AIRBRUSH) {
        draw_subopts_airbrush_sizes(oy);
    } else if (t == TOOL_RECT || t == TOOL_ELLIPSE ||
               t == TOOL_ELLIPSE) {
        draw_subopts_fill_modes(oy);
    }
}

/*==========================================================================
 * Color palette — bottom strip
 *=========================================================================*/

void paint_palette(void) {
    rect_t cr = wm_get_client_rect(pb.hwnd);
    int16_t py = cr.h - PALETTE_H;
    int16_t pw = cr.w;

    /* Background */
    wd_fill_rect(0, py, pw, PALETTE_H, THEME_BUTTON_FACE);

    /* Top separator */
    wd_hline(0, py, pw, COLOR_DARK_GRAY);
    wd_hline(0, py + 1, pw, COLOR_WHITE);

    /* FG/BG color indicator */
    int16_t fbx = 3, fby = py + 3;
    /* BG color (bottom-right) */
    wd_fill_rect(fbx + 8, fby + 8, FGBG_BOX - 8, FGBG_BOX - 8, pb.bg_color);
    wd_rect(fbx + 8, fby + 8, FGBG_BOX - 8, FGBG_BOX - 8, COLOR_BLACK);
    /* FG color (top-left, overlapping) */
    wd_fill_rect(fbx, fby, FGBG_BOX - 8, FGBG_BOX - 8, pb.fg_color);
    wd_rect(fbx, fby, FGBG_BOX - 8, FGBG_BOX - 8, COLOR_BLACK);

    /* Color swatches — 2 rows of 8 */
    int16_t sx = fbx + FGBG_BOX + 6;
    for (int i = 0; i < 16; i++) {
        int col = i % 8;
        int row = i / 8;
        int16_t cx = sx + col * (SWATCH_SIZE + 1);
        int16_t cy = py + 3 + row * (SWATCH_SIZE - 1);

        wd_fill_rect(cx, cy, SWATCH_SIZE, SWATCH_SIZE - 2, (uint8_t)i);
        wd_rect(cx, cy, SWATCH_SIZE, SWATCH_SIZE - 2, COLOR_BLACK);
    }
}

/*==========================================================================
 * Canvas rendering — fast wd_fb_ptr blit
 *=========================================================================*/

void paint_canvas(void) {
    int16_t vx = pb.view_x, vy = pb.view_y;
    int16_t vw = pb.view_w, vh = pb.view_h;

    /* How much canvas is visible */
    int16_t dw = vw, dh = vh;
    if (pb.scroll_x + dw > pb.canvas_w) dw = pb.canvas_w - pb.scroll_x;
    if (pb.scroll_y + dh > pb.canvas_h) dh = pb.canvas_h - pb.scroll_y;
    if (dw < 0) dw = 0;
    if (dh < 0) dh = 0;

    /* Clip to visible client area */
    int16_t clip_w, clip_h;
    wd_get_clip_size(&clip_w, &clip_h);
    if (vx + dw > clip_w) dw = clip_w - vx;
    if (vy + dh > clip_h) dh = clip_h - vy;

    /* Render canvas pixels (fast path: 2 pixels per byte) */
    for (int16_t dy = 0; dy < dh; dy++) {
        int16_t stride;
        uint8_t *fb = wd_fb_ptr(vx, vy + dy, &stride);
        if (!fb) continue;

        const uint8_t *src = &pb.canvas[(int32_t)(pb.scroll_y + dy) * pb.canvas_w + pb.scroll_x];
        int16_t dx;
        for (dx = 0; dx + 1 < dw; dx += 2)
            fb[dx >> 1] = (uint8_t)((src[dx] << 4) | (src[dx + 1] & 0x0F));
        if (dx < dw)
            fb[dx >> 1] = (uint8_t)((src[dx] << 4) | (fb[dx >> 1] & 0x0F));
    }

    /* Fill space beyond canvas */
    if (dw < vw)
        wd_fill_rect(vx + dw, vy, vw - dw, vh, THEME_BUTTON_FACE);
    if (dh < vh)
        wd_fill_rect(vx, vy + dh, dw > 0 ? dw : vw, vh - dh, THEME_BUTTON_FACE);
}

/*==========================================================================
 * Shape preview — drawn on screen during drag (screen coords via wd_pixel)
 *=========================================================================*/

static void screen_pixel(int16_t cx, int16_t cy, uint8_t color) {
    int16_t sx = cx - pb.scroll_x + pb.view_x;
    int16_t sy = cy - pb.scroll_y + pb.view_y;
    if (sx >= pb.view_x && sx < pb.view_x + pb.view_w &&
        sy >= pb.view_y && sy < pb.view_y + pb.view_h)
        wd_pixel(sx, sy, color);
}

static void screen_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         uint8_t color) {
    int16_t adx = x1 - x0, ady = y1 - y0;
    int16_t asx = 1, asy = 1;
    if (adx < 0) { adx = -adx; asx = -1; }
    if (ady < 0) { ady = -ady; asy = -1; }
    int16_t er = adx - ady;
    int lim = adx + ady + 1;
    if (lim > 800) lim = 800;
    while (lim-- > 0) {
        screen_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * er;
        if (e2 > -ady) { er -= ady; x0 += asx; }
        if (e2 < adx)  { er += adx; y0 += asy; }
    }
}

static void screen_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          uint8_t color) {
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    for (int16_t x = x0; x <= x1; x++) {
        screen_pixel(x, y0, color);
        screen_pixel(x, y1, color);
    }
    for (int16_t y = y0 + 1; y < y1; y++) {
        screen_pixel(x0, y, color);
        screen_pixel(x1, y, color);
    }
}

static void screen_fill_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               uint8_t color) {
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    for (int16_t y = y0; y <= y1; y++)
        for (int16_t x = x0; x <= x1; x++)
            screen_pixel(x, y, color);
}

/* Draw dashed rectangle for selection */
static void screen_dashed_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int16_t t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    int16_t x, y;
    for (x = x0; x <= x1; x++) {
        screen_pixel(x, y0, ((x + y0) & 2) ? COLOR_WHITE : COLOR_BLACK);
        screen_pixel(x, y1, ((x + y1) & 2) ? COLOR_WHITE : COLOR_BLACK);
    }
    for (y = y0 + 1; y < y1; y++) {
        screen_pixel(x0, y, ((x0 + y) & 2) ? COLOR_WHITE : COLOR_BLACK);
        screen_pixel(x1, y, ((x1 + y) & 2) ? COLOR_WHITE : COLOR_BLACK);
    }
}

void paint_shape_preview(void) {
    /* Draw floating paste preview */
    if (pb.floating && pb.sel_buf) {
        int16_t fx = pb.float_x, fy = pb.float_y;
        int16_t y, x;
        for (y = 0; y < pb.sel_buf_h; y++)
            for (x = 0; x < pb.sel_buf_w; x++)
                screen_pixel(fx + x, fy + y,
                             pb.sel_buf[(int32_t)y * pb.sel_buf_w + x]);
        screen_dashed_rect(fx, fy, fx + pb.sel_buf_w - 1, fy + pb.sel_buf_h - 1);
        return;
    }

    /* Draw existing selection rectangle (even when not drawing) */
    if (pb.has_selection && !pb.drawing)
        screen_dashed_rect(pb.sel_x, pb.sel_y,
                           pb.sel_x + pb.sel_w - 1, pb.sel_y + pb.sel_h - 1);

    if (!pb.drawing) return;

    uint8_t c = pb.draw_color;
    int16_t x0 = pb.start_x, y0 = pb.start_y;
    int16_t x1 = pb.last_x, y1 = pb.last_y;

    /* Selection drag preview */
    if (pb.tool == TOOL_SELECT || pb.tool == TOOL_SELECT) {
        screen_dashed_rect(x0, y0, x1, y1);
        return;
    }

    if (pb.tool == TOOL_LINE) {
        screen_line(x0, y0, x1, y1, c);
    } else if (pb.tool == TOOL_RECT) {
        if (pb.fill_mode == FILL_OUTLINE)
            screen_rect(x0, y0, x1, y1, c);
        else if (pb.fill_mode == FILL_OUTLINE_FILL) {
            screen_fill_rect(x0, y0, x1, y1, pb.bg_color);
            screen_rect(x0, y0, x1, y1, c);
        } else
            screen_fill_rect(x0, y0, x1, y1, c);
    } else if (pb.tool == TOOL_ELLIPSE) {
        screen_rect(x0, y0, x1, y1, c);
    }
}

/*==========================================================================
 * Hit testing
 *=========================================================================*/

int hit_tool(int16_t mx, int16_t my) {
    for (int i = 0; i < TOOL_COUNT; i++) {
        int col = i % TOOL_COLS;
        int row = i / TOOL_COLS;
        int16_t bx = 1 + col * TOOL_BTN_SIZE;
        int16_t by = 1 + row * TOOL_BTN_SIZE;
        if (mx >= bx && mx < bx + TOOL_BTN_SIZE &&
            my >= by && my < by + TOOL_BTN_SIZE)
            return i;
    }
    return -1;
}

int hit_sub_option(int16_t mx, int16_t my) {
    if (mx < 1 || mx >= TOOL_PANEL_W - 1) return -1;
    int16_t oy = SUBOPTS_Y + 3;
    if (my < oy || my >= oy + SUBOPTS_H) return -1;

    int count = 0;
    if (pb.tool == TOOL_LINE)
        count = LINE_WIDTH_COUNT;
    else if (pb.tool == TOOL_ERASER)
        count = ERASER_SIZE_COUNT;
    else if (pb.tool == TOOL_BRUSH)
        count = BRUSH_SIZE_COUNT;
    else if (pb.tool == TOOL_AIRBRUSH)
        count = AIRBRUSH_SIZE_COUNT;
    else if (pb.tool == TOOL_RECT || pb.tool == TOOL_ELLIPSE ||
             pb.tool == TOOL_ELLIPSE)
        count = FILL_MODE_COUNT;
    else
        return -1;

    int16_t row_h = SUBOPTS_H / count;
    int idx = (my - oy) / row_h;
    if (idx >= count) idx = count - 1;
    return idx;
}

int hit_palette_color(int16_t mx, int16_t my) {
    rect_t cr = wm_get_client_rect(pb.hwnd);
    int16_t py = cr.h - PALETTE_H;
    int16_t sx = 3 + FGBG_BOX + 6;

    for (int i = 0; i < 16; i++) {
        int col = i % 8;
        int row = i / 8;
        int16_t cx = sx + col * (SWATCH_SIZE + 1);
        int16_t cy = py + 3 + row * (SWATCH_SIZE - 1);
        if (mx >= cx && mx < cx + SWATCH_SIZE &&
            my >= cy && my < cy + SWATCH_SIZE - 2)
            return i;
    }
    return -1;
}
