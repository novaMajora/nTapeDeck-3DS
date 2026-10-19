#include "menu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "themes/theme.h"

static const char *ROOT_LABELS[ROOT_COUNT] = {"Now Playing", "All Songs", "Playlists", "Artists", "Albums", "Genres", "Folders", "Settings"};
static const char *SET_LABELS[SET_COUNT] = {"Shuffle", "Repeat", "Theme", "Volume", "About"};

static void build(App *app, MenuLevel *m) {
    Library *lib = &app->lib;
    free(m->items);
    m->items = NULL;
    m->sub_count = 0;
    switch (m->kind) {
    case MENU_ROOT: m->count = ROOT_COUNT; snprintf(m->title, sizeof m->title, "Music"); break;
    case MENU_SONGS: m->count = lib->count; snprintf(m->title, sizeof m->title, "All Songs"); break;
    case MENU_ARTISTS: m->count = lib->artist_count; snprintf(m->title, sizeof m->title, "Artists"); break;
    case MENU_ARTIST: {
        int *albums;
        int n = library_artist_albums(lib, m->param, &albums);
        m->items = albums;
        m->count = n + 1; // "All Songs" first
        snprintf(m->title, sizeof m->title, "%s", lib->artists[m->param].name);
        break;
    }
    case MENU_ARTIST_SONGS: m->count = lib->artists[m->param].count; snprintf(m->title, sizeof m->title, "%s", lib->artists[m->param].name); break;
    case MENU_ALBUMS: m->count = lib->album_count; snprintf(m->title, sizeof m->title, "Albums"); break;
    case MENU_ALBUM: m->count = lib->albums[m->param].count; snprintf(m->title, sizeof m->title, "%s", lib->albums[m->param].name); break;
    case MENU_GENRES: m->count = lib->genre_count; snprintf(m->title, sizeof m->title, "Genres"); break;
    case MENU_GENRE: m->count = lib->genres[m->param].count; snprintf(m->title, sizeof m->title, "%s", lib->genres[m->param].name); break;
    case MENU_PLAYLISTS: m->count = lib->playlist_count; snprintf(m->title, sizeof m->title, "Playlists"); break;
    case MENU_PLAYLIST: m->count = lib->playlists[m->param].count; snprintf(m->title, sizeof m->title, "%s", lib->playlists[m->param].name); break;
    case MENU_FOLDER: {
        int *subs, *tracks;
        int ns = library_subfolders(lib, m->param, &subs);
        int nt = library_folder_tracks(lib, m->param, &tracks);
        m->items = malloc((ns + nt + 1) * sizeof(int));
        if (m->items) {
            if (ns) memcpy(m->items, subs, ns * sizeof(int));
            if (nt) memcpy(m->items + ns, tracks, nt * sizeof(int));
            m->sub_count = ns;
            m->count = ns + nt;
        } else {
            m->count = 0;
        }
        free(subs);
        free(tracks);
        snprintf(m->title, sizeof m->title, "%s", m->param == 0 ? "Folders" : lib->folders[m->param].name);
        break;
    }
    case MENU_SETTINGS: m->count = SET_COUNT; snprintf(m->title, sizeof m->title, "Settings"); break;
    case MENU_THEMES: m->count = theme_count(); snprintf(m->title, sizeof m->title, "Theme"); break;
    }
    if (m->cursor >= m->count) m->cursor = m->count ? m->count - 1 : 0;
}

void menu_init(App *app) {
    memset(&app->menu, 0, sizeof app->menu);
    app->menu.depth = 1;
    app->menu.levels[0].kind = MENU_ROOT;
    app->menu.levels[0].cursor = ROOT_SONGS;
    build(app, &app->menu.levels[0]);
}

void menu_free(App *app) {
    for (int i = 0; i < app->menu.depth; i++) free(app->menu.levels[i].items);
    app->menu.depth = 0;
}

MenuLevel *menu_top(App *app) { return &app->menu.levels[app->menu.depth - 1]; }

void menu_push(App *app, MenuKind kind, int param) {
    if (app->menu.depth >= MENU_DEPTH) return;
    MenuLevel *m = &app->menu.levels[app->menu.depth++];
    memset(m, 0, sizeof *m);
    m->kind = kind;
    m->param = param;
    build(app, m);
    if (kind == MENU_THEMES) m->cursor = app->cfg.theme;
}

bool menu_pop(App *app) {
    if (app->menu.depth <= 1) return false;
    MenuLevel *m = menu_top(app);
    free(m->items);
    m->items = NULL;
    app->menu.depth--;
    return true;
}

void menu_refresh(App *app) { build(app, menu_top(app)); }

int menu_track(App *app, const MenuLevel *m, int i) {
    Library *lib = &app->lib;
    if (i < 0 || i >= m->count) return -1;
    switch (m->kind) {
    case MENU_SONGS: return lib->by_title[i];
    case MENU_ARTIST_SONGS: return lib->artists[m->param].tracks[i];
    case MENU_ALBUM: return lib->albums[m->param].tracks[i];
    case MENU_GENRE: return lib->genres[m->param].tracks[i];
    case MENU_PLAYLIST: return lib->playlists[m->param].tracks[i];
    case MENU_FOLDER: return m->items && i >= m->sub_count ? m->items[i] : -1;
    default: return -1;
    }
}

