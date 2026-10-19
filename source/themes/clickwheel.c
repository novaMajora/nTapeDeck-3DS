// Click Wheel: a classic pocket player. The top screen is its LCD (split menus, Now Playing);
// the bottom screen is the wheel: drag around the ring to scroll, tap the center to select.
#include <math.h>
#include <stdio.h>

#include "common.h"

static const u32 INK = RGB(0, 0, 0), GRAY = RGB(110, 110, 116), LINE = RGB(214, 214, 218), WHITE = RGB(255, 255, 255);
static const u32 BLUE_HI = RGB(70, 150, 245), BLUE_LO = RGB(20, 100, 220);

#define BAR_H 22
#define HDR_H 24
#define LIST_W 252

#define WHEEL_X 160
#define WHEEL_Y 128
#define WHEEL_R 100
#define CENTER_R 36

static float wheel_acc;
static bool wheel_dragging;
static int pressed_region;
static float pressed_ttl;

static void status_bar(App *app, float w, float right) {
    ui_rect(0, 0, w, BAR_H, INK);
    ui_signal(6, 5, 12, WHITE, RGB(90, 90, 90));
    float lw = ui_text_width(app, APP_SHORT, 0.42f) + 24;
    ui_round_rect(30, 3, lw, 16, 4, RGB(236, 236, 238));
    ui_text(app, APP_SHORT, 30 + lw / 2, 3, 0.42f, INK, ALIGN_CENTER);
    ui_clock(app, right, 4, 0.42f, WHITE);
}

static void header(App *app, const char *title, float w, bool play_icon) {
    ui_vgrad(0, BAR_H, w, HDR_H, RGB(250, 250, 252), RGB(206, 208, 214));
    ui_rect(0, BAR_H + HDR_H - 1, w, 1, RGB(150, 152, 160));
    ui_text_fit(app, title, 12, BAR_H + 3, 0.54f, INK, ALIGN_LEFT, w - 60);
    if (play_icon && app_playing(app)) ui_sym_play(w - 18, BAR_H + HDR_H / 2, 13, BLUE_LO);
}

// A generic pocket player: rounded body, screen with a note, wheel.
static void device(float cx, float cy, float s, u32 body_hi, u32 body_lo) {
    float w = 70 * s, h = 112 * s;
    ui_round_vgrad(cx - w / 2, cy - h / 2, w, h, 10 * s, body_hi, body_lo);
    ui_round_rect(cx - w / 2 + 8 * s, cy - h / 2 + 8 * s, w - 16 * s, 42 * s, 3 * s, RGB(236, 238, 242));
    ui_icon(ICON_SONG, cx, cy - h / 2 + 29 * s, 26 * s, BLUE_LO);
    ui_circle(cx, cy + 26 * s, 24 * s, RGB(240, 240, 242));
    ui_circle(cx, cy + 26 * s, 9 * s, body_lo);
}

static void splash(App *app) {
    ui_vgrad(0, 0, TOP_W, SCREEN_H, RGB(246, 246, 248), RGB(196, 198, 204));
    status_bar(app, TOP_W, TOP_W - 6);
    device(132, 128, 1.15f, RGB(130, 132, 140), RGB(80, 82, 90));
    ui_text(app, APP_SHORT, 200, 84, 0.95f, INK, ALIGN_LEFT);
    ui_text(app, "MP3 Player", 202, 120, 0.65f, RGB(70, 70, 76), ALIGN_LEFT);
    float px = 270;
    ui_text(app, "Press", px, 204, 0.48f, GRAY, ALIGN_RIGHT);
    ui_circle(px + 14, 212, 9, RGB(70, 70, 76));
    ui_text(app, "A", px + 14, 205, 0.45f, WHITE, ALIGN_CENTER);
    ui_text(app, "to Start", px + 28, 204, 0.48f, GRAY, ALIGN_LEFT);
}

static void highlight(float x, float y, float w, float h) { ui_vgrad(x, y, w, h, BLUE_HI, BLUE_LO); }

