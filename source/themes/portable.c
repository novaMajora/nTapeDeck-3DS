// Portable: brushed silver and bright orange, like a sporty personal stereo.
#include <math.h>
#include <stdio.h>

#include "common.h"

static const u32 SILVER_HI = RGB(240, 240, 242), SILVER_LO = RGB(190, 191, 196), CHAR = RGB(40, 40, 44), WHITE = RGB(255, 255, 255);
static const u32 ORANGE = RGB(255, 122, 26), ORANGE_HI = RGB(255, 160, 70), ORANGE_LO = RGB(226, 86, 10), GRAY = RGB(100, 100, 106);

#define BAR_H 22
#define SIDE_W 118

static Pressed pressed;

static void status_bar(App *app, float w, float right) {
    ui_rect(0, 0, w, BAR_H, RGB(14, 14, 16));
    ui_sym_play(12, 11, 11, ORANGE);
    ui_text(app, APP_SHORT, 22, 3, 0.48f, WHITE, ALIGN_LEFT);
    ui_clock(app, right, 4, 0.42f, WHITE);
}

// Orange swoosh: a thick curve made of short segments.
static void swoosh(float x0, float y0, float x1, float y1, float bend, float thick, u32 c) {
    float px = x0, py = y0;
    for (int i = 1; i <= 24; i++) {
        float t = i / 24.0f;
        float x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t - sinf(t * (float)M_PI) * bend;
        ui_line(px, py, x, y, thick * (0.4f + 0.6f * sinf(t * (float)M_PI)), c);
        px = x;
        py = y;
    }
}

static void silver_bg(float w, float h) {
    ui_vgrad(0, 0, w, h, SILVER_HI, SILVER_LO);
    for (int y = 0; y < h; y += 2) ui_rect(0, y, w, 1, RGBA(255, 255, 255, 18));
}

static void splash(App *app) {
    silver_bg(TOP_W, SCREEN_H);
    swoosh(140, 230, 410, 60, 60, 18, RGBA(255, 122, 26, 200));
    swoosh(170, 236, 410, 100, 40, 8, RGBA(255, 160, 70, 150));
    status_bar(app, TOP_W, TOP_W - 6);
    ui_text(app, APP_SHORT, TOP_W / 2, 74, 1.25f, ORANGE, ALIGN_CENTER);
    ui_text(app, "3 D S   M U S I C   P L A Y E R", TOP_W / 2, 124, 0.5f, GRAY, ALIGN_CENTER);
    ui_vgrad(0, 176, TOP_W, 64, ORANGE_HI, ORANGE_LO);
    ui_rect(0, 176, TOP_W, 2, RGBA(255, 255, 255, 120));
    float cx = TOP_W / 2;
    ui_sym_play(cx - 112, 208, 12, RGB(255, 236, 200));
    ui_text(app, "P R E S S", cx - 58, 199, 0.5f, RGB(255, 236, 200), ALIGN_CENTER);
    ui_ring(cx, 208, 11, 9, RGB(255, 236, 200), 24);
    ui_text(app, "A", cx, 200, 0.45f, RGB(255, 236, 200), ALIGN_CENTER);
    ui_text(app, "T O   S T A R T", cx + 72, 199, 0.5f, RGB(255, 236, 200), ALIGN_CENTER);
}

static void orange_pill(float x, float y, float w, float h) {
    ui_round_vgrad(x, y, w, h, 5, ORANGE_HI, ORANGE_LO);
    ui_round_rect(x + 2, y + 1, w - 4, h * 0.4f, 4, RGBA(255, 255, 255, 50));
}

static void highlight(float x, float y, float w, float h) { orange_pill(x + 2, y + 1, w - 4, h - 2); }

static void draw_side(App *app) {
    MenuLevel *root = &app->menu.levels[0];
    ui_vgrad(0, BAR_H, SIDE_W, SCREEN_H - BAR_H, RGB(232, 232, 235), RGB(200, 201, 206));
    char buf[64];
    bool at_root = app->menu.depth == 1;
    for (int i = 1; i < root->count; i++) {
        float y = BAR_H + 6 + (i - 1) * 29;
        bool sel = i == root->cursor;
        if (sel) orange_pill(4, y, SIDE_W - 8, 26);
        u32 c = sel ? WHITE : CHAR;
        ui_icon(menu_icon(app, root, i), 18, y + 13, 15, sel ? WHITE : GRAY);
        ui_text_fit(app, menu_label(app, root, i, buf, sizeof buf), 32, y + 5, 0.46f, c, ALIGN_LEFT, SIDE_W - 36);
        (void)at_root;
    }
}

