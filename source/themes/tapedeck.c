// Tape Deck (default): cream index cards, retro red/orange/yellow stripes and a cassette with
// turning reels. Home shows the nTapeDeck cassette art over a row of category tiles.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

static const u32 CREAM = RGB(240, 228, 198), CREAM_DK = RGB(222, 205, 168), INK = RGB(38, 32, 28), INK_SOFT = RGB(110, 96, 82);
static const u32 BLACK = RGB(22, 20, 18), PANEL = RGB(43, 37, 33), RED = RGB(214, 66, 40), ORANGE = RGB(240, 132, 36), YELLOW = RGB(246, 186, 44);

static Pressed pressed;
static float reel_angle, tile_scroll;

#define BAR_H 22
#define SIDEBAR_X 14
#define SIDEBAR_W 98
#define PANEL_X 116

// Labels this theme renames (a playlist is a mixtape here).
static const char *label_for(App *app, const MenuLevel *m, int i, char *buf, int len) {
    if (m->kind == MENU_ROOT && i == ROOT_SONGS) return "Music";
    if (m->kind == MENU_ROOT && i == ROOT_PLAYLISTS) return "Mixtapes";
    if (m->kind == MENU_PLAYLISTS) return menu_label(app, m, i, buf, len);
    return menu_label(app, m, i, buf, len);
}

static void stripes_h(float x, float y, float w, float band) {
    ui_rect(x, y, w, band, RED);
    ui_rect(x, y + band, w, band, ORANGE);
    ui_rect(x, y + band * 2, w, band, YELLOW);
}

static void stripes_v(float x, float y, float h, float band) {
    ui_rect(x, y, band, h, RED);
    ui_rect(x + band, y, band, h, ORANGE);
    ui_rect(x + band * 2, y, band, h, YELLOW);
}

static void status_bar(App *app, float w, const char *title, bool screen_bottom) {
    ui_rect(0, 0, w, BAR_H, BLACK);
    float x = 6;
    if (!screen_bottom) {
        ui_signal(x, 6, 11, RGB(240, 240, 240), RGB(90, 90, 90));
        x += 20;
    } else {
        ui_icon(ICON_SONG, x + 7, 11, 14, CREAM);
        x += 20;
    }
    ui_text(app, title, x, 3, 0.5f, CREAM, ALIGN_LEFT);
    ui_clock(app, screen_bottom ? THEME_BTN_X - 4 : w - 6, 4, 0.44f, RGB(240, 240, 240));
}

// A reel hub: cream disc with notches, turned by `angle`.
static void reel(float cx, float cy, float r, float angle, u32 ring) {
    if (ring) ui_circle(cx, cy, r + 2, ring);
    ui_circle(cx, cy, r, CREAM);
    for (int k = 0; k < 6; k++) {
        float a = angle + k * (float)M_PI / 3;
        ui_rect(cx + cosf(a) * r * 0.72f - r * 0.12f, cy + sinf(a) * r * 0.72f - r * 0.12f, r * 0.24f, r * 0.24f, BLACK);
    }
    ui_circle(cx, cy, r * 0.32f, PANEL);
}

