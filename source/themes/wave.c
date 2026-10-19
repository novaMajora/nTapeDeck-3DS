// Wave: a cross-shaped media bar over flowing waves. At the top level the categories (Settings,
// Music, Now Playing) run left-right and each one's items list below it; deeper levels open as
// glossy panels.
#include <math.h>
#include <stdio.h>

#include "common.h"

static const u32 WHITE = RGB(255, 255, 255), SOFT = RGB(200, 220, 245), DIM = RGBA(255, 255, 255, 140);

enum { CAT_SETTINGS, CAT_MUSIC, CAT_NOW, CAT_COUNT };
static int cat = CAT_MUSIC;
static float cat_anim = CAT_MUSIC, list_anim;
static int settings_cursor;
static Pressed pressed;

static void wave_line(App *app, float w, float y0, float amp, float phase, float speed, u32 c) {
    const int segs = 40;
    float px = 0, py = y0 + sinf(phase + app->time * speed) * amp;
    for (int i = 1; i <= segs; i++) {
        float x = w * i / segs;
        float y = y0 + sinf(x * 0.012f + app->time * speed + phase) * amp + sinf(x * 0.029f - app->time * speed * 0.7f) * amp * 0.4f;
        u32 band = c & 0x40FFFFFF, clear = c & 0x00FFFFFF;
        C2D_DrawTriangle(px, py, band, x, y, band, px, py + 30, clear, 0.5f);
        C2D_DrawTriangle(x, y, band, x, y + 30, clear, px, py + 30, clear, 0.5f);
        ui_line(px, py, x, y, 1.2f, c);
        px = x;
        py = y;
    }
}

static void background(App *app, float w) {
    ui_vgrad(0, 0, w, SCREEN_H, RGB(8, 40, 104), RGB(4, 16, 46));
    wave_line(app, w, 140, 18, 0.0f, 0.55f, RGBA(170, 210, 255, 170));
    wave_line(app, w, 156, 14, 2.1f, 0.42f, RGBA(120, 180, 255, 130));
    wave_line(app, w, 170, 22, 4.0f, 0.3f, RGBA(220, 240, 255, 90));
    wave_line(app, w, 120, 10, 5.2f, 0.25f, RGBA(140, 200, 255, 60));
}

static void pill(float x, float y, float w, float h) {
    ui_round_rect(x - 1, y - 1, w + 2, h + 2, h / 2 + 1, RGBA(200, 230, 255, 230));
    ui_round_vgrad(x, y, w, h, h / 2, RGB(70, 140, 230), RGB(24, 74, 170));
    ui_round_rect(x + 3, y + 2, w - 6, h * 0.42f, h * 0.2f, RGBA(255, 255, 255, 50));
}

static void panel_header(App *app, MenuIcon icon, const char *title, float w, float right) {
    ui_vgrad(0, 0, w, 26, RGBA(10, 30, 80, 230), RGBA(10, 30, 80, 160));
    ui_rect(0, 26, w, 1, RGBA(160, 200, 255, 120));
    ui_icon(icon, 16, 13, 16, WHITE);
    ui_text_fit(app, title, 32, 4, 0.52f, WHITE, ALIGN_LEFT, w - 220);
    ui_clock(app, right, 6, 0.42f, WHITE);
}

// ------------------------------------------------------------------------------------- top

static void cat_items(App *app, int c, int *count) {
    (void)app;
    *count = c == CAT_SETTINGS ? SET_COUNT : c == CAT_MUSIC ? ROOT_SETTINGS - 1 : 1;
}

