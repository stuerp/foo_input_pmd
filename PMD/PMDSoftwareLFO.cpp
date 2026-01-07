
/** $VER: PMDSoftwareLFO.cpp (2026.01.06) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

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
    if (channel->_LFO1SpeedCounter != 1)
    {
        if (channel->_LFO1SpeedCounter != 255)
            channel->_LFO1SpeedCounter--;

        return;
    }

    channel->_LFO1SpeedCounter = channel->_LFO1Speed;

    int Time, Step;

    switch (channel->_LFO1Waveform)
    {
        case 0: // Triangle wave 1
        case 4: // Triangle wave 2
        case 5: // Triangle wave 3
        {
            // Triangle wave
            if (channel->_LFO1Waveform == 5)
                Step = channel->_LFO1StepCounter * std::abs(channel->_LFO1StepCounter);
            else
                Step = channel->_LFO1StepCounter;

            channel->_LFO1Data += Step;

            if (channel->_LFO1Data == 0)
                LFOSetStepUsingMDValue(channel);

            Time = channel->_LFO1TimeCounter;

            if (Time != 255)
            {
                if (--Time == 0)
                {
                    Time = channel->_LFO1Time;

                    if (channel->_LFO1Waveform != 4)
                        Time += Time; // Double the time when inverting and waveform is 0 or 5.

                    channel->_LFO1TimeCounter = Time;
                    channel->_LFO1StepCounter = -channel->_LFO1StepCounter;

                    return;
                }
            }

            channel->_LFO1TimeCounter = Time;
            break;
        }

        case 1:
        {
            // Sawtooth wave
            channel->_LFO1Data += channel->_LFO1StepCounter;

            Time = channel->_LFO1TimeCounter;

            if (Time != -1)
            {
                if (--Time == 0)
                {
                    channel->_LFO1Data = -channel->_LFO1Data;

                    LFOSetStepUsingMDValue(channel);

                    Time = (channel->_LFO1Time) * 2;
                }
            }

            channel->_LFO1TimeCounter = Time;
            break;
        }

        case 2:
        {
            // Square wave
            channel->_LFO1Data = (channel->_LFO1StepCounter * channel->_LFO1TimeCounter);

            LFOSetStepUsingMDValue(channel);

            channel->_LFO1StepCounter = -channel->_LFO1StepCounter;
            break;
        }

        case 6:
        {
            // One-shot
            if (channel->_LFO1TimeCounter != 0)
            {
                if (channel->_LFO1TimeCounter != 255)
                    channel->_LFO1TimeCounter--;

                channel->_LFO1Data += channel->_LFO1StepCounter;
            }
            break;
        }

        default:
        {
            // Random wave
            Step = std::abs(channel->_LFO1StepCounter) * channel->_LFO1TimeCounter;

            channel->_LFO1Data = Step - rnd(Step * 2);

            LFOSetStepUsingMDValue(channel);
        }
    }
}

/// <summary>
/// Start the FM LFO.
/// </summary>
int pmd_driver_t::StartLFO(channel_t * channel, int value)
{
    int LoNibble = value & 0x0F;

    if (LoNibble == 0x0C)
    {
        value = channel->_DefaultTone;

        LoNibble = value & 0x0F;
    }

    channel->_DefaultTone = value;

    if (LoNibble != 0x0F)
    {
        channel->_Portamento = 0; // Reset the portamento.

        if (!_Driver._IsTieSet)
            LFOInitialize(channel);
        else
            StopLFO(channel);
    }
    else
        StopLFO(channel);

    return value;
}

/// <summary>
/// Start the SSG/PCM LFO.
/// </summary>
int pmd_driver_t::StartPCMLFO(channel_t * channel, int value)
{
    int LoNibble = value & 0x0F;

    if (LoNibble == 0x0C)
    {
        value = channel->_DefaultTone;

        LoNibble = value & 0x0F;
    }

    channel->_DefaultTone = value;

    if (LoNibble == 0x0F)
    {
        SSGPCMSoftwareEnvelope(channel);
        StopLFO(channel);

        return value;
    }

    channel->_Portamento = 0; // Initialize the portamento.

    if (_Driver._IsTieSet)
    {
        SSGPCMSoftwareEnvelope(channel); // Only execute the software envelope once when preceded by a "&" command (Tie).
        StopLFO(channel);

        return value;
    }

    //  Initialize the software envelope.
    if (channel->_SSGEnvelopFlag != -1)
    {
        channel->_SSGEnvelopFlag = 0;
        channel->ExtendedAttackLevel = 0;
        channel->AttackDuration = channel->ExtendedAttackDuration;

        if (channel->AttackDuration == 0)
        {
            channel->_SSGEnvelopFlag = 1;  // ATTACK=0 ... ｽｸﾞ Decay ﾆ
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

    LFOInitialize(channel);

    return value;
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
/// Initializes the LFO 1 and LFO 2.
/// </summary>
void pmd_driver_t::LFOInitialize(channel_t * channel)
{
    channel->_HardwareLFODelayCounter = channel->_HardwareLFODelay;

    if (channel->_HardwareLFODelay != 0)
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xB4 + (_Driver._CurrentChannel - 1)), (uint32_t) (channel->_PanAndVolume & 0xC0));

    channel->_FMSlotDelayCounter = channel->_SlotDelay;

    // LFO 1
    if ((channel->_HardwareLFO & 0x03) != 0)
    {   // LFO not used
        if ((channel->_HardwareLFO & 0x04) == 0)
            LFOReset(channel); // Is KeyOn asynchronous?

        SetLFO(channel);
    }

    // LFO 2
    if ((channel->_HardwareLFO & 0x30) != 0)
    {   // LFO not used
        if ((channel->_HardwareLFO & 0x40) == 0)
        {
            LFOSwap(channel); // Is KeyOn asynchronous?
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
    channel->_LFO1DelayCounter = channel->_LFO1Delay;
    channel->_LFO1SpeedCounter = channel->_LFO1Speed;
    channel->_LFO1StepCounter  = channel->_LFO1Step;
    channel->_LFO1TimeCounter  = channel->_LFO1Time;
    channel->_LFO1DepthSpeed1  = channel->_LFO1DepthSpeed2;

    // Square wave or random wave?
    if (channel->_LFO1Waveform == 2 || channel->_LFO1Waveform == 3)
        channel->_LFO1SpeedCounter = 1; // Make the LFO apply immediately after the delay.
    else
        channel->_LFO1SpeedCounter++;   // Otherwise, +1 to the speed value immediately after the delay.
}

int pmd_driver_t::SetLFO(channel_t * channel)
{
    return SetSSGLFO(channel);
}

int pmd_driver_t::SetSSGLFO(channel_t * channel)
{
    if (channel->_LFO1DelayCounter != 0)
    {
        channel->_LFO1DelayCounter--;

        return 0;
    }

    int32_t Data;

    if (channel->_ExtendMode & 0x02)
    {
        // Match with TimerA? If not, unconditionally process lfo
        int32_t Value = _State.TimerACounter - _Driver._PreviousTimerACounter;

        if (Value == 0)
            return 0;

        Data = channel->_LFO1Data;

        for (; Value > 0; --Value)
            LFOMain(channel);
    }
    else
    {
        Data = channel->_LFO1Data;

        LFOMain(channel);
    }

    return (Data == channel->_LFO1Data) ? 0 : 1;
}

/// <summary>
/// 9.7. LFO Depth Temporal Change Setting, Sets the step value from the MD command
/// </summary>
void pmd_driver_t::LFOSetStepUsingMDValue(channel_t * channel)
{
    if (--channel->_LFO1DepthSpeedCounter1)
        return;

    channel->_LFO1DepthSpeedCounter1 = channel->_LFO1DepthSpeedCounter2;

    if (channel->_LFO1DepthSpeed1 == 0)
        return;

    if (channel->_LFO1DepthSpeed1 <= 127)
        channel->_LFO1DepthSpeed1--;

    int32_t Value;

    if (channel->_LFO1StepCounter < 0)
    {
        Value = channel->_LFO1Depth - channel->_LFO1StepCounter;

        if (Value < 128)
            channel->_LFO1StepCounter = -Value;
        else
            channel->_LFO1StepCounter = (channel->_LFO1Depth < 0) ? 0 : -127;
    }
    else
    {
        Value = channel->_LFO1StepCounter + channel->_LFO1Depth;

        if (Value < 128)
            channel->_LFO1StepCounter = Value;
        else
            channel->_LFO1StepCounter = (channel->_LFO1Depth < 0) ? 0 : 127;
    }
}

/// <summary>
/// 9.6. Dedicated Rise/Fall LFO Setting, Selects the rise/fall-type software LFO and turns it on, Command 'MPA ±number1', Range -128–+127
/// </summary>
uint8_t * pmd_driver_t::LFOSetRiseFallType(channel_t * channel, uint8_t * si) const noexcept
{
    int32_t Value = *si++;

    if (Value < 0x80)
    {
        if (Value == 0)
            Value = 255;

        channel->_LFO1DepthSpeed1 = Value;
        channel->_LFO1DepthSpeed2 = Value;
    }
    else
    {
        Value &= 0x7F;

        if (Value == 0)
            Value = 255;

        channel->_LFO2DepthSpeed1 = Value;
        channel->_LFO2DepthSpeed2 = Value;
    }

    return si;
}

/// <summary>
/// Swaps LFO 1 and LFO 2.
/// </summary>
void pmd_driver_t::LFOSwap(channel_t * channel) noexcept
{
    channel->_HardwareLFO = ((channel->_HardwareLFO & 0x0F) << 4) + (channel->_HardwareLFO >> 4);
    channel->_ExtendMode  = ((channel->_ExtendMode  & 0x0F) << 4) + (channel->_ExtendMode >> 4);

    std::swap(channel->_LFO1Data, channel->_LFO2Data);
    std::swap(channel->_LFO1Waveform, channel->_LFO2Waveform);

    std::swap(channel->_LFO1Delay, channel->_LFO2Delay);
    std::swap(channel->_LFO1Speed, channel->_LFO2Speed);
    std::swap(channel->_LFO1Step, channel->_LFO2Step);
    std::swap(channel->_LFO1Time, channel->_LFO2Time);

    std::swap(channel->_LFO1DepthSpeed1, channel->_LFO2DepthSpeed1);
    std::swap(channel->_LFO1DepthSpeed2, channel->_LFO2DepthSpeed2);

    std::swap(channel->_LFO1Depth, channel->_LFO2Depth);

    std::swap(channel->_LFO1DelayCounter, channel->_LFO2DelayCounter);
    std::swap(channel->_LFO1SpeedCounter, channel->_LFO2SpeedCounter);
    std::swap(channel->_LFO1StepCounter, channel->_LFO2StepCounter);
    std::swap(channel->_LFO1TimeCounter, channel->_LFO2TimeCounter);

    std::swap(channel->_LFO1DepthSpeedCounter1, channel->_LFO2DepthSpeedCounter1);
    std::swap(channel->_LFO1DepthSpeedCounter2, channel->_LFO2DepthSpeedCounter2);
}

/// <summary>
/// 9.1. Software LFO Setting, Sets the modulation parameters, Command 'M l length[.], number2, number3, number4'
/// </summary>
uint8_t * pmd_driver_t::LFO1SetModulation(channel_t * channel, uint8_t * si) noexcept
{
    channel->_LFO1DelayCounter =
    channel->_LFO1Delay = *si++; // 0 - 255

    channel->_LFO1SpeedCounter =
    channel->_LFO1Speed = *si++; // 0 - 255

    channel->_LFO1StepCounter =
    channel->_LFO1Step = *(int8_t *) si++; // -128 - 127

    channel->_LFO1TimeCounter =
    channel->_LFO1Time = *si++; // 0 - 255

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
