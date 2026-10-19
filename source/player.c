#include "player.h"

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD // the 3DS's ARM11 has no NEON
#include "vendor/minimp3.h"

#define CHANNEL 0
#define NUM_BUFS 4
#define BUF_FRAMES 4096 // stereo frames per wave buffer (~93 ms at 44.1 kHz)
#define READ_SIZE (16 * 1024)
#define FFT_N 512

typedef enum { CMD_NONE, CMD_PLAY, CMD_STOP, CMD_SEEK, CMD_QUIT } Command;

struct Player {
    Thread thread;
    LightLock lock;
    LightEvent wake;     // signalled by commands and by the DSP when a buffer finishes
    // Command mailbox (guarded by lock)
    Command cmd;
    char cmd_path[1024];
    float cmd_seek;
    // State (guarded by lock)
    PlayerState state;
    float duration;
    int rate;
    bool finished;
    bool error;
    char error_msg[128];
    float volume;
    // Audio buffers (owned by the thread; read-only elsewhere for the spectrum)
    ndspWaveBuf wave[NUM_BUFS];
    s16 *pcm[NUM_BUFS];
    u64 buf_start[NUM_BUFS]; // first sample index of each buffer within the track
    volatile u64 played_base; // sample index when the current file/seek started
};

static void dsp_callback(void *arg) {
    Player *p = arg;
    LightEvent_Signal(&p->wake);
}

// ---------------------------------------------------------------------------------------------
// Decoder