static void draw_menu(App *app) {
    MenuLevel *m = menu_top(app);
    ui_rect(0, 0, TOP_W, SCREEN_H, WHITE);
    status_bar(app, TOP_W, TOP_W - 6);
    float y0 = BAR_H;
    bool split = m->kind != MENU_ALBUMS;
    if (m->kind != MENU_ROOT) {
        header(app, m->title, split ? LIST_W : TOP_W, false);
        y0 += HDR_H;
    }
    if (m->kind == MENU_ALBUMS) {
        StripStyle st = {.art = 74, .gap = 10, .text_size = 0.38f, .caption = INK, .caption_sub = GRAY, .border = BLUE_LO, .ph_bg = RGB(170, 172, 180), .ph_fg = WHITE, .arrow = GRAY};
        ui_album_grid(app, m, 4, y0 + 4, TOP_W - 8, SCREEN_H - y0 - 4, 3, &st);
        return;
    }
    ListStyle ls = {
        .row_h = 28, .text_size = 0.56f, .detail_size = 0.44f, .pad = 10,
        .value = m->kind == MENU_SETTINGS || m->kind == MENU_THEMES, .chevron = true, .divider = true,
        .text = INK, .text_sel = WHITE, .val = GRAY, .val_sel = WHITE, .chev = GRAY, .chev_sel = WHITE, .div = LINE,
        .highlight = highlight,
    };
    ui_list(app, m, 0, y0, LIST_W, SCREEN_H - y0, &ls);
    int visible = (int)((SCREEN_H - y0) / 28);
    if (m->count > visible) { // scroll bar
        float h = (SCREEN_H - y0) * visible / (float)m->count;
        float y = y0 + (SCREEN_H - y0 - h) * (m->scroll / (m->count - visible));
        ui_rect(LIST_W - 5, y0, 5, SCREEN_H - y0, RGB(236, 236, 238));
        ui_round_rect(LIST_W - 5, y, 5, h, 2, RGB(150, 152, 160));
    }
    // Right half: art for the highlighted item, or the player itself.
    ui_vgrad(LIST_W, BAR_H, TOP_W - LIST_W, SCREEN_H - BAR_H, RGB(232, 233, 236), RGB(196, 198, 204));
    ui_rect(LIST_W, BAR_H, 1, SCREEN_H - BAR_H, RGB(160, 162, 168));
    int album = menu_art(app, m, m->cursor);
    float cx = LIST_W + (TOP_W - LIST_W) / 2.0f;
    if (album >= 0) {
        ui_art(app, album, cx - 56, BAR_H + 30, 112, RGB(170, 172, 180), WHITE);
        ui_vgrad(cx - 56, BAR_H + 144, 112, 30, RGBA(0, 0, 0, 40), RGBA(0, 0, 0, 0));
    } else {
        device(cx, BAR_H + 110, 1.2f, RGB(150, 152, 160), RGB(108, 110, 118));
    }
}

static void draw_now(App *app) {
    ui_rect(0, 0, TOP_W, SCREEN_H, WHITE);
    status_bar(app, TOP_W, TOP_W - 6);
    header(app, "Now Playing", TOP_W, true);
    NowStyle ns = {.art = 128, .title_size = 0.64f, .text_size = 0.52f, .title = INK, .artist = GRAY, .album = GRAY, .meta = GRAY,
                   .ph_bg = RGB(170, 172, 180), .ph_fg = WHITE, .counter = true};
    ui_now_info(app, 22, BAR_H + HDR_H + 12, TOP_W - 44, &ns);
    BarStyle bs = {.h = 9, .track = RGB(214, 214, 218), .fill_top = BLUE_HI, .fill_bottom = BLUE_LO, .time = INK, .time_size = 0.52f, .remaining = true};
    ui_progress(app, 22, 196, TOP_W - 44, &bs);
}

static void draw_top(App *app) {
    if (app->show_now_playing) draw_now(app);
    else draw_menu(app);
    toast(app, RGBA(0, 0, 0, 200), WHITE);
}

// ------------------------------------------------------------------------------------- bottom