static void draw_library(App *app) {
    MenuLevel *m = menu_top(app);
    silver_bg(TOP_W, SCREEN_H);
    status_bar(app, TOP_W, TOP_W - 6);
    draw_side(app);
    float px = SIDE_W + 6, py = BAR_H + 6, pw = TOP_W - px - 6, ph = SCREEN_H - py - 6;
    ui_round_vgrad(px, py, pw, ph, 8, RGB(72, 72, 78), RGB(36, 36, 40));
    swoosh(px + pw * 0.4f, py + ph, px + pw, py + ph * 0.5f, 18, 6, RGBA(255, 122, 26, 120));
    const char *title = app->menu.depth == 1 ? "Music Library" : m->title;
    ui_text_fit(app, title, px + 12, py + 6, 0.54f, WHITE, ALIGN_LEFT, pw - 24);
    if (app->menu.depth == 1) {
        // Root: show what the highlighted category holds.
        int t = app_current_track(app);
        ui_round_rect(px + 8, py + 32, pw - 16, ph - 40, 6, RGB(250, 250, 252));
        if (t >= 0) {
            NowStyle ns = {.art = 96, .title_size = 0.56f, .text_size = 0.46f, .title = CHAR, .artist = ORANGE_LO, .album = GRAY, .meta = GRAY, .ph_bg = SILVER_LO, .ph_fg = WHITE, .counter = true};
            ui_now_info(app, px + 18, py + 44, pw - 36, &ns);
        } else {
            ui_text(app, "Press A to open", px + pw / 2, py + 90, 0.5f, GRAY, ALIGN_CENTER);
        }
        return;
    }
    if (m->kind == MENU_ALBUMS) {
        StripStyle st = {.art = 74, .gap = 14, .text_size = 0.38f, .caption = WHITE, .caption_sub = RGB(200, 200, 205), .border = ORANGE, .ph_bg = RGB(110, 110, 116), .ph_fg = WHITE, .arrow = ORANGE};
        ui_album_strip(app, m, px, py + 40, pw, ph - 40, &st);
        return;
    }
    ui_round_rect(px + 8, py + 30, pw - 16, ph - 38, 6, RGB(250, 250, 252));
    ListStyle ls = {
        .row_h = 26, .text_size = 0.48f, .detail_size = 0.4f, .pad = 10,
        .icon = m->kind == MENU_SETTINGS, .value = true, .chevron = m->kind == MENU_SETTINGS, .divider = true,
        .text = CHAR, .text_sel = WHITE, .val = GRAY, .val_sel = WHITE, .chev = GRAY, .chev_sel = WHITE, .div = RGB(226, 226, 230),
        .icon_c = GRAY, .icon_sel = WHITE, .highlight = highlight,
    };
    ui_list(app, m, px + 10, py + 33, pw - 20, ph - 44, &ls);
    if (!m->count) ui_text(app, "Nothing here yet", px + pw / 2, py + 90, 0.5f, GRAY, ALIGN_CENTER);
}

