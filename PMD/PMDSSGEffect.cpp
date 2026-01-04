
// PMD driver (Based on PMDWin code by C60)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

void PMD::SSGEffectMain(channel_t * channel, int effectNumber)
{
    if (_UsePPSForDrums && (effectNumber & 0x80))
    {
        if (_SSGEffect._Priority >= 2)
            return;

        _SSGChannels[2].PartMask |= 0x02;

        _SSGEffect._Priority = 1;
        _SSGEffect._Number = effectNumber;

        const int Flags = _SSGEffect.Flags;

        const int Shift = (Flags & 0x01) ? channel->_DetuneValue % 256 : 0; // Keep only the lower 8 bits.
        int Volume = 15;

        if (Flags & 0x02)
        {
            if (channel->_Volume < 15)
                Volume = channel->_Volume;

            if (_State._FadeOutVolume)
                Volume = (Volume * (256 - _State._FadeOutVolume)) >> 8;
        }

        if (Volume != 0)
        {
            effectNumber &= 0x7F;
            Volume ^= 0x0F;

            _PPS->Start(effectNumber, Shift, Volume);
        }
    }
    else
    {
        _SSGEffect._Number = effectNumber;

        if (_SSGEffect._Priority <= SSGEffects[effectNumber].Priority)
        {
            if (_UsePPSForDrums)
                _PPS->Stop();

            _SSGChannels[2].PartMask |= 0x02;

            SSGStartEffect(SSGEffects[effectNumber].Data);

            _SSGEffect._Priority = SSGEffects[effectNumber].Priority;
        }
    }
}

/// <summary>
/// Plays an SSG effect.
/// </summary>
void PMD::SSGPlayEffect() noexcept
{
    if (--_SSGEffect._ToneCounter)
        SSGSweep();
    else
        SSGStartEffect(_SSGEffect._Address);
}

/// <summary>
/// Starts to play an effect on the SSG.
/// </summary>
void PMD::SSGStartEffect(const int * si)
{
    int ToneCounter = *si++;

    if (ToneCounter != -1)
    {
        _SSGEffect._ToneCounter = ToneCounter;

        int cl = *si++;
        int ch = *si++;

        _OPNAW->SetReg(0x04, (uint32_t) cl); // Channel C Tone Period (Fine Tune)
        _OPNAW->SetReg(0x05, (uint32_t) cl); // Channel C Tone Period (Coarse Tune)

        _SSGEffect._TonePeriod  = (ch << 8) + cl;
        _SSGEffect._NoisePeriod = *si++;

        _OPNAW->SetReg(0x06, (uint32_t) _SSGEffect._NoisePeriod); // Noise Period

        _OPNAW->SetReg(0x07, ((*si++ << 2) & 0x24) | (_OPNAW->GetReg(0x07) & 0xDB)); // Enable

        _OPNAW->SetReg(0x0A, (uint32_t) *si++); // Channel C Amplitude / Mode
        _OPNAW->SetReg(0x0B, (uint32_t) *si++); // Envelope Period (Fine Tune)
        _OPNAW->SetReg(0x0C, (uint32_t) *si++); // Envelope Period (Coarse Tune)
        _OPNAW->SetReg(0x0D, (uint32_t) *si++); // Envelope Shape Cycle

        _SSGEffect._TonePeriodIncrement  = *si++;
        _SSGEffect._NoisePeriodIncrement = *si++;

        _SSGEffect._NoiseCounter = _SSGEffect._NoisePeriodIncrement & 15;

        _SSGEffect._Address = (int *) si;

        _OldSSGNoiseFrequency = _SSGEffect._NoisePeriod;
    }
    else
        SSGStopEffect();
}

/// <summary>
/// Stops the SSG from playing an effect.
/// </summary>
void PMD::SSGStopEffect()
{
    if (_UsePPSForDrums)
        _PPS->Stop();

    _OPNAW->SetReg(0x0A, 0x00);                                     // Channel C Amplitude / Mode
    _OPNAW->SetReg(0x07, ((_OPNAW->GetReg(0x07)) & 0xDB) | 0x24);   // Disable

    _SSGEffect._Priority = 0;
    _SSGEffect._Number = 0xFF;
}

/// <summary>
/// Performs the tone and noise sweep.
/// </summary>
void PMD::SSGSweep()
{
    // Sweep the tone.
    {
        _SSGEffect._TonePeriod += _SSGEffect._TonePeriodIncrement;

        _OPNAW->SetReg(0x04, (uint32_t) LOBYTE(_SSGEffect._TonePeriod)); // Channel C Tone Period (Fine Tune)
        _OPNAW->SetReg(0x05, (uint32_t) HIBYTE(_SSGEffect._TonePeriod)); // Channel C Tone Perdio (Coarse Tune)
    }

    // Sweep the noise.
    {
        if (_SSGEffect._NoisePeriodIncrement == 0)
            return;

        if (--_SSGEffect._NoiseCounter)
            return;

        const int Increment = _SSGEffect._NoisePeriodIncrement;

        _SSGEffect._NoiseCounter = Increment & 0x0F;
        _SSGEffect._NoisePeriod += Increment >> 4;

        _OPNAW->SetReg(0x06, (uint32_t) _SSGEffect._NoisePeriod); // Noise Period

        _OldSSGNoiseFrequency = _SSGEffect._NoisePeriod;
    }
}
