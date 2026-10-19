// Helpers shared by the theme files: transport button hit areas and actions.
#pragma once

#include "../app.h"
#include "../ui.h"
#include "../widgets.h"
#include "theme.h"

typedef enum { B_NONE, B_PREV, B_PLAY, B_NEXT, B_STOP, B_SHUFFLE, B_REPEAT, B_FAV, B_NOWTOP, B_VOLDOWN, B_VOLUP, B_BACK, B_SELECT } BtnId;

typedef struct {
    BtnId id;
    float x, y, w, h;
} Btn;

typedef struct {
    BtnId id;       // the button under the finger (drawn pressed)
    float ttl;
} Pressed;

static inline BtnId btn_at(const Btn *b, int n, int x, int y) {
    for (int i = 0; i < n; i++)
        if (ui_hit(x, y, b[i].x, b[i].y, b[i].w, b[i].h)) return b[i].id;
    return B_NONE;
}

static inline void btn_run(App *app, BtnId id) {
    switch (id) {
    case B_PREV: app_prev(app); break;
    case B_PLAY: app_play_pause(app); break;
    case B_NEXT: app_next(app); break;
    case B_STOP: if (app->player) player_stop(app->player); break;
    case B_SHUFFLE: app_set_shuffle(app, !app->cfg.shuffle); break;
    case B_REPEAT: app_cycle_repeat(app); break;
    case B_FAV: app_toggle_favorite(app); break;
    case B_NOWTOP: app->show_now_playing = !app->show_now_playing; break;
    case B_VOLDOWN: app_set_volume(app, app->cfg.volume - 0.1f); break;
    case B_VOLUP: app_set_volume(app, app->cfg.volume + 0.1f); break;
    case B_BACK: if (app->show_now_playing) app->show_now_playing = false; else menu_pop(app); break;
    case B_SELECT: if (app->show_now_playing) app->show_now_playing = false; else menu_activate(app); break;
    default: break;
    }
}

// Press feedback and tap-to-run for a set of buttons. Returns true if the touch began on one.
static inline bool btn_touch(App *app, const TouchInput *t, const Btn *b, int n, Pressed *p) {
    BtnId id = btn_at(b, n, t->start_x, t->start_y);
    if (id == B_NONE) return false;
    if (t->down || t->held) { p->id = id; p->ttl = 10; }
    if (t->up) {
        p->ttl = 0.12f;
        if (theme_tap(t, 0, 0, BOTTOM_W, SCREEN_H) && btn_at(b, n, t->x, t->y) == id) btn_run(app, id);
    }
    return true;
}

static inline void pressed_tick(App *app, Pressed *p) {
    if (p->ttl > 0) p->ttl -= app->dt;
    else p->id = B_NONE;
}

static inline bool is_fav(App *app) {
    int t = app_current_track(app);
    return t >= 0 && library_is_favorite(&app->lib, t);
}

// A small status toast near the bottom of the top screen.
static inline void toast(App *app, u32 bg, u32 fg) {
    if (app->status_ttl <= 0) return;
    ui_round_rect(24, SCREEN_H - 32, TOP_W - 48, 24, 8, bg);
    ui_text_fit(app, app->status, TOP_W / 2, SCREEN_H - 28, 0.42f, fg, ALIGN_CENTER, TOP_W - 64);
}
