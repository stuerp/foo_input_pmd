
/** $VER: State.h (2026.01.04) Driver state (Based on PMDWin code by C60 / Masahiro Kajihara) **/

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

#define FREQUENCY_55_5K         55555
#define FREQUENCY_55_4K         55466
#define FREQUENCY_48_0K         48000
#define FREQUENCY_44_1K         44100
#define FREQUENCY_22_0K         22050
#define FREQUENCY_11_0K         11025

#define PPZ8_i0                 44100
#define PPZ8_i1                 33080
#define PPZ8_i2                 22050
#define PPZ8_i3                 16540
#define PPZ8_i4                 11025
#define PPZ8_i5                  8270
#define PPZ8_i6                  5513
#define PPZ8_i7                  4135

#define DEFAULT_REG_WAIT        15000

#define OPNAClock       (3993600 * 2) // in Hz, Clock rate of the FM Sound Source based (on the YM2203), ~4MHz, fixed sample clock division of 72

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
    uint8_t * MData;                    // Address of MML data + 1

    uint8_t * VData;                    // Voice data
    uint8_t * EData;                    // FM Effect data

    uint8_t * RhythmData;
    uint8_t * InstrumentDefinitions;    // Address of the FM instrument definitions, if any.

    uint8_t DummyRhythmData;

    uint16_t * RhythmDataTable;         // Rhythm Data table

    bool UseInterpolation;
    bool UseInterpolationPPZ;
    bool UseInterpolationPPS;
    bool UseInterpolationP86;

    channel_t * _Channels[MaxChannels];

    int32_t FMVolumeAdjust, DefaultFMVolumeAdjust;
    int32_t SSGVolumeAdjust, DefaultSSGVolumeAdjust;
    int32_t ADPCMVolumeAdjust, DefaultADPCMVolumeAdjust;
    int32_t _RhythmVolumeAdjust, DefaultRhythmVolumeAdjust;
    int32_t PPZVolumeAdjust, DefaultPPZVolumeAdjust;

    int32_t FMSlot1Detune;
    int32_t FMSlot2Detune;
    int32_t FMSlot3Detune;
    int32_t FMSlot4Detune;

    bool PMDB2CompatibilityMode, DefaultPMDB2CompatibilityMode;

    // MData characteristics
    uint8_t x68_flg;                    // OPM flag

    int32_t Status;                     // Unused

    int32_t LoopCount;

    int32_t FadeOutSpeed;
    bool IsFadeOutSpeedSet;
    int32_t FadeOutSpeedHQ;                 // Fadeout speed (High Sound Quality)
    int32_t _FadeOutVolume;

    int32_t BarLength;                      // Time signature 4/4 = 96 (default); E.g. time signature 3/4 = 72
    int32_t OpsCounter;                     // Shortest note counter

    int32_t _PCMBegin;
    int32_t _PCMEnd;

    int32_t Tempo;                          // Timer B Tempo
    int32_t TempoPush;                      // Timer B Tempo (for saving)

    int32_t MetronomeTempo;                 // Duration of a quarter note (in ticks)
    int32_t MetronomeTempoPush;             // Duration of a quarter note (in ticks) (for saving)

    bool StopAfterFadeout;

    bool UseRhythmChannel;              // Use the PMD rhythm channel to sequence drums (by default SSG channel 3) instead of the OPNA's Rhythm Sound Source (RSS).
//  int32_t pcm_gs_flag;                    // ADPCM use permission flag (0 allows)

    int32_t BarCounter;

    int32_t _RhythmPanAndVolumes[6];        // Pan value and volume

    int32_t RhythmBassDrumOn;
    int32_t RhythmSnareDrumOn;
    int32_t RhythmCymbalOn;
    int32_t RhythmHiHatOn;
    int32_t RhythmTomDrumOn;
    int32_t RhythmRimShotOn;

    int32_t RhythmBassDrumOff;
    int32_t RhythmSnareDrumOff;
    int32_t RhythmCymbalOff;
    int32_t RhythmHiHatOff;
    int32_t RhythmTomDrumOff;
    int32_t RhythmRimShotOff;

    uint32_t FMChannel3Mode;

    int32_t TimerACounter;

    int32_t TimerBTempo;                    // Current value of TimerB (= ff_tempo during ff)

    uint32_t OPNASampleRate;            // PCM output frequency (11k, 22k, 44k, 55k)
    uint32_t PPZSampleRate;             // PPZ output frequency
};
#pragma warning(default: 4820) // x bytes padding added after last data member
