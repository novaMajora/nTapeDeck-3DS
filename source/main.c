#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app.h"
#include "themes/theme.h"
#include "ui.h"
#include "widgets.h"

static C3D_RenderTarget *top_target, *bottom_target;
static C3D_Tex logo_tex;
static Tex3DS_SubTexture logo_sub;

// ---------------------------------------------------------------------------------------------
// Config

void config_load(Config *cfg) {
    // Tape Deck is the default; the last theme used is restored on the next boot.
    *cfg = (Config){.theme = 0, .shuffle = false, .repeat = REPEAT_ALL, .volume = 1.0f};
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        char key[32];
        float v;
        if (sscanf(line, "%31[^=]=%f", key, &v) != 2) continue;
        if (!strcmp(key, "theme")) cfg->theme = (int)v;
        else if (!strcmp(key, "shuffle")) cfg->shuffle = v != 0;
        else if (!strcmp(key, "repeat")) cfg->repeat = (RepeatMode)(int)v;
        else if (!strcmp(key, "volume")) cfg->volume = v;
    }
    fclose(f);
    if (cfg->theme < 0 || cfg->theme >= theme_count()) cfg->theme = 0;
    if (cfg->repeat < 0 || cfg->repeat >= REPEAT_COUNT) cfg->repeat = REPEAT_ALL;
    if (!(cfg->volume >= 0 && cfg->volume <= 1)) cfg->volume = 1;
}

void config_save(const Config *cfg) {
    mkdir("sdmc:/3ds", 0777);
    mkdir(CONFIG_DIR, 0777);
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) return;
    fprintf(f, "theme=%d\nshuffle=%d\nrepeat=%d\nvolume=%.2f\n", cfg->theme, cfg->shuffle, cfg->repeat, cfg->volume);
    fclose(f);
}

// ---------------------------------------------------------------------------------------------
// Queue and transport

void app_status(App *app, const char *msg) {
    snprintf(app->status, sizeof app->status, "%s", msg);
    app->status_ttl = 3.0f;
}

int app_current_track(const App *app) {
    const Queue *q = &app->queue;
    if (q->pos < 0 || q->pos >= q->count) return -1;
    return q->items[q->pos];
}

float app_position(App *app) { return app->player ? player_position(app->player) : 0; }
float app_duration(App *app) { return app->player ? player_duration(app->player) : 0; }
bool app_playing(App *app) { return app->player && player_state(app->player) == PLAYER_PLAYING; }

static void play_current(App *app) {
    int t = app_current_track(app);
    if (t < 0 || !app->player) return;
    player_play(app->player, app->lib.tracks[t].path);
}

