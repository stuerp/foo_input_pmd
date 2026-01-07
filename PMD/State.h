
/** $VER: State.h (2026.01.07) Driver state (Based on PMDWin code by C60 / Masahiro Kajihara) **/

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
#define MaxADPCMChannels        1
#define MaxRhythmChannels       1
#define MaxFMExtensionChannels  3
#define MaxDummyChannels        1
#define MaxSSGEffectChannels    1
#define MaxPPZ8Channels         8

#define MaxChannels             (MaxFMChannels + MaxSSGChannels + MaxADPCMChannels + MaxRhythmChannels + MaxFMExtensionChannels + MaxDummyChannels + MaxSSGEffectChannels + MaxPPZ8Channels)

#pragma warning(disable: 4820) // x bytes padding added after last data member

class state_t
{
public:
    state_t()
    {
        Reset();
    }

    /// <summary>
    /// Initializes the playing state.
    /// </summary>
    void Initialize() noexcept
    {
        console::printf("state_t::Initialize");

        Reset();

        _FMVolumeAdjust = 0;
        _FMVolumeAdjustDefault = 0;

        _SSGVolumeAdjust = 0;
        _SSGVolumeAdjustDefault = 0;

        _ADPCMVolumeAdjust =
        _ADPCMVolumeAdjustDefault = 0;

        _RhythmVolumeAdjust = 0;
        _RhythmVolumeAdjustDefault = 0;

        _PPZ8VolumeAdjust = 0;
        _PPZ8VolumeAdjustDefault = 0;

        // Initialize the counters.
        _RhythmBassDrumOn = 0;
        _RhythmSnareDrumOn = 0;
        _RhythmCymbalOn = 0;
        _RhythmHiHatOn = 0;
        _RhythmTomDrumOn = 0;
        _RhythmRimShotOn = 0;

        // Initialize the counters.
        _RhythmBassDrumOff = 0;
        _RhythmSnareDrumOff = 0;
        _RhythmCymbalOff = 0;
        _RhythmHiHatOff = 0;
        _RhythmTomDrumOff = 0;
        _RhythmRimShotOff = 0;

        // Set to true if PMD86's PCM is compatible with PMDB2.COM (For songs targetting the Speakboard or compatible sound board which has a YM2608 with ADPCM functionality enabled).
        _IsCompatibleWithPMDB2 =
        _IsCompatibleWithPMDB2Default = false;
    }

    void Reset() noexcept
    {
        console::printf("state_t::Reset");

        ::memset(this, 0, sizeof(*this));
    }

public:
    uint8_t * _MData;                   // Address of MML data + 1
    uint8_t * _VData;                   // Voice data
    uint8_t * _EData;                   // FM Effect data

    uint8_t * _InstrumentData;          // Address of the FM instrument definitions, if any.

    uint16_t * _RhythmDataTable;        // Rhythm Data table
    uint8_t * _RhythmData;              // Address of the rhythm definitions, if any.

    channel_t * _Channels[MaxChannels];

    int32_t _FMVolumeAdjust, _FMVolumeAdjustDefault;
    int32_t _SSGVolumeAdjust, _SSGVolumeAdjustDefault;
    int32_t _ADPCMVolumeAdjust, _ADPCMVolumeAdjustDefault;
    int32_t _RhythmVolumeAdjust, _RhythmVolumeAdjustDefault;
    int32_t _PPZ8VolumeAdjust, _PPZ8VolumeAdjustDefault;

    bool _IsCompatibleWithPMDB2, _IsCompatibleWithPMDB2Default;

    int32_t _FMSlot1Detune;
    int32_t _FMSlot2Detune;
    int32_t _FMSlot3Detune;
    int32_t _FMSlot4Detune;

    uint32_t _FMChannel3Mode;

    int32_t _LoopCount;

    int32_t _FadeOutSpeed;
    int32_t _FadeOutSpeedHQ;                // Fadeout speed (High Sound Quality)
    int32_t _FadeOutVolume;
    bool _IsFadeOutSpeedSet;

    int32_t _BarLength;                     // Time signature 4/4 = 96 (default); E.g. time signature 3/4 = 72
    int32_t _BarCounter;
    int32_t _OpsCounter;                    // Position in the current bar

    int32_t _PCMBegin;
    int32_t _PCMEnd;

    int32_t _Tempo;                         // Timer B Tempo
    int32_t _TempoDefault;                  // Timer B Tempo (for saving)

    int32_t _MetronomeTempo;                // Duration of a quarter note (in ticks)
    int32_t _MetronomeTempoDefault;         // Duration of a quarter note (in ticks) (for saving)

    bool _UseRhythmChannel;              // Use the PMD rhythm channel to sequence drums (by default SSG channel 3) instead of the OPNA's Rhythm Sound Source (RSS).
//  int32_t pcm_gs_flag;                    // ADPCM use permission flag (0 allows)

    int32_t _RhythmPanAndVolumes[6];        // Pan value and volume

    int32_t _RhythmBassDrumOn;
    int32_t _RhythmSnareDrumOn;
    int32_t _RhythmCymbalOn;
    int32_t _RhythmHiHatOn;
    int32_t _RhythmTomDrumOn;
    int32_t _RhythmRimShotOn;

    int32_t _RhythmBassDrumOff;
    int32_t _RhythmSnareDrumOff;
    int32_t _RhythmCymbalOff;
    int32_t _RhythmHiHatOff;
    int32_t _RhythmTomDrumOff;
    int32_t _RhythmRimShotOff;

    int32_t _TimerACounter;
};
#pragma warning(default: 4820) // x bytes padding added after last data member
