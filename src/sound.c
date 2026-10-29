/*
 * sound.c - sampled-sound channels mixed in software (see sound.h).
 *
 * Engine note: Engine() in GameTools.p sends flushCmd, rateCmd, ampCmd and
 * then SndPlay() every frame. Because flushCmd only drops queued commands,
 * the engine sample (snd 2000, recorded at a nominal 1890 Hz) plays
 * continuously and each frame just re-queues it and retunes it. The rate
 * argument, ((speed / 50) * 8000 + 1000), is a Fixed fraction of 22254.54 Hz,
 * so the engine plays at about 340 Hz at a standstill up to about 3056 Hz
 * flat out.
 */
#include "sound.h"
#include "resources.h"

#include <SDL.h>

#define MIX_RATE 44100
#define RATE_22KHZ 22254.54545  /* rate22khz, 0x56EE8BA3 */

typedef struct {
    const uint8_t *pcm;     /* 8-bit unsigned or 16-bit signed big-endian */
    int32_t frames;
    int channels, bits;
    double rate;            /* native sample rate */
} Sample;

typedef struct {
    Sample cur;
    bool playing;
    double pos, step;
    double rateOverride;    /* Hz; 0 = native */
    float amp;
    bool queued;
    Sample next;
    bool interpolate;
} Channel;

static Channel gChan[kNumChannels];
static SDL_AudioDeviceID gDev;

static bool ParseSnd(int id, Sample *s)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('s', 'n', 'd', ' '), id, &size);
    memset(s, 0, sizeof *s);
    if (!d || size < 6) return false;
    int format = rd16s(d);
    int32_t o;
    if (format == 1) {
        int mods = rd16s(d + 2);
        o = 4 + mods * 6;
    } else if (format == 2) {
        o = 4;
    } else {
        return false;
    }
    if (o + 2 > size) return false;
    int ncmds = rd16s(d + o);
    o += 2;
    uint32_t hdr = 0;
    for (int i = 0; i < ncmds; i++, o += 8) {
        if (o + 8 > size) return false;
        uint16_t cmd = rd16(d + o) & 0x7FFF;
        if (cmd == 80 || cmd == 81) { hdr = rd32(d + o + 4); break; }   /* soundCmd / bufferCmd */
    }
    if (!hdr || hdr + 22 > (uint32_t)size) return false;
    const uint8_t *h = d + hdr;
    uint8_t encode = h[20];
    s->rate = rd32(h + 8) / 65536.0;
    if (encode == 0x00) {
        s->frames = (int32_t)rd32(h + 4);
        s->channels = 1;
        s->bits = 8;
        s->pcm = h + 22;
    } else if (encode == 0xFF) {
        if (hdr + 64 > (uint32_t)size) return false;
        s->channels = (int)rd32(h + 4);
        s->frames = (int32_t)rd32(h + 22);
        s->bits = rd16s(h + 48);
        s->pcm = h + 64;
    } else {
        return false;   /* compressed headers are not used by the game */
    }
    if (s->channels < 1 || (s->bits != 8 && s->bits != 16)) return false;
    int32_t avail = (int32_t)((size - (s->pcm - d)) / (s->channels * s->bits / 8));
    if (s->frames > avail) s->frames = avail;
    return s->frames > 0 && s->rate > 0;
}

static inline float SampleAt(const Sample *s, int32_t i)
{
    if (s->bits == 8) {
        int v = 0;
        for (int c = 0; c < s->channels; c++) v += s->pcm[i * s->channels + c] - 128;
        return (float)v / (128.0f * s->channels);
    }
    int v = 0;
    for (int c = 0; c < s->channels; c++) v += rd16s(s->pcm + (i * s->channels + c) * 2);
    return (float)v / (32768.0f * s->channels);
}

static void UpdateStep(Channel *c)
{
    double rate = c->rateOverride > 0 ? c->rateOverride : c->cur.rate;
    c->step = rate / MIX_RATE;
}

static void Start(Channel *c, const Sample *s)
{
    c->cur = *s;
    c->pos = 0;
    c->playing = true;
    UpdateStep(c);
}

static void Mix(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    int16_t *out = (int16_t *)stream;
    int frames = len / 4;
    for (int f = 0; f < frames; f++) {
        float acc = 0;
        for (int k = 0; k < kNumChannels; k++) {
            Channel *c = &gChan[k];
            if (!c->playing) continue;
            int32_t i = (int32_t)c->pos;
            if (i >= c->cur.frames) {
                if (c->queued) {
                    c->queued = false;
                    Start(c, &c->next);
                    i = 0;
                } else {
                    c->playing = false;
                    continue;
                }
            }
            float v = SampleAt(&c->cur, i);
            if (c->interpolate && i + 1 < c->cur.frames) {
                float t = (float)(c->pos - i);
                v += (SampleAt(&c->cur, i + 1) - v) * t;
            }
            acc += v * c->amp;
            c->pos += c->step;
        }
        acc *= 0.6f;
        if (acc > 1) acc = 1;
        if (acc < -1) acc = -1;
        out[f * 2] = out[f * 2 + 1] = (int16_t)(acc * 32767);
    }
}

bool Snd_Init(void)
{
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof want);
    want.freq = MIX_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = Mix;
    gDev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    for (int i = 0; i < kNumChannels; i++) {
        gChan[i].amp = 1.0f;
        gChan[i].interpolate = i != kEngineChannel;   /* engine channel: initNoInterp */
    }
    if (!gDev) return false;
    SDL_PauseAudioDevice(gDev, 0);
    return true;
}

void Snd_Shutdown(void)
{
    if (gDev) SDL_CloseAudioDevice(gDev);
    gDev = 0;
}

void Snd_Quiet(int chan)
{
    if (!gDev || chan < 0 || chan >= kNumChannels) return;
    SDL_LockAudioDevice(gDev);
    gChan[chan].playing = false;
    SDL_UnlockAudioDevice(gDev);
}

void Snd_Flush(int chan)
{
    if (!gDev || chan < 0 || chan >= kNumChannels) return;
    SDL_LockAudioDevice(gDev);
    gChan[chan].queued = false;
    SDL_UnlockAudioDevice(gDev);
}

void Snd_Play(int chan, int id)
{
    Sample s;
    if (!gDev || chan < 0 || chan >= kNumChannels || !ParseSnd(id, &s)) return;
    SDL_LockAudioDevice(gDev);
    Channel *c = &gChan[chan];
    if (c->playing) {
        c->next = s;
        c->queued = true;
    } else {
        Start(c, &s);
    }
    SDL_UnlockAudioDevice(gDev);
}

void Snd_SetRate(int chan, long rate)
{
    if (!gDev || chan < 0 || chan >= kNumChannels) return;
    SDL_LockAudioDevice(gDev);
    gChan[chan].rateOverride = rate / 65536.0 * RATE_22KHZ;
    UpdateStep(&gChan[chan]);
    SDL_UnlockAudioDevice(gDev);
}

void Snd_SetAmp(int chan, int amp)
{
    if (!gDev || chan < 0 || chan >= kNumChannels) return;
    SDL_LockAudioDevice(gDev);
    gChan[chan].amp = (float)(amp < 0 ? 0 : amp > 255 ? 255 : amp) / 255.0f;
    SDL_UnlockAudioDevice(gDev);
}