const char *menu_label(App *app, const MenuLevel *m, int i, char *buf, int len) {
    Library *lib = &app->lib;
    (void)buf; (void)len;
    if (i < 0 || i >= m->count) return "";
    int t = menu_track(app, m, i);
    if (t >= 0) return lib->tracks[t].title;
    switch (m->kind) {
    case MENU_ROOT: return ROOT_LABELS[i];
    case MENU_ARTISTS: return lib->artists[i].name;
    case MENU_ARTIST: return i == 0 ? "All Songs" : lib->albums[m->items[i - 1]].name;
    case MENU_ALBUMS: return lib->albums[i].name;
    case MENU_GENRES: return lib->genres[i].name;
    case MENU_PLAYLISTS: return lib->playlists[i].name;
    case MENU_FOLDER: return m->items ? lib->folders[m->items[i]].name : "";
    case MENU_SETTINGS: return SET_LABELS[i];
    case MENU_THEMES: return theme_get(i)->name;
    default: return "";
    }
}

const char *menu_detail(App *app, const MenuLevel *m, int i, char *buf, int len) {
    Library *lib = &app->lib;
    if (i < 0 || i >= m->count) return "";
    int t = menu_track(app, m, i);
    if (t >= 0) return lib->tracks[t].artist;
    switch (m->kind) {
    case MENU_ALBUMS: {
        const Group *g = &lib->albums[i];
        return g->count ? lib->tracks[g->tracks[0]].artist : "";
    }
    case MENU_ARTIST:
        if (i == 0) return "";
        snprintf(buf, len, "%d songs", lib->albums[m->items[i - 1]].count);
        return buf;
    case MENU_THEMES: return theme_get(i)->description;
    default: return "";
    }
}

static const char *repeat_name(RepeatMode r) { return r == REPEAT_ALL ? "All" : r == REPEAT_ONE ? "One" : "Off"; }

static const char *songs(int n, char *buf, int len) {
    snprintf(buf, len, n == 1 ? "1 Song" : "%d Songs", n);
    return buf;
}

const char *menu_value(App *app, const MenuLevel *m, int i, char *buf, int len) {
    Library *lib = &app->lib;
    if (i < 0 || i >= m->count) return "";
    switch (m->kind) {
    case MENU_ARTISTS: return songs(lib->artists[i].count, buf, len);
    case MENU_GENRES: return songs(lib->genres[i].count, buf, len);
    case MENU_PLAYLISTS: return songs(lib->playlists[i].count, buf, len);
    case MENU_ALBUMS: return songs(lib->albums[i].count, buf, len);
    case MENU_SETTINGS:
        switch (i) {
        case SET_SHUFFLE: return app->cfg.shuffle ? "On" : "Off";
        case SET_REPEAT: return repeat_name(app->cfg.repeat);
        case SET_THEME: return theme_get(app->cfg.theme)->name;
        case SET_VOLUME: snprintf(buf, len, "%d%%", (int)(app->cfg.volume * 100 + 0.5f)); return buf;
        case SET_ABOUT: return APP_VERSION;
        }
        return "";
    case MENU_THEMES: return i == app->cfg.theme ? "Current" : "";
    default: {
        int t = menu_track(app, m, i);
        if (t >= 0 && t == app_current_track(app)) return "Playing";
        return "";
    }
    }
}

int menu_art(App *app, const MenuLevel *m, int i) {
    Library *lib = &app->lib;
    if (i < 0 || i >= m->count) return -1;
    int t = menu_track(app, m, i);
    if (t >= 0) return lib->tracks[t].album_id;
    switch (m->kind) {
    case MENU_ALBUMS: return i;
    case MENU_ARTIST:
        if (i > 0) return m->items[i - 1];
        return lib->artists[m->param].count ? lib->tracks[lib->artists[m->param].tracks[0]].album_id : -1;
    case MENU_ARTISTS: return lib->artists[i].count ? lib->tracks[lib->artists[i].tracks[0]].album_id : -1;
    case MENU_GENRES: return lib->genres[i].count ? lib->tracks[lib->genres[i].tracks[0]].album_id : -1;
    case MENU_PLAYLISTS: return lib->playlists[i].count ? lib->tracks[lib->playlists[i].tracks[0]].album_id : -1;
    default: return -1;
    }
}