// A cassette `w` wide. The label shows `title`; `progress` moves tape between the reels.
static void cassette(App *app, float x, float y, float w, u32 label, const char *title, const char *sub, float progress, float angle) {
    float h = w * 0.62f, s = w / 300.0f;
    ui_round_rect(x + 3 * s, y + 5 * s, w, h, 12 * s, RGBA(0, 0, 0, 110));
    ui_round_rect(x, y, w, h, 12 * s, BLACK);
    for (int i = 0; i < 4; i++) { // screws
        float sx = x + (i % 2 ? w - 12 * s : 12 * s), sy = y + (i < 2 ? 11 * s : h - 11 * s);
        ui_circle(sx, sy, 4.5f * s, RGB(170, 168, 160));
        ui_rect(sx - 3 * s, sy - 0.6f * s, 6 * s, 1.2f * s, RGB(70, 70, 70));
    }
    float lx = x + 22 * s, ly = y + 18 * s, lw = w - 44 * s, lh = h * 0.62f;
    ui_round_rect(lx, ly, lw, lh, 7 * s, label);
    stripes_h(lx, ly + lh * 0.5f, lw, 5 * s);
    if (title) ui_text_fit(app, title, lx + 10 * s, ly + 4 * s, 0.62f * s * 1.4f, INK, ALIGN_LEFT, lw - 20 * s);
    if (sub) ui_text_fit(app, sub, lx + 10 * s, ly + 4 * s + 26 * s, 0.42f * s * 1.4f, RED, ALIGN_LEFT, lw - 20 * s);
    // Window and reels
    float wx = x + w * 0.22f, wy = ly + lh * 0.5f + 18 * s, ww = w * 0.56f, wh = 40 * s;
    ui_round_rect(wx, wy, ww, wh, wh / 2, RGB(30, 27, 24));
    float lr = (10 + 16 * (1 - progress)) * s, rr = (10 + 16 * progress) * s;
    float c1 = wx + 34 * s, c2 = wx + ww - 34 * s, cy = wy + wh / 2;
    ui_circle(c1, cy, lr > 14 * s ? lr : 14 * s, RGB(58, 40, 30));
    ui_circle(c2, cy, rr > 14 * s ? rr : 14 * s, RGB(58, 40, 30));
    reel(c1, cy, 12 * s, angle, ORANGE);
    reel(c2, cy, 12 * s, angle * 0.93f, ORANGE);
    // Bottom trapezoid
    float by = y + h - 30 * s;
    u32 tc = RGB(34, 31, 28);
    C2D_DrawTriangle(x + 60 * s, y + h, tc, x + 78 * s, by, tc, x + w - 78 * s, by, tc, 0.5f);
    C2D_DrawTriangle(x + 60 * s, y + h, tc, x + w - 78 * s, by, tc, x + w - 60 * s, y + h, tc, 0.5f);
    for (int i = 0; i < 2; i++) ui_rect(x + w * (0.38f + i * 0.2f), by + 12 * s, 9 * s, 9 * s, CREAM_DK);
}

// ------------------------------------------------------------------------------------- top

static void draw_home(App *app) {
    ui_vgrad(0, 0, TOP_W, SCREEN_H, RGB(52, 44, 38), RGB(24, 20, 18));
    status_bar(app, TOP_W, APP_SHORT, false);
    stripes_h(0, 92, TOP_W, 9);
    if (app->has_logo) {
        float h = 150, w = h * app->logo_w / app->logo_h;
        ui_image(&app->logo, (TOP_W - w) / 2, 26, w, h);
    } else {
        cassette(app, 80, 30, 240, CREAM, APP_SHORT, "3DS", 0.3f, app->time);
    }
    // Category tiles
    MenuLevel *m = menu_top(app);
    float tw = 70, gap = 6, k = app->dt * 10;
    if (k > 1) k = 1;
    float target = m->cursor - 2;
    if (target < 0) target = 0;
    if (target > m->count - 5) target = m->count - 5;
    tile_scroll += (target - tile_scroll) * k;
    char buf[64];
    for (int i = 0; i < m->count; i++) {
        float x = 12 + (i - tile_scroll) * (tw + gap);
        if (x < -tw || x > TOP_W) continue;
        float y = 182;
        bool sel = i == m->cursor;
        if (sel) ui_round_rect(x - 3, y - 3, tw + 6, 56, 8, RGBA(240, 132, 36, 90));
        ui_round_rect(x, y, tw, 50, 6, sel ? ORANGE : RGB(64, 56, 50));
        ui_round_vgrad(x + 2, y + 2, tw - 4, 46, 5, RGB(56, 49, 44), RGB(36, 31, 28));
        ui_icon(menu_icon(app, m, i), x + tw / 2, y + 18, 20, sel ? ORANGE : CREAM);
        ui_text_fit(app, label_for(app, m, i, buf, sizeof buf), x + tw / 2, y + 31, 0.4f, sel ? RGB(255, 255, 255) : CREAM, ALIGN_CENTER, tw - 6);
    }
}

static void highlight_row(float x, float y, float w, float h) {
    ui_rect(x, y + 1, w, h - 2, RGB(246, 214, 168));
    ui_rect(x, y + 1, 4, h - 2, ORANGE);
}

static void draw_sidebar(App *app) {
    MenuLevel *root = &app->menu.levels[0];
    ui_rect(SIDEBAR_X, BAR_H, SIDEBAR_W, SCREEN_H - BAR_H, PANEL);
    char buf[64];
    for (int i = 1; i < root->count; i++) { // skip "Now Playing"
        float y = BAR_H + 6 + (i - 1) * 29;
        bool sel = i == root->cursor;
        if (sel) ui_round_vgrad(SIDEBAR_X + 3, y, SIDEBAR_W - 6, 26, 4, RGB(236, 112, 40), RGB(200, 74, 30));
        ui_icon(menu_icon(app, root, i), SIDEBAR_X + 16, y + 13, 14, sel ? RGB(255, 255, 255) : CREAM);
        ui_text_fit(app, label_for(app, root, i, buf, sizeof buf), SIDEBAR_X + 30, y + 5, 0.44f, sel ? RGB(255, 255, 255) : CREAM, ALIGN_LEFT, SIDEBAR_W - 34);
    }
}