static void shuffle_ints(int *a, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

// Replaces the queue with `tracks`; `start` is the index to play first (-1: random/first).
void app_play_list(App *app, const int *tracks, int count, int start) {
    Queue *q = &app->queue;
    free(q->items);
    free(q->original);
    q->items = malloc((count ? count : 1) * sizeof(int));
    q->original = malloc((count ? count : 1) * sizeof(int));
    if (!q->items || !q->original || count == 0) {
        q->count = 0;
        q->pos = -1;
        return;
    }
    memcpy(q->items, tracks, count * sizeof(int));
    memcpy(q->original, tracks, count * sizeof(int));
    q->count = count;
    if (start < 0) start = app->cfg.shuffle ? rand() % count : 0;
    if (app->cfg.shuffle) {
        // The chosen song plays first, the rest in random order.
        int chosen = q->items[start];
        q->items[start] = q->items[0];
        q->items[0] = chosen;
        shuffle_ints(q->items + 1, count - 1);
        q->pos = 0;
    } else {
        q->pos = start;
    }
    play_current(app);
}

void app_play_pause(App *app) {
    if (!app->player) return;
    PlayerState s = player_state(app->player);
    if (s == PLAYER_STOPPED) {
        if (app_current_track(app) < 0 && app->lib.count) app_play_list(app, app->lib.by_title, app->lib.count, -1);
        else play_current(app);
    } else {
        player_toggle_pause(app->player);
    }
}

void app_next(App *app) {
    Queue *q = &app->queue;
    if (!q->count) return;
    if (q->pos + 1 < q->count) q->pos++;
    else if (app->cfg.repeat != REPEAT_OFF) q->pos = 0;
    else { if (app->player) player_stop(app->player); return; }
    play_current(app);
}

void app_prev(App *app) {
    Queue *q = &app->queue;
    if (!q->count) return;
    // Like most players: restart the song unless we're in its first few seconds.
    if (app_position(app) > 3.0f) { player_seek(app->player, 0); return; }
    if (q->pos > 0) q->pos--;
    else if (app->cfg.repeat != REPEAT_OFF) q->pos = q->count - 1;
    play_current(app);
}

void app_seek(App *app, float fraction) {
    if (app->player && player_state(app->player) != PLAYER_STOPPED) player_seek(app->player, fraction);
}

void app_set_shuffle(App *app, bool on) {
    Queue *q = &app->queue;
    if (on == app->cfg.shuffle) return;
    app->cfg.shuffle = on;
    int cur = app_current_track(app);
    if (q->count > 1) {
        memcpy(q->items, q->original, q->count * sizeof(int));
        if (on) {
            int at = 0;
            for (int i = 0; i < q->count; i++) if (q->items[i] == cur) at = i;
            q->items[at] = q->items[0];
            q->items[0] = cur;
            shuffle_ints(q->items + 1, q->count - 1);
            q->pos = 0;
        } else {
            for (int i = 0; i < q->count; i++) if (q->items[i] == cur) q->pos = i;
        }
    }
    config_save(&app->cfg);
    app_status(app, on ? "Shuffle on" : "Shuffle off");
}

void app_cycle_repeat(App *app) {
    app->cfg.repeat = (app->cfg.repeat + 1) % REPEAT_COUNT;
    config_save(&app->cfg);
    static const char *names[] = {"Repeat off", "Repeat all", "Repeat one"};
    app_status(app, names[app->cfg.repeat]);
}

void app_set_volume(App *app, float v) {
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    app->cfg.volume = v;
    if (app->player) player_set_volume(app->player, v);
    config_save(&app->cfg);
}

void app_set_theme(App *app, int theme) {
    if (theme < 0 || theme >= theme_count()) return;
    app->cfg.theme = theme;
    const Theme *t = theme_get(theme);
    if (t->enter) t->enter(app);
    config_save(&app->cfg); // remembered for the next boot
}

void app_toggle_favorite(App *app) {
    int t = app_current_track(app);
    if (t < 0) return;
    mkdir("sdmc:/3ds", 0777);
    mkdir(CONFIG_DIR, 0777);
    bool fav = library_toggle_favorite(&app->lib, t, FAVORITES_PATH);
    app_status(app, fav ? "Added to Favorites" : "Removed from Favorites");
    MenuLevel *m = menu_top(app);
    if (m->kind == MENU_PLAYLIST || m->kind == MENU_PLAYLISTS) menu_refresh(app);
}

static void on_track_finished(App *app) {
    if (app->cfg.repeat == REPEAT_ONE) play_current(app);
    else app_next(app);
}

// ---------------------------------------------------------------------------------------------
// Themes

int theme_count(void) { return 6; }

const Theme *theme_get(int i) {
    static const Theme *themes[] = {&THEME_TAPEDECK, &THEME_WAVE, &THEME_CLICKWHEEL, &THEME_PORTABLE, &THEME_MEDIABLUE, &THEME_DASHBOARD};
    if (i < 0 || i >= 6) i = 0;
    return themes[i];
}

bool theme_scrub(App *app, const TouchInput *t, float x, float y, float w, float h) {
    if (!ui_hit(t->start_x, t->start_y, x - 6, y - 10, w + 12, h + 20)) return false;
    if (t->up) {
        float f = (t->x - x) / w;
        app_seek(app, f < 0 ? 0 : f > 1 ? 1 : f);
    }
    return true;
}

bool theme_tap(const TouchInput *t, float x, float y, float w, float h) {
    if (!t->up || !ui_hit(t->start_x, t->start_y, x, y, w, h)) return false;
    int dx = t->x - t->start_x, dy = t->y - t->start_y;
    return dx * dx + dy * dy < 20 * 20;
}

// ---------------------------------------------------------------------------------------------
// Theme button and picker (shared by every theme)

static void draw_theme_button(App *app) {
    const Theme *th = theme_get(app->cfg.theme);
    float x = THEME_BTN_X + 3, y = THEME_BTN_Y + 3, w = THEME_BTN_W - 6, h = THEME_BTN_H - 6;
    ui_round_rect(x, y, w, h, 5, ui_mix(th->swatch, RGB(0, 0, 0), 0.25f));
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 4, th->swatch);
    ui_sym_palette(x + w / 2, y + h / 2, h - 6, th->accent);
    (void)app;
}

