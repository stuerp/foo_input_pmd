
/** $VER: PMDSoftwareLFO.cpp (2026.01.05) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

/// <summary>
/// Main LFO processing
/// </summary>
void pmd_driver_t::LFOMain(channel_t * channel)
{
    if (channel->_LFO1Speed1 != 1)
    {
        if (channel->_LFO1Speed1 != 255)
            channel->_LFO1Speed1--;

        return;
    }

    channel->_LFO1Speed1 = channel->_LFO1Speed2;

    int Time, Step;

    switch (channel->_LFO1Waveform)
    {
        case 0: // Triangle wave 1
        case 4: // Triangle wave 2
        case 5: // Triangle wave 3
        {
            // Triangle wave
            if (channel->_LFO1Waveform == 5)
                Step = channel->_LFO1Step1 * std::abs(channel->_LFO1Step1);
            else
                Step = channel->_LFO1Step1;

            channel->_LFO1Data += Step;

            if (channel->_LFO1Data == 0)
                SetStepUsingMDValue(channel);

            Time = channel->_LFO1Time1;

            if (Time != 255)
            {
                if (--Time == 0)
                {
                    Time = channel->_LFO1Time2;

                    if (channel->_LFO1Waveform != 4)
                        Time += Time; // Double the time when inverting and waveform is 0 or 5.

                    channel->_LFO1Time1 = Time;
                    channel->_LFO1Step1 = -channel->_LFO1Step1;

                    return;
                }
            }

            channel->_LFO1Time1 = Time;
            break;
        }

        case 1:
        {
            // Sawtooth wave
            channel->_LFO1Data += channel->_LFO1Step1;

            Time = channel->_LFO1Time1;

            if (Time != -1)
            {
                if (--Time == 0)
                {
                    channel->_LFO1Data = -channel->_LFO1Data;

                    SetStepUsingMDValue(channel);

                    Time = (channel->_LFO1Time2) * 2;
                }
            }

            channel->_LFO1Time1 = Time;
            break;
        }

        case 2:
        {
            // Square wave
            channel->_LFO1Data = (channel->_LFO1Step1 * channel->_LFO1Time1);

            SetStepUsingMDValue(channel);

            channel->_LFO1Step1 = -channel->_LFO1Step1;
            break;
        }

        case 6:
        {
            // One-shot
            if (channel->_LFO1Time1 != 0)
            {
                if (channel->_LFO1Time1 != 255)
                    channel->_LFO1Time1--;

                channel->_LFO1Data += channel->_LFO1Step1;
            }
            break;
        }

        default:
        {
            // Random wave
            Step = std::abs(channel->_LFO1Step1) * channel->_LFO1Time1;

            channel->_LFO1Data = Step - rnd(Step * 2);

            SetStepUsingMDValue(channel);
        }
    }
}

/// <summary>
/// Start the FM LFO.
/// </summary>
int pmd_driver_t::StartLFO(channel_t * channel, int al)
{
    int LoNibble = al & 0x0F;

    if (LoNibble == 0x0C)
    {
        al = channel->DefaultTone;

        LoNibble = al & 0x0F;
    }

    channel->DefaultTone = al;

    if (LoNibble != 0x0F)
    {
        channel->_Portamento = 0; // Reset the portamento.

        if (!_Driver._IsTieSet)
            InitializeLFO(channel);
        else
            StopLFO(channel);
    }
    else
        StopLFO(channel);

    return al;
}

/// <summary>
/// Start the SSG/PCM LFO.
/// </summary>
int pmd_driver_t::StartPCMLFO(channel_t * channel, int al)
{
    int LoNibble = al & 0x0F;

    if (LoNibble == 0x0C)
    {
        al = channel->DefaultTone;

        LoNibble = al & 0x0F;
    }

    channel->DefaultTone = al;

    if (LoNibble == 0x0F)
    {
        SSGPCMSoftwareEnvelope(channel);
        StopLFO(channel);

        return al;
    }

    channel->_Portamento = 0; // Initialize the portamento.

    if (_Driver._IsTieSet)
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
    }

    InitializeLFO(channel);

    return al;
}

void pmd_driver_t::StopLFO(channel_t * channel)
{
    if ((channel->_HardwareLFO & 0x03) != 0)
        SetLFO(channel); // Only execute the LFO once when preceded by a "&" command (Tie).

    if ((channel->_HardwareLFO & 0x30) != 0)
    {
        LFOSwap(channel);

        SetLFO(channel); // Only execute the LFO once when preceded by a "&" command (Tie).

        LFOSwap(channel);
    }
}

/// <summary>
/// Initializes the LFO.
/// </summary>
void pmd_driver_t::InitializeLFO(channel_t * channel)
{
    channel->HardwareLFODelayCounter = channel->_HardwareLFODelay;

    if (channel->_HardwareLFODelay != 0)
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xB4 + (_Driver._CurrentChannel - 1)), (uint32_t) (channel->_PanAndVolume & 0xC0));

    channel->SlotDelayCounter = channel->SlotDelay;

    if ((channel->_HardwareLFO & 0x03) != 0)
    {   // LFO not used
        if ((channel->_HardwareLFO & 0x04) == 0)
            LFOReset(channel); // Is keyon asynchronous?

        SetLFO(channel);
    }

    if ((channel->_HardwareLFO & 0x30) != 0)
    {   // LFO not used
        if ((channel->_HardwareLFO & 0x40) == 0)
        {
            LFOSwap(channel); // Is keyon asynchronous?

            LFOReset(channel);

            LFOSwap(channel);
        }

        LFOSwap(channel);

        SetLFO(channel);

        LFOSwap(channel);
    }
}

/// <summary>
/// Reset LFO 1 and LFO 2.
/// </summary>
void pmd_driver_t::LFOReset(channel_t * channel)
{
    channel->_LFO1Data         = 0;
    channel->_LFO1Delay1       = channel->_LFO1Delay2;
    channel->_LFO1Speed1       = channel->_LFO1Speed2;
    channel->_LFO1Step1        = channel->_LFO1Step2;
    channel->_LFO1Time1        = channel->_LFO1Time2;
    channel->_LFO1MDepthCount1 = channel->_LFO1MDepthCount2;

    // Square wave or random wave?
    if (channel->_LFO1Waveform == 2 || channel->_LFO1Waveform == 3)
        channel->_LFO1Speed1 = 1; // Make the LFO apply immediately after the delay.
    else
        channel->_LFO1Speed1++;   // Otherwise, +1 to the speed value immediately after the delay.
}

int pmd_driver_t::SetLFO(channel_t * channel)
{
    return SetSSGLFO(channel);
}

int pmd_driver_t::SetSSGLFO(channel_t * channel)
{
    if (channel->_LFO1Delay1)
    {
        channel->_LFO1Delay1--;

        return 0;
    }

    int ax, ch;

    if (channel->_ExtendMode & 0x02)
    {
        // Match with TimerA? If not, unconditionally process lfo
        ch = _State.TimerACounter - _Driver._PreviousTimerACounter;

        if (ch == 0)
            return 0;

        ax = channel->_LFO1Data;

        for (; ch > 0; ch--)
            LFOMain(channel);
    }
    else
    {
        ax = channel->_LFO1Data;

        LFOMain(channel);
    }

    return (ax == channel->_LFO1Data) ? 0 : 1;
}

// Change STEP value by value of MD command
void pmd_driver_t::SetStepUsingMDValue(channel_t * channel)
{
    if (--channel->_LFO1MDepthSpeed1)
        return;

    channel->_LFO1MDepthSpeed1 = channel->_LFO1MDepthSpeed2;

    if (channel->_LFO1MDepthCount1 == 0)
        return;

    if (channel->_LFO1MDepthCount1 <= 127)
        channel->_LFO1MDepthCount1--;

    int al;

    if (channel->_LFO1Step1 < 0)
    {
        al = channel->_LFO1MDepth - channel->_LFO1Step1;

        if (al < 128)
            channel->_LFO1Step1 = -al;
        else
            channel->_LFO1Step1 = (channel->_LFO1MDepth < 0) ? 0 : -127;
    }
    else
    {
        al = channel->_LFO1Step1 + channel->_LFO1MDepth;

        if (al < 128)
            channel->_LFO1Step1 = al;
        else
            channel->_LFO1Step1 = (channel->_LFO1MDepth < 0) ? 0 : 127;
    }
}

/// <summary>
/// Swaps LFO 1 and LFO 2.
/// </summary>
void pmd_driver_t::LFOSwap(channel_t * channel) noexcept
{
    channel->_HardwareLFO = ((channel->_HardwareLFO & 0x0F) << 4) + (channel->_HardwareLFO >> 4);
    channel->_ExtendMode  = ((channel->_ExtendMode  & 0x0F) << 4) + (channel->_ExtendMode >> 4);

    std::swap(channel->_LFO1Data, channel->LFO2Data);

    std::swap(channel->_LFO1Delay1, channel->LFO2Delay1);
    std::swap(channel->_LFO1Speed1, channel->LFO2Speed1);
    std::swap(channel->_LFO1Step1, channel->LFO2Step1);
    std::swap(channel->_LFO1Time1, channel->LFO2Time1);
    std::swap(channel->_LFO1Delay2, channel->LFO2Delay2);
    std::swap(channel->_LFO1Speed2, channel->LFO2Speed2);
    std::swap(channel->_LFO1Step2, channel->LFO2Step2);
    std::swap(channel->_LFO1Time2, channel->LFO2Time2);
    std::swap(channel->_LFO1MDepth, channel->LFO2MDepth);
    std::swap(channel->_LFO1MDepthSpeed1, channel->LFO2MDepthSpeed1);
    std::swap(channel->_LFO1MDepthSpeed2, channel->LFO2MDepthSpeed2);
    std::swap(channel->_LFO1Waveform, channel->LFO2Waveform);
    std::swap(channel->_LFO1MDepthCount1, channel->LFO2MDepthCount1);
    std::swap(channel->_LFO1MDepthCount2, channel->LFO2MDepthCount2);
}

/// <summary>
/// 9.1. Software LFO Setting, Sets the modulation parameters, Command 'M l length[.], number2, number3, number4'
/// </summary>
uint8_t * pmd_driver_t::LFO1SetModulation(channel_t * channel, uint8_t * si) noexcept
{
    channel->_LFO1Delay1 =
    channel->_LFO1Delay2 = *si++; // 0 - 255

    channel->_LFO1Speed1 =
    channel->_LFO1Speed2 = *si++; // 0 - 255

    channel->_LFO1Step1 =
    channel->_LFO1Step2 = *(int8_t *) si++; // -128 - 127

    channel->_LFO1Time1 =
    channel->_LFO1Time2 = *si++; // 0 - 255

    LFOReset(channel);

    return si;
}

/// <summary>
/// 9.3. Software LFO Switch, Controls on/off and keyon synchronization of the software LFO, Command '*A number'
/// </summary>
uint8_t * pmd_driver_t::LFO1SetSwitch(channel_t * channel, uint8_t * si)
{
    int32_t Value = *si++;

    if (Value & 0xF8)
        Value = 0x01;
    else
        Value &= 0x07;

    channel->_HardwareLFO = (channel->_HardwareLFO & 0xF8) | Value;

    LFOReset(channel);

    return si;
}

/// <summary>
/// 9.3. Software LFO Switch, Controls on/off and keyon synchronization of the software LFO, Command '*B number'
/// </summary>
uint8_t * pmd_driver_t::LFO2SetSwitch(channel_t * channel, uint8_t * si)
{
    channel->_HardwareLFO = (channel->_HardwareLFO & 0x8F) | ((*si++ & 0x07) << 4);

    LFOSwap(channel);

    LFOReset(channel);

    LFOSwap(channel);

    return si;
}

/// <summary>
/// 9.4. Software LFO Slot Setting, Sets the slot number to apply the effect of the software LFO to, Command 'MMA slotnum' (FM Sound Source only)
/// </summary>
uint8_t * pmd_driver_t::LFO1SetSlotMask(channel_t * channel, uint8_t * si)
{
    int32_t Mask = *si++ & 0x0F; // Range 0 - 15, 0x01: Slot 1, 0x02: Slot 2, 0x04: Slot 3, 0x08: Slot 4

    if (Mask != 0)
    {
        Mask = (Mask << 4) | 0x0F;

        channel->_LFO1Mask = Mask;
    }
    else
        channel->_LFO1Mask = channel->FMCarrier;

    SetFMChannelLFOs(channel);

    return si;
}

/// <summary>
/// 9.4. Software LFO Slot Setting, Sets the slot number to apply the effect of the software LFO to, Command 'MMB slotnum' (FM Sound Source only)
/// </summary>
uint8_t * pmd_driver_t::LFO2SetSlotMask(channel_t * channel, uint8_t * si)
{
    int32_t Mask = *si++ & 0x0F; // Range 0 - 15, 0x01: Slot 1, 0x02: Slot 2, 0x04: Slot 3, 0x08: Slot 4

    if (Mask != 0)
    {
        Mask = (Mask << 4) | 0x0F;

        channel->_LFO2Mask = Mask;
    }
    else
        channel->_LFO2Mask = channel->FMCarrier;

    // For the FM channel 3, both pitch and volume LFOs are affected.
    SetFMChannelLFOs(channel);

    return si;
}

