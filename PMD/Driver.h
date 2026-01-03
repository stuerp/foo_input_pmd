
/** $VER: Driver.h (2023.10.29) Driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

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
        _Flags = 0;

        TieNotesTogether = false;
        _PreviousTimerACounter = 0;

        ::memset(omote_key, 0, _countof(omote_key));
        ::memset(ura_key, 0, _countof(ura_key));

        _AlgorithmAndFeedbackLoopsFM3 = 0;

        _LoopBegin = 0;
        _LoopEnd = 0;
        _LoopRelease = 0;

        _IsFMSlotDetuneSet = false;
        _Slot3Flags = 0;
        _FMSelector = 0;

        _CurrentChannel = 0;
        _IsVolumeBoostSet = 0;
        _LoopWork = 0;
        _HardwareLFOModulationMode = 0;
    }

public:
    uint8_t _Flags;

    bool TieNotesTogether;      // True if notes should be tied together ("&" command)
    int _PreviousTimerACounter;  // TimerACounter value at the previous interrupt

    int omote_key[3];           // FM keyondata table (=0)
    int ura_key[3];             // FM keyondata back (=0x100)

    int _AlgorithmAndFeedbackLoopsFM3; // Algorithm and feedback loops defined at the end of FM channel 3.

    int _LoopBegin;          // PCM loop begin address
    int _LoopEnd;            // PCM loop end address
    int _LoopRelease;        // PCM loop release address

    bool _IsFMSlotDetuneSet;    // Is FM3 using detune?
    int _Slot3Flags;            // Required sound effect mode flag for each FM3 slot
    int _FMSelector;         // Head (0x000) or tail (0x100)

    int _CurrentChannel;
    int _IsVolumeBoostSet;   // Set when a modified volume for the next note has been set.
    int _LoopWork;
    int _HardwareLFOModulationMode;         // Local LFO switch
};
#pragma warning(default: 4820)
