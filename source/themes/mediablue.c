// Media Blue: an early-2000s desktop media player in glossy blue, with a pale navigation pane
// and yellow highlights.
#include <math.h>
#include <stdio.h>

#include "common.h"

static const u32 TITLE_HI = RGB(76, 140, 240), TITLE_LO = RGB(18, 70, 190), NAV_HI = RGB(214, 228, 250), NAV_LO = RGB(168, 196, 238);
static const u32 SEL_HI = RGB(255, 240, 170), SEL_LO = RGB(248, 196, 70), SEL_EDGE = RGB(214, 150, 20);
static const u32 INK = RGB(14, 30, 70), INK_SOFT = RGB(70, 90, 130), WHITE = RGB(255, 255, 255), PANE = RGB(250, 252, 255);
static const u32 GREEN_HI = RGB(130, 226, 110), GREEN_LO = RGB(40, 150, 40);

#define BAR_H 26
#define NAV_W 112

static Pressed pressed;

static void title_bar(App *app, float w, float right) {
    ui_vgrad(0, 0, w, BAR_H, TITLE_HI, TITLE_LO);
    ui_rect(0, 1, w, 1, RGBA(255, 255, 255, 110));
    ui_circle(15, 13, 9, RGB(240, 248, 255));
    ui_icon(ICON_SONG, 15, 13, 12, TITLE_LO);
    ui_text(app, APP_SHORT, 30, 4, 0.54f, WHITE, ALIGN_LEFT);
    ui_clock(app, right, 6, 0.42f, WHITE);
}

static void sel_bar(float x, float y, float w, float h) {
    ui_round_rect(x, y, w, h, 4, SEL_EDGE);
    ui_round_vgrad(x + 1, y + 1, w - 2, h - 2, 3, SEL_HI, SEL_LO);
}

static void nav(App *app) {
    MenuLevel *root = &app->menu.levels[0];
    ui_vgrad(0, BAR_H, NAV_W, SCREEN_H - BAR_H, NAV_HI, NAV_LO);
    ui_rect(NAV_W - 1, BAR_H, 1, SCREEN_H - BAR_H, RGB(120, 150, 200));
    char buf[64];
    for (int i = 0; i < root->count; i++) {
        float y = BAR_H + 4 + i * 26;
        bool sel = i == root->cursor;
        if (sel) sel_bar(4, y, NAV_W - 8, 24);
        u32 ic = i == ROOT_NOW_PLAYING ? RGB(40, 160, 60) : TITLE_LO;
        ui_icon(menu_icon(app, root, i), 18, y + 12, 15, ic);
        ui_text_fit(app, menu_label(app, root, i, buf, sizeof buf), 32, y + 4, 0.44f, INK, ALIGN_LEFT, NAV_W - 36);
    }
}

static void orb(float cx, float cy, float r) {
    ui_circle(cx, cy, r + 6, RGBA(255, 255, 255, 40));
    ui_circle(cx, cy, r + 2, RGB(230, 240, 255));
    ui_circle(cx, cy, r, RGB(30, 90, 210));
    ui_circle(cx, cy + r * 0.15f, r * 0.85f, RGB(50, 120, 235));
    ui_circle(cx, cy - r * 0.35f, r * 0.55f, RGBA(255, 255, 255, 60));
    ui_sym_play(cx + r * 0.1f, cy, r * 0.95f, WHITE);
}

static void highlight(float x, float y, float w, float h) {
    ui_rect(x, y, w, h, RGB(206, 226, 255));
    ui_frame(x, y, w, h, 1, RGB(110, 160, 230));
}

static void draw_home(App *app) {
    float x = NAV_W, w = TOP_W - NAV_W;
    ui_vgrad(x, BAR_H, w, SCREEN_H - BAR_H, RGB(60, 120, 225), RGB(20, 64, 170));
    for (int i = 0; i < 6; i++) ui_circle(x + w * 0.8f, 40 + i * 6, 120 - i * 14, RGBA(255, 255, 255, 6));
    orb(x + w / 2, 100, 40);
    ui_text(app, APP_SHORT, x + w / 2, 152, 0.95f, WHITE, ALIGN_CENTER);
    ui_text(app, "Media Player", x + w / 2, 190, 0.52f, RGB(214, 230, 255), ALIGN_CENTER);
}

