
/** $VER: Driver.h (2026.01.04) Driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#pragma once

const uint8_t DriverIdle = 0x00;
const uint8_t DriverStartRequested = 0x01;
const uint8_t DriverStopRequested = 0x02;

#pragma warning(disable: 4820) // x bytes padding added after last data member

class Driver
{
public:
    Driver()
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

        _PreviousTimerACounter = 0;

        ::memset(omote_key, 0, _countof(omote_key));
        ::memset(ura_key, 0, _countof(ura_key));

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

    int _PreviousTimerACounter;  // TimerACounter value at the previous interrupt

    int omote_key[3];           // FM keyondata table (=0)
    int ura_key[3];             // FM keyondata back (=0x100)

    int _AlgorithmAndFeedbackLoopsFM3; // Algorithm and feedback loops defined at the end of FM channel 3.

    int _LoopBegin;          // PCM loop begin address
    int _LoopEnd;            // PCM loop end address
    int _LoopRelease;        // PCM loop release address

    int _Slot3Flags;                    // Required sound effect mode flag for each FM3 slot
    int _FMSelector;                    // 0x000 or 0x100

    int _CurrentChannel;
    int _VolumeBoostCount;              // Set when a modified volume for the next note has been set.
    int _HardwareLFOModulationMode;     // Local LFO switch

    bool _IsTieSet;                     // True if notes should be tied together ("&" command)
    bool _IsFMSlotDetuneSet;            // Is FM3 using detune?
};

#pragma warning(default: 4820)