#define PICK_X 20
#define PICK_Y 30
#define PICK_W 280
#define PICK_ROW 32

static void draw_picker(App *app) {
    ui_rect(0, 0, BOTTOM_W, SCREEN_H, RGBA(0, 0, 0, 160));
    int n = theme_count();
    float h = 30 + n * PICK_ROW + 8;
    ui_round_rect(PICK_X, PICK_Y - 22, PICK_W, h, 10, RGB(28, 28, 32));
    ui_text(app, "Choose a theme", PICK_X + 14, PICK_Y - 16, 0.52f, RGB(255, 255, 255), ALIGN_LEFT);
    ui_text(app, "B: close", PICK_X + PICK_W - 12, PICK_Y - 14, 0.4f, RGB(150, 150, 160), ALIGN_RIGHT);
    for (int i = 0; i < n; i++) {
        const Theme *th = theme_get(i);
        float y = PICK_Y + 10 + i * PICK_ROW;
        bool cur = i == app->cfg.theme, sel = i == app->picker_cursor;
        if (sel) ui_round_rect(PICK_X + 6, y, PICK_W - 12, PICK_ROW - 3, 6, RGB(60, 60, 70));
        ui_round_rect(PICK_X + 14, y + 4, 36, PICK_ROW - 11, 4, th->swatch);
        ui_rect(PICK_X + 18, y + 8 + (PICK_ROW - 19) / 2.0f, 28, 3, th->accent);
        ui_text(app, th->name, PICK_X + 60, y + 2, 0.5f, RGB(255, 255, 255), ALIGN_LEFT);
        ui_text_fit(app, th->description, PICK_X + 60, y + 16, 0.36f, RGB(160, 160, 170), ALIGN_LEFT, PICK_W - 100);
        if (cur) ui_sym_play(PICK_X + PICK_W - 22, y + PICK_ROW / 2 - 1, 10, th->accent);
    }
}

static void picker_touch(App *app, const TouchInput *t) {
    if (!t->up) return;
    int n = theme_count();
    for (int i = 0; i < n; i++)
        if (theme_tap(t, PICK_X + 6, PICK_Y + 10 + i * PICK_ROW, PICK_W - 12, PICK_ROW - 3)) {
            app_set_theme(app, i);
            app->picker_open = false;
            return;
        }
    if (!ui_hit(t->start_x, t->start_y, PICK_X, PICK_Y - 22, PICK_W, 30 + n * PICK_ROW + 8)) app->picker_open = false;
}

// ---------------------------------------------------------------------------------------------
// Scan screen

typedef struct { App *app; u64 last; } ScanUi;

