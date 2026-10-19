#include "widgets.h"

#include <stdio.h>
#include <time.h>

#include "app.h"

#define Z 0.5f

void ui_image(const C2D_Image *img, float x, float y, float w, float h) {
    if (!img || !img->subtex) return;
    C2D_DrawImageAt(*img, x, y, Z, NULL, w / img->subtex->width, h / img->subtex->height);
}

void ui_art(App *app, int album, float x, float y, float size, u32 ph_bg, u32 ph_fg) {
    const C2D_Image *img = art_get(app->art, album);
    if (img) {
        ui_image(img, x, y, size, size);
        return;
    }
    ui_vgrad(x, y, size, size, ph_bg, ui_mix(ph_bg, RGB(0, 0, 0), 0.35f));
    ui_icon(ICON_SONG, x + size / 2, y + size / 2, size * 0.5f, ph_fg);
}

float ui_clock(App *app, float right, float y, float size, u32 color) {
    static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    float x = right;
    // Battery: outline plus up to three green segments.
    float bh = size * 26, bw = bh * 1.9f, by = y + ui_line_height(size) / 2 - bh / 2;
    x -= bw + 3;
    // Read the battery every few seconds, not every frame (and the same value on both screens).
    static u8 level = 100;
    static u64 last_read;
    u64 now_ms = osGetTime();
    if (!last_read || now_ms - last_read > 5000) {
        last_read = now_ms;
        u8 l = 0;
        if (R_FAILED(MCUHWC_GetBatteryLevel(&l)) || l == 0) {
            u8 lv = 0;
            if (R_SUCCEEDED(PTMU_GetBatteryLevel(&lv))) l = lv * 20;
        }
        level = l ? l : 100; // unknown: draw full rather than alarmingly empty
    }
    ui_frame(x, by, bw, bh, 1.5f, color);
    ui_rect(x + bw, by + bh * 0.3f, 2, bh * 0.4f, color);
    int bars = level > 66 ? 3 : level > 33 ? 2 : level > 10 ? 1 : 0;
    float seg = (bw - 5) / 3;
    u32 fill = level > 20 ? RGB(70, 210, 70) : RGB(240, 70, 50);
    for (int i = 0; i < bars; i++) ui_rect(x + 2.5f + i * seg, by + 2.5f, seg - 1, bh - 5, fill);
    x -= 9;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (tm) {
        char buf[32];
        int h12 = tm->tm_hour % 12 ? tm->tm_hour % 12 : 12;
        snprintf(buf, sizeof buf, "%d:%02d", h12, tm->tm_min);
        x -= ui_text(app, buf, x, y, size, color, ALIGN_RIGHT) + 11;
        snprintf(buf, sizeof buf, "%d/%d (%s)", tm->tm_mon + 1, tm->tm_mday, days[tm->tm_wday % 7]);
        x -= ui_text(app, buf, x, y, size, color, ALIGN_RIGHT);
    }
    return x;
}

void ui_signal(float x, float y, float h, u32 on, u32 off) {
    u8 strength = osGetWifiStrength(); // 0..3
    for (int i = 0; i < 4; i++) {
        float bh = h * (i + 1) / 4;
        ui_rect(x + i * (h * 0.3f), y + h - bh, h * 0.2f, bh, i <= strength ? on : off);
    }
}

