
// PMD driver (Based on PMDWin code by C60)

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4710 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "PMD.h"

#include "Utility.h"
#include "Table.h"

#include "OPNAW.h"
#include "PPZ.h"
#include "PPS.h"
#include "P86.h"

void PMD::LFOMain(Channel * channel)
{
    if (channel->speed != 1)
    {
        if (channel->speed != 255)
            channel->speed--;

        return;
    }

    channel->speed = channel->speed2;

    int al, ax;

    if (channel->LFOWaveform == 0 || channel->LFOWaveform == 4 || channel->LFOWaveform == 5)
    {
        // Triangle wave
        if (channel->LFOWaveform == 5)
            ax = ::abs(channel->step) * channel->step;
        else
            ax = channel->step;

        if ((channel->lfodat += ax) == 0)
            SetStepUsingMDValue(channel);

        al = channel->time;

        if (al != 255)
        {
            if (--al == 0)
            {
                al = channel->time2;

                if (channel->LFOWaveform != 4)
                    al += al;  // When lfowave = 0 or 5, double the time when inverting.

                channel->time = al;
                channel->step = -channel->step;

                return;
            }
        }

        channel->time = al;
    }
    else
    if (channel->LFOWaveform == 2)
    {
        // Square wave
        channel->lfodat = (channel->step * channel->time);

        SetStepUsingMDValue(channel);

        channel->step = -channel->step;

    }
    else
    if (channel->LFOWaveform == 6)
    {
        // One shot
        if (channel->time)
        {
            if (channel->time != 255)
                channel->time--;

            channel->lfodat += channel->step;
        }
    }
    else
    if (channel->LFOWaveform == 1)
    {
        // Sawtooth wave
        channel->lfodat += channel->step;

        al = channel->time;

        if (al != -1)
        {
            al--;

            if (al == 0)
            {
                channel->lfodat = -channel->lfodat;

                SetStepUsingMDValue(channel);

                al = (channel->time2) * 2;
            }
        }

        channel->time = al;
    }
    else
    {
        // Random wave
        ax = abs(channel->step) * channel->time;

        channel->lfodat = ax - rnd(ax * 2);

        SetStepUsingMDValue(channel);
    }
}

int PMD::StartLFO(Channel * channel, int al)
{
    int ah = al & 0x0F;

    if (ah == 0x0C)
    {
        al = channel->DefaultNote;
        ah = al & 0x0F;
    }

    channel->DefaultNote = al;

    if (ah == 0x0F)
    {
        StopLFO(channel);

        return al;
    }

    channel->porta_num = 0; // Initialize the portamento.

    if (_Driver.TieMode & 0x01)
        StopLFO(channel);
    else
        InitializeLFO(channel);

    return al;
}

// Entry for SSG/PCM sound source
int PMD::StartPCMLFO(Channel * channel, int al)
{
    int ah = al & 0x0F;

    if (ah == 0x0C)
    {
        al = channel->DefaultNote;
        ah = al & 0x0F;
    }

    channel->DefaultNote = al;

    if (ah == 0x0F)
    {
        SSGPCMSoftwareEnvelope(channel);
        StopLFO(channel);

        return al;
    }

    channel->porta_num = 0; // Initialize the portamento.

    if (_Driver.TieMode & 1)
    {
        SSGPCMSoftwareEnvelope(channel); // Only execute the software envelope once when preceded by a "&" command (Tie).
        StopLFO(channel);

        return al;
    }

    //  Initialize the software envelope.
    if (channel->envf != -1)
    {
        channel->envf = 0;
        channel->ExtendedAttackLevel = 0;
        channel->AttackDuration = channel->ExtendedAttackDuration;

        if (channel->AttackDuration == 0)
        {
            channel->envf = 1;  // ATTACK=0 ... ｽｸﾞ Decay ﾆ
            channel->ExtendedAttackLevel = channel->DecayDepth;
        }

        channel->SustainRate = channel->ExtendedSustainRate;
        channel->ReleaseRate = channel->ExtendedReleaseRate;

        InitializeLFO(channel);
    }
    else
    {
        // Extended SSG envelope processing
        channel->ExtendedAttackDuration = channel->AttackDuration - 16;

        if (channel->DecayDepth < 16)
            channel->ExtendedDecayDepth = (channel->DecayDepth - 16) * 2;
        else
            channel->ExtendedDecayDepth = channel->DecayDepth - 16;

        if (channel->SustainRate < 16)
            channel->ExtendedSustainRate = (channel->SustainRate - 16) * 2;
        else
            channel->ExtendedSustainRate = channel->SustainRate - 16;

        channel->ExtendedReleaseRate = (channel->ReleaseRate) * 2 - 16;
        channel->ExtendedAttackLevel = channel->AttackLevel;
        channel->ExtendedCount = 1;

        ExtendedSSGPCMSoftwareEnvelopeMain(channel);

        InitializeLFO(channel);
    }

    return al;
}