static void draw_bar(App *app) {
    background(app, TOP_W);
    ui_clock(app, TOP_W - 8, 6, 0.42f, WHITE);
    float k = app->dt * 9;
    if (k > 1) k = 1;
    cat_anim += (cat - cat_anim) * k;
    static const MenuIcon icons[CAT_COUNT] = {ICON_SETTINGS, ICON_SONG, ICON_PLAYING};
    static const char *names[CAT_COUNT] = {"Settings", "Music", "Now Playing"};
    const float ax = 132, gap = 86, iy = 70;
    for (int i = 0; i < CAT_COUNT; i++) {
        float x = ax + (i - cat_anim) * gap;
        bool sel = i == cat;
        ui_icon(icons[i], x, iy, sel ? 44 : 30, sel ? WHITE : DIM);
        if (sel) ui_text(app, names[i], x, iy + 26, 0.55f, WHITE, ALIGN_CENTER);
    }
    // Items of the selected category, under its icon.
    MenuLevel *root = &app->menu.levels[0];
    char buf[64], val[48];
    float lx = ax - 26, ly = 126;
    if (cat == CAT_MUSIC) {
        int sel = root->cursor - 1; // root items 1..6
        float kk = app->dt * 12;
        if (kk > 1) kk = 1;
        list_anim += (sel - list_anim) * kk;
        for (int i = 0; i < ROOT_SETTINGS - 1; i++) {
            float y = ly + (i - list_anim) * 26;
            if (y < 108 || y > SCREEN_H - 20) continue;
            bool s = i == sel;
            if (s) pill(lx - 6, y - 3, 250, 25);
            ui_icon(menu_icon(app, root, i + 1), lx + 10, y + 9, 16, s ? WHITE : SOFT);
            ui_text(app, menu_label(app, root, i + 1, buf, sizeof buf), lx + 28, y, 0.52f, s ? WHITE : SOFT, ALIGN_LEFT);
        }
    } else if (cat == CAT_SETTINGS) {
        MenuLevel fake = {.kind = MENU_SETTINGS, .count = SET_COUNT};
        for (int i = 0; i < SET_COUNT; i++) {
            float y = ly + (i - settings_cursor) * 26;
            if (y < 108 || y > SCREEN_H - 20) continue;
            bool s = i == settings_cursor;
            if (s) pill(lx - 6, y - 3, 250, 25);
            ui_icon(menu_icon(app, &fake, i), lx + 10, y + 9, 16, s ? WHITE : SOFT);
            ui_text(app, menu_label(app, &fake, i, buf, sizeof buf), lx + 28, y, 0.52f, s ? WHITE : SOFT, ALIGN_LEFT);
            ui_text(app, menu_value(app, &fake, i, val, sizeof val), lx + 236, y + 1, 0.46f, s ? WHITE : SOFT, ALIGN_RIGHT);
        }
    } else {
        int t = app_current_track(app);
        if (t >= 0) {
            ui_art(app, app->lib.tracks[t].album_id, lx, ly - 4, 60, RGB(40, 80, 150), WHITE);
            ui_text_fit(app, app->lib.tracks[t].title, lx + 70, ly, 0.55f, WHITE, ALIGN_LEFT, 220);
            ui_text_fit(app, app->lib.tracks[t].artist, lx + 70, ly + 22, 0.45f, SOFT, ALIGN_LEFT, 220);
        } else {
            ui_text(app, "Nothing playing yet", lx, ly, 0.5f, SOFT, ALIGN_LEFT);
        }
    }
}

static void highlight(float x, float y, float w, float h) { pill(x + 2, y + 1, w - 4, h - 2); }

static void draw_panel(App *app) {
    background(app, TOP_W);
    MenuLevel *m = menu_top(app);
    panel_header(app, menu_icon(app, &app->menu.levels[0], app->menu.levels[0].cursor), m->title, TOP_W, TOP_W - 8);
    if (m->kind == MENU_ALBUMS) {
        StripStyle st = {.art = 82, .gap = 16, .text_size = 0.4f, .caption = WHITE, .caption_sub = SOFT, .border = RGB(200, 230, 255), .ph_bg = RGB(40, 80, 150), .ph_fg = WHITE, .arrow = WHITE};
        ui_album_strip(app, m, 0, 52, TOP_W, 160, &st);
        return;
    }
    ListStyle ls = {
        .row_h = 35, .text_size = 0.52f, .detail_size = 0.42f, .pad = 12,
        .art = m->kind != MENU_SETTINGS && m->kind != MENU_THEMES, .icon = true, .detail = true, .value = true, .chevron = true, .divider = true,
        .text = WHITE, .text_sel = WHITE, .sub = SOFT, .sub_sel = WHITE, .val = SOFT, .val_sel = WHITE,
        .chev = SOFT, .chev_sel = WHITE, .div = RGBA(160, 200, 255, 60), .icon_c = SOFT, .icon_sel = WHITE,
        .ph_bg = RGB(40, 80, 150), .ph_fg = WHITE, .highlight = highlight,
    };
    ui_list(app, m, 10, 32, TOP_W - 20, SCREEN_H - 36, &ls);
    if (!m->count) ui_text(app, "Nothing here yet", TOP_W / 2, 110, 0.5f, SOFT, ALIGN_CENTER);
}

static void draw_now(App *app, float w, float y0, bool controls_only_display) {
    NowStyle ns = {.art = w > 330 ? 116 : 92, .title_size = 0.62f, .text_size = 0.47f, .title = WHITE, .artist = SOFT, .album = SOFT, .meta = SOFT,
                   .ph_bg = RGB(40, 80, 150), .ph_fg = WHITE, .art_border = RGBA(200, 230, 255, 200), .counter = true};
    float x = w > 330 ? 58 : 16;
    ui_now_info(app, x, y0, w - x - 16, &ns);
    BarStyle bs = {.h = 4, .track = RGBA(255, 255, 255, 70), .fill_top = WHITE, .fill_bottom = RGB(170, 210, 255), .knob = WHITE,
                   .knob_ring = RGBA(120, 190, 255, 200), .time = WHITE, .knob_r = 5, .time_size = 0.44f, .remaining = true};
    float by = y0 + ns.art + 16;
    ui_progress(app, x, by, w - 2 * x, &bs);
    (void)controls_only_display;
}