typedef struct {
    FILE *f;
    mp3dec_t dec;
    unsigned char *in;
    size_t in_len;
    long data_start;   // after the ID3v2 tag
    long file_size;
    bool eof;
    s16 frame[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int frame_len;     // stereo frames left in `frame`
    int frame_pos;
    int hz;
    int channels;
} Decoder;

static void dec_close(Decoder *d) {
    if (d->f) fclose(d->f);
    free(d->in);
    memset(d, 0, sizeof *d);
}

static bool dec_fill(Decoder *d) {
    if (d->eof) return d->in_len > 0;
    size_t space = READ_SIZE - d->in_len;
    if (space) {
        size_t n = fread(d->in + d->in_len, 1, space, d->f);
        d->in_len += n;
        if (n < space) d->eof = true;
    }
    return d->in_len > 0;
}

// Decodes the next MP3 frame into d->frame (always stereo). Returns false at end of file.
static bool dec_next(Decoder *d) {
    mp3dec_frame_info_t info;
    s16 raw[MINIMP3_MAX_SAMPLES_PER_FRAME];
    for (int tries = 0; tries < 64; tries++) {
        if (d->in_len < 2048 && !d->eof) dec_fill(d);
        if (!d->in_len) return false;
        int samples = mp3dec_decode_frame(&d->dec, d->in, (int)d->in_len, raw, &info);
        size_t used = (size_t)info.frame_bytes;
        if (used > d->in_len) used = d->in_len;
        memmove(d->in, d->in + used, d->in_len - used);
        d->in_len -= used;
        if (samples > 0) {
            if (!d->hz) {
                d->hz = info.hz;
                d->channels = info.channels;
            }
            if (info.channels == 1) {
                for (int i = 0; i < samples; i++) d->frame[2 * i] = d->frame[2 * i + 1] = raw[i];
            } else {
                memcpy(d->frame, raw, samples * 2 * sizeof(s16));
            }
            d->frame_len = samples;
            d->frame_pos = 0;
            return true;
        }
        if (used == 0) {
            if (d->eof) return false;
            // No frame found in a full buffer: drop it and keep looking.
            if (d->in_len >= READ_SIZE) d->in_len = 0;
            dec_fill(d);
        }
    }
    return false;
}

static unsigned syncsafe(const unsigned char *b) { return (b[0] & 0x7F) << 21 | (b[1] & 0x7F) << 14 | (b[2] & 0x7F) << 7 | (b[3] & 0x7F); }

// Opens a file and reads the first frame; estimates the duration (Xing header if present).
static bool dec_open(Decoder *d, const char *path, float *duration, char *err, int errlen) {
    memset(d, 0, sizeof *d);
    d->f = fopen(path, "rb");
    if (!d->f) { snprintf(err, errlen, "Can't open file"); return false; }
    setvbuf(d->f, NULL, _IOFBF, 64 * 1024);
    d->in = malloc(READ_SIZE);
    if (!d->in) { snprintf(err, errlen, "Out of memory"); dec_close(d); return false; }
    fseek(d->f, 0, SEEK_END);
    d->file_size = ftell(d->f);
    fseek(d->f, 0, SEEK_SET);
    unsigned char h[10];
    if (fread(h, 1, 10, d->f) == 10 && memcmp(h, "ID3", 3) == 0) d->data_start = 10 + syncsafe(h + 6) + ((h[5] & 0x10) ? 10 : 0);
    fseek(d->f, d->data_start, SEEK_SET);
    mp3dec_init(&d->dec);

    // Look for a Xing/Info header in the first frame for an exact duration.
    *duration = 0;
    dec_fill(d);
    for (size_t i = 0; i + 120 < d->in_len && i < 4096; i++) {
        if ((!memcmp(d->in + i, "Xing", 4) || !memcmp(d->in + i, "Info", 4)) && (d->in[i + 7] & 1)) {
            unsigned frames = d->in[i + 8] << 24 | d->in[i + 9] << 16 | d->in[i + 10] << 8 | d->in[i + 11];
            mp3dec_frame_info_t info;
            s16 tmp[MINIMP3_MAX_SAMPLES_PER_FRAME];
            mp3dec_t probe;
            mp3dec_init(&probe);
            int s = mp3dec_decode_frame(&probe, d->in, (int)d->in_len, tmp, &info);
            if (info.hz > 0) *duration = (float)frames * (s > 0 ? s : 1152) / info.hz;
            break;
        }
    }
    if (!dec_next(d)) { snprintf(err, errlen, "Not a playable MP3"); dec_close(d); return false; }
    if (*duration <= 0) {
        mp3dec_frame_info_t info;
        (void)info;
        // Constant-bitrate estimate from the first frame's bitrate.
        int kbps = 0;
        mp3dec_t probe;
        mp3dec_init(&probe);
        s16 tmp[MINIMP3_MAX_SAMPLES_PER_FRAME];
        long pos = ftell(d->f);
        unsigned char buf[4096];
        fseek(d->f, d->data_start, SEEK_SET);
        size_t n = fread(buf, 1, sizeof buf, d->f);
        fseek(d->f, pos, SEEK_SET);
        mp3dec_frame_info_t fi;
        if (mp3dec_decode_frame(&probe, buf, (int)n, tmp, &fi) > 0 || fi.bitrate_kbps) kbps = fi.bitrate_kbps;
        if (kbps > 0) *duration = (float)(d->file_size - d->data_start) * 8.0f / (kbps * 1000.0f);
    }
    return true;
}

// Approximate seek: jump to a byte offset proportional to the fraction, then resync.
static void dec_seek(Decoder *d, float fraction) {
    if (fraction < 0) fraction = 0;
    if (fraction > 0.999f) fraction = 0.999f;
    long target = d->data_start + (long)((d->file_size - d->data_start) * fraction);
    fseek(d->f, target, SEEK_SET);
    d->in_len = 0;
    d->eof = false;
    d->frame_len = d->frame_pos = 0;
    mp3dec_init(&d->dec);
}

// ---------------------------------------------------------------------------------------------
// Thread

static void set_error(Player *p, const char *msg) {
    LightLock_Lock(&p->lock);
    p->error = true;
    snprintf(p->error_msg, sizeof p->error_msg, "%s", msg);
    p->state = PLAYER_STOPPED;
    LightLock_Unlock(&p->lock);
}

static void reset_channel(Player *p, int rate) {
    ndspChnWaveBufClear(CHANNEL);
    ndspChnReset(CHANNEL);
    ndspChnSetInterp(CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetRate(CHANNEL, (float)rate);
    ndspChnSetFormat(CHANNEL, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = {0};
    mix[0] = mix[1] = p->volume;
    ndspChnSetMix(CHANNEL, mix);
    for (int i = 0; i < NUM_BUFS; i++) p->wave[i].status = NDSP_WBUF_DONE;
}

// Fills one wave buffer; returns frames written (0 at end of file).
static int fill_buffer(Player *p, Decoder *d, int i, u64 *cursor) {
    s16 *out = p->pcm[i];
    int n = 0;
    while (n < BUF_FRAMES) {
        if (d->frame_pos >= d->frame_len && !dec_next(d)) break;
        int take = d->frame_len - d->frame_pos;
        if (take > BUF_FRAMES - n) take = BUF_FRAMES - n;
        memcpy(out + 2 * n, d->frame + 2 * d->frame_pos, take * 2 * sizeof(s16));
        d->frame_pos += take;
        n += take;
    }
    if (n == 0) return 0;
    DSP_FlushDataCache(out, n * 2 * sizeof(s16));
    p->wave[i].data_vaddr = out;
    p->wave[i].nsamples = n;
    p->buf_start[i] = *cursor;
    *cursor += n;
    ndspChnWaveBufAdd(CHANNEL, &p->wave[i]);
    return n;
}

static void thread_main(void *arg) {
    Player *p = arg;
    // The decoder state is large (and minimp3 itself uses ~16 KB of stack per frame), so it
    // lives on the heap; the thread gets a generous stack too.
    Decoder *dp = calloc(1, sizeof *dp);
    if (!dp) { set_error(p, "Out of memory"); return; }
#define d (*dp)
    bool active = false, draining = false;
    u64 cursor = 0;
    char err[128];

    for (;;) {
        LightEvent_Wait(&p->wake);

        LightLock_Lock(&p->lock);
        Command cmd = p->cmd;
        p->cmd = CMD_NONE;
        char path[1024];
        memcpy(path, p->cmd_path, sizeof path);
        float seek = p->cmd_seek;
        LightLock_Unlock(&p->lock);

        if (cmd == CMD_QUIT) break;
        if (cmd == CMD_STOP || cmd == CMD_PLAY) {
            ndspChnWaveBufClear(CHANNEL);
            dec_close(&d);
            active = draining = false;
            LightLock_Lock(&p->lock);
            p->state = PLAYER_STOPPED;
            LightLock_Unlock(&p->lock);
        }
        if (cmd == CMD_PLAY) {
            float duration = 0;
            if (!dec_open(&d, path, &duration, err, sizeof err)) {
                set_error(p, err);
                continue;
            }
            reset_channel(p, d.hz);
            cursor = 0;
            p->played_base = 0;
            LightLock_Lock(&p->lock);
            p->duration = duration;
            p->rate = d.hz;
            p->state = PLAYER_PLAYING;
            LightLock_Unlock(&p->lock);
            ndspChnSetPaused(CHANNEL, false);
            active = true;
        }
        if (cmd == CMD_SEEK && active) {
            ndspChnWaveBufClear(CHANNEL);
            for (int i = 0; i < NUM_BUFS; i++) p->wave[i].status = NDSP_WBUF_DONE;
            dec_seek(&d, seek);
            cursor = (u64)(seek * p->duration * d.hz);
            draining = false;
        }

        if (!active) continue;
        // Keep every free buffer full.
        for (int i = 0; i < NUM_BUFS && !draining; i++) {
            if (p->wave[i].status == NDSP_WBUF_DONE || p->wave[i].status == NDSP_WBUF_FREE) {
                if (fill_buffer(p, &d, i, &cursor) == 0) draining = true;
            }
        }
        if (draining) {
            bool busy = false;
            for (int i = 0; i < NUM_BUFS; i++)
                if (p->wave[i].status == NDSP_WBUF_QUEUED || p->wave[i].status == NDSP_WBUF_PLAYING) busy = true;
            if (!busy) {
                dec_close(&d);
                active = draining = false;
                LightLock_Lock(&p->lock);
                p->state = PLAYER_STOPPED;
                p->finished = true;
                LightLock_Unlock(&p->lock);
            }
        }
    }
    ndspChnWaveBufClear(CHANNEL);
    dec_close(&d);
#undef d
    free(dp);
}

// ---------------------------------------------------------------------------------------------
// API

Player *player_create(char *err, int errlen) {
    if (R_FAILED(ndspInit())) {
        snprintf(err, errlen, "Audio failed to start. Dump your DSP firmware to sdmc:/3ds/dspfirm.cdc (run the DSP1 homebrew once).");
        return NULL;
    }
    Player *p = calloc(1, sizeof *p);
    if (!p) { ndspExit(); snprintf(err, errlen, "Out of memory"); return NULL; }
    LightLock_Init(&p->lock);
    LightEvent_Init(&p->wake, RESET_ONESHOT);
    p->volume = 1.0f;
    for (int i = 0; i < NUM_BUFS; i++) {
        p->pcm[i] = linearAlloc(BUF_FRAMES * 2 * sizeof(s16));
        if (!p->pcm[i]) { player_destroy(p); snprintf(err, errlen, "Out of audio memory"); return NULL; }
        p->wave[i].status = NDSP_WBUF_DONE;
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetCallback(dsp_callback, p);
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    p->thread = threadCreate(thread_main, p, 128 * 1024, prio - 1, -2, false);
    if (!p->thread) { player_destroy(p); snprintf(err, errlen, "Couldn't start the audio thread"); return NULL; }
    return p;
}

static void player_send(Player *p, Command c) {
    LightLock_Lock(&p->lock);
    p->cmd = c;
    LightLock_Unlock(&p->lock);
    LightEvent_Signal(&p->wake);
}

void player_destroy(Player *p) {
    if (!p) return;
    if (p->thread) {
        player_send(p, CMD_QUIT);
        threadJoin(p->thread, U64_MAX);
        threadFree(p->thread);
    }
    ndspSetCallback(NULL, NULL);
    for (int i = 0; i < NUM_BUFS; i++)
        if (p->pcm[i]) linearFree(p->pcm[i]);
    free(p);
    ndspExit();
}

bool player_play(Player *p, const char *path) {
    LightLock_Lock(&p->lock);
    snprintf(p->cmd_path, sizeof p->cmd_path, "%s", path);
    p->finished = false;
    p->cmd = CMD_PLAY;
    LightLock_Unlock(&p->lock);
    LightEvent_Signal(&p->wake);
    return true;
}

void player_stop(Player *p) { player_send(p, CMD_STOP); }

void player_toggle_pause(Player *p) {
    LightLock_Lock(&p->lock);
    if (p->state == PLAYER_PLAYING) {
        p->state = PLAYER_PAUSED;
        ndspChnSetPaused(CHANNEL, true);
    } else if (p->state == PLAYER_PAUSED) {
        p->state = PLAYER_PLAYING;
        ndspChnSetPaused(CHANNEL, false);
    }
    LightLock_Unlock(&p->lock);
}

void player_seek(Player *p, float fraction) {
    LightLock_Lock(&p->lock);
    p->cmd_seek = fraction;
    p->cmd = CMD_SEEK;
    LightLock_Unlock(&p->lock);
    LightEvent_Signal(&p->wake);
}

void player_set_volume(Player *p, float volume) {
    if (volume < 0) volume = 0;
    if (volume > 1) volume = 1;
    p->volume = volume;
    float mix[12] = {0};
    mix[0] = mix[1] = volume;
    ndspChnSetMix(CHANNEL, mix);
}

PlayerState player_state(Player *p) {
    LightLock_Lock(&p->lock);
    PlayerState s = p->state;
    LightLock_Unlock(&p->lock);
    return s;
}

// Index of the buffer the DSP is playing now, or -1.
static int playing_buffer(Player *p) {
    int best = -1;
    for (int i = 0; i < NUM_BUFS; i++) {
        if (p->wave[i].status == NDSP_WBUF_PLAYING) return i;
        if (p->wave[i].status == NDSP_WBUF_QUEUED && (best < 0 || p->buf_start[i] < p->buf_start[best])) best = i;
    }
    return best;
}

float player_position(Player *p) {
    LightLock_Lock(&p->lock);
    int rate = p->rate;
    PlayerState s = p->state;
    LightLock_Unlock(&p->lock);
    if (s == PLAYER_STOPPED || rate <= 0) return 0;
    int b = playing_buffer(p);
    if (b < 0) return 0;
    u64 sample = p->buf_start[b] + (p->wave[b].status == NDSP_WBUF_PLAYING ? ndspChnGetSamplePos(CHANNEL) : 0);
    return (float)sample / rate;
}

float player_duration(Player *p) {
    LightLock_Lock(&p->lock);
    float d = p->duration;
    LightLock_Unlock(&p->lock);
    return d;
}

bool player_take_finished(Player *p) {
    LightLock_Lock(&p->lock);
    bool f = p->finished;
    p->finished = false;
    LightLock_Unlock(&p->lock);
    return f;
}

bool player_take_error(Player *p, char *msg, int len) {
    LightLock_Lock(&p->lock);
    bool e = p->error;
    if (e) snprintf(msg, len, "%s", p->error_msg);
    p->error = false;
    LightLock_Unlock(&p->lock);
    return e;
}

// In-place radix-2 FFT on FFT_N complex points.
static void fft(float *re, float *im) {
    for (int i = 1, j = 0; i < FFT_N; i++) {
        int bit = FFT_N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (int len = 2; len <= FFT_N; len <<= 1) {
        float ang = -2.0f * (float)M_PI / len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < FFT_N; i += len) {
            float cr = 1, ci = 0;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k, b = a + len / 2;
                float tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
                float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

void player_spectrum(Player *p, float *out, int bands) {
    memset(out, 0, bands * sizeof(float));
    if (player_state(p) != PLAYER_PLAYING) return;
    int b = playing_buffer(p);
    if (b < 0) return;
    int pos = p->wave[b].status == NDSP_WBUF_PLAYING ? (int)ndspChnGetSamplePos(CHANNEL) : 0;
    int n = (int)p->wave[b].nsamples;
    if (pos + FFT_N > n) pos = n - FFT_N;
    if (pos < 0) return;
    static float re[FFT_N], im[FFT_N];
    const s16 *pcm = p->pcm[b];
    for (int i = 0; i < FFT_N; i++) {
        float w = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / (FFT_N - 1)); // Hann window
        re[i] = (pcm[2 * (pos + i)] + pcm[2 * (pos + i) + 1]) * (0.5f / 32768.0f) * w;
        im[i] = 0;
    }
    fft(re, im);
    // Log-spaced bands from bin 1 to FFT_N/2.
    for (int k = 0; k < bands; k++) {
        int lo = (int)powf(FFT_N / 2.0f, (float)k / bands);
        int hi = (int)powf(FFT_N / 2.0f, (float)(k + 1) / bands);
        if (hi <= lo) hi = lo + 1;
        float m = 0;
        for (int i = lo; i < hi && i < FFT_N / 2; i++) {
            float mag = sqrtf(re[i] * re[i] + im[i] * im[i]);
            if (mag > m) m = mag;
        }
        float db = 20.0f * log10f(m + 1e-6f); // about -120..+30
        float v = (db + 50.0f) / 50.0f;
        out[k] = v < 0 ? 0 : v > 1 ? 1 : v;
    }
}