static void draw_mixtapes(App *app, MenuLevel *m, float x, float y, float w, float h) {
    static const u32 labels[] = {RGB(90, 70, 140), RGB(240, 150, 60), RGB(40, 40, 44), RGB(60, 110, 190), RGB(222, 205, 120), RGB(210, 70, 60)};
    int cols = 3;
    float cw = w / cols, ch = 88;
    int rows = (int)(h / ch);
    MenuLevel rv = *m;
    rv.count = (m->count + cols - 1) / cols;
    rv.cursor = m->cursor / cols;
    float first = ui_list_scroll(&rv, rows, app->dt);
    m->scroll = rv.scroll;
    char buf[64], val[32];
    for (int i = 0; i < m->count; i++) {
        float cx = x + (i % cols) * cw + 4, cy = y + (i / cols - first) * ch + 4;
        if (cy < y - 2 || cy + ch > y + h + 6) continue;
        bool sel = i == m->cursor;
        if (sel) ui_round_rect(cx - 3, cy - 3, cw - 2, ch - 2, 8, ORANGE);
        ui_round_rect(cx, cy, cw - 8, ch - 8, 6, CREAM_DK);
        cassette(app, cx + 12, cy + 5, cw - 32, labels[i % 6], NULL, NULL, 0.5f, 0);
        ui_text_fit(app, label_for(app, m, i, buf, sizeof buf), cx + (cw - 8) / 2, cy + 50, 0.42f, INK, ALIGN_CENTER, cw - 14);
        ui_text(app, menu_value(app, m, i, val, sizeof val), cx + (cw - 8) / 2, cy + 64, 0.34f, INK_SOFT, ALIGN_CENTER);
    }
    if (!m->count) ui_text(app, "No mixtapes yet", x + w / 2, y + 60, 0.5f, INK_SOFT, ALIGN_CENTER);
}

static void draw_settings(App *app, MenuLevel *m, float x, float y, float w) {
    char buf[64], val[48];
    for (int i = 0; i < m->count; i++) {
        float ry = y + 8 + i * 34;
        bool sel = i == m->cursor;
        if (sel) highlight_row(x + 6, ry, w - 12, 30);
        else ui_rect(x + 10, ry + 29, w - 20, 1, CREAM_DK);
        ui_text(app, menu_label(app, m, i, buf, sizeof buf), x + 16, ry + 6, 0.5f, INK, ALIGN_LEFT);
        const char *v = menu_value(app, m, i, val, sizeof val);
        float vx = x + w - 60;
        ui_text_fit(app, v, vx, ry + 7, 0.45f, INK, ALIGN_CENTER, 90);
        if (i != SET_ABOUT) {
            ui_line(vx - 52, ry + 15, vx - 46, ry + 9, 2, INK_SOFT);
            ui_line(vx - 52, ry + 15, vx - 46, ry + 21, 2, INK_SOFT);
            ui_line(vx + 52, ry + 15, vx + 46, ry + 9, 2, INK_SOFT);
            ui_line(vx + 52, ry + 15, vx + 46, ry + 21, 2, INK_SOFT);
        }
    }
}

static void draw_library(App *app) {
    MenuLevel *m = menu_top(app);
    ui_rect(0, 0, TOP_W, SCREEN_H, RGB(30, 26, 23));
    char title[80], buf[64];
    snprintf(title, sizeof title, "%s", m->kind == MENU_PLAYLISTS ? "Mixtapes" : m->title);
    status_bar(app, TOP_W, title, false);
    stripes_v(0, BAR_H, SCREEN_H - BAR_H, 4.6f);
    draw_sidebar(app);
    float px = PANEL_X, py = BAR_H + 4, pw = TOP_W - PANEL_X - 4, ph = SCREEN_H - py - 4;
    ui_round_rect(px, py, pw, ph, 6, CREAM);
    (void)buf;
    if (m->kind == MENU_PLAYLISTS) {
        draw_mixtapes(app, m, px + 4, py + 4, pw - 8, ph - 8);
    } else if (m->kind == MENU_SETTINGS) {
        draw_settings(app, m, px, py, pw);
    } else if (m->kind == MENU_ALBUMS) {
        StripStyle st = {.art = 56, .gap = 10, .text_size = 0.36f, .caption = INK, .caption_sub = INK_SOFT, .border = ORANGE, .ph_bg = RGB(120, 96, 70), .ph_fg = CREAM, .arrow = INK_SOFT};
        ui_album_grid(app, m, px + 2, py + 4, pw - 4, ph - 6, 4, &st);
    } else {
        ListStyle ls = {
            .row_h = 34, .text_size = 0.5f, .detail_size = 0.4f, .pad = 8,
            .art = true, .detail = true, .value = true, .chevron = true, .divider = true,
            .text = INK, .text_sel = INK, .sub = INK_SOFT, .sub_sel = INK_SOFT, .val = INK_SOFT, .val_sel = INK,
            .chev = INK_SOFT, .chev_sel = ORANGE, .div = CREAM_DK, .icon_c = INK_SOFT, .icon_sel = ORANGE,
            .ph_bg = RGB(120, 96, 70), .ph_fg = CREAM, .highlight = highlight_row,
        };
        ui_list(app, m, px + 2, py + 2, pw - 4, ph - 4, &ls);
        if (!m->count) ui_text(app, "Nothing here yet", px + pw / 2, py + 60, 0.5f, INK_SOFT, ALIGN_CENTER);
    }
}