void ui_list(App *app, MenuLevel *m, float x, float y, float w, float h, const ListStyle *s) {
    int visible = (int)(h / s->row_h);
    if (visible < 1) visible = 1;
    float first = ui_list_scroll(m, visible, app->dt);
    char a[96], b[96], c[48];
    for (int r = -1; r <= visible; r++) {
        int i = (int)first + r;
        if (i < 0 || i >= m->count) continue;
        float ry = y + (i - first) * s->row_h;
        if (ry < y - 1 || ry + s->row_h > y + h + 1) continue;
        bool sel = i == m->cursor;
        if (s->zebra && i % 2) ui_rect(x, ry, w, s->row_h, s->zebra_c);
        if (s->divider && !sel) ui_rect(x + s->pad, ry + s->row_h - 1, w - 2 * s->pad, 1, s->div);
        if (sel && s->highlight) s->highlight(x, ry, w, s->row_h);
        float tx = x + s->pad;
        if (s->art) {
            float as = s->row_h - 6;
            int album = menu_art(app, m, i);
            if (album >= 0) ui_art(app, album, tx, ry + 3, as, s->ph_bg, s->ph_fg);
            else ui_icon(menu_icon(app, m, i), tx + as / 2, ry + s->row_h / 2, as * 0.6f, sel ? s->icon_sel : s->icon_c);
            tx += as + 9;
        } else if (s->icon) {
            float is = s->row_h * 0.55f;
            ui_icon(menu_icon(app, m, i), tx + is / 2, ry + s->row_h / 2, is, sel ? s->icon_sel : s->icon_c);
            tx += is + 11;
        }
        float right = x + w - s->pad;
        if (s->chevron && menu_has_children(app, m, i)) {
            float cx = right - 3, cy = ry + s->row_h / 2, k = s->row_h * 0.15f;
            if (k > 6) k = 6;
            ui_line(cx - k, cy - k, cx, cy, 2, sel ? s->chev_sel : s->chev);
            ui_line(cx - k, cy + k, cx, cy, 2, sel ? s->chev_sel : s->chev);
            right -= 16;
        }
        if (s->value) {
            const char *v = menu_value(app, m, i, c, sizeof c);
            if (*v) right -= ui_text(app, v, right, ry + (s->row_h - ui_line_height(s->detail_size)) / 2, s->detail_size, sel ? s->val_sel : s->val, ALIGN_RIGHT) + 8;
        }
        const char *label = menu_label(app, m, i, a, sizeof a);
        const char *sub = s->detail ? menu_detail(app, m, i, b, sizeof b) : "";
        float tw = right - tx;
        float two = ui_line_height(s->text_size) + ui_line_height(s->detail_size) - 6;
        if (*sub && s->row_h >= two) {
            float ty = ry + (s->row_h - two) / 2;
            ui_text_fit(app, label, tx, ty, s->text_size, sel ? s->text_sel : s->text, ALIGN_LEFT, tw);
            ui_text_fit(app, sub, tx, ty + ui_line_height(s->text_size) - 5, s->detail_size, sel ? s->sub_sel : s->sub, ALIGN_LEFT, tw);
        } else {
            ui_text_fit(app, label, tx, ry + (s->row_h - ui_line_height(s->text_size)) / 2 + 1, s->text_size, sel ? s->text_sel : s->text, ALIGN_LEFT, tw);
        }
    }
}

static void album_cell(App *app, MenuLevel *m, int i, float x, float y, float slot, const StripStyle *s, bool sel) {
    if (sel) ui_rect(x - 3, y - 3, s->art + 6, s->art + 6, s->border);
    ui_art(app, i, x, y, s->art, s->ph_bg, s->ph_fg);
    char a[96], b[96];
    float cx = x + s->art / 2, ty = y + s->art + 4;
    ui_text_fit(app, menu_label(app, m, i, a, sizeof a), cx, ty, s->text_size, s->caption, ALIGN_CENTER, slot - 4);
    ui_text_fit(app, menu_detail(app, m, i, b, sizeof b), cx, ty + ui_line_height(s->text_size) - 4, s->text_size * 0.92f, s->caption_sub, ALIGN_CENTER, slot - 4);
}

void ui_album_strip(App *app, MenuLevel *m, float x, float y, float w, float h, const StripStyle *s) {
    (void)h;
    float step = s->art + s->gap;
    int visible = (int)((w - 30 + s->gap) / step);
    if (visible < 1) visible = 1;
    float first = ui_list_scroll(m, visible, app->dt);
    float x0 = x + (w - (visible * step - s->gap)) / 2;
    for (int r = -1; r <= visible; r++) {
        int i = (int)first + r;
        if (i < 0 || i >= m->count) continue;
        float cx = x0 + (i - first) * step;
        if (cx < x + 12 || cx + s->art > x + w - 12) continue;
        album_cell(app, m, i, cx, y + 5, step, s, i == m->cursor);
    }
    float ay = y + 5 + s->art / 2;
    if (first > 0.01f) { ui_line(x + 11, ay - 9, x + 4, ay, 3, s->arrow); ui_line(x + 11, ay + 9, x + 4, ay, 3, s->arrow); }
    if (first + visible < m->count - 0.01f) { ui_line(x + w - 11, ay - 9, x + w - 4, ay, 3, s->arrow); ui_line(x + w - 11, ay + 9, x + w - 4, ay, 3, s->arrow); }
}