static void draw_message(App *app, const char *title, const char *line) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    ui_begin_frame(app);
    C2D_TargetClear(top_target, RGB(24, 20, 18));
    C2D_SceneBegin(top_target);
    if (app->has_logo) {
        float w = 300, h = w * app->logo_h / app->logo_w;
        ui_image(&app->logo, (TOP_W - w) / 2, 14, w, h);
    } else {
        ui_text(app, APP_NAME, TOP_W / 2, 70, 0.9f, RGB(255, 255, 255), ALIGN_CENTER);
    }
    ui_text(app, title, TOP_W / 2, 200, 0.5f, RGB(240, 220, 190), ALIGN_CENTER);
    C2D_TargetClear(bottom_target, RGB(24, 20, 18));
    C2D_SceneBegin(bottom_target);
    ui_text_fit(app, line, BOTTOM_W / 2, 110, 0.45f, RGB(170, 160, 150), ALIGN_CENTER, BOTTOM_W - 30);
    C3D_FrameEnd(0);
}

static void scan_progress(int found, const char *current, void *user) {
    ScanUi *s = user;
    u64 now = osGetTime();
    if (now - s->last < 100) return; // redraw at most 10 times a second
    s->last = now;
    char title[64];
    snprintf(title, sizeof title, "Loading your music... %d songs", found);
    draw_message(s->app, title, current);
}

// ---------------------------------------------------------------------------------------------
// Input

static void handle_buttons(App *app, u32 down, u32 held) {
    if (app->splash) {
        if (down & (KEY_A | KEY_START | KEY_TOUCH | KEY_B)) app->splash = false;
        return;
    }
    if (app->picker_open) {
        int n = theme_count();
        if (down & (KEY_DUP | KEY_CPAD_UP)) app->picker_cursor = (app->picker_cursor + n - 1) % n;
        if (down & (KEY_DDOWN | KEY_CPAD_DOWN)) app->picker_cursor = (app->picker_cursor + 1) % n;
        if (down & KEY_A) { app_set_theme(app, app->picker_cursor); app->picker_open = false; }
        if (down & (KEY_B | KEY_SELECT)) app->picker_open = false;
        return;
    }
    const Theme *th = theme_get(app->cfg.theme);
    if (th->buttons && th->buttons(app, down, held)) return;

    static float repeat_timer = 0;
    u32 nav = KEY_DUP | KEY_DDOWN | KEY_CPAD_UP | KEY_CPAD_DOWN | KEY_DLEFT | KEY_DRIGHT | KEY_CPAD_LEFT | KEY_CPAD_RIGHT;
    if (held & nav) repeat_timer += app->dt;
    else repeat_timer = 0;
    bool rep = repeat_timer > 0.4f;
    if (rep) repeat_timer = 0.33f; // ~15 steps per second while held
    u32 press = down | (rep ? held : 0);

    if (!app->show_now_playing) {
        MenuLevel *m = menu_top(app);
        int cols = m->kind == MENU_ALBUMS ? th->album_cols : 0;
        int step = (held & KEY_ZR) ? 10 : 1;
        if (cols == 1) { // carousel: left/right
            if (press & (KEY_DLEFT | KEY_CPAD_LEFT | KEY_DUP | KEY_CPAD_UP)) menu_move(app, -step);
            if (press & (KEY_DRIGHT | KEY_CPAD_RIGHT | KEY_DDOWN | KEY_CPAD_DOWN)) menu_move(app, step);
        } else if (cols > 1) { // grid
            if (press & (KEY_DLEFT | KEY_CPAD_LEFT)) menu_move(app, -1);
            if (press & (KEY_DRIGHT | KEY_CPAD_RIGHT)) menu_move(app, 1);
            if (press & (KEY_DUP | KEY_CPAD_UP)) menu_move(app, -cols);
            if (press & (KEY_DDOWN | KEY_CPAD_DOWN)) menu_move(app, cols);
        } else {
            if (press & (KEY_DUP | KEY_CPAD_UP)) menu_move(app, -step);
            if (press & (KEY_DDOWN | KEY_CPAD_DOWN)) menu_move(app, step);
            if (m->kind == MENU_SETTINGS) {
                if (down & (KEY_DLEFT | KEY_CPAD_LEFT)) menu_adjust(app, -1);
                if (down & (KEY_DRIGHT | KEY_CPAD_RIGHT)) menu_adjust(app, 1);
            } else {
                if (press & KEY_DLEFT) menu_move(app, -8);
                if (press & KEY_DRIGHT) menu_move(app, 8);
            }
        }
        if (down & KEY_A) menu_activate(app);
        if (down & KEY_B) menu_pop(app);
    } else {
        if (down & (KEY_B | KEY_A)) app->show_now_playing = false;
        if (down & KEY_DUP) app_set_volume(app, app->cfg.volume + 0.05f);
        if (down & KEY_DDOWN) app_set_volume(app, app->cfg.volume - 0.05f);
        float d = app_duration(app) + 0.001f;
        if (down & KEY_DRIGHT) app_seek(app, (app_position(app) + 10) / d);
        if (down & KEY_DLEFT) app_seek(app, (app_position(app) - 10) / d);
    }
    if (down & KEY_X) app->show_now_playing = !app->show_now_playing;
    if (down & KEY_Y) app_set_shuffle(app, !app->cfg.shuffle);
    if (down & KEY_START) app_play_pause(app);
    if (down & KEY_R) app_next(app);
    if (down & KEY_L) app_prev(app);
    if (down & KEY_SELECT) { app->picker_open = true; app->picker_cursor = app->cfg.theme; }
}

