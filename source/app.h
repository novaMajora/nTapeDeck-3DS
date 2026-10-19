// nTapeDeck 3DS: an MP3 player for the Nintendo 3DS with swappable retro themes.
#pragma once

#include <3ds.h>
#include <citro2d.h>
#include <stdbool.h>

#include "art.h"
#include "library.h"
#include "menu.h"
#include "player.h"

#define APP_NAME "nTapeDeck 3DS"
#define APP_SHORT "nTapeDeck"
#define APP_VERSION "0.2.0"
#define MUSIC_DIR "sdmc:/MP3"
#define CONFIG_DIR "sdmc:/3ds/nTapeDeck"
#define CONFIG_PATH CONFIG_DIR "/config.ini"
#define FAVORITES_PATH CONFIG_DIR "/favorites.m3u"

#define TOP_W 400
#define BOTTOM_W 320
#define SCREEN_H 240

// The theme button every theme shows in the top-right corner of the bottom screen.
#define THEME_BTN_X 288
#define THEME_BTN_Y 0
#define THEME_BTN_W 32
#define THEME_BTN_H 26

typedef enum { REPEAT_OFF, REPEAT_ALL, REPEAT_ONE, REPEAT_COUNT } RepeatMode;

typedef struct {
    int theme;
    bool shuffle;
    RepeatMode repeat;
    float volume; // 0..1
} Config;

// The play queue: indices into the library's track list, in play order.
typedef struct {
    int *items;
    int count;
    int pos;          // index into items of the current track, -1 when empty
    int *original;    // unshuffled order (to restore when shuffle is turned off)
} Queue;

typedef struct App {
    Library lib;
    Player *player;
    ArtCache *art;
    Queue queue;
    Config cfg;
    MenuStack menu;
    bool splash;           // "Press A to start" title screen (themes that have one)
    bool show_now_playing; // themes draw the player view on top instead of the menu
    bool picker_open;      // the theme picker overlay on the bottom screen
    int picker_cursor;
    float time;            // seconds since start, for animations
    float dt;
    char status[192];       // one-line message shown by themes (errors, "library empty"...)
    float status_ttl;
    C2D_TextBuf textbuf;
    C2D_TextBuf measurebuf;
    C2D_Font font;
    bool quit;
    float spectrum[32];    // 0..1 per band, refreshed every frame
    // The nTapeDeck cassette artwork (romfs), used by the Tape Deck theme and the splash screens.
    C2D_Image logo;
    int logo_w, logo_h;
    bool has_logo;
} App;

// Queue and transport, shared by every theme.
void app_play_list(App *app, const int *tracks, int count, int start);
void app_play_pause(App *app);
void app_next(App *app);
void app_prev(App *app);
void app_seek(App *app, float fraction);
void app_set_shuffle(App *app, bool on);
void app_cycle_repeat(App *app);
void app_set_volume(App *app, float v);
void app_set_theme(App *app, int theme);
void app_toggle_favorite(App *app);
void app_status(App *app, const char *msg);
int app_current_track(const App *app); // library index or -1
float app_position(App *app);
float app_duration(App *app);
bool app_playing(App *app);

void config_load(Config *cfg);
void config_save(const Config *cfg);
