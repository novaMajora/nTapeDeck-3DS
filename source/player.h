// MP3 playback: a decoder thread feeding the DSP through ndsp.
#pragma once

#include <stdbool.h>

typedef struct Player Player;

typedef enum { PLAYER_STOPPED, PLAYER_PLAYING, PLAYER_PAUSED } PlayerState;

// Returns NULL (and fills `err`) when the DSP can't start, e.g. the DSP firmware dump
// (sdmc:/3ds/dspfirm.cdc) is missing.
Player *player_create(char *err, int errlen);
void player_destroy(Player *p);

bool player_play(Player *p, const char *path);  // starts a new file from the beginning
void player_stop(Player *p);
void player_toggle_pause(Player *p);
void player_seek(Player *p, float fraction);    // 0..1
void player_set_volume(Player *p, float volume);

PlayerState player_state(Player *p);
float player_position(Player *p);  // seconds
float player_duration(Player *p);  // seconds (estimated for VBR files without a Xing header)
// True once after the current track played to its end.
bool player_take_finished(Player *p);
// True once after a file failed to decode; the message describes why.
bool player_take_error(Player *p, char *msg, int len);

// Magnitude spectrum (0..1) of what is playing now, `bands` log-spaced bands.
void player_spectrum(Player *p, float *out, int bands);
