#pragma once

#pragma warning(disable: 4820) // x bytes padding added after last data member
class Driver
{
public:
    Driver()
    {
        ::memset(this, 0, sizeof(*this));
    }

public:
    int CurrentChannel;
    int TieMode;        // 1: Tie notes together ("&" command)
    int volpush_flag;   // Flag for next one note volume down (1 : voldown)
    int FMSelector;     // Head (0x000) or tail (0x100)
    int omote_key[3];   // FM keyondata table (=0)
    int ura_key[3];     // FM keyondata back (=0x100)
    int loop_work;      // Loop Work
    bool UsePPS;

    int PCMRepeat1;
    int PCMRepeat2;
    int PCMRelease;

    int OldTimerATime;  // TimerATime value at the previous interrupt
    int music_flag; // B0: Next MSTART 1: Next MSTOP Flag
    int slotdetune_flag; // Are you using FM3 Slot Detune?
    int slot3_flag; // Required sound effect mode flag for each FM3 slot
    int fm3_alg_fb; // alg/fb of the tone defined at the end of FM3ch
    int af_check; // Whether to set alg/fb of FM3ch flag
    int lfo_switch; // Local LFO switch
};
#pragma warning(default: 4820)
