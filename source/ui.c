#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"

#define Z 0.5f

void ui_init(App *app) {
    app->textbuf = C2D_TextBufNew(8192);
    app->measurebuf = C2D_TextBufNew(1024);
    app->font = NULL; // system font
}

void ui_begin_frame(App *app) { C2D_TextBufClear(app->textbuf); }

float ui_line_height(float size) { return 30.0f * size; }

float ui_text_width(App *app, const char *s, float size) {
    C2D_TextBufClear(app->measurebuf);
    C2D_Text t;
    C2D_TextFontParse(&t, app->font, app->measurebuf, s);
    float w = 0, h = 0;
    C2D_TextGetDimensions(&t, size, size, &w, &h);
    return w;
}

float ui_text(App *app, const char *s, float x, float y, float size, u32 color, int align) {
    if (!s || !*s) return 0;
    C2D_Text t;
    if (!C2D_TextFontParse(&t, app->font, app->textbuf, s)) return 0;
    C2D_TextOptimize(&t);
    float w = 0, h = 0;
    C2D_TextGetDimensions(&t, size, size, &w, &h);
    float dx = align == ALIGN_CENTER ? x - w / 2 : align == ALIGN_RIGHT ? x - w : x;
    C2D_DrawText(&t, C2D_WithColor, dx, y, Z, size, size, color);
    return w;
}

float ui_text_fit(App *app, const char *s, float x, float y, float size, u32 color, int align, float maxw) {
    if (!s || !*s) return 0;
    if (ui_text_width(app, s, size) <= maxw) return ui_text(app, s, x, y, size, color, align);
    char buf[256];
    size_t n = strlen(s);
    if (n > sizeof buf - 4) n = sizeof buf - 4;
    // Binary search on the byte length, kept at UTF-8 character boundaries.
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = (lo + hi + 1) / 2;
        while (mid > 0 && ((unsigned char)s[mid] & 0xC0) == 0x80) mid--;
        if (mid <= lo) { break; }
        memcpy(buf, s, mid);
        memcpy(buf + mid, "...", 4);
        if (ui_text_width(app, buf, size) <= maxw) lo = mid; else hi = mid - 1;
    }
    while (lo > 0 && ((unsigned char)s[lo] & 0xC0) == 0x80) lo--;
    memcpy(buf, s, lo);
    memcpy(buf + lo, "...", 4);
    return ui_text(app, buf, x, y, size, color, align);
}

void ui_rect(float x, float y, float w, float h, u32 c) { C2D_DrawRectSolid(x, y, Z, w, h, c); }
void ui_vgrad(float x, float y, float w, float h, u32 top, u32 bottom) { C2D_DrawRectangle(x, y, Z, w, h, top, top, bottom, bottom); }
void ui_hgrad(float x, float y, float w, float h, u32 left, u32 right) { C2D_DrawRectangle(x, y, Z, w, h, left, right, left, right); }

void ui_circle(float cx, float cy, float r, u32 c) { C2D_DrawCircleSolid(cx, cy, Z, r, c); }

// Quarter-circle fan centered at (cx, cy) from angle a0, colored per vertex by `col(y)`.
typedef struct { float y0, h; u32 top, bottom; } VGrad;

static u32 vcol(const VGrad *g, float y) {
    if (g->h <= 0) return g->top;
    return ui_mix(g->top, g->bottom, (y - g->y0) / g->h);
}

static void corner(float cx, float cy, float r, float a0, const VGrad *g) {
    const int n = 6;
    for (int i = 0; i < n; i++) {
        float a1 = a0 + (float)M_PI / 2 * i / n, a2 = a0 + (float)M_PI / 2 * (i + 1) / n;
        float x1 = cx + cosf(a1) * r, y1 = cy + sinf(a1) * r, x2 = cx + cosf(a2) * r, y2 = cy + sinf(a2) * r;
        C2D_DrawTriangle(cx, cy, vcol(g, cy), x1, y1, vcol(g, y1), x2, y2, vcol(g, y2), Z);
    }
}

