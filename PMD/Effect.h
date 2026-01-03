
// Based on PMDWin code by C60

#pragma once

#pragma warning(disable: 4820) // x bytes padding added after last data member

class effect_t
{
public:
    effect_t() noexcept
    {
        _Address = nullptr;

        _TonePeriod  = 0;
        _TonePeriodIncrement  = 0;
        _ToneCounter = 0;

        _NoisePeriod  = 0;
        _NoisePeriodIncrement = 0;
        _NoiseCounter = 0;

        _Priority = 0;
        _Number   = 0xFF;

        Flags = 0;
    }

public:
    int * _Address;

    int _TonePeriod;
    int _TonePeriodIncrement;
    int _ToneCounter;

    int _NoisePeriod;
    int _NoisePeriodIncrement;
    int _NoiseCounter;

    int _Priority;
    int _Number;

    int Flags; // 0x01: Pitch correction / 0x02: Volume correction
};
