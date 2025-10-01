
// PMD driver (Based on PMDWin code by C60)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

void PMD::EffectMain(Channel * channel, int al)
{
    if (_State.SSGEffectFlag)
        return;    //  Mode without sound effects

    if (_Driver.UsePPS && (al & 0x80))
    {
        // Play PPS, if enabled.
        if (_Effect.Priority >= 2)
            return;

        _SSGChannels[2].MuteMask |= 0x02;

        _Effect.Priority = 1;
        _Effect.Number = al;

        int ah = _Effect.Flags;

        int bh = (ah & 0x01) ? channel->DetuneValue % 256 : 0; // Keep only the lower 8 bits.
        int bl = 15;

        if (ah & 0x02)
        {
            if (channel->Volume < 15)
                bl = channel->Volume;

            if (_State.FadeOutVolume)
                bl = (bl * (256 - _State.FadeOutVolume)) >> 8;
        }

        if (bl != 0)
        {
            bl ^= 0x0F;
            ah = 1;
            al &= 0x7F;

            _PPS->Play(al, bh, bl);
        }
    }
    else
    {
        _Effect.Number = al;

        if (_Effect.Priority <= SSGEffects[al].Priority)
        {
            if (_Driver.UsePPS)
                _PPS->Stop();

            _SSGChannels[2].MuteMask |= 0x02;

            StartEffect(SSGEffects[al].Data);

            _Effect.Priority = SSGEffects[al].Priority;
        }
    }
}

void PMD::PlayEffect()
{
    if (--_Effect.ToneSweepCounter)
        Sweep();
    else
        StartEffect(_Effect.Address);
}

void PMD::StartEffect(const int * si)
{
    int al = *si++;

    if (al != -1)
    {
        _Effect.ToneSweepCounter = al;

        int cl = *si;

        _OPNAW->SetReg(4, (uint32_t) (*si++)); // Set frequency

        int ch = *si;

        _OPNAW->SetReg(5, (uint32_t) (*si++)); // Set frequency

        _Effect.ToneSweepFrequency = (ch << 8) + cl;

        _State.OldSSGNoiseFrequency = _Effect.NoiseSweepFrequency = *si;

        _OPNAW->SetReg(6, (uint32_t) *si++); // Noise

        _OPNAW->SetReg(7, ((*si++ << 2) & 0x24) | (_OPNAW->GetReg(0x07) & 0xdb));

        _OPNAW->SetReg(10, (uint32_t) *si++); // Volume
        _OPNAW->SetReg(11, (uint32_t) *si++); // Envelope Frequency
        _OPNAW->SetReg(12, (uint32_t) *si++);
        _OPNAW->SetReg(13, (uint32_t) *si++); // Envelope Pattern

        _Effect.ToneSweepIncrement = *si++;
        _Effect.NoiseSweepIncrement = *si++;

        _Effect.NoiseSweepCounter = _Effect.NoiseSweepIncrement & 15;

        _Effect.Address = (int *) si;
    }
    else
        StopEffect();
}

void PMD::StopEffect()
{
    if (_Driver.UsePPS)
        _PPS->Stop();

    _OPNAW->SetReg(0x0a, 0x00);
    _OPNAW->SetReg(0x07, ((_OPNAW->GetReg(0x07)) & 0xdb) | 0x24);

    _Effect.Priority = 0;
    _Effect.Number = 0xFF;
}

/// <summary>
/// Performs the tone and noise sweep.
/// </summary>
void PMD::Sweep()
{
    // Perform tone sweep.
    {
        _Effect.ToneSweepFrequency += _Effect.ToneSweepIncrement;

        _OPNAW->SetReg(4, (uint32_t) LOBYTE(_Effect.ToneSweepFrequency));
        _OPNAW->SetReg(5, (uint32_t) HIBYTE(_Effect.ToneSweepFrequency));
    }

    // Perform noise sweep.
    {
        if (_Effect.NoiseSweepIncrement == 0)
            return; // No noise sweep

        if (--_Effect.NoiseSweepCounter)
            return;

        int dl = _Effect.NoiseSweepIncrement;

        _Effect.NoiseSweepCounter = dl & 15;
        _Effect.NoiseSweepFrequency += dl >> 4;

        _OPNAW->SetReg(6, (uint32_t) _Effect.NoiseSweepFrequency);

        _State.OldSSGNoiseFrequency = _Effect.NoiseSweepFrequency;
    }
}
