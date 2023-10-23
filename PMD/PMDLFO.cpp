
// $VER: PMDLFO.cpp (2023.10.23) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

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
    if (channel->LFO1Speed1 != 1)
    {
        if (channel->LFO1Speed1 != 255)
            channel->LFO1Speed1--;

        return;
    }

    channel->LFO1Speed1 = channel->LFO1Speed2;

    int al, ax;

    if (channel->LFO1Waveform == 0 || channel->LFO1Waveform == 4 || channel->LFO1Waveform == 5)
    {
        // Triangle wave
        if (channel->LFO1Waveform == 5)
            ax = ::abs(channel->LFO1Step1) * channel->LFO1Step1;
        else
            ax = channel->LFO1Step1;

        if ((channel->LFO1Data += ax) == 0)
            SetStepUsingMDValue(channel);

        al = channel->LFO1Time1;

        if (al != 255)
        {
            if (--al == 0)
            {
                al = channel->LFO1Time2;

                if (channel->LFO1Waveform != 4)
                    al += al;  // When lfowave = 0 or 5, double the time when inverting.

                channel->LFO1Time1 = al;
                channel->LFO1Step1 = -channel->LFO1Step1;

                return;
            }
        }

        channel->LFO1Time1 = al;
    }
    else
    if (channel->LFO1Waveform == 2)
    {
        // Square wave
        channel->LFO1Data = (channel->LFO1Step1 * channel->LFO1Time1);

        SetStepUsingMDValue(channel);

        channel->LFO1Step1 = -channel->LFO1Step1;

    }
    else
    if (channel->LFO1Waveform == 6)
    {
        // One shot
        if (channel->LFO1Time1)
        {
            if (channel->LFO1Time1 != 255)
                channel->LFO1Time1--;

            channel->LFO1Data += channel->LFO1Step1;
        }
    }
    else
    if (channel->LFO1Waveform == 1)
    {
        // Sawtooth wave
        channel->LFO1Data += channel->LFO1Step1;

        al = channel->LFO1Time1;

        if (al != -1)
        {
            al--;

            if (al == 0)
            {
                channel->LFO1Data = -channel->LFO1Data;

                SetStepUsingMDValue(channel);

                al = (channel->LFO1Time2) * 2;
            }
        }

        channel->LFO1Time1 = al;
    }
    else
    {
        // Random wave
        ax = abs(channel->LFO1Step1) * channel->LFO1Time1;

        channel->LFO1Data = ax - rnd(ax * 2);

        SetStepUsingMDValue(channel);
    }
}

int PMD::StartLFO(Channel * channel, int al)
{
    int ah = al & 0x0F;

    if (ah == 0x0C)
    {
        al = channel->DefaultTone;
        ah = al & 0x0F;
    }

    channel->DefaultTone = al;

    if (ah == 0x0F)
    {
        StopLFO(channel);

        return al;
    }

    channel->Portamento = 0; // Initialize the portamento.

    if (_Driver.TieNotesTogether)
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
        al = channel->DefaultTone;
        ah = al & 0x0F;
    }

    channel->DefaultTone = al;

    if (ah == 0x0F)
    {
        SSGPCMSoftwareEnvelope(channel);
        StopLFO(channel);

        return al;
    }

    channel->Portamento = 0; // Initialize the portamento.

    if (_Driver.TieNotesTogether)
    {
        SSGPCMSoftwareEnvelope(channel); // Only execute the software envelope once when preceded by a "&" command (Tie).
        StopLFO(channel);

        return al;
    }

    //  Initialize the software envelope.
    if (channel->SSGEnvelopFlag != -1)
    {
        channel->SSGEnvelopFlag = 0;
        channel->ExtendedAttackLevel = 0;
        channel->AttackDuration = channel->ExtendedAttackDuration;

        if (channel->AttackDuration == 0)
        {
            channel->SSGEnvelopFlag = 1;  // ATTACK=0 ... ｽｸﾞ Decay ﾆ
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
    if ((channel->ModulationMode & 0x03) != 0)
        SetLFO(channel); // Only execute the LFO once when preceded by a "&" command (Tie).

    if ((channel->ModulationMode & 0x30) != 0)
    {
        // Only execute the LFO once when preceded by a "&" command (Tie).
        SwapLFO(channel);

        SetLFO(channel);

        SwapLFO(channel);
    }
}

/// <summary>
/// Initializes the LFO.
/// </summary>
void PMD::InitializeLFO(Channel * channel)
{
    channel->HardwareLFODelayCounter = channel->HardwareLFODelay;

    if (channel->HardwareLFODelay != 0)
        _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + _Driver.FMSelector + 0xB4 - 1), (uint32_t) (channel->PanAndVolume & 0xC0));

    channel->SlotDelayCounter = channel->SlotDelay;

    if ((channel->ModulationMode & 0x03) != 0)
    {   // LFO not used
        if ((channel->ModulationMode & 0x04) == 0)
            InitializeLFOMain(channel); // Is keyon asynchronous?

        SetLFO(channel);
    }

    if ((channel->ModulationMode & 0x30) != 0)
    {   // LFO not used
        if ((channel->ModulationMode & 0x40) == 0)
        {
            SwapLFO(channel); // Is keyon asynchronous?

            InitializeLFOMain(channel);

            SwapLFO(channel);
        }

        SwapLFO(channel);

        SetLFO(channel);

        SwapLFO(channel);
    }
}

