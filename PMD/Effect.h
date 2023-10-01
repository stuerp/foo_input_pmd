
// Based on PMDWin code by C60

#pragma once

class Effect
{
public:
    Effect()
    {
        ::memset(this, 0, sizeof(*this));

        Number = 0xFF;
    }

public:
    int * Address;

    int ToneSweepFrequency;
    int ToneSweepIncrement;
    int ToneSweepCounter;

    int NoiseSweepFrequency;
    int NoiseSweepIncrement;
    int NoiseSweepCounter;

    int Priority;
    int Number;
    int PreviousNumber;

    int Flags; // 0x01: Pitch correction / 0x02: Volume correction
};
