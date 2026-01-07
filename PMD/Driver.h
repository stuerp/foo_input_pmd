
/** $VER: Driver.h (2026.01.07) Driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#pragma once

const uint8_t DriverIdle = 0x00;
const uint8_t DriverStartRequested = 0x01;
const uint8_t DriverStopRequested = 0x02;

#pragma warning(disable: 4820) // x bytes padding added after last data member

class driver_t
{
public:
    driver_t()
    {
        Reset();
    }

    void Initialize() noexcept
    {
        Reset();

        _LoopRelease = 0x8000;
    }

private:
    void Reset() noexcept
    {
        _Flags     = 0x00;
        _LoopCheck = 0x00;

        _OldTimerACounter = 0;

        _AlgorithmAndFeedbackLoopsFM3 = 0;

        _LoopBegin = 0;
        _LoopEnd = 0;
        _LoopRelease = 0;

        _Slot3Flags = 0;
        _FMSelector = 0x000;

        _CurrentChannel = 0;
        _VolumeBoostCount = 0;
        _HardwareLFOModulationMode = 0;

        _IsTieSet = false;
        _IsFMSlotDetuneSet = false;
    }

public:
    uint8_t _Flags;
    uint8_t _LoopCheck;

    int32_t _OldTimerACounter;              // TimerA counter value at the previous interrupt

    int32_t _AlgorithmAndFeedbackLoopsFM3;  // Algorithm and feedback loops defined at the end of FM channel 3.

    int32_t _LoopBegin;                     // PCM loop begin address
    int32_t _LoopEnd;                       // PCM loop end address
    int32_t _LoopRelease;                   // PCM loop release address

    int32_t _Slot3Flags;                    // Required sound effect mode flag for each FM3 slot
    int32_t _FMSelector;                    // 0x000 or 0x100

    int32_t _CurrentChannel;
    int32_t _VolumeBoostCount;              // Set when a modified volume for the next note has been set.
    int32_t _HardwareLFOModulationMode;

    bool _IsTieSet;                         // True if notes should be tied together ("&" command)
    bool _IsFMSlotDetuneSet;                // Is FM3 using detune?
};

#pragma warning(default: 4820)