MenuIcon menu_icon(App *app, const MenuLevel *m, int i) {
    switch (m->kind) {
    case MENU_ROOT: {
        static const MenuIcon icons[ROOT_COUNT] = {ICON_PLAYING, ICON_SONG, ICON_PLAYLIST, ICON_ARTIST, ICON_ALBUM, ICON_GENRE, ICON_FOLDER, ICON_SETTINGS};
        return icons[i];
    }
    case MENU_ARTISTS: return ICON_ARTIST;
    case MENU_ALBUMS: case MENU_ARTIST: return ICON_ALBUM;
    case MENU_GENRES: return ICON_GENRE;
    case MENU_PLAYLISTS: {
        PlaylistKind k = app->lib.playlists[i].kind;
        return k == PLAYLIST_FAVORITES ? ICON_HEART : k == PLAYLIST_RECENT ? ICON_CLOCK : ICON_PLAYLIST;
    }
    case MENU_FOLDER: return i < m->sub_count ? ICON_FOLDER : ICON_SONG;
    case MENU_SETTINGS: {
        static const MenuIcon icons[SET_COUNT] = {ICON_SHUFFLE, ICON_REPEAT, ICON_THEME, ICON_VOLUME, ICON_INFO};
        return icons[i];
    }
    case MENU_THEMES: return ICON_THEME;
    default: return ICON_SONG;
    }
}

bool menu_has_children(App *app, const MenuLevel *m, int i) {
    (void)app;
    switch (m->kind) {
    case MENU_ROOT: return i != ROOT_NOW_PLAYING;
    case MENU_ARTISTS: case MENU_ALBUMS: case MENU_ARTIST: case MENU_GENRES: case MENU_PLAYLISTS: case MENU_SETTINGS: return true;
    case MENU_FOLDER: return i < m->sub_count;
    default: return false;
    }
}

void menu_move(App *app, int delta) {
    MenuLevel *m = menu_top(app);
    if (!m->count) return;
    int c = m->cursor + delta;
    if (c < 0) c = 0;
    if (c >= m->count) c = m->count - 1;
    m->cursor = c;
}

void menu_adjust(App *app, int dir) {
    MenuLevel *m = menu_top(app);
    if (m->kind != MENU_SETTINGS) return;
    switch (m->cursor) {
    case SET_SHUFFLE: app_set_shuffle(app, !app->cfg.shuffle); break;
    case SET_REPEAT:
        app_cycle_repeat(app);
        if (dir < 0) app_cycle_repeat(app); // three modes: twice forward is once back
        break;
    case SET_THEME: app_set_theme(app, (app->cfg.theme + (dir < 0 ? theme_count() - 1 : 1)) % theme_count()); break;
    case SET_VOLUME: app_set_volume(app, app->cfg.volume + dir * 0.05f); break;
    }
}

void menu_activate(App *app) {
    MenuLevel *m = menu_top(app);
    Library *lib = &app->lib;
    int i = m->cursor;
    if (i < 0 || i >= m->count) return;
    switch (m->kind) {
    case MENU_ROOT:
        switch (i) {
        case ROOT_NOW_PLAYING: app->show_now_playing = true; break;
        case ROOT_SONGS: menu_push(app, MENU_SONGS, 0); break;
        case ROOT_PLAYLISTS: menu_push(app, MENU_PLAYLISTS, 0); break;
        case ROOT_ARTISTS: menu_push(app, MENU_ARTISTS, 0); break;
        case ROOT_ALBUMS: menu_push(app, MENU_ALBUMS, 0); break;
        case ROOT_GENRES: menu_push(app, MENU_GENRES, 0); break;
        case ROOT_FOLDERS: menu_push(app, MENU_FOLDER, 0); break;
        case ROOT_SETTINGS: menu_push(app, MENU_SETTINGS, 0); break;
        }
        break;
    case MENU_SONGS: app_play_list(app, lib->by_title, lib->count, i); break;
    case MENU_ARTISTS: menu_push(app, MENU_ARTIST, i); break;
    case MENU_ARTIST:
        if (i == 0) menu_push(app, MENU_ARTIST_SONGS, m->param);
        else menu_push(app, MENU_ALBUM, m->items[i - 1]);
        break;
    case MENU_ARTIST_SONGS: app_play_list(app, lib->artists[m->param].tracks, lib->artists[m->param].count, i); break;
    case MENU_ALBUMS: menu_push(app, MENU_ALBUM, i); break;
    case MENU_ALBUM: app_play_list(app, lib->albums[m->param].tracks, lib->albums[m->param].count, i); break;
    case MENU_GENRES: menu_push(app, MENU_GENRE, i); break;
    case MENU_GENRE: app_play_list(app, lib->genres[m->param].tracks, lib->genres[m->param].count, i); break;
    case MENU_PLAYLISTS: menu_push(app, MENU_PLAYLIST, i); break;
    case MENU_PLAYLIST: app_play_list(app, lib->playlists[m->param].tracks, lib->playlists[m->param].count, i); break;
    case MENU_FOLDER:
        if (!m->items) break;
        if (i < m->sub_count) menu_push(app, MENU_FOLDER, m->items[i]);
        else app_play_list(app, m->items + m->sub_count, m->count - m->sub_count, i - m->sub_count);
        break;
    case MENU_SETTINGS:
        switch (i) {
        case SET_THEME: menu_push(app, MENU_THEMES, 0); break;
        case SET_ABOUT: app_status(app, APP_NAME " " APP_VERSION " - plays music from sdmc:/MP3"); break;
        default: menu_adjust(app, 1); break;
        }
        break;
    case MENU_THEMES:
        app_set_theme(app, i);
        menu_pop(app);
        break;
    }
}
