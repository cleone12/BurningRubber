/*
 * sound.h - the Sound Manager calls Burning Rubber makes, on an SDL mixer.
 *
 * The game opens three sampled-sound channels: one for the engine and two
 * that sound effects alternate between. 'snd ' resources are format 1 or 2
 * with standard or extended sampled-sound headers.
 */
#ifndef SOUND_H
#define SOUND_H

#include "mac.h"

enum { kEngineChannel = 0, kEffectAChannel = 1, kEffectBChannel = 2, kNumChannels = 3 };

bool Snd_Init(void);
void Snd_Shutdown(void);

/* SndDoImmediate(chan, quietCmd) */
void Snd_Quiet(int chan);
/* SndDoImmediate(chan, flushCmd): drop the queued sound, keep playing. */
void Snd_Flush(int chan);
/* SndPlay(chan, 'snd ' id, async): plays now if the channel is idle,
 * otherwise queues behind the sound that is playing. */
void Snd_Play(int chan, int id);
/* rateCmd: playback rate of the channel in samples per second. */
void Snd_SetRate(int chan, long rateHz);
/* ampCmd: 0..255 */
void Snd_SetAmp(int chan, int amp);

#endif