static void round_rect_grad(float x, float y, float w, float h, float r, u32 top, u32 bottom) {
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 0.5f) { ui_vgrad(x, y, w, h, top, bottom); return; }
    VGrad g = {y, h, top, bottom};
    u32 a = vcol(&g, y + r), b = vcol(&g, y + h - r);
    // Center column (full height), then the two side strips between the corners.
    C2D_DrawRectangle(x + r, y, Z, w - 2 * r, h, top, top, bottom, bottom);
    C2D_DrawRectangle(x, y + r, Z, r, h - 2 * r, a, a, b, b);
    C2D_DrawRectangle(x + w - r, y + r, Z, r, h - 2 * r, a, a, b, b);
    corner(x + r, y + r, r, (float)M_PI, &g);
    corner(x + w - r, y + r, r, (float)M_PI * 1.5f, &g);
    corner(x + w - r, y + h - r, r, 0, &g);
    corner(x + r, y + h - r, r, (float)M_PI / 2, &g);
}

void ui_round_rect(float x, float y, float w, float h, float r, u32 c) { round_rect_grad(x, y, w, h, r, c, c); }

void ui_round_vgrad(float x, float y, float w, float h, float r, u32 top, u32 bottom) { round_rect_grad(x, y, w, h, r, top, bottom); }

void ui_ring(float cx, float cy, float ro, float ri, u32 c, int segs) {
    for (int i = 0; i < segs; i++) {
        float a0 = 2 * (float)M_PI * i / segs, a1 = 2 * (float)M_PI * (i + 1) / segs;
        float x0o = cx + cosf(a0) * ro, y0o = cy + sinf(a0) * ro, x1o = cx + cosf(a1) * ro, y1o = cy + sinf(a1) * ro;
        float x0i = cx + cosf(a0) * ri, y0i = cy + sinf(a0) * ri, x1i = cx + cosf(a1) * ri, y1i = cy + sinf(a1) * ri;
        C2D_DrawTriangle(x0o, y0o, c, x1o, y1o, c, x0i, y0i, c, Z);
        C2D_DrawTriangle(x1o, y1o, c, x1i, y1i, c, x0i, y0i, c, Z);
    }
}

void ui_line(float x0, float y0, float x1, float y1, float t, u32 c) { C2D_DrawLine(x0, y0, c, x1, y1, c, t, Z); }

void ui_frame(float x, float y, float w, float h, float t, u32 c) {
    ui_rect(x, y, w, t, c);
    ui_rect(x, y + h - t, w, t, c);
    ui_rect(x, y, t, h, c);
    ui_rect(x + w - t, y, t, h, c);
}

static void tri(float x0, float y0, float x1, float y1, float x2, float y2, u32 c) { C2D_DrawTriangle(x0, y0, c, x1, y1, c, x2, y2, c, Z); }

void ui_sym_play(float cx, float cy, float s, u32 c) { tri(cx - s * 0.4f, cy - s / 2, cx - s * 0.4f, cy + s / 2, cx + s * 0.5f, cy, c); }

void ui_sym_pause(float cx, float cy, float s, u32 c) {
    ui_rect(cx - s * 0.4f, cy - s / 2, s * 0.28f, s, c);
    ui_rect(cx + s * 0.12f, cy - s / 2, s * 0.28f, s, c);
}

void ui_sym_stop(float cx, float cy, float s, u32 c) { ui_rect(cx - s * 0.42f, cy - s * 0.42f, s * 0.84f, s * 0.84f, c); }

void ui_sym_next(float cx, float cy, float s, u32 c) {
    tri(cx - s * 0.5f, cy - s / 2, cx - s * 0.5f, cy + s / 2, cx, cy, c);
    tri(cx, cy - s / 2, cx, cy + s / 2, cx + s * 0.45f, cy, c);
    ui_rect(cx + s * 0.4f, cy - s / 2, s * 0.12f, s, c);
}

void ui_sym_prev(float cx, float cy, float s, u32 c) {
    tri(cx + s * 0.5f, cy - s / 2, cx + s * 0.5f, cy + s / 2, cx, cy, c);
    tri(cx, cy - s / 2, cx, cy + s / 2, cx - s * 0.45f, cy, c);
    ui_rect(cx - s * 0.52f, cy - s / 2, s * 0.12f, s, c);
}

void ui_sym_shuffle(float cx, float cy, float s, u32 c) {
    float h = s / 2, t = s * 0.1f;
    ui_line(cx - h, cy - h * 0.6f, cx + h * 0.6f, cy + h * 0.6f, t, c);
    ui_line(cx - h, cy + h * 0.6f, cx + h * 0.6f, cy - h * 0.6f, t, c);
    tri(cx + h, cy + h * 0.6f, cx + h * 0.4f, cy + h * 0.2f, cx + h * 0.4f, cy + h, c);
    tri(cx + h, cy - h * 0.6f, cx + h * 0.4f, cy - h * 0.2f, cx + h * 0.4f, cy - h, c);
}

