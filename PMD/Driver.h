#pragma once

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

        LoopRelease = 0x8000;
    }

private:
    void Reset() noexcept
    {
        ::memset(this, 0, sizeof(*this));
    }

public:
    bool TieNotesTogether;  // True if notes should be tied together ("&" command)
    int OldTimerATime;      // TimerATime value at the previous interrupt

    int omote_key[3];       // FM keyondata table (=0)

    int ura_key[3];         // FM keyondata back (=0x100)

    int AlgorithmAndFeedbackLoopsFM3; // Algorithm and feedback loops defined at the end of FM channel 3.

    int LoopBegin;          // PCM loop begin address
    int LoopEnd;            // PCM loop end address
    int LoopRelease;        // PCM loop release address

    int slotdetune_flag;    // Are you using FM3 Slot Detune?
    int slot3_flag;         // Required sound effect mode flag for each FM3 slot
    int FMSelector;         // Head (0x000) or tail (0x100)

    int CurrentChannel;
    int IsVolumeBoostSet;   // Set when a modified volume for the next note has been set.
    int loop_work;          // Loop Work
    bool UsePPS;
    int music_flag;         // B0: Next MSTART 1: Next MSTOP Flag
    int ModulationMode;         // Local LFO switch
};
#pragma warning(default: 4820)