static void draw_bottom(App *app) {
    if (pressed_ttl > 0) pressed_ttl -= app->dt; else pressed_region = 0;
    ui_vgrad(0, 0, BOTTOM_W, SCREEN_H, RGB(222, 223, 226), RGB(170, 172, 178));
    for (int y = 0; y < SCREEN_H; y += 3) ui_rect(0, y, BOTTOM_W, 1, RGBA(255, 255, 255, 30));
    ui_circle(WHEEL_X, WHEEL_Y + 3, WHEEL_R + 2, RGBA(0, 0, 0, 50));
    ui_circle(WHEEL_X, WHEEL_Y, WHEEL_R, RGB(52, 52, 56));
    ui_circle(WHEEL_X, WHEEL_Y - 2, WHEEL_R - 4, RGB(62, 62, 66));
    u32 lab = RGB(236, 236, 240), hot = BLUE_HI;
    ui_text(app, "MENU", WHEEL_X, WHEEL_Y - WHEEL_R + 14, 0.52f, pressed_region == 1 ? hot : lab, ALIGN_CENTER);
    ui_sym_next(WHEEL_X + WHEEL_R - 30, WHEEL_Y, 15, pressed_region == 2 ? hot : lab);
    ui_sym_prev(WHEEL_X - WHEEL_R + 30, WHEEL_Y, 15, pressed_region == 4 ? hot : lab);
    ui_sym_play(WHEEL_X - 9, WHEEL_Y + WHEEL_R - 26, 13, pressed_region == 3 ? hot : lab);
    ui_sym_pause(WHEEL_X + 10, WHEEL_Y + WHEEL_R - 26, 13, pressed_region == 3 ? hot : lab);
    ui_circle(WHEEL_X, WHEEL_Y, CENTER_R + 2, RGB(36, 36, 40));
    ui_circle(WHEEL_X, WHEEL_Y, CENTER_R, pressed_region == 5 ? RGB(190, 192, 198) : RGB(214, 215, 220));
    ui_circle(WHEEL_X, WHEEL_Y - 5, CENTER_R - 9, RGBA(255, 255, 255, 70));
}

static int region(int x, int y) {
    float dx = x - WHEEL_X, dy = y - WHEEL_Y, d = sqrtf(dx * dx + dy * dy);
    if (d > WHEEL_R + 6) return 0;
    if (d < CENTER_R) return 5;
    if (fabsf(dy) > fabsf(dx)) return dy < 0 ? 1 : 3;
    return dx > 0 ? 2 : 4;
}

static void touch(App *app, const TouchInput *t) {
    if (t->down) { wheel_acc = 0; wheel_dragging = false; }
    float dx = t->x - WHEEL_X, dy = t->y - WHEEL_Y, r = sqrtf(dx * dx + dy * dy);
    if (t->held && !t->down && r > CENTER_R && r < WHEEL_R + 10) {
        float a0 = atan2f(t->prev_y - WHEEL_Y, t->prev_x - WHEEL_X), a1 = atan2f(dy, dx), d = a1 - a0;
        if (d > (float)M_PI) d -= 2 * (float)M_PI;
        if (d < -(float)M_PI) d += 2 * (float)M_PI;
        wheel_acc += d;
        if (fabsf(wheel_acc) > 0.08f) wheel_dragging = true;
        const float step = 0.38f; // about 22 degrees per item
        int cols = menu_top(app)->kind == MENU_ALBUMS ? 1 : 1;
        while (wheel_acc > step) { wheel_acc -= step; if (app->show_now_playing) app_set_volume(app, app->cfg.volume + 0.05f); else menu_move(app, cols); }
        while (wheel_acc < -step) { wheel_acc += step; if (app->show_now_playing) app_set_volume(app, app->cfg.volume - 0.05f); else menu_move(app, -cols); }
    }
    if (t->down) { pressed_region = region(t->x, t->y); pressed_ttl = 10; }
    if (t->up && !wheel_dragging) {
        int reg = region(t->start_x, t->start_y);
        pressed_region = reg;
        pressed_ttl = 0.15f;
        switch (reg) {
        case 1: btn_run(app, B_BACK); break;
        case 2: app_next(app); break;
        case 3: app_play_pause(app); break;
        case 4: app_prev(app); break;
        case 5: btn_run(app, B_SELECT); break;
        }
    } else if (t->up) {
        pressed_ttl = 0;
    }
}

const Theme THEME_CLICKWHEEL = {
    .name = "Click Wheel",
    .description = "Pocket player with a touch click wheel",
    .accent = RGB(70, 150, 245), .swatch = RGB(225, 226, 230),
    .album_cols = 3,
    .draw_splash = splash,
    .draw_top = draw_top,
    .draw_bottom = draw_bottom,
    .touch = touch,
};
