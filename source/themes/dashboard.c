// Dashboard: an early-2000s console dashboard. Black and neon green, a glowing orb, uppercase
// menus and a green visualizer.
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

static const u32 GREEN = RGB(150, 230, 30), GREEN_HI = RGB(200, 255, 90);
static const u32 WHITE = RGB(238, 246, 230), GRAY = RGB(150, 160, 140), BG = RGB(4, 10, 2);

#define BAR_H 22

static Pressed pressed;

static void upper(const char *s, char *out, int len) {
    int i = 0;
    for (; s[i] && i < len - 1; i++) out[i] = (char)toupper((unsigned char)s[i]);
    out[i] = 0;
}

static void backdrop(App *app, float w) {
    ui_vgrad(0, 0, w, SCREEN_H, RGB(10, 26, 6), BG);
    // Organic green swirls
    for (int i = 0; i < 3; i++) {
        float t = app->time * 0.15f + i * 2.1f;
        float cx = w * (0.35f + 0.25f * sinf(t)), cy = 120 + 50 * cosf(t * 0.8f);
        ui_ring(cx, cy, 150 - i * 30, 146 - i * 30, RGBA(120, 220, 30, 30), 48);
        ui_circle(cx, cy, 90 - i * 20, RGBA(90, 180, 20, 8));
    }
    for (int y = 0; y < SCREEN_H; y += 4) ui_rect(0, y, w, 1, RGBA(150, 230, 30, 6));
}

static void status_bar(App *app, float w, float right, const char *left) {
    ui_rect(0, 0, w, BAR_H, RGBA(0, 0, 0, 150));
    ui_signal(6, 5, 12, WHITE, RGB(60, 70, 50));
    ui_text(app, left, 30, 3, 0.46f, WHITE, ALIGN_LEFT);
    ui_clock(app, right, 4, 0.42f, WHITE);
    ui_hgrad(0, BAR_H, w * 0.7f, 1, GREEN, RGBA(150, 230, 30, 0));
}

static void orb(App *app, float cx, float cy, float r) {
    float pulse = 0.5f + 0.5f * sinf(app->time * 2);
    for (int i = 5; i > 0; i--) ui_circle(cx, cy, r + i * 7 + pulse * 3, RGBA(140, 230, 30, 12));
    ui_circle(cx, cy, r, RGB(30, 70, 8));
    ui_circle(cx, cy, r * 0.93f, RGB(70, 150, 16));
    ui_circle(cx - r * 0.1f, cy - r * 0.1f, r * 0.75f, RGB(120, 210, 30));
    ui_ring(cx, cy, r * 0.5f, r * 0.38f, RGBA(20, 50, 4, 170), 36); // core ring
    ui_circle(cx, cy, r * 0.22f, RGBA(220, 255, 140, 160));
    ui_circle(cx - r * 0.32f, cy - r * 0.42f, r * 0.25f, RGBA(255, 255, 255, 90));
    ui_ring(cx, cy, r + 12, r + 10, RGBA(150, 230, 30, 110), 60);
}

static void sel_bar(float x, float y, float w, float h) {
    ui_hgrad(x, y, w, h, RGBA(150, 230, 30, 230), RGBA(150, 230, 30, 0));
    ui_rect(x, y, w * 0.7f, 1, GREEN_HI);
}

static void outline_sel(float x, float y, float w, float h) {
    ui_round_rect(x - 2, y - 2, w + 4, h + 4, 8, RGBA(150, 230, 30, 60));
    ui_round_rect(x, y, w, h, 7, GREEN);
    ui_round_rect(x + 2, y + 2, w - 4, h - 4, 6, RGB(16, 34, 8));
}