void ui_sym_repeat(float cx, float cy, float s, u32 c, bool one) {
    float w = s, h = s * 0.6f, t = s * 0.1f;
    ui_frame(cx - w / 2, cy - h / 2, w, h, t, c);
    tri(cx + w * 0.1f, cy - h / 2 - s * 0.18f, cx + w * 0.1f, cy - h / 2 + s * 0.18f, cx + w * 0.3f, cy - h / 2, c);
    if (one) ui_rect(cx - t / 2, cy - h * 0.3f, t, h * 0.6f, c);
}

void ui_sym_battery(float x, float y, float w, float h, u32 c) {
    ui_frame(x, y, w, h, 1, c);
    ui_rect(x + w, y + h * 0.3f, 2, h * 0.4f, c);
    u8 level = 0;
    MCUHWC_GetBatteryLevel(&level);
    float f = level / 100.0f;
    ui_rect(x + 2, y + 2, (w - 4) * f, h - 4, c);
}

void ui_icon(MenuIcon icon, float cx, float cy, float s, u32 c) {
    float h = s / 2;
    switch (icon) {
    case ICON_FOLDER:
        ui_rect(cx - h, cy - h * 0.5f, s, h * 1.3f, c);
        ui_rect(cx - h, cy - h * 0.8f, h * 0.8f, h * 0.4f, c);
        break;
    case ICON_SONG: // eighth note
        ui_circle(cx - h * 0.3f, cy + h * 0.55f, h * 0.38f, c);
        ui_rect(cx + h * 0.02f, cy - h, h * 0.16f, h * 1.55f, c);
        tri(cx + h * 0.1f, cy - h, cx + h * 0.75f, cy - h * 0.45f, cx + h * 0.1f, cy - h * 0.45f, c);
        break;
    case ICON_ARTIST:
        ui_circle(cx, cy - h * 0.4f, h * 0.42f, c);
        ui_round_rect(cx - h * 0.8f, cy + h * 0.1f, h * 1.6f, h * 0.9f, h * 0.4f, c);
        break;
    case ICON_ALBUM:
        ui_circle(cx, cy, h, c);
        ui_circle(cx, cy, h * 0.25f, RGBA(0, 0, 0, 120));
        break;
    case ICON_SETTINGS:
        ui_ring(cx, cy, h * 0.85f, h * 0.45f, c, 16);
        for (int i = 0; i < 6; i++) {
            float a = i * (float)M_PI / 3;
            ui_line(cx + cosf(a) * h * 0.6f, cy + sinf(a) * h * 0.6f, cx + cosf(a) * h, cy + sinf(a) * h, h * 0.35f, c);
        }
        break;
    case ICON_PLAYING: ui_sym_play(cx, cy, s * 0.9f, c); break;
    case ICON_SHUFFLE: ui_sym_shuffle(cx, cy, s * 0.9f, c); break;
    case ICON_REPEAT: ui_sym_repeat(cx, cy, s * 0.9f, c, false); break;
    case ICON_PLAYLIST: // list lines with bullets
        for (int i = 0; i < 3; i++) {
            float y = cy - h * 0.6f + i * h * 0.6f;
            ui_rect(cx - h, y - h * 0.1f, h * 0.22f, h * 0.22f, c);
            ui_rect(cx - h * 0.6f, y - h * 0.08f, h * 1.6f, h * 0.18f, c);
        }
        break;
    case ICON_GENRE: // a guitar: body, neck, head
        ui_circle(cx - h * 0.35f, cy + h * 0.4f, h * 0.45f, c);
        ui_circle(cx - h * 0.05f, cy + h * 0.1f, h * 0.32f, c);
        ui_line(cx, cy + h * 0.05f, cx + h * 0.8f, cy - h * 0.75f, h * 0.18f, c);
        ui_rect(cx + h * 0.65f, cy - h, h * 0.35f, h * 0.35f, c);
        break;
    case ICON_HEART: ui_sym_heart(cx, cy, s, c); break;
    case ICON_CLOCK:
        ui_ring(cx, cy, h, h * 0.78f, c, 24);
        ui_line(cx, cy, cx, cy - h * 0.55f, h * 0.16f, c);
        ui_line(cx, cy, cx + h * 0.4f, cy + h * 0.1f, h * 0.16f, c);
        break;
    case ICON_VOLUME: ui_sym_volume(cx, cy, s, c); break;
    case ICON_THEME: ui_sym_palette(cx, cy, s, c); break;
    case ICON_INFO:
        ui_circle(cx, cy, h, c);
        ui_rect(cx - h * 0.12f, cy - h * 0.15f, h * 0.24f, h * 0.7f, RGBA(0, 0, 0, 160));
        ui_circle(cx, cy - h * 0.45f, h * 0.14f, RGBA(0, 0, 0, 160));
        break;
    case ICON_NONE: break;
    }
}

