// Drawing helpers on top of citro2d, shared by the themes.
#pragma once

#include <citro2d.h>
#include <stdbool.h>

#include "menu.h"

struct App;

// Same layout as C2D_Color32, but usable in constant initializers.
#define RGBA(r, g, b, a) ((u32)((r) & 0xFF) | ((u32)((g) & 0xFF) << 8) | ((u32)((b) & 0xFF) << 16) | ((u32)((a) & 0xFF) << 24))
#define RGB(r, g, b) RGBA((r), (g), (b), 0xFF)

enum { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };

void ui_init(struct App *app);
void ui_begin_frame(struct App *app);

// Text with the system font. `size` is a scale (0.5 ≈ 15 px tall). Returns the drawn width.
float ui_text(struct App *app, const char *s, float x, float y, float size, u32 color, int align);
// Like ui_text, but shortens with "..." to fit `maxw`.
float ui_text_fit(struct App *app, const char *s, float x, float y, float size, u32 color, int align, float maxw);
float ui_text_width(struct App *app, const char *s, float size);
// Line height for a size.
float ui_line_height(float size);

void ui_rect(float x, float y, float w, float h, u32 c);
void ui_vgrad(float x, float y, float w, float h, u32 top, u32 bottom);
void ui_hgrad(float x, float y, float w, float h, u32 left, u32 right);
void ui_round_rect(float x, float y, float w, float h, float r, u32 c);
void ui_round_vgrad(float x, float y, float w, float h, float r, u32 top, u32 bottom);
void ui_circle(float cx, float cy, float r, u32 c);
void ui_ring(float cx, float cy, float r_outer, float r_inner, u32 c, int segments);
void ui_line(float x0, float y0, float x1, float y1, float thickness, u32 c);
void ui_frame(float x, float y, float w, float h, float t, u32 c); // rectangle outline

// Transport symbols centered on (cx, cy), `s` ≈ height.
void ui_sym_play(float cx, float cy, float s, u32 c);
void ui_sym_pause(float cx, float cy, float s, u32 c);
void ui_sym_stop(float cx, float cy, float s, u32 c);
void ui_sym_next(float cx, float cy, float s, u32 c);
void ui_sym_prev(float cx, float cy, float s, u32 c);
void ui_sym_shuffle(float cx, float cy, float s, u32 c);
void ui_sym_repeat(float cx, float cy, float s, u32 c, bool one);
void ui_sym_battery(float x, float y, float w, float h, u32 c);
void ui_icon(MenuIcon icon, float cx, float cy, float s, u32 c);
void ui_sym_heart(float cx, float cy, float s, u32 c);
void ui_sym_heart_outline(float cx, float cy, float s, u32 c, u32 inner);
void ui_sym_volume(float cx, float cy, float s, u32 c);
void ui_sym_palette(float cx, float cy, float s, u32 c);

void ui_fmt_time(float seconds, char *buf, int len);
bool ui_hit(int px, int py, float x, float y, float w, float h);
u32 ui_mix(u32 a, u32 b, float t); // blend two colors

// Smoothly scrolls `m` so the cursor stays inside `visible` rows; returns the first row (fractional).
float ui_list_scroll(MenuLevel *m, int visible, float dt);