static void draw_now_top(App *app) {
    ui_vgrad(0, 0, TOP_W, SCREEN_H, RGB(52, 44, 38), RGB(24, 20, 18));
    status_bar(app, TOP_W, "Now Playing", false);
    stripes_h(0, 120, TOP_W, 8);
    int t = app_current_track(app);
    float pos = app_position(app), dur = app_duration(app);
    float f = dur > 0 ? pos / dur : 0;
    if (app_playing(app)) reel_angle += app->dt * 3.2f;
    cassette(app, 50, 32, 300, CREAM, t >= 0 ? app->lib.tracks[t].title : "Insert a song", t >= 0 ? app->lib.tracks[t].artist : NULL, f > 1 ? 1 : f, reel_angle);
}

static void draw_top(App *app) {
    if (app->show_now_playing) draw_now_top(app);
    else if (app->menu.depth == 1) draw_home(app);
    else draw_library(app);
    toast(app, RGBA(22, 20, 18, 230), YELLOW);
}

// ------------------------------------------------------------------------------------- bottom

#define CARD_Y 28
#define TAPE_X 16
#define TAPE_Y 138
#define TAPE_W 288
#define CTRL_Y 190

static const Btn BUTTONS[] = {
    {B_REPEAT, 6, CTRL_Y, 44, 46},
    {B_PREV, 72, CTRL_Y + 3, 56, 40},
    {B_PLAY, 132, CTRL_Y + 3, 56, 40},
    {B_NEXT, 192, CTRL_Y + 3, 56, 40},
    {B_SHUFFLE, 270, CTRL_Y, 44, 46},
    {B_FAV, 270, CARD_Y + 4, 40, 34},
    {B_NOWTOP, 14, CARD_Y + 6, 96, 96},
};

static void key(float x, float y, float w, float h, bool hot, bool lit) {
    if (lit) ui_round_rect(x - 2, y - 2, w + 4, h + 4, 7, RGBA(240, 132, 36, 160));
    ui_round_rect(x, y, w, h, 6, lit ? ORANGE : RGB(80, 70, 62));
    ui_round_vgrad(x + 2, y + 2 + (hot ? 2 : 0), w - 4, h - 4, 5, lit ? RGB(90, 46, 22) : RGB(70, 62, 56), lit ? RGB(60, 30, 14) : RGB(40, 35, 31));
}