void PMD::StopLFO(Channel * channel)
{
    if ((channel->LFOSwitch & 0x03) != 0)
        lfo(channel); // Only execute the LFO once when preceded by a "&" command (Tie).

    if ((channel->LFOSwitch & 0x30) != 0)
    {
        // Only execute the LFO once when preceded by a "&" command (Tie).
        SwapLFO(channel);

        lfo(channel);

        SwapLFO(channel);
    }
}

// Initialize LFO
void PMD::InitializeLFO(Channel * channel)
{
    channel->HardwareLFODelayCounter = channel->HardwareLFODelay;

    if (channel->HardwareLFODelay != 0)
        _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + _Driver.FMSelector + 0xB4 - 1), (uint32_t) (channel->PanAndVolume & 0xC0));

    channel->sdelay_c = channel->sdelay;

    if ((channel->LFOSwitch & 0x03) != 0)
    {   // LFO not used
        if ((channel->LFOSwitch & 4) == 0)
            lfoinit_main(channel); // Is keyon asynchronous?

        lfo(channel);
    }

    if ((channel->LFOSwitch & 0x30) != 0)
    {   // LFO not used
        if ((channel->LFOSwitch & 0x40) == 0)
        {
            SwapLFO(channel); // Is keyon asynchronous?

            lfoinit_main(channel);

            SwapLFO(channel);
        }

        SwapLFO(channel);

        lfo(channel);

        SwapLFO(channel);
    }
}

void PMD::lfoinit_main(Channel * channel)
{
    channel->lfodat = 0;
    channel->delay = channel->delay2;
    channel->speed = channel->speed2;
    channel->step = channel->step2;
    channel->time = channel->time2;
    channel->mdc = channel->mdc2;

    if (channel->LFOWaveform == 2 || channel->LFOWaveform == 3) // Square wave or random wave?
        channel->speed = 1;  // Make the LFO apply immediately after the delay
    else
        channel->speed++; // Otherwise, +1 to the speed value immediately after the delay
}

//  ＬＦＯ処理
//    Don't Break cl
//    output    cy=1  変化があった
int PMD::lfo(Channel * channel)
{
    return SetSSGLFO(channel);
}

int PMD::SetSSGLFO(Channel * channel)
{
    if (channel->delay)
    {
        channel->delay--;

        return 0;
    }

    int ax, ch;

    if (channel->extendmode & 0x02)
    {
        // Match with TimerA? If not, unconditionally process lfo
        ch = _State.TimerATime - _Driver.OldTimerATime;

        if (ch == 0)
            return 0;

        ax = channel->lfodat;

        for (; ch > 0; ch--)
            LFOMain(channel);
    }
    else
    {
        ax = channel->lfodat;

        LFOMain(channel);
    }

    return (ax == channel->lfodat) ? 0 : 1;
}

uint8_t * PMD::lfoswitch(Channel * channel, uint8_t * si)
{
    int al = *si++;

    if (al & 0xf8)
        al = 1;

    al &= 7;

    channel->LFOSwitch = (channel->LFOSwitch & 0xf8) | al;

    lfoinit_main(channel);

    return si;
}

// Change STEP value by value of MD command
void PMD::SetStepUsingMDValue(Channel * channel)
{
    if (--channel->MDepthSpeedA)
        return;

    channel->MDepthSpeedA = channel->MDepthSpeedB;

    if (channel->mdc == 0)
        return;

    if (channel->mdc <= 127)
        channel->mdc--;

    int al;

    if (channel->step < 0)
    {
        al = channel->MDepth - channel->step;

        if (al < 128)
            channel->step = -al;
        else
            channel->step = (channel->MDepth < 0) ? 0 : -127;
    }
    else
    {
        al = channel->step + channel->MDepth;

        if (al < 128)
            channel->step = al;
        else
            channel->step = (channel->MDepth < 0) ? 0 : 127;
    }
}
