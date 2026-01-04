
// $VER: PMDSSG.cpp (2025.12.23) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

void pmd_driver_t::SSGMain(channel_t * channel)
{
    if (channel->_Data == nullptr)
        return;

    uint8_t * si = channel->_Data;

    channel->_Size--;

    // When using the PPS and SSG channel 3 and the SSG is playing sound effects.
    if (_UsePPSForDrums && (channel == &_SSGChannels[2]) && !_State.UseRhythmChannel && (channel->_Size <= channel->GateTime))
    {
        SSGKeyOff(channel);

        _OPNAW->SetReg((uint32_t) (_Driver._CurrentChannel + 8 - 1), 0U); // Stop playing.

        channel->KeyOffFlag = -1;
    }

    if (channel->PartMask != 0x00)
        channel->KeyOffFlag = -1;
    else
    if ((channel->KeyOffFlag & 0x03) == 0)
    {
        if (channel->_Size <= channel->GateTime)
        {
            SSGKeyOff(channel);

            channel->KeyOffFlag = -1;
        }
    }

    if (channel->_Size == 0)
    {
        channel->HardwareLFOModulationMode &= 0xF7;

        // DATA READ
        while (1)
        {
            if ((*si == 0xDA) && CheckSSGDrum(channel, *si))
            {
                si++;
            }
            else
            if ((*si != 0xDA) && (*si > 0x80))
            {
                si = ExecuteSSGCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->_Data = si;
                channel->Tone = 0xFF;

                channel->_LoopCheck = 0x03;

                if (channel->_LoopData == nullptr)
                {
                    if (channel->PartMask != 0x00)
                    {
                        _Driver._IsTieSet = false;
                        _Driver._VolumeBoostCount = 0;

                        _Driver._LoopCheck &= channel->_LoopCheck;

                        return;
                    }
                    else
                        break;
                }

                // Start executing a loop.
                si = channel->_LoopData;

                channel->_LoopCheck = 0x01;
            }
            else
            {
                if (*si == 0xDA)
                {
                    si = SetSSGPortamentoCommand(channel, ++si);

                    _Driver._LoopCheck &= channel->_LoopCheck;

                    return;
                }
                else
                if (channel->PartMask != 0x00)
                {
                    if (!CheckSSGDrum(channel, *si))
                    {
                        si++;

                        // Set to 'rest'.
                        channel->Factor = 0;
                        channel->Tone   = 0xFF;
                        channel->_Size = *si++;
                        channel->KeyOnFlag++;

                        channel->_Data = si;

                        if (--_Driver._VolumeBoostCount)
                            channel->VolumeBoost = 0;

                        _Driver._IsTieSet = false;
                        _Driver._VolumeBoostCount = 0;
                        break;
                    }
                }

                SetSSGTone(channel, TransposeSSG(channel, StartPCMLFO(channel, *si++)));

                channel->_Size = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
                {
                    if (--_Driver._VolumeBoostCount)
                    {
                        _Driver._VolumeBoostCount = 0;
                        channel->VolumeBoost = 0;
                    }
                }

                SetSSGVolume(channel);
                SetSSGPitch(channel);
                SSGKeyOn(channel);

                channel->KeyOnFlag++;
                channel->_Data = si;

                _Driver._IsTieSet = false;
                _Driver._VolumeBoostCount = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
        }
    }

    _Driver._HardwareLFOModulationMode = (channel->HardwareLFOModulationMode & 0x08);

    if (channel->HardwareLFOModulationMode != 0x00)
    {
        if (channel->HardwareLFOModulationMode & 0x03)
        {
            if (SetSSGLFO(channel))
                _Driver._HardwareLFOModulationMode |= (channel->HardwareLFOModulationMode & 0x03);
        }

        if (channel->HardwareLFOModulationMode & 0x30)
        {
            SwapLFO(channel);

            if (SetSSGLFO(channel))
            {
                SwapLFO(channel);

                _Driver._HardwareLFOModulationMode |= (channel->HardwareLFOModulationMode & 0x30);
            }
            else
                SwapLFO(channel);
        }

        if (_Driver._HardwareLFOModulationMode & 0x19)
        {
            if (_Driver._HardwareLFOModulationMode & 0x08)
                CalculatePortamento(channel);

            // Do not operate while using SSG channel 3 and the SSG drum is playing.
            if (!(!_UsePPSForDrums && (channel == &_SSGChannels[2]) && (channel->Tone == 0xFF) && !_State.UseRhythmChannel))
                SetSSGPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if ((temp != 0) || _Driver._HardwareLFOModulationMode & 0x22 || (_State.FadeOutSpeed != 0))
    {
        // Do not operate while using SSG channel 3 and the SSG drum is playing.
        if (!(!_UsePPSForDrums && (channel == &_SSGChannels[2]) && (channel->Tone == 0xFF) && !_State.UseRhythmChannel))
            SetSSGVolume(channel);
    }

    _Driver._LoopCheck &= channel->_LoopCheck;
}

uint8_t * pmd_driver_t::ExecuteSSGCommand(channel_t * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        // 4.12. Sound Cut Setting 1, Command 'Q [%] numerical value' / 4.13. Sound Cut Setting 2, Command 'q [number1][-[number2]] [,number3]' / Command 'q [l length[.]][-[l length]] [,l length[.]]'
        case 0xFE:
        {
            channel->EarlyKeyOffTimeout = *si++;
            channel->EarlyKeyOffTimeoutRandomRange = 0;
            break;
        }

        // 5.5. Relative Volume Change, Increase volume by 3dB.
        case 0xF4:
        {
            if (channel->_Volume < 15)
                channel->_Volume++;
            break;
        }

        // 5.5. Relative Volume Change, Decrease volume by 3dB.
        case 0xF3:
        {
            if (channel->_Volume > 0)
                channel->_Volume--;
            break;
        }

        // 15.1. FM Chip Direct Output, Direct register write. Writes val to address reg of the YM2608's internal memory, Command 'y number1, number2'
        case 0xEF:
        {
            _OPNAW->SetReg(si[0], si[1]);
            si += 2;
            break;
        }

        // 6.6. Noise frequency setting, Command 'w number'
        case 0xEE:
        {
            _SSGNoiseFrequency = *si++;
            break;
        }

        // 6.5. SSG/OPM Tone/Noise Output Selection, Command 'P number'
        case 0xED:
        {
            channel->SSGMask = *si++;
            break;
        }

        case 0xEC: si++; break;

        // 5.5. Relative Volume Change, Command ') %number'
        case 0xE3:
        {
            channel->_Volume += *si++;

            if (channel->_Volume > 15)
                channel->_Volume = 15;
            break;
        }

        // 5.5. Relative Volume Change, Command ') %number'
        case 0xE2:
        {
            channel->_Volume -= *si++;

            if (channel->_Volume < 0)
                channel->_Volume = 0;
            break;
        }

        // 5.5. Relative Volume Change, Command ') ^%number'
        case 0xDE:
        {
            si = IncreaseVolumeForNextNote(channel, si, 15);
            break;
        }

        // 4.3. Portamento Setting
        case 0xDA:
        {
            si = SetSSGPortamentoCommand(channel, si);
            break;
        }

        // 6.6. Noise Frequency Setting, Command 'w ±number'
        case 0xD0:
        {
            si = SetSSGNoiseFrequencyCommand(si);
            break;
        }

        // 7.3. SSG Pitch Interval Correction Setting, Selects whether or not to adjust the SSG pitch interval, Set SSG Extend Mode (bit 0), Command 'DX number'
        case 0xCC:
        {
            channel->ExtendMode = (channel->ExtendMode & 0xFE) | (*si++ & 0x01);
            break;
        }

        // Set SSG Extend Mode (bit 1).
        case 0xCA:
            channel->ExtendMode = (channel->ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);
            break;

        // 8.2. Software Envelope Speed Setting, Set SSG Extend Mode (bit 2), Command 'EX number'
        case 0xC9:
        {
            channel->ExtendMode = (channel->ExtendMode & 0xFB) | ((*si++ & 0x01) << 2);
            break;
        }

        case 0xC3: si += 2; break;

        // 15.7. Channel Mask Control, Sets the channel mask to on or off, Command 'm number'
        case 0xC0:
        {
            si = SetSSGChannelMaskCommand(channel, si);
            break;
        }

        // 9.11. Hardware LFO Switch/Depth Setting (OPNA), Command "# number1, [number2]": Sets the hardware LFO on (1) or off (0). (OPNA FM sound source only). Number2 = depth. Can be omitted only when switch is 0.
        case 0xBE:
        {
            channel->HardwareLFOModulationMode = (channel->HardwareLFOModulationMode & 0x8F) | ((*si++ & 0x07) << 4);

            SwapLFO(channel);

            InitializeLFOMain(channel);

            SwapLFO(channel);
            break;
        }

        case 0xBA: si++; break;

        default:
            si = ExecuteCommand(channel, si, Command);
    }

    return si;
}

/// <summary>
/// Sets SSG Wait after register output.
/// </summary>
void pmd_driver_t::SetSSGDelay(int nsec)
{
    _OPNAW->SetSSGDelay(nsec);
}

#pragma region Commands

/// <summary>
///
/// </summary>
void pmd_driver_t::SetSSGTone(channel_t * channel, int tone)
{
    if ((tone & 0x0F) != 0x0F)
    {
        channel->Tone = tone;

        const int Octave = (tone & 0xF0) >> 4;
        const int Note   = (tone & 0x0F);

        int Factor = SSGScaleFactor[Note];

        if (Octave > 0)
        {
            Factor >>= Octave - 1;

            const int Carry = Factor & 1;

            Factor = (Factor >> 1) + Carry;
        }

        channel->Factor = (uint32_t) Factor;
    }
    else
    {
        channel->Tone = 0xFF;

        if (channel->HardwareLFOModulationMode & 0x11)
            return;

        channel->Factor = 0; // Don't use LFO pitch.

        return;
    }
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SetSSGVolume(channel_t * channel)
{
    if ((channel->SSGEnvelopFlag == 3) || ((channel->SSGEnvelopFlag == -1) && (channel->ExtendedCount == 0)))
        return;

    const uint32_t Register = (uint32_t) (_Driver._CurrentChannel + 8 - 1);

    int dl = (channel->VolumeBoost) ? channel->VolumeBoost - 1 : channel->_Volume;

    // Volume Down calculation
    dl = ((256 - _State.SSGVolumeAdjust) * dl) >> 8;

    // Fade-out calculation
    dl = ((256 - _State._FadeOutVolume) * dl) >> 8;

    // Envelope calculation
    if (dl <= 0)
    {
        _OPNAW->SetReg(Register, 0);
        return;
    }

    if (channel->SSGEnvelopFlag == -1)
    {
        if (channel->ExtendedAttackLevel == 0)
        {
            _OPNAW->SetReg(Register, 0);
            return;
        }

        dl = ((((dl * (channel->ExtendedAttackLevel + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        dl += channel->ExtendedAttackLevel;

        if (dl <= 0)
        {
            _OPNAW->SetReg(Register, 0);
            return;
        }

        if (dl > 15)
            dl = 15;
    }

    // Volume LFO calculation
    if ((channel->HardwareLFOModulationMode & 0x22) == 0)
    {
        _OPNAW->SetReg(Register, (uint32_t) dl);
        return;
    }

    {
        int ax = (channel->HardwareLFOModulationMode & 0x02) ? channel->LFO1Data : 0;

        if (channel->HardwareLFOModulationMode & 0x20)
            ax += channel->LFO2Data;

        dl += ax;

        if (dl < 0)
            dl = 0;
        else
        if (dl > 15)
            dl = 15;

        _OPNAW->SetReg(Register, (uint32_t) dl);
    }
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SetSSGPitch(channel_t * channel)
{
    if (channel->Factor == 0)
        return;

    int Pitch = (int) (channel->Factor + channel->_Portamento);

    {
        int dx = 0;

        // SSG Detune/LFO set
        if ((channel->ExtendMode & 0x01) == 0)
        {
            Pitch -= channel->_DetuneValue;

            if (channel->HardwareLFOModulationMode & 0x01)
                Pitch -= channel->LFO1Data;

            if (channel->HardwareLFOModulationMode & 0x10)
                Pitch -= channel->LFO2Data;
        }
        else
        {
            // Calculating extended DETUNE (DETUNE)
            if (channel->_DetuneValue)
            {
                dx = (Pitch * channel->_DetuneValue) >> 12;

                if (dx >= 0)
                    dx++;
                else
                    dx--;

                Pitch -= dx;
            }

            // Extended DETUNE (LFO) calculation
            if (channel->HardwareLFOModulationMode & 0x11)
            {
                if (channel->HardwareLFOModulationMode & 0x01)
                    dx = channel->LFO1Data;
                else
                    dx = 0;

                if (channel->HardwareLFOModulationMode & 0x10)
                    dx += channel->LFO2Data;

                if (dx != 0)
                {
                    dx = (Pitch * dx) >> 12;

                    if (dx >= 0)
                        dx++;
                    else
                        dx--;
                }

                Pitch -= dx;
            }
        }

        if (Pitch >= 0x1000)
            Pitch = (Pitch >= 0) ? 0x0FFF : 0;
    }

    _OPNAW->SetReg((uint32_t) ((_Driver._CurrentChannel - 1) * 2),     (uint32_t) LOBYTE(Pitch));
    _OPNAW->SetReg((uint32_t) ((_Driver._CurrentChannel - 1) * 2 + 1), (uint32_t) HIBYTE(Pitch));
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SSGKeyOn(channel_t * channel)
{
    if (channel->Tone == 0xFF)
        return;

    // Enable tone or noise mode for channel A, B or C.
    int ah = (1 << (_Driver._CurrentChannel - 1)) | (1 << (_Driver._CurrentChannel + 2));
    int al = ((int32_t) _OPNAW->GetReg(0x07) | ah);

    ah = ~(ah & channel->SSGMask);
    al &= ah;

    _OPNAW->SetReg(0x07, (uint32_t) al);

    // Set the SSG noise frequency.
    if ((_SSGNoiseFrequency != _OldSSGNoiseFrequency) && (_SSGEffect._Priority == 0))
    {
        _OPNAW->SetReg(0x06, (uint32_t) _SSGNoiseFrequency); // Noise Period

        _OldSSGNoiseFrequency = _SSGNoiseFrequency;
    }
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SSGKeyOff(channel_t * channel)
{
    if (channel->Tone == 0xFF)
        return;

    if (channel->SSGEnvelopFlag != -1)
        channel->SSGEnvelopFlag = 2;
    else
        channel->ExtendedCount = 4;
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SetSSGDrumInstrument(channel_t * channel, int instrumentNumber)
{
    if (_UsePPSForDrums)
    {
        instrumentNumber |= 0x80;

        if (_OldInstrumentNumber != instrumentNumber)
            _OldInstrumentNumber = instrumentNumber;
        else
            _PPS->Stop();
    }

    _SSGEffect.Flags = 0x03; // Correct the pitch and volume (K command).

    SSGEffectMain(channel, instrumentNumber);
}

#pragma endregion

/// <summary>
/// Set the SSG Envelope (Format 1). Command "E number1, number2, number3, number4"
/// </summary>
uint8_t * pmd_driver_t::SetSSGEnvelopeFormat1Command(channel_t * channel, uint8_t * si)
{
    channel->AttackDuration         = *si;
    channel->ExtendedAttackDuration = *si++;
    channel->DecayDepth             = *(int8_t *) si++;
    channel->SustainRate            = *si;
    channel->ExtendedSustainRate    = *si++;
    channel->ReleaseRate            = *si;
    channel->ExtendedReleaseRate    = *si++;

    if (channel->SSGEnvelopFlag == -1)
    {
        channel->SSGEnvelopFlag = 2; // RR
        channel->ExtendedAttackLevel = -15; // Volume
    }

    return si;
}

/// <summary>
/// Set the SSG Envelope (Format 2). Command "E number1, number2, number3, number4, number5 [,number6]"
/// </summary>
uint8_t * pmd_driver_t::SetSSGEnvelopeFormat2Command(channel_t * channel, uint8_t * si)
{
    channel->AttackDuration = *si++ & 0x1F;
    channel->DecayDepth     = *si++ & 0x1F;
    channel->SustainRate    = *si++ & 0x1F;
    channel->ReleaseRate    = *si & 0x0F;
    channel->SustainLevel   = ((*si++ >> 4) & 0x0F) ^ 0x0F;
    channel->AttackLevel    = *si++ & 0x0F;

    // Move from normal to expanded?
    if (channel->SSGEnvelopFlag != -1)
    {
        channel->SSGEnvelopFlag = -1;

        channel->ExtendedCount = 4; // RR
        channel->ExtendedAttackLevel = 0; // Volume
    }

    return si;
}

uint8_t * pmd_driver_t::SetSSGPortamentoCommand(channel_t * channel, uint8_t * si)
{
    if (channel->PartMask != 0x00)
    {
        // Set to 'rest'.
        channel->Factor = 0;
        channel->Tone   = 0xFF;
        channel->_Size = si[2];
        channel->_Data   = si + 3;
        channel->KeyOnFlag++;

        if (--_Driver._VolumeBoostCount)
            channel->VolumeBoost = 0;

        _Driver._IsTieSet = false;
        _Driver._VolumeBoostCount = 0;

        _Driver._LoopCheck &= channel->_LoopCheck;

        return si + 3; // Skip when masking
    }

    SetSSGTone(channel, TransposeSSG(channel, StartPCMLFO(channel, *si++)));

    int bx_ = (int) channel->Factor;
    int al_ = channel->Tone;

    SetSSGTone(channel, TransposeSSG(channel, *si++));

    int ax = (int) channel->Factor;   // ax = portamento destination psg_tune value

    channel->Tone = al_;
    channel->Factor = (uint32_t) bx_; // bx = portamento original psg_tune value

    ax -= bx_;

    channel->_Size = *si++;

    si = CalculateQ(channel, si);

    channel->PortamentoQuotient = ax / channel->_Size;
    channel->PortamentoRemainder = ax % channel->_Size;
    channel->HardwareLFOModulationMode |= 0x08; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
    {
        if (--_Driver._VolumeBoostCount)
        {
            channel->VolumeBoost = 0;

            _Driver._VolumeBoostCount = 0;
        }
    }

    SetSSGVolume(channel);
    SetSSGPitch(channel);
    SSGKeyOn(channel);

    channel->KeyOnFlag++;
    channel->_Data = si;

    _Driver._IsTieSet = false;
    _Driver._VolumeBoostCount = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver._LoopCheck &= channel->_LoopCheck;

    return si;
}

/// <summary>
/// Sets the SSG effect to play. 15.6. SSG Sound Effect Playback, Command 'n number'
/// </summary>
uint8_t * pmd_driver_t::SetSSGEffect(channel_t * channel, uint8_t * si)
{
    const int EffectNumber = *si++;

    if (channel->PartMask != 0x00)
        return si;

    if (EffectNumber != 0)
    {
        _SSGEffect.Flags = 0x01; // Correct the pitch.

        SSGEffectMain(channel, EffectNumber);
    }
    else
        SSGStopEffect();

    return si;
}

/// <summary>
/// 6.6. Noise Frequency Setting, Command 'w ±number'
/// </summary>
uint8_t * pmd_driver_t::SetSSGNoiseFrequencyCommand(uint8_t * si)
{
    _SSGNoiseFrequency += *(int8_t *) si++;

    if (_SSGNoiseFrequency < 0)
        _SSGNoiseFrequency = 0;
    else
    if (_SSGNoiseFrequency > 31)
        _SSGNoiseFrequency = 31;

    return si;
}

/// <summary>
/// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// </summary>
uint8_t * pmd_driver_t::SetSSGChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept
{
    const uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->PartMask |= 0x40;

            if (channel->PartMask == 0x40)
            {
                const int ah = ((1 << (_Driver._CurrentChannel - 1)) | (4 << _Driver._CurrentChannel));
                const uint32_t al = _OPNAW->GetReg(0x07);

                _OPNAW->SetReg(0x07, ah | al);
            }
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->PartMask &= 0xBF; // 1011 1111

    return si;
}

/// <summary>
/// Decides to stop the SSG drum and reset the SSG.
/// </summary>
bool pmd_driver_t::CheckSSGDrum(channel_t * channel, int al)
{
    // Do not stop the drum during the SSG mask. SSG drums are not playing.
    if ((channel->PartMask & 0x01) || ((channel->PartMask & 0x02) == 0))
        return false;

    // Do not turn off normal sound effects.
    if (_SSGEffect._Priority >= 2)
        return false;

    // Don't stop the drums during rests.
    if ((al & 0x0F) == 0x0F)
        return false;

    // Is the SSG drum still playing?
    if (_SSGEffect._Priority == 1)
        SSGStopEffect(); // Turn off the SSG drum.

    channel->PartMask &= 0xFD;

    return (channel->PartMask == 0x00);
}

uint8_t * pmd_driver_t::DecreaseSSGVolumeCommand(channel_t *, uint8_t * si)
{
    const int al = *(int8_t *) si++;

    if (al != 0)
        _State.SSGVolumeAdjust = std::clamp(al + _State.SSGVolumeAdjust, 0, 255);
    else
        _State.SSGVolumeAdjust = _State.DefaultSSGVolumeAdjust;

    return si;
}
