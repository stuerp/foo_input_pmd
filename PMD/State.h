﻿#pragma once

#include "Channel.h"

#define MaxFMChannels           6
#define MaxSSGChannels          3
#define MaxADPCMTracks          1
#define MaxOPNARhythmTracks     1
#define MaxFMExtensionChannels  3
#define MaxRhythmTracks         1
#define MaxEffectTracks         1
#define MaxPPZChannels          8

#define MaxChannels             (MaxFMChannels + MaxSSGChannels + MaxADPCMTracks + MaxOPNARhythmTracks + MaxFMExtensionChannels + MaxRhythmTracks + MaxEffectTracks + MaxPPZChannels)

#pragma warning(disable: 4820) // x bytes padding added after last data member
class State
{
public:
    State()
    {
        Reset();
    }

    void Reset() noexcept
    {
        ::memset(this, 0, sizeof(*this));
    }

public:
    uint8_t * MData;            // Address of MML data + 1

    uint8_t * VData;            // Voice data
    uint8_t * EData;            // FM Effect data

    uint8_t * RhythmData;
    uint8_t * InstrumentDefinitions;    // Address of the FM instrument definitions, if any.

    uint8_t DummyRhythmData;

    uint16_t * RhythmDataTable; // Rhythm Data table

    bool UseRhythm;             // Use Rhythm sound source with K/R part.
    bool UseFM55kHzSynthesis;
    bool UseInterpolationPPZ;
    bool UseInterpolationPPS;
    bool UseInterpolationP86;

    Channel * Channel[MaxChannels];

    int RhythmMask;             // Rhythm sound source mask. Compatible with x8c/10h bit
    int RhythmVolume;           // Rhythm volume

    int FMVolumeDown, DefaultFMVolumeDown;
    int SSGVolumeDown, DefaultSSGVolumeDown;
    int ADPCMVolumeDown, DefaultADPCMVolumeDown;
    int RhythmVolumeDown, DefaultRhythmVolumeDown;
    int PPZVolumeDown, DefaultPPZVolumeDown;

    bool PMDB2CompatibilityMode, DefaultPMDB2CompatibilityMode;

    // MData characteristics
    uint8_t x68_flg;    // OPM flag

    int status;

    int LoopCount;

    int FadeOutSpeed;
    int FadeOutVolume;

    int BarLength;  // Time signature 4/4 = 96 (default); E.g. time signature 3/4 = 72
    int OpsCounter; // Shortest note counter

    int SSGEffectFlag; // SSG sound effect on/off flag (substituted by user)
    int SSGNoiseFrequency;
    int OldSSGNoiseFrequency;

    int PCMStart;
    int PCMStop;

    int tempo_d; // Tempo (TIMER-B)
    int tempo_d_push;  // Tempo (TIMER-B) / for saving

    int tempo_48; // Current tempo (value of clock = 48 t)
    int tempo_48_push;  // Current tempo (same as above / for saving)

    int kshot_dat; // SSG rhythm shot flag
    int fade_stop_flag;  // Flag for whether to MSTOP after Fadeout
    int pcm_gs_flag;  // ADPCM use permission flag (0 allows)

    int slot_detune1;  // FM3 Slot Detune値 slot1
    int slot_detune2;  // FM3 Slot Detune値 slot2
    int slot_detune3;  // FM3 Slot Detune値 slot3
    int slot_detune4;  // FM3 Slot Detune値 slot4

    int fadeout_flag;  // When calling Fade from inside 1
    int BarCounter;
    int port22h; // Last value output to OPN-PORT 22H (hlfo)

    int rshot_dat; // Rhythm shot flag
    int RhythmPanAndVolume[6]; // Pan value and volume

    int rshot_bd; // Rhythm shot inc flag (BD)
    int rshot_sd; // Rhythm shot inc flag (SD)
    int rshot_sym; // Rhythm shot inc flag (CYM)
    int rshot_hh; // Rhythm shot inc flag (HH)
    int rshot_tom; // Rhythm shot inc flag (TOM)
    int rshot_rim; // Rhythm shot inc flag (RIM)

    int rdump_bd; // Rhythm dump inc flag (BD)
    int rdump_sd; // Rhythm dump inc flag (SD)
    int rdump_sym; // Rhythm dump inc flag (CYM)
    int rdump_hh; // Rhythm dump inc flag (HH)
    int rdump_tom; // Rhythm dump inc flag (TOM)
    int rdump_rim; // Rhythm dump inc flag (RIM)

    uint32_t FMChannel3Mode; // ch3 Mode

    bool IsTimerABusy;
    int TimerATime;

    bool IsTimerBBusy;
    int TimerBTempo;  // Current value of TimerB (= ff_tempo during ff)

    uint32_t OPNARate; // PCM output frequency (11k, 22k, 44k, 55k)
    uint32_t PPZRate; // PPZ output frequency

    bool IsUsingP86;

    int FadeOutSpeedHQ; // Fadeout (High Sound Quality) speed (fadeout at > 0)
};
#pragma warning(default: 4820) // x bytes padding added after last data member
