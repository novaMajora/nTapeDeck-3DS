// A theme draws both screens and may claim touch and button input.
#pragma once

#include <3ds.h>
#include <stdbool.h>

struct App;

typedef struct {
    bool down, held, up;  // touch began / is held / ended this frame
    int x, y;             // current (or last, on `up`) position
    int start_x, start_y; // where the touch began
    int prev_x, prev_y;   // position on the previous frame
} TouchInput;

typedef struct Theme {
    const char *name;
    const char *description;
    u32 accent, swatch;                    // picker colors (and the theme button)
    int album_cols;                        // Albums view: 0 list, 1 carousel, N grid columns
    void (*enter)(struct App *app);        // optional: theme selected
    void (*draw_splash)(struct App *app);  // optional "Press A" title screen (top)
    void (*draw_top)(struct App *app);
    void (*draw_bottom)(struct App *app);
    void (*touch)(struct App *app, const TouchInput *t);
    // Optional: handle buttons first; return true to skip the default handling.
    bool (*buttons)(struct App *app, u32 down, u32 held);
} Theme;

int theme_count(void);
const Theme *theme_get(int i);

extern const Theme THEME_TAPEDECK;   // default
extern const Theme THEME_WAVE;
extern const Theme THEME_CLICKWHEEL;
extern const Theme THEME_PORTABLE;
extern const Theme THEME_MEDIABLUE;
extern const Theme THEME_DASHBOARD;

// Shared touch helper: a progress bar that seeks when a touch that began on it is released.
bool theme_scrub(struct App *app, const TouchInput *t, float x, float y, float w, float h);
// Shared touch helper: was this a tap (released near where it began) inside the rect?
bool theme_tap(const TouchInput *t, float x, float y, float w, float h);