static void draw_home(App *app) {
    backdrop(app, TOP_W);
    status_bar(app, TOP_W, TOP_W - 6, APP_SHORT);
    orb(app, 110, 108, 52);
    ui_text(app, "nTape", 24, 184, 0.95f, GREEN, ALIGN_LEFT);
    ui_text(app, "Deck", 24 + ui_text_width(app, "nTape", 0.95f) + 2, 184, 0.95f, WHITE, ALIGN_LEFT);
    MenuLevel *m = menu_top(app);
    char buf[64], up[64];
    for (int i = 0; i < m->count; i++) {
        float y = 34 + i * 25;
        bool sel = i == m->cursor;
        if (sel) sel_bar(232, y - 2, 168, 23);
        upper(menu_label(app, m, i, buf, sizeof buf), up, sizeof up);
        ui_text(app, up, 246, y, 0.5f, sel ? RGB(10, 30, 2) : WHITE, ALIGN_LEFT);
        if (!sel) ui_rect(246, y + 21, 140, 1, RGBA(150, 230, 30, 40));
    }
}

static void draw_list(App *app) {
    MenuLevel *m = menu_top(app);
    backdrop(app, TOP_W);
    status_bar(app, TOP_W, TOP_W - 6, APP_SHORT);
    // Faded orb decoration on the right
    ui_circle(340, 150, 70, RGBA(120, 210, 30, 30));
    ui_circle(340, 150, 50, RGBA(120, 210, 30, 30));
    char title[64];
    upper(m->title, title, sizeof title);
    ui_text_fit(app, title, 14, BAR_H + 4, 0.6f, GREEN, ALIGN_LEFT, 360);
    float y0 = BAR_H + 32;
    if (m->kind == MENU_ALBUMS) {
        StripStyle st = {.art = 84, .gap = 14, .text_size = 0.4f, .caption = WHITE, .caption_sub = GRAY, .border = GREEN, .ph_bg = RGB(30, 70, 8), .ph_fg = GREEN, .arrow = GREEN};
        ui_album_strip(app, m, 0, y0 + 6, TOP_W, SCREEN_H - y0, &st);
        return;
    }
    ListStyle ls = {
        .row_h = 27, .text_size = 0.52f, .detail_size = 0.42f, .pad = 14,
        .icon = m->kind == MENU_SETTINGS, .value = true, .text = WHITE, .text_sel = WHITE, .val = GRAY, .val_sel = GREEN_HI,
        .icon_c = GRAY, .icon_sel = GREEN, .highlight = outline_sel,
    };
    ui_list(app, m, 10, y0, 290, SCREEN_H - y0 - 4, &ls);
    int visible = (int)((SCREEN_H - y0 - 4) / 27);
    if (m->scroll > 0.05f) { ui_line(306, y0 + 10, 312, y0 + 4, 2, GREEN); ui_line(318, y0 + 10, 312, y0 + 4, 2, GREEN); }
    if (m->scroll + visible < m->count) { ui_line(306, SCREEN_H - 14, 312, SCREEN_H - 8, 2, GREEN); ui_line(318, SCREEN_H - 14, 312, SCREEN_H - 8, 2, GREEN); }
    if (!m->count) ui_text(app, "NOTHING HERE YET", 30, 110, 0.5f, GRAY, ALIGN_LEFT);
}

static void info(App *app, float x, float y, float w, float art) {
    NowStyle ns = {.art = art, .title_size = 0.6f, .text_size = 0.47f, .title = WHITE, .artist = GREEN, .album = GRAY, .meta = GRAY,
                   .ph_bg = RGB(30, 70, 8), .ph_fg = GREEN, .art_border = GREEN};
    ui_now_info(app, x, y, w, &ns);
}

static void spectrum(App *app, float x, float y, float w, float h) {
    int bands = 24;
    float bw = w / bands;
    for (int k = 0; k < bands; k++) {
        int segs = (int)(app->spectrum[k] * 8 + 0.5f);
        for (int s = 0; s < 8; s++) {
            u32 c = s < segs ? (s > 5 ? GREEN_HI : GREEN) : RGBA(150, 230, 30, 25);
            ui_rect(x + k * bw, y + h - (s + 1) * (h / 8), bw - 2, h / 8 - 1.5f, c);
        }
    }
}