static void draw_controls(App *app, float w, float y, bool hot_feedback) {
    float cx = w / 2;
    u32 on = WHITE, off = RGBA(255, 255, 255, 110);
    ui_sym_repeat(cx - 100, y, 18, app->cfg.repeat ? on : off, app->cfg.repeat == REPEAT_ONE);
    ui_sym_prev(cx - 50, y, 18, hot_feedback && pressed.id == B_PREV ? SOFT : on);
    if (hot_feedback && pressed.id == B_PLAY) ui_circle(cx, y, 20, RGBA(255, 255, 255, 50));
    if (app_playing(app)) ui_sym_pause(cx, y, 24, on);
    else ui_sym_play(cx + 2, y, 24, on);
    ui_sym_next(cx + 50, y, 18, hot_feedback && pressed.id == B_NEXT ? SOFT : on);
    ui_sym_shuffle(cx + 100, y, 18, app->cfg.shuffle ? on : off);
}

static void draw_top(App *app) {
    if (app->show_now_playing) {
        background(app, TOP_W);
        panel_header(app, ICON_SONG, "Now Playing", TOP_W, TOP_W - 8);
        draw_now(app, TOP_W, 44, true);
        draw_controls(app, TOP_W, 214, false);
    } else if (app->menu.depth == 1) {
        draw_bar(app);
    } else {
        draw_panel(app);
    }
    toast(app, RGBA(0, 20, 60, 200), WHITE);
}

// ------------------------------------------------------------------------------------- bottom

#define CTRL_Y 206

static const Btn BUTTONS[] = {
    {B_REPEAT, 30, CTRL_Y - 20, 40, 40}, {B_PREV, 85, CTRL_Y - 20, 40, 40}, {B_PLAY, 135, CTRL_Y - 22, 50, 44},
    {B_NEXT, 195, CTRL_Y - 20, 40, 40}, {B_SHUFFLE, 250, CTRL_Y - 20, 40, 40}, {B_NOWTOP, 10, 34, 100, 100},
};

static void draw_bottom(App *app) {
    pressed_tick(app, &pressed);
    background(app, BOTTOM_W);
    panel_header(app, ICON_SONG, "Now Playing", BOTTOM_W, THEME_BTN_X - 4);
    draw_now(app, BOTTOM_W, 38, false);
    draw_controls(app, BOTTOM_W, CTRL_Y, true);
}

static void touch(App *app, const TouchInput *t) {
    if (theme_scrub(app, t, 16, 38 + 92 + 16, BOTTOM_W - 32, 4)) return;
    btn_touch(app, t, BUTTONS, sizeof BUTTONS / sizeof *BUTTONS, &pressed);
}

static bool buttons(App *app, u32 down, u32 held) {
    (void)held;
    if (app->show_now_playing || app->menu.depth > 1) return false;
    MenuLevel *root = &app->menu.levels[0];
    if (down & (KEY_DLEFT | KEY_CPAD_LEFT) && cat > 0) cat--;
    if (down & (KEY_DRIGHT | KEY_CPAD_RIGHT) && cat < CAT_COUNT - 1) cat++;
    int dir = (down & (KEY_DUP | KEY_CPAD_UP)) ? -1 : (down & (KEY_DDOWN | KEY_CPAD_DOWN)) ? 1 : 0;
    if (cat == CAT_MUSIC && dir) {
        int c = root->cursor + dir;
        if (c >= 1 && c < ROOT_SETTINGS) root->cursor = c;
    }
    if (cat == CAT_SETTINGS && dir) {
        int c = settings_cursor + dir;
        if (c >= 0 && c < SET_COUNT) settings_cursor = c;
    }
    if (down & KEY_A) {
        if (cat == CAT_MUSIC) menu_activate(app);
        else if (cat == CAT_NOW) app->show_now_playing = true;
        else {
            root->cursor = ROOT_SETTINGS;
            menu_push(app, MENU_SETTINGS, 0);
            menu_top(app)->cursor = settings_cursor;
            root->cursor = ROOT_SONGS;
        }
    }
    int n;
    cat_items(app, cat, &n);
    (void)n;
    // X/Y/START/L/R/SELECT still work.
    return (down & (KEY_DLEFT | KEY_DRIGHT | KEY_CPAD_LEFT | KEY_CPAD_RIGHT | KEY_DUP | KEY_DDOWN | KEY_CPAD_UP | KEY_CPAD_DOWN | KEY_A | KEY_B)) != 0;
}

static void enter(App *app) {
    MenuLevel *root = &app->menu.levels[0];
    if (root->cursor < 1 || root->cursor >= ROOT_SETTINGS) root->cursor = ROOT_SONGS;
    list_anim = root->cursor - 1;
    cat = CAT_MUSIC;
    cat_anim = cat;
}

const Theme THEME_WAVE = {
    .name = "Wave",
    .description = "Cross media bar over flowing waves",
    .accent = RGB(160, 210, 255), .swatch = RGB(16, 60, 140),
    .album_cols = 1,
    .enter = enter,
    .draw_top = draw_top,
    .draw_bottom = draw_bottom,
    .touch = touch,
    .buttons = buttons,
};