// Now Playing panel, shared by both screens (`w` wide).
#define CTRL_Y 196
static void now_panel(App *app, float w, float y0, bool bottom) {
    float x = 8;
    ui_round_vgrad(x, y0, w - 16, SCREEN_H - y0 - 6, 8, SILVER_HI, SILVER_LO);
    ui_ring(x + 18, y0 + 15, 9, 7, ORANGE, 24);
    ui_sym_play(x + 19, y0 + 15, 8, ORANGE);
    ui_text(app, "Now Playing", x + 32, y0 + 4, 0.54f, ORANGE, ALIGN_LEFT);
    NowStyle ns = {.art = 88, .title_size = 0.56f, .text_size = 0.46f, .title = CHAR, .artist = ORANGE_LO, .album = CHAR, .meta = GRAY,
                   .ph_bg = RGB(150, 150, 156), .ph_fg = WHITE};
    ui_now_info(app, x + 10, y0 + 30, w - 110, &ns);
    // Right column: position in the queue and the mode lamps
    float rx = w - 62;
    ui_rect(rx - 8, y0 + 28, 1, 90, RGBA(0, 0, 0, 40));
    char buf[32];
    if (app->queue.count) {
        snprintf(buf, sizeof buf, "%d/%d", app->queue.pos + 1, app->queue.count);
        ui_text(app, buf, rx + 22, y0 + 30, 0.46f, CHAR, ALIGN_CENTER);
    }
    ui_sym_shuffle(rx, y0 + 62, 12, app->cfg.shuffle ? ORANGE : GRAY);
    ui_text(app, "SHUF", rx + 10, y0 + 55, 0.36f, app->cfg.shuffle ? ORANGE : GRAY, ALIGN_LEFT);
    ui_sym_repeat(rx, y0 + 82, 12, app->cfg.repeat ? ORANGE : GRAY, app->cfg.repeat == REPEAT_ONE);
    ui_text(app, "REP", rx + 10, y0 + 75, 0.36f, app->cfg.repeat ? ORANGE : GRAY, ALIGN_LEFT);
    ui_round_rect(rx - 2, y0 + 96, 30, 14, 3, GRAY);
    ui_text(app, "MP3", rx + 13, y0 + 96, 0.34f, WHITE, ALIGN_CENTER);
    BarStyle bs = {.h = 6, .track = RGB(60, 60, 64), .fill_top = ORANGE_HI, .fill_bottom = ORANGE_LO, .knob = RGB(255, 240, 220), .knob_ring = ORANGE_LO,
                   .time = CHAR, .knob_r = 6, .time_size = 0.44f, .remaining = true};
    ui_progress(app, x + 14, y0 + 130, w - 44, &bs);
    if (!bottom) return;
    // Transport
    float cx = w / 2;
    ui_circle(cx - 66, CTRL_Y + 12, 19, RGB(30, 30, 34));
    ui_circle(cx - 66, CTRL_Y + 10, 16, pressed.id == B_PREV ? RGB(70, 70, 76) : RGB(52, 52, 58));
    ui_sym_prev(cx - 66, CTRL_Y + 11, 12, WHITE);
    ui_circle(cx, CTRL_Y + 12, 27, ORANGE_LO);
    ui_circle(cx, CTRL_Y + 11, 24, pressed.id == B_PLAY ? RGB(70, 70, 76) : RGB(40, 40, 44));
    ui_circle(cx, CTRL_Y + 4, 16, RGBA(255, 255, 255, 25));
    if (app_playing(app)) ui_sym_pause(cx, CTRL_Y + 11, 18, WHITE);
    else ui_sym_play(cx + 2, CTRL_Y + 11, 18, WHITE);
    ui_circle(cx + 66, CTRL_Y + 12, 19, RGB(30, 30, 34));
    ui_circle(cx + 66, CTRL_Y + 10, 16, pressed.id == B_NEXT ? RGB(70, 70, 76) : RGB(52, 52, 58));
    ui_sym_next(cx + 66, CTRL_Y + 11, 12, WHITE);
    // Volume: tap the left half of the meter to lower, the right half to raise.
    ui_sym_volume(w - 76, CTRL_Y + 4, 13, CHAR);
    for (int i = 0; i < 8; i++) ui_rect(w - 64 + i * 6, CTRL_Y + 12 - i, 4, 6 + i, i < (int)(app->cfg.volume * 8 + 0.5f) ? ORANGE : RGB(150, 150, 156));
    ui_text(app, "VOL", w - 40, CTRL_Y + 20, 0.36f, CHAR, ALIGN_CENTER);
}

static void draw_top(App *app) {
    if (app->show_now_playing) {
        silver_bg(TOP_W, SCREEN_H);
        status_bar(app, TOP_W, TOP_W - 6);
        now_panel(app, TOP_W, BAR_H + 6, false);
    } else {
        draw_library(app);
    }
    toast(app, RGBA(30, 30, 34, 220), ORANGE_HI);
}

static const Btn BUTTONS[] = {
    {B_PREV, 160 - 66 - 22, CTRL_Y - 12, 44, 46}, {B_PLAY, 160 - 30, CTRL_Y - 18, 60, 58}, {B_NEXT, 160 + 66 - 22, CTRL_Y - 12, 44, 46},
    {B_VOLDOWN, BOTTOM_W - 86, CTRL_Y - 8, 40, 42}, {B_VOLUP, BOTTOM_W - 46, CTRL_Y - 8, 40, 42}, {B_SHUFFLE, 250, 52, 60, 22}, {B_REPEAT, 250, 74, 60, 22},
    {B_NOWTOP, 16, 52, 90, 90},
};

static void draw_bottom(App *app) {
    pressed_tick(app, &pressed);
    silver_bg(BOTTOM_W, SCREEN_H);
    status_bar(app, BOTTOM_W, THEME_BTN_X - 4);
    now_panel(app, BOTTOM_W, BAR_H + 2, true);
}

static void touch(App *app, const TouchInput *t) {
    if (theme_scrub(app, t, 22, BAR_H + 2 + 130, BOTTOM_W - 44, 6)) return;
    btn_touch(app, t, BUTTONS, sizeof BUTTONS / sizeof *BUTTONS, &pressed);
}

const Theme THEME_PORTABLE = {
    .name = "Portable",
    .description = "Silver and orange personal stereo",
    .accent = RGB(255, 122, 26), .swatch = RGB(205, 206, 210),
    .album_cols = 1,
    .draw_splash = splash,
    .draw_top = draw_top,
    .draw_bottom = draw_bottom,
    .touch = touch,
};