static void handle_touch(App *app, const TouchInput *t) {
    if (app->splash) return;
    if (app->picker_open) { picker_touch(app, t); return; }
    if (ui_hit(t->start_x, t->start_y, THEME_BTN_X, THEME_BTN_Y, THEME_BTN_W, THEME_BTN_H)) {
        if (theme_tap(t, THEME_BTN_X, THEME_BTN_Y, THEME_BTN_W, THEME_BTN_H)) {
            app->picker_open = true;
            app->picker_cursor = app->cfg.theme;
        }
        return;
    }
    const Theme *th = theme_get(app->cfg.theme);
    if (th->touch) th->touch(app, t);
}

// ---------------------------------------------------------------------------------------------
// Self-test build (make AUTOTEST=1): drives every theme through its screens and saves screenshots
// to sdmc:/3ds/nTapeDeck/shots/. Not part of the normal app.

#ifdef AUTOTEST
static void save_screenshot(const char *name) {
    mkdir(CONFIG_DIR "/shots", 0777);
    char path[160];
    snprintf(path, sizeof path, CONFIG_DIR "/shots/%s.bmp", name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const int W = 400, H = 480, row = W * 3;
    u8 hdr[54] = {'B', 'M'};
    u32 size = 54 + row * H;
    memcpy(hdr + 2, &size, 4);
    hdr[10] = 54; hdr[14] = 40;
    memcpy(hdr + 18, &W, 4);
    memcpy(hdr + 22, &H, 4);
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    u16 tw, th, bw, bh;
    u8 *top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &tw, &th);
    u8 *bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bw, &bh);
    u8 *line = malloc(row);
    for (int y = H - 1; y >= 0; y--) { // BMP rows are bottom-up
        memset(line, 0, row);
        for (int x = 0; x < W; x++) {
            u8 *src = NULL;
            if (y < 240) src = top + (x * 240 + (239 - y)) * 3;
            else if (x >= 40 && x < 360) src = bot + ((x - 40) * 240 + (239 - (y - 240))) * 3;
            if (src) memcpy(line + x * 3, src, 3);
        }
        fwrite(line, 1, row, f);
    }
    free(line);
    fclose(f);
}

