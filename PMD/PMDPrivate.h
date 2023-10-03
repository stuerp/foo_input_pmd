
// Based on PMDWin code by C60

#pragma once

#define ERR_SUCCESS                 0

#define ERR_OPEN_FAILED             1
#define ERR_UNKNOWN_FORMAT          2
#define ERR_ALREADY_LOADED          3
#define ERR_OUT_OF_MEMORY           4

#define ERR_WRONG_PARTNO           32
#define ERR_NOT_MASKED             33 // The specified part is not masked.
#define ERR_EFFECT_USED            34 // The mask cannot be operated because it is being used by sound effects.
#define ERR_MUSIC_STOPPED          99 // You performed a mask operation while the song was stopped.

#define ERR_UNKNOWN               999

#define SOUND_55K           55555
#define SOUND_55K_2         55466
#define SOUND_48K           48000
#define SOUND_44K           44100
#define SOUND_22K           22050
#define SOUND_11K           11025

#define PPZ8_i0             44100
#define PPZ8_i1             33080
#define PPZ8_i2             22050
#define PPZ8_i3             16540
#define PPZ8_i4             11025
#define PPZ8_i5              8270
#define PPZ8_i6              5513
#define PPZ8_i7              4135

#define DEFAULT_REG_WAIT    15000
#define MAX_PCMDIR             64
#define MAX_MEMO             1024

#define OPNAClock   (3993600 * 2)

#define MaxFMChannels           6
#define MaxSSGChannels          3
#define MaxADPCMTracks          1
#define MaxOPNARhythmTracks     1
#define MaxFMExtensionChannels  3
#define MaxRhythmTracks         1
#define MaxEffectTracks         1
#define MaxPPZChannels          8
#define MaxChannels             (MaxFMChannels + MaxSSGChannels + MaxADPCMTracks + MaxOPNARhythmTracks + MaxFMExtensionChannels + MaxRhythmTracks + MaxEffectTracks + MaxPPZChannels)

#include "State.h"