void PMD::InitializeLFOMain(Channel * channel)
{
    channel->LFO1Data   = 0;
    channel->LFO1Delay1 = channel->LFO1Delay2;
    channel->LFO1Speed1 = channel->LFO1Speed2;
    channel->LFO1Step1  = channel->LFO1Step2;
    channel->LFO1Time1  = channel->LFO1Time2;
    channel->LFO1MDepthCount1   = channel->LFO1MDepthCount2;

    // Square wave or random wave?
    if (channel->LFO1Waveform == 2 || channel->LFO1Waveform == 3)
        channel->LFO1Speed1 = 1; // Make the LFO apply immediately after the delay
    else
        channel->LFO1Speed1++;   // Otherwise, +1 to the speed value immediately after the delay
}

int PMD::SetLFO(Channel * channel)
{
    return SetSSGLFO(channel);
}

int PMD::SetSSGLFO(Channel * channel)
{
    if (channel->LFO1Delay1)
    {
        channel->LFO1Delay1--;

        return 0;
    }

    int ax, ch;

    if (channel->ExtendMode & 0x02)
    {
        // Match with TimerA? If not, unconditionally process lfo
        ch = _State.TimerATime - _Driver.OldTimerATime;

        if (ch == 0)
            return 0;

        ax = channel->LFO1Data;

        for (; ch > 0; ch--)
            LFOMain(channel);
    }
    else
    {
        ax = channel->LFO1Data;

        LFOMain(channel);
    }

    return (ax == channel->LFO1Data) ? 0 : 1;
}

uint8_t * PMD::SetModulationMask(Channel * channel, uint8_t * si)
{
    int al = *si++;

    if (al & 0xF8)
        al = 1;

    al &= 7;

    channel->ModulationMode = (channel->ModulationMode & 0xF8) | al;

    InitializeLFOMain(channel);

    return si;
}

// Change STEP value by value of MD command
void PMD::SetStepUsingMDValue(Channel * channel)
{
    if (--channel->LFO1MDepthSpeed1)
        return;

    channel->LFO1MDepthSpeed1 = channel->LFO1MDepthSpeed2;

    if (channel->LFO1MDepthCount1 == 0)
        return;

    if (channel->LFO1MDepthCount1 <= 127)
        channel->LFO1MDepthCount1--;

    int al;

    if (channel->LFO1Step1 < 0)
    {
        al = channel->LFO1MDepth - channel->LFO1Step1;

        if (al < 128)
            channel->LFO1Step1 = -al;
        else
            channel->LFO1Step1 = (channel->LFO1MDepth < 0) ? 0 : -127;
    }
    else
    {
        al = channel->LFO1Step1 + channel->LFO1MDepth;

        if (al < 128)
            channel->LFO1Step1 = al;
        else
            channel->LFO1Step1 = (channel->LFO1MDepth < 0) ? 0 : 127;
    }
}