static void draw_library(App *app) {
    MenuLevel *m = menu_top(app);
    float x = NAV_W, w = TOP_W - NAV_W;
    ui_vgrad(x, BAR_H, w, SCREEN_H - BAR_H, RGB(60, 120, 225), RGB(20, 64, 170));
    ui_vgrad(x, BAR_H, w, 22, RGB(40, 90, 200), RGB(26, 66, 170));
    ui_sym_play(x + 12, BAR_H + 11, 8, WHITE);
    ui_text_fit(app, m->title, x + 22, BAR_H + 3, 0.48f, WHITE, ALIGN_LEFT, w - 30);
    float py = BAR_H + 26, ph = SCREEN_H - py - 4;
    ui_rect(x + 4, py, w - 8, ph, PANE);
    if (m->kind == MENU_ALBUMS) {
        StripStyle st = {.art = 58, .gap = 8, .text_size = 0.34f, .caption = INK, .caption_sub = INK_SOFT, .border = SEL_LO, .ph_bg = RGB(120, 150, 210), .ph_fg = WHITE, .arrow = INK_SOFT};
        ui_album_grid(app, m, x + 6, py + 2, w - 12, ph - 4, 4, &st);
        return;
    }
    // Column header
    ui_vgrad(x + 4, py, w - 8, 18, RGB(250, 252, 255), RGB(220, 230, 245));
    ui_rect(x + 4, py + 17, w - 8, 1, RGB(180, 196, 220));
    ui_text(app, "Name", x + 14, py + 1, 0.4f, INK, ALIGN_LEFT);
    ListStyle ls = {
        .row_h = 23, .text_size = 0.46f, .detail_size = 0.38f, .pad = 8,
        .icon = m->kind == MENU_SETTINGS || m->kind == MENU_PLAYLISTS, .value = true, .chevron = true, .divider = true,
        .text = INK, .text_sel = INK, .val = INK_SOFT, .val_sel = INK, .chev = INK_SOFT, .chev_sel = INK, .div = RGB(230, 236, 246),
        .icon_c = TITLE_LO, .icon_sel = TITLE_LO, .highlight = highlight,
    };
    ui_list(app, m, x + 4, py + 19, w - 8, ph - 20, &ls);
    if (!m->count) ui_text(app, "Nothing here yet", x + w / 2, py + 60, 0.48f, INK_SOFT, ALIGN_CENTER);
}

static void now_info(App *app, float x, float y, float w, float art) {
    NowStyle ns = {.art = art, .title_size = 0.6f, .text_size = 0.48f, .title = INK, .artist = INK, .album = INK_SOFT, .meta = INK_SOFT,
                   .ph_bg = RGB(120, 150, 210), .ph_fg = WHITE, .art_border = RGB(140, 170, 220)};
    ui_now_info(app, x, y, w, &ns);
}

static void draw_top(App *app) {
    ui_rect(0, 0, TOP_W, SCREEN_H, NAV_LO);
    title_bar(app, TOP_W, TOP_W - 6);
    nav(app);
    if (app->show_now_playing) {
        float x = NAV_W, w = TOP_W - NAV_W;
        ui_vgrad(x, BAR_H, w, SCREEN_H - BAR_H, RGB(214, 228, 250), RGB(170, 196, 240));
        now_info(app, x + 12, BAR_H + 16, w - 24, 100);
        BarStyle bs = {.h = 7, .track = RGB(200, 210, 226), .fill_top = GREEN_HI, .fill_bottom = GREEN_LO, .knob = RGB(240, 244, 250), .knob_ring = RGB(120, 140, 170),
                       .time = INK, .knob_r = 7, .time_size = 0.44f, .remaining = true};
        ui_progress(app, x + 14, 170, w - 28, &bs);
    } else if (app->menu.depth == 1) {
        draw_home(app);
    } else {
        draw_library(app);
    }
    toast(app, RGBA(255, 250, 220, 240), INK);
}

// ------------------------------------------------------------------------------------- bottom

#define CTRL_Y 202