typedef enum { A_THEME, A_SPLASH, A_HOME, A_PUSH, A_MOVE, A_ACTIVATE, A_NOWTOP, A_POP, A_PICKER, A_SHOT, A_QUIT } Act;
typedef struct { Act act; int arg; const char *shot; } Step;

static const Step SCRIPT[] = {
    {A_SPLASH, 1, "splash"}, {A_HOME, 0, "home"},
    {A_PUSH, MENU_SONGS, NULL}, {A_MOVE, 2, "songs"},
    {A_ACTIVATE, 0, "playing"},
    {A_NOWTOP, 1, "nowplaying"}, {A_NOWTOP, 0, NULL},
    {A_POP, 0, NULL}, {A_PUSH, MENU_ALBUMS, NULL}, {A_MOVE, 1, "albums"},
    {A_POP, 0, NULL}, {A_PUSH, MENU_ARTISTS, "artists"},
    {A_POP, 0, NULL}, {A_PUSH, MENU_PLAYLISTS, "playlists"},
    {A_POP, 0, NULL}, {A_PUSH, MENU_SETTINGS, "settings"},
    {A_POP, 0, NULL}, {A_PICKER, 1, "picker"}, {A_PICKER, 0, NULL},
};

static void autotest_step(App *app) {
    static int theme = 0, step = -1, wait = 0;
    if (wait > 0) { wait--; return; }
    char shot[64];
    if (step >= 0 && step < (int)(sizeof SCRIPT / sizeof *SCRIPT) && SCRIPT[step].shot) {
        snprintf(shot, sizeof shot, "%d_%s_%s", theme, theme_get(theme)->name, SCRIPT[step].shot);
        for (char *p = shot; *p; p++) if (*p == ' ') *p = '-';
        save_screenshot(shot);
    }
    step++;
    if (step >= (int)(sizeof SCRIPT / sizeof *SCRIPT)) {
        step = 0;
        theme++;
        if (theme >= theme_count()) { app->quit = true; return; }
    }
    if (step == 0) { app_set_theme(app, theme); while (menu_pop(app)) {} }
    const Step *s = &SCRIPT[step];
    switch (s->act) {
    case A_SPLASH: app->splash = s->arg && theme_get(theme)->draw_splash; break;
    case A_HOME: app->splash = false; app->show_now_playing = false; menu_top(app)->cursor = ROOT_SONGS; break;
    case A_PUSH: menu_push(app, (MenuKind)s->arg, 0); break;
    case A_MOVE: menu_move(app, s->arg); break;
    case A_ACTIVATE: menu_activate(app); break;
    case A_NOWTOP: app->show_now_playing = s->arg; break;
    case A_POP: menu_pop(app); break;
    case A_PICKER: app->picker_open = s->arg; app->picker_cursor = (theme + 1) % theme_count(); break;
    default: break;
    }
    // Long waits where art loads or playback must advance.
    wait = s->act == A_ACTIVATE ? 150 : s->shot ? 45 : 6;
}
#endif

// ---------------------------------------------------------------------------------------------
// Main

