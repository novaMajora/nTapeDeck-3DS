// The browse hierarchy every theme draws: Music → Artists/Albums/Songs/Folders, Settings.
#pragma once

#include <stdbool.h>

struct App;

typedef enum {
    MENU_ROOT,
    MENU_SONGS,          // all songs by title
    MENU_ARTISTS,
    MENU_ARTIST,         // albums of artist `param` (plus "All Songs")
    MENU_ALBUMS,
    MENU_ALBUM,          // songs of album `param`
    MENU_ARTIST_SONGS,   // all songs of artist `param`
    MENU_FOLDER,         // subfolders then songs of folder `param`
    MENU_PLAYLISTS,
    MENU_PLAYLIST,       // songs of playlist `param`
    MENU_GENRES,
    MENU_GENRE,          // songs of genre `param`
    MENU_SETTINGS,
    MENU_THEMES,
} MenuKind;

typedef struct {
    MenuKind kind;
    int param;
    int cursor;
    float scroll;      // first visible row, animated by themes
    int count;
    int *items;        // per-kind item ids (tracks, groups, folders), malloc'd
    int sub_count;     // MENU_FOLDER: how many of `items` are subfolders (the rest are tracks)
    char title[64];
} MenuLevel;

#define MENU_DEPTH 8

typedef struct {
    MenuLevel levels[MENU_DEPTH];
    int depth;
} MenuStack;

typedef enum {
    ICON_NONE, ICON_FOLDER, ICON_SONG, ICON_ARTIST, ICON_ALBUM, ICON_SETTINGS, ICON_PLAYING, ICON_SHUFFLE,
    ICON_PLAYLIST, ICON_GENRE, ICON_HEART, ICON_CLOCK, ICON_REPEAT, ICON_VOLUME, ICON_THEME, ICON_INFO,
} MenuIcon;

// Root items, in order.
enum { ROOT_NOW_PLAYING, ROOT_SONGS, ROOT_PLAYLISTS, ROOT_ARTISTS, ROOT_ALBUMS, ROOT_GENRES, ROOT_FOLDERS, ROOT_SETTINGS, ROOT_COUNT };
// Settings items, in order.
enum { SET_SHUFFLE, SET_REPEAT, SET_THEME, SET_VOLUME, SET_ABOUT, SET_COUNT };

void menu_init(struct App *app);
void menu_free(struct App *app);
MenuLevel *menu_top(struct App *app);
void menu_push(struct App *app, MenuKind kind, int param);
bool menu_pop(struct App *app);            // false at the root
void menu_refresh(struct App *app);        // rebuild the top level (e.g. after a settings change)

// Labels for drawing.
const char *menu_label(struct App *app, const MenuLevel *m, int i, char *buf, int len);
const char *menu_detail(struct App *app, const MenuLevel *m, int i, char *buf, int len); // second line (artist...)
const char *menu_value(struct App *app, const MenuLevel *m, int i, char *buf, int len);  // right column ("24 Songs", "Off")
int menu_art(struct App *app, const MenuLevel *m, int i);  // album whose art represents the item, or -1
int menu_track(struct App *app, const MenuLevel *m, int i); // library track of a song item, or -1
MenuIcon menu_icon(struct App *app, const MenuLevel *m, int i);
bool menu_has_children(struct App *app, const MenuLevel *m, int i);

void menu_move(struct App *app, int delta);   // cursor up/down, clamped
void menu_activate(struct App *app);          // select the cursor item
void menu_adjust(struct App *app, int dir);   // Left/Right on a settings value