static void draw_top(App *app) {
    if (app->show_now_playing) {
        backdrop(app, TOP_W);
        status_bar(app, TOP_W, TOP_W - 6, "Now Playing");
        info(app, 24, 40, TOP_W - 48, 110);
        spectrum(app, 148, 120, 230, 36);
        BarStyle bs = {.h = 5, .track = RGBA(150, 230, 30, 50), .fill_top = GREEN_HI, .fill_bottom = GREEN, .knob = GREEN_HI, .knob_ring = RGBA(150, 230, 30, 90),
                       .time = WHITE, .knob_r = 6, .time_size = 0.44f, .remaining = true};
        ui_progress(app, 24, 182, TOP_W - 48, &bs);
    } else if (app->menu.depth == 1) {
        draw_home(app);
    } else {
        draw_list(app);
    }
    toast(app, RGBA(16, 34, 8, 230), GREEN);
}

// ------------------------------------------------------------------------------------- bottom

#define CTRL_Y 206

static const Btn BUTTONS[] = {
    {B_REPEAT, 6, CTRL_Y - 20, 46, 40}, {B_PREV, 60, CTRL_Y - 22, 50, 44}, {B_PLAY, 128, CTRL_Y - 30, 64, 60},
    {B_NEXT, 210, CTRL_Y - 22, 50, 44}, {B_SHUFFLE, 268, CTRL_Y - 20, 46, 40}, {B_NOWTOP, 14, 30, 90, 90},
};

static void pod(float cx, float cy, float r, bool hot) {
    ui_circle(cx, cy, r + 3, hot ? RGBA(150, 230, 30, 140) : RGBA(150, 230, 30, 50));
    ui_circle(cx, cy, r, RGB(14, 30, 6));
    ui_circle(cx, cy - r * 0.3f, r * 0.6f, RGBA(150, 230, 30, 25));
}

static void draw_bottom(App *app) {
    pressed_tick(app, &pressed);
    backdrop(app, BOTTOM_W);
    status_bar(app, BOTTOM_W, THEME_BTN_X - 4, "Now Playing");
    if (app->queue.count) {
        char c[16];
        snprintf(c, sizeof c, "%d / %d", app->queue.pos + 1, app->queue.count);
        ui_text(app, c, BOTTOM_W - 18, 32, 0.42f, GREEN, ALIGN_RIGHT);
    }
    info(app, 16, 32, BOTTOM_W - 76, 92);
    spectrum(app, 124, 98, 180, 26);
    BarStyle bs = {.h = 5, .track = RGBA(150, 230, 30, 50), .fill_top = GREEN_HI, .fill_bottom = GREEN, .knob = GREEN_HI, .knob_ring = RGBA(150, 230, 30, 90),
                   .time = WHITE, .knob_r = 6, .time_size = 0.44f, .remaining = true};
    ui_progress(app, 18, 140, BOTTOM_W - 36, &bs);
    u32 off = RGB(90, 110, 70);
    ui_sym_repeat(28, CTRL_Y, 18, app->cfg.repeat ? GREEN : off, app->cfg.repeat == REPEAT_ONE);
    pod(85, CTRL_Y, 20, pressed.id == B_PREV);
    ui_sym_prev(85, CTRL_Y, 14, WHITE);
    ui_circle(160, CTRL_Y, 30, RGBA(150, 230, 30, pressed.id == B_PLAY ? 160 : 90));
    ui_ring(160, CTRL_Y, 28, 24, GREEN, 40);
    ui_circle(160, CTRL_Y, 24, RGB(14, 34, 6));
    if (app_playing(app)) ui_sym_pause(160, CTRL_Y, 20, GREEN_HI);
    else ui_sym_play(162, CTRL_Y, 20, GREEN_HI);
    pod(235, CTRL_Y, 20, pressed.id == B_NEXT);
    ui_sym_next(235, CTRL_Y, 14, WHITE);
    ui_sym_shuffle(292, CTRL_Y, 18, app->cfg.shuffle ? GREEN : off);
}

static void touch(App *app, const TouchInput *t) {
    if (theme_scrub(app, t, 18, 140, BOTTOM_W - 36, 5)) return;
    btn_touch(app, t, BUTTONS, sizeof BUTTONS / sizeof *BUTTONS, &pressed);
}

const Theme THEME_DASHBOARD = {
    .name = "Dashboard",
    .description = "Neon green console dashboard",
    .accent = RGB(150, 230, 30), .swatch = RGB(14, 34, 6),
    .album_cols = 1,
    .draw_top = draw_top,
    .draw_bottom = draw_bottom,
    .touch = touch,
};