static void draw_bottom(App *app) {
    pressed_tick(app, &pressed);
    ui_rect(0, 0, BOTTOM_W, SCREEN_H, RGB(30, 26, 23));
    status_bar(app, BOTTOM_W, "Now Playing", true);
    // Card
    ui_round_rect(6, CARD_Y, BOTTOM_W - 12, 154, 6, CREAM);
    NowStyle ns = {.art = 92, .title_size = 0.58f, .text_size = 0.46f, .title = INK, .artist = RED, .album = INK, .meta = INK_SOFT,
                   .ph_bg = RGB(120, 96, 70), .ph_fg = CREAM, .counter = true};
    ui_now_info(app, 16, CARD_Y + 8, BOTTOM_W - 72, &ns);
    // Favorite heart
    bool fav = is_fav(app);
    if (fav) ui_sym_heart(290, CARD_Y + 20, 20, ORANGE);
    else ui_sym_heart_outline(290, CARD_Y + 20, 20, ORANGE, CREAM);
    // Tape progress: reels at both ends, striped tape between.
    float pos = app_position(app), dur = app_duration(app);
    float f = dur > 0 ? pos / dur : 0;
    if (f > 1) f = 1;
    if (app_playing(app)) reel_angle += app->dt * 3.2f;
    ui_round_rect(TAPE_X, TAPE_Y - 10, TAPE_W, 28, 14, BLACK);
    float inner = TAPE_W - 64;
    ui_rect(TAPE_X + 32, TAPE_Y - 4, inner, 16, RGB(50, 44, 40));
    float fill = inner * f;
    ui_rect(TAPE_X + 32, TAPE_Y - 4, fill, 6, ORANGE);
    ui_rect(TAPE_X + 32, TAPE_Y + 2, fill, 5, RED);
    ui_rect(TAPE_X + 32, TAPE_Y + 7, fill, 5, YELLOW);
    for (int i = 1; i < 12; i++) ui_rect(TAPE_X + 32 + inner * i / 12, TAPE_Y + 1, 1, 6, RGBA(255, 255, 255, 50));
    reel(TAPE_X + 16, TAPE_Y + 4, 11, reel_angle, 0);
    reel(TAPE_X + TAPE_W - 16, TAPE_Y + 4, 11, reel_angle, 0);
    char a[16], b[20];
    ui_fmt_time(pos, a, sizeof a);
    b[0] = '-';
    ui_fmt_time(dur - pos, b + 1, sizeof b - 1);
    ui_text(app, a, TAPE_X + 2, TAPE_Y + 20, 0.48f, INK, ALIGN_LEFT);
    ui_text(app, b, TAPE_X + TAPE_W - 2, TAPE_Y + 20, 0.48f, INK, ALIGN_RIGHT);
    // Controls
    ui_rect(0, CTRL_Y - 4, BOTTOM_W, SCREEN_H - CTRL_Y + 4, RGB(36, 31, 28));
    ui_sym_repeat(28, CTRL_Y + 23, 20, app->cfg.repeat ? ORANGE : RGB(130, 116, 100), app->cfg.repeat == REPEAT_ONE);
    key(72, CTRL_Y + 3, 56, 40, pressed.id == B_PREV, false);
    ui_sym_prev(100, CTRL_Y + 23, 16, CREAM);
    key(132, CTRL_Y + 3, 56, 40, pressed.id == B_PLAY, true);
    if (app_playing(app)) ui_sym_pause(160, CTRL_Y + 23, 18, ORANGE);
    else ui_sym_play(162, CTRL_Y + 23, 18, ORANGE);
    key(192, CTRL_Y + 3, 56, 40, pressed.id == B_NEXT, false);
    ui_sym_next(220, CTRL_Y + 23, 16, CREAM);
    ui_sym_shuffle(292, CTRL_Y + 23, 20, app->cfg.shuffle ? ORANGE : RGB(130, 116, 100));
}

static void touch(App *app, const TouchInput *t) {
    if (theme_scrub(app, t, TAPE_X + 32, TAPE_Y - 4, TAPE_W - 64, 16)) return;
    btn_touch(app, t, BUTTONS, sizeof BUTTONS / sizeof *BUTTONS, &pressed);
}

// Home: tiles are a horizontal row. Mixtapes: a three-column grid.
static bool buttons(App *app, u32 down, u32 held) {
    (void)held;
    if (app->show_now_playing) return false;
    MenuLevel *m = menu_top(app);
    if (app->menu.depth == 1) {
        if (down & (KEY_DLEFT | KEY_CPAD_LEFT | KEY_DUP | KEY_CPAD_UP)) { menu_move(app, -1); return true; }
        if (down & (KEY_DRIGHT | KEY_CPAD_RIGHT | KEY_DDOWN | KEY_CPAD_DOWN)) { menu_move(app, 1); return true; }
    }
    if (m->kind == MENU_PLAYLISTS) {
        if (down & (KEY_DLEFT | KEY_CPAD_LEFT)) { menu_move(app, -1); return true; }
        if (down & (KEY_DRIGHT | KEY_CPAD_RIGHT)) { menu_move(app, 1); return true; }
        if (down & (KEY_DUP | KEY_CPAD_UP)) { menu_move(app, -3); return true; }
        if (down & (KEY_DDOWN | KEY_CPAD_DOWN)) { menu_move(app, 3); return true; }
    }
    return false;
}

static void enter(App *app) {
    (void)app;
    tile_scroll = 0;
}

const Theme THEME_TAPEDECK = {
    .name = "Tape Deck",
    .description = "Cassettes, index cards and turning reels",
    .accent = RGB(240, 132, 36), .swatch = RGB(43, 37, 33),
    .album_cols = 4,
    .enter = enter,
    .draw_top = draw_top,
    .draw_bottom = draw_bottom,
    .touch = touch,
    .buttons = buttons,
};
