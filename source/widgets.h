// Widgets shared by the themes: art, clock, lists, album carousels, now-playing blocks.
#pragma once

#include "ui.h"

struct App;

void ui_image(const C2D_Image *img, float x, float y, float w, float h);
// Album art (or a placeholder note while it loads / when there is none).
void ui_art(struct App *app, int album, float x, float y, float size, u32 ph_bg, u32 ph_fg);
// Date, time and battery, right-aligned ending at `right`. Returns the left edge used.
float ui_clock(struct App *app, float right, float y, float size, u32 color);
void ui_signal(float x, float y, float h, u32 on, u32 off); // Wi-Fi strength bars

typedef struct ListStyle {
    float row_h, text_size, detail_size, pad;
    bool art, icon, detail, value, chevron, zebra, divider;
    u32 text, text_sel, sub, sub_sel, val, val_sel, chev, chev_sel, div, zebra_c, icon_c, icon_sel;
    u32 ph_bg, ph_fg; // art placeholder
    // Draws the selected row background; NULL for none.
    void (*highlight)(float x, float y, float w, float h);
} ListStyle;
void ui_list(struct App *app, MenuLevel *m, float x, float y, float w, float h, const ListStyle *s);

typedef struct {
    float art, gap, text_size;
    u32 caption, caption_sub, border, ph_bg, ph_fg, arrow;
} StripStyle;
// Albums as a horizontal carousel (selection kept in view) or a grid with `cols` columns.
void ui_album_strip(struct App *app, MenuLevel *m, float x, float y, float w, float h, const StripStyle *s);
void ui_album_grid(struct App *app, MenuLevel *m, float x, float y, float w, float h, int cols, const StripStyle *s);

typedef struct {
    float art;            // art size (0: no art)
    float title_size, text_size;
    u32 title, artist, album, meta;
    u32 ph_bg, ph_fg, art_border;
    bool counter;         // "1 / 12"
} NowStyle;
// Art + title/artist/album (+ counter). Returns the y below the block.
float ui_now_info(struct App *app, float x, float y, float w, const NowStyle *s);

typedef struct {
    float h;
    u32 track, fill_top, fill_bottom, knob, knob_ring, time;
    float knob_r, time_size;
    bool remaining; // right label shows "-1:23" instead of the duration
} BarStyle;
// A progress bar with times under it; touch seeking uses theme_scrub on the same rect.
void ui_progress(struct App *app, float x, float y, float w, const BarStyle *s);