int main(void) {
    gfxInitDefault();
    romfsInit();
    mcuHwcInit();
    ptmuInit();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 4);
    C2D_Prepare();
    top_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    // Keep playing with the lid closed: we turn the backlights off ourselves.
    aptSetSleepAllowed(false);
    srand((unsigned)time(NULL));

    static App app;
    memset(&app, 0, sizeof app);
    ui_init(&app);
    config_load(&app.cfg);
    app.queue.pos = -1;
    app.has_logo = image_load("romfs:/logo.png", &app.logo, &logo_tex, &logo_sub, &app.logo_w, &app.logo_h);

    char err[160] = "";
    app.player = player_create(err, sizeof err);
    if (app.player) player_set_volume(app.player, app.cfg.volume);

    ScanUi scan = {.app = &app};
    draw_message(&app, "Loading your music...", MUSIC_DIR);
    if (!library_scan(&app.lib, MUSIC_DIR, FAVORITES_PATH, scan_progress, &scan)) app_status(&app, "No MP3 folder: put your music in sdmc:/MP3");
    else if (!app.lib.count) app_status(&app, "No .mp3 files found in sdmc:/MP3");
    else if (app.lib.truncated) app_status(&app, "Large library: only the first 8000 songs were loaded");
    if (!app.player) app_status(&app, err); // the audio error matters most
    app.art = art_create(&app.lib);

    menu_init(&app);
    const Theme *th = theme_get(app.cfg.theme);
    if (th->enter) th->enter(&app);
    app.splash = th->draw_splash != NULL;

    TouchInput touch = {0};
    bool lid_closed = false;
    u64 last = osGetTime();
    float lid_check = 0;

    while (aptMainLoop() && !app.quit) {
        u64 now = osGetTime();
        app.dt = (now - last) / 1000.0f;
        if (app.dt > 0.1f) app.dt = 0.1f;
        last = now;
        app.time += app.dt;
        if (app.status_ttl > 0) app.status_ttl -= app.dt;

        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld(), up = hidKeysUp();
        touchPosition tp;
        hidTouchRead(&tp);
        touch.prev_x = touch.x;
        touch.prev_y = touch.y;
        touch.down = down & KEY_TOUCH;
        touch.held = held & KEY_TOUCH;
        touch.up = up & KEY_TOUCH;
        if (touch.held || touch.down) { touch.x = tp.px; touch.y = tp.py; }
        if (touch.down) { touch.start_x = touch.prev_x = tp.px; touch.start_y = touch.prev_y = tp.py; }

        // Lid closed: backlights off, music keeps playing.
        lid_check -= app.dt;
        if (lid_check <= 0) {
            lid_check = 0.25f;
            u8 shell = 1;
            PTMU_GetShellState(&shell);
            if (!shell != lid_closed) {
                lid_closed = !shell;
                gspLcdInit();
                if (lid_closed) GSPLCD_PowerOffAllBacklights();
                else GSPLCD_PowerOnAllBacklights();
                gspLcdExit();
            }
        }

#ifdef AUTOTEST
        autotest_step(&app);
#endif
        handle_buttons(&app, down, held);
        if (touch.down || touch.held || touch.up) handle_touch(&app, &touch);

        if (app.player) {
            if (player_take_finished(app.player)) on_track_finished(&app);
            char msg[128];
            if (player_take_error(app.player, msg, sizeof msg)) {
                char line[192];
                int t = app_current_track(&app);
                snprintf(line, sizeof line, "%s: %s", msg, t >= 0 ? app.lib.tracks[t].title : "");
                app_status(&app, line);
            }
            player_spectrum(app.player, app.spectrum, 32);
        }
        art_update(app.art);

        if (lid_closed) { gspWaitForVBlank(); continue; } // don't draw while closed

        th = theme_get(app.cfg.theme);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        ui_begin_frame(&app);
        C2D_TargetClear(top_target, RGB(0, 0, 0));
        C2D_SceneBegin(top_target);
        if (app.splash && th->draw_splash) th->draw_splash(&app);
        else th->draw_top(&app);
        C2D_TargetClear(bottom_target, RGB(0, 0, 0));
        C2D_SceneBegin(bottom_target);
        th->draw_bottom(&app);
        draw_theme_button(&app);
        if (app.picker_open) draw_picker(&app);
        C3D_FrameEnd(0);
    }

    config_save(&app.cfg);
    art_destroy(app.art);
    player_destroy(app.player);
    menu_free(&app);
    library_free(&app.lib);
    free(app.queue.items);
    free(app.queue.original);
    if (app.has_logo) C3D_TexDelete(&logo_tex);
    C2D_TextBufDelete(app.textbuf);
    C2D_TextBufDelete(app.measurebuf);
    C2D_Fini();
    C3D_Fini();
    ptmuExit();
    mcuHwcExit();
    romfsExit();
    gfxExit();
    return 0;
}