void ui_album_grid(App *app, MenuLevel *m, float x, float y, float w, float h, int cols, const StripStyle *s) {
    float cw = w / cols, ch = s->art + 2 * ui_line_height(s->text_size) - 2;
    int rows = (int)(h / ch);
    if (rows < 1) rows = 1;
    // Scroll by rows, reusing the list scroller on a row view.
    MenuLevel rowview = *m;
    rowview.count = (m->count + cols - 1) / cols;
    rowview.cursor = m->cursor / cols;
    float first = ui_list_scroll(&rowview, rows, app->dt);
    m->scroll = rowview.scroll;
    for (int r = -1; r <= rows; r++) {
        int row = (int)first + r;
        float ry = y + (row - first) * ch;
        if (row < 0 || ry < y - 1 || ry + ch > y + h + 4) continue;
        for (int c = 0; c < cols; c++) {
            int i = row * cols + c;
            if (i >= m->count) break;
            album_cell(app, m, i, x + c * cw + (cw - s->art) / 2, ry + 4, cw, s, i == m->cursor);
        }
    }
}

float ui_now_info(App *app, float x, float y, float w, const NowStyle *s) {
    int t = app_current_track(app);
    float tx = x;
    if (s->art > 0) {
        if (s->art_border) ui_rect(x - 2, y - 2, s->art + 4, s->art + 4, s->art_border);
        ui_art(app, t >= 0 ? app->lib.tracks[t].album_id : -1, x, y, s->art, s->ph_bg, s->ph_fg);
        tx += s->art + 14;
    }
    float tw = x + w - tx;
    float ty = y + 4;
    if (t >= 0) {
        const Track *tr = &app->lib.tracks[t];
        ui_text_fit(app, tr->title, tx, ty, s->title_size, s->title, ALIGN_LEFT, tw);
        ty += ui_line_height(s->title_size) + 1;
        ui_text_fit(app, tr->artist, tx, ty, s->text_size, s->artist, ALIGN_LEFT, tw);
        ty += ui_line_height(s->text_size);
        ui_text_fit(app, tr->album, tx, ty, s->text_size, s->album, ALIGN_LEFT, tw);
        ty += ui_line_height(s->text_size) + 6;
        if (s->counter && app->queue.count) {
            char buf[32];
            snprintf(buf, sizeof buf, "%d / %d", app->queue.pos + 1, app->queue.count);
            ui_text(app, buf, tx, ty, s->text_size, s->meta, ALIGN_LEFT);
            ty += ui_line_height(s->text_size);
        }
    } else {
        ui_text_fit(app, "Nothing playing", tx, ty + 6, s->title_size, s->meta, ALIGN_LEFT, tw);
        ui_text_fit(app, "Pick a song in the library", tx, ty + 8 + ui_line_height(s->title_size), s->text_size, s->meta, ALIGN_LEFT, tw);
    }
    float bottom = y + s->art;
    return ty > bottom ? ty : bottom;
}

void ui_progress(App *app, float x, float y, float w, const BarStyle *s) {
    float pos = app_position(app), dur = app_duration(app);
    float f = dur > 0 ? pos / dur : 0;
    if (f > 1) f = 1;
    if (f < 0) f = 0;
    float r = s->h / 2;
    ui_round_rect(x, y, w, s->h, r, s->track);
    if (f > 0) ui_round_vgrad(x, y, w * f < s->h ? s->h : w * f, s->h, r, s->fill_top, s->fill_bottom);
    if (s->knob_r > 0) {
        if (s->knob_ring) ui_circle(x + w * f, y + r, s->knob_r + 1.5f, s->knob_ring);
        ui_circle(x + w * f, y + r, s->knob_r, s->knob);
    }
    if (s->time_size > 0) {
        char a[16], b[20];
        ui_fmt_time(pos, a, sizeof a);
        if (s->remaining) { b[0] = '-'; ui_fmt_time(dur - pos, b + 1, sizeof b - 1); }
        else ui_fmt_time(dur, b, sizeof b);
        float ty = y + s->h + (s->knob_r > r ? s->knob_r - r : 0) + 3;
        ui_text(app, a, x, ty, s->time_size, s->time, ALIGN_LEFT);
        ui_text(app, b, x + w, ty, s->time_size, s->time, ALIGN_RIGHT);
    }
}