void ui_sym_heart(float cx, float cy, float s, u32 c) {
    float h = s / 2;
    ui_circle(cx - h * 0.45f, cy - h * 0.25f, h * 0.5f, c);
    ui_circle(cx + h * 0.45f, cy - h * 0.25f, h * 0.5f, c);
    tri(cx - h * 0.93f, cy - h * 0.1f, cx + h * 0.93f, cy - h * 0.1f, cx, cy + h * 0.85f, c);
}

void ui_sym_heart_outline(float cx, float cy, float s, u32 c, u32 inner) {
    ui_sym_heart(cx, cy, s, c);
    ui_sym_heart(cx, cy - s * 0.02f, s * 0.68f, inner);
}

void ui_sym_volume(float cx, float cy, float s, u32 c) {
    float h = s / 2;
    ui_rect(cx - h, cy - h * 0.3f, h * 0.45f, h * 0.6f, c);
    tri(cx - h * 0.6f, cy - h * 0.3f, cx, cy - h * 0.8f, cx, cy + h * 0.8f, c);
    tri(cx - h * 0.6f, cy - h * 0.3f, cx, cy + h * 0.8f, cx - h * 0.6f, cy + h * 0.3f, c);
    for (int i = 1; i <= 2; i++) {
        float r = h * 0.3f * i + h * 0.15f;
        ui_line(cx + h * 0.15f + r * 0.6f, cy - r * 0.8f, cx + h * 0.15f + r, cy, h * 0.12f, c);
        ui_line(cx + h * 0.15f + r, cy, cx + h * 0.15f + r * 0.6f, cy + r * 0.8f, h * 0.12f, c);
    }
}

void ui_sym_palette(float cx, float cy, float s, u32 c) {
    float h = s / 2;
    ui_circle(cx, cy, h, c);
    ui_circle(cx + h * 0.35f, cy + h * 0.4f, h * 0.28f, RGBA(0, 0, 0, 150)); // thumb hole
    static const u32 dots[4] = {RGB(230, 60, 50), RGB(250, 190, 40), RGB(60, 170, 80), RGB(50, 120, 230)};
    for (int i = 0; i < 4; i++) {
        float a = -2.6f + i * 0.75f;
        ui_circle(cx + cosf(a) * h * 0.55f, cy + sinf(a) * h * 0.55f, h * 0.17f, dots[i]);
    }
}

void ui_fmt_time(float seconds, char *buf, int len) {
    if (seconds < 0 || seconds != seconds) seconds = 0;
    int s = (int)seconds;
    if (s >= 3600) snprintf(buf, len, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
    else snprintf(buf, len, "%d:%02d", s / 60, s % 60);
}

bool ui_hit(int px, int py, float x, float y, float w, float h) { return px >= x && px < x + w && py >= y && py < y + h; }

u32 ui_mix(u32 a, u32 b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    u32 out = 0;
    for (int sh = 0; sh < 32; sh += 8) {
        float ca = (a >> sh) & 0xFF, cb = (b >> sh) & 0xFF;
        out |= (u32)(ca + (cb - ca) * t + 0.5f) << sh;
    }
    return out;
}

float ui_list_scroll(MenuLevel *m, int visible, float dt) {
    float target = m->scroll;
    if (m->cursor < target + 1) target = m->cursor - 1;
    if (m->cursor > target + visible - 2) target = m->cursor - visible + 2;
    float max = m->count - visible;
    if (target > max) target = max;
    if (target < 0) target = 0;
    float k = dt * 14.0f;
    if (k > 1) k = 1;
    m->scroll += (target - m->scroll) * k;
    if (m->scroll < 0) m->scroll = 0;
    return m->scroll;
}