static const Btn BUTTONS[] = {
    {B_STOP, 18, CTRL_Y - 18, 36, 36}, {B_PREV, 64, CTRL_Y - 20, 40, 40}, {B_PLAY, 110, CTRL_Y - 30, 60, 60}, {B_NEXT, 176, CTRL_Y - 20, 40, 40},
    {B_VOLDOWN, 226, CTRL_Y - 16, 36, 32}, {B_VOLUP, 262, CTRL_Y - 16, 52, 32}, {B_NOWTOP, 16, 40, 90, 90},
};

static void glossy(float cx, float cy, float r, bool hot) {
    ui_circle(cx, cy, r + 2, RGB(16, 50, 140));
    ui_circle(cx, cy, r, hot ? RGB(90, 160, 250) : RGB(50, 120, 230));
    ui_circle(cx, cy + r * 0.25f, r * 0.75f, hot ? RGB(70, 140, 240) : RGB(30, 96, 210));
    ui_circle(cx, cy - r * 0.4f, r * 0.55f, RGBA(255, 255, 255, 70));
}

static void draw_bottom(App *app) {
    pressed_tick(app, &pressed);
    ui_rect(0, 0, BOTTOM_W, SCREEN_H, NAV_LO);
    title_bar(app, BOTTOM_W, THEME_BTN_X - 4);
    ui_round_vgrad(8, BAR_H + 6, BOTTOM_W - 16, 140, 8, RGB(228, 238, 252), RGB(180, 204, 242));
    ui_frame(8, BAR_H + 6, BOTTOM_W - 16, 140, 1, RGB(150, 180, 225));
    now_info(app, 18, BAR_H + 16, BOTTOM_W - 36, 84);
    BarStyle bs = {.h = 7, .track = RGB(200, 210, 226), .fill_top = GREEN_HI, .fill_bottom = GREEN_LO, .knob = RGB(240, 244, 250), .knob_ring = RGB(120, 140, 170),
                   .time = INK, .knob_r = 7, .time_size = 0.44f, .remaining = true};
    ui_progress(app, 22, BAR_H + 112, BOTTOM_W - 44, &bs);
    // Transport bar
    ui_vgrad(0, CTRL_Y - 32, BOTTOM_W, SCREEN_H - CTRL_Y + 32, RGB(70, 130, 230), RGB(20, 60, 170));
    ui_rect(0, CTRL_Y - 32, BOTTOM_W, 1, RGBA(255, 255, 255, 120));
    glossy(36, CTRL_Y, 15, pressed.id == B_STOP);
    ui_sym_stop(36, CTRL_Y, 11, WHITE);
    glossy(84, CTRL_Y, 18, pressed.id == B_PREV);
    ui_sym_prev(84, CTRL_Y, 13, WHITE);
    glossy(140, CTRL_Y, 26, pressed.id == B_PLAY);
    if (app_playing(app)) ui_sym_pause(140, CTRL_Y, 20, WHITE);
    else ui_sym_play(142, CTRL_Y, 20, WHITE);
    glossy(196, CTRL_Y, 18, pressed.id == B_NEXT);
    ui_sym_next(196, CTRL_Y, 13, WHITE);
    ui_sym_volume(240, CTRL_Y, 16, WHITE);
    ui_round_rect(256, CTRL_Y - 3, 52, 6, 3, RGB(30, 60, 120));
    ui_round_vgrad(256, CTRL_Y - 3, 52 * app->cfg.volume + 4, 6, 3, GREEN_HI, GREEN_LO);
    ui_circle(256 + 52 * app->cfg.volume, CTRL_Y, 6, RGB(240, 244, 250));
}

static void touch(App *app, const TouchInput *t) {
    if (theme_scrub(app, t, 22, BAR_H + 112, BOTTOM_W - 44, 7)) return;
    // Volume slider: drag anywhere along it.
    if (ui_hit(t->start_x, t->start_y, 252, CTRL_Y - 14, 64, 28)) {
        float v = (t->x - 256) / 52.0f;
        app_set_volume(app, v);
        return;
    }
    btn_touch(app, t, BUTTONS, sizeof BUTTONS / sizeof *BUTTONS, &pressed);
}

const Theme THEME_MEDIABLUE = {
    .name = "Media Blue",
    .description = "Glossy blue desktop media player",
    .accent = RGB(248, 196, 70), .swatch = RGB(30, 84, 200),
    .album_cols = 4,
    .draw_top = draw_top,
    .draw_bottom = draw_bottom,
    .touch = touch,
};
