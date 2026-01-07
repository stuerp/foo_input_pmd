
// $VER: PMDSSG.cpp (2026.01.05) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

/// <summary>
/// Main SSG processing
/// </summary>
void pmd_driver_t::SSGMain(channel_t * channel)
{
    if (channel->_Data == nullptr)
        return;

    uint8_t * si = channel->_Data;

    channel->_Size--;

    // When using the PPS and SSG channel 3 and the SSG is playing sound effects.
    if (_UsePPSForDrums && (channel == &_SSGChannels[2]) && !_State.UseRhythmChannel && (channel->_Size <= channel->_GateTime))
    {
        SSGKeyOff(channel);

        _OPNAW->SetReg((uint32_t) (_Driver._CurrentChannel + 8 - 1), 0U); // Stop playing.

        channel->_KeyOffFlag = -1;
    }

    if (channel->_PartMask != 0x00)
        channel->_KeyOffFlag = -1;
    else
    if ((channel->_KeyOffFlag & 0x03) == 0)
    {
        if (channel->_Size <= channel->_GateTime)
        {
            SSGKeyOff(channel);

            channel->_KeyOffFlag = -1;
        }
    }

    if (channel->_Size == 0)
    {
        channel->_HardwareLFO &= 0xF7;

        // DATA READ
        while (1)
        {
            if ((*si == 0xDA) && SSGCheckDrums(channel, *si))
            {
                si++;
            }
            else
            if ((*si != 0xDA) && (*si > 0x80))
            {
                si = SSGExecuteCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->_Data = si;
                channel->_Tone = 0xFF;

                channel->_LoopCheck = 0x03;

                if (channel->_LoopData == nullptr)
                {
                    if (channel->_PartMask != 0x00)
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
                    si = SSGSetPortamento(channel, ++si);

                    _Driver._LoopCheck &= channel->_LoopCheck;

                    return;
                }
                else
                if (channel->_PartMask != 0x00)
                {
                    if (!SSGCheckDrums(channel, *si))
                    {
                        si++;

                        // Set to 'rest'.
                        channel->_Factor = 0;
                        channel->_Tone   = 0xFF;
                        channel->_Size = *si++;
                        channel->_KeyOnFlag++;

                        channel->_Data = si;

                        if (--_Driver._VolumeBoostCount)
                            channel->VolumeBoost = 0;

                        _Driver._IsTieSet = false;
                        _Driver._VolumeBoostCount = 0;
                        break;
                    }
                }

                SSGSetTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

                channel->_Size = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumeBoost != 0) && (channel->_Tone != 0xFF))
                {
                    if (--_Driver._VolumeBoostCount)
                    {
                        _Driver._VolumeBoostCount = 0;
                        channel->VolumeBoost = 0;
                    }
                }

                SSGSetVolume(channel);
                SetSSGPitch(channel);
                SSGKeyOn(channel);

                channel->_KeyOnFlag++;
                channel->_Data = si;

                _Driver._IsTieSet = false;
                _Driver._VolumeBoostCount = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->_KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
        }
    }

    _Driver._HardwareLFOModulationMode = (channel->_HardwareLFO & 0x08);

    if (channel->_HardwareLFO != 0x00)
    {
        if (channel->_HardwareLFO & 0x03)
        {
            if (SetSSGLFO(channel))
                _Driver._HardwareLFOModulationMode |= (channel->_HardwareLFO & 0x03);
        }

        if (channel->_HardwareLFO & 0x30)
        {
            LFOSwap(channel);

            if (SetSSGLFO(channel))
            {
                LFOSwap(channel);

                _Driver._HardwareLFOModulationMode |= (channel->_HardwareLFO & 0x30);
            }
            else
                LFOSwap(channel);
        }

        if (_Driver._HardwareLFOModulationMode & 0x19)
        {
            if (_Driver._HardwareLFOModulationMode & 0x08)
                CalculatePortamento(channel);

            // Do not operate while using SSG channel 3 and the SSG drum is playing.
            if (!(!_UsePPSForDrums && (channel == &_SSGChannels[2]) && (channel->_Tone == 0xFF) && !_State.UseRhythmChannel))
                SetSSGPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if ((temp != 0) || _Driver._HardwareLFOModulationMode & 0x22 || (_State.FadeOutSpeed != 0))
    {
        // Do not operate while using SSG channel 3 and the SSG drum is playing.
        if (!(!_UsePPSForDrums && (channel == &_SSGChannels[2]) && (channel->_Tone == 0xFF) && !_State.UseRhythmChannel))
            SSGSetVolume(channel);
    }

    _Driver._LoopCheck &= channel->_LoopCheck;
}

/// <summary>
/// 
/// </summary>
uint8_t * pmd_driver_t::SSGExecuteCommand(channel_t * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        // 4.12. Sound Cut Setting 1, Command 'Q [%] numerical value' / 4.13. Sound Cut Setting 2, Command 'q [number1][-[number2]] [,number3]' / Command 'q [l length[.]][-[l length]] [,l length[.]]'
        case 0xFE:
        {
            channel->_EarlyKeyOffTimeout1 = *si++;
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
            channel->_SSGMask = *si++;
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
            si = SSGSetPortamento(channel, si);
            break;
        }

        // 6.6. Noise Frequency Setting, Command 'w ±number'
        case 0xD0:
        {
            si = SSGSetNoiseFrequency(si);
            break;
        }

        // 7.3. SSG Pitch Interval Correction Setting, Selects whether or not to adjust the SSG pitch interval, Set SSG Extend Mode (bit 0), Command 'DX number'
        case 0xCC:
        {
            channel->_ExtendMode = (channel->_ExtendMode & 0xFE) | (*si++ & 0x01);
            break;
        }

        // 8.2. Software Envelope Speed Setting, Set SSG Extend Mode (bit 2), Command 'EX number'
        case 0xC9:
        {
            channel->_ExtendMode = (channel->_ExtendMode & 0xFB) | ((*si++ & 0x01) << 2);
            break;
        }

        case 0xC3: si += 2; break;

        // 15.7. Channel Mask Control, Sets the channel mask to on or off, Command 'm number'
        case 0xC0:
        {
            si = SSGSetChannelMask(channel, si);
            break;
        }

        // 9.3. Software LFO Switch, Controls on/off and keyon synchronization of the software LFO, Command '*B number'
        case 0xBE:
        {
            channel->_HardwareLFO = (channel->_HardwareLFO & 0x8F) | ((*si++ & 0x07) << 4);

            LFOSwap(channel);

            LFOReset(channel);

            LFOSwap(channel);
            break;
        }

        default:
            si = ExecuteCommand(channel, si, Command);
    }

    return si;
}

/// <summary>
/// Sets SSG Wait after register output.
/// </summary>
void pmd_driver_t::SSGSetDelay(int nsec)
{
    _OPNAW->SetSSGDelay(nsec);
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SSGSetTone(channel_t * channel, int tone)
{
    if ((tone & 0x0F) != 0x0F)
    {
        channel->_Tone = tone;

        const int Octave = (tone & 0xF0) >> 4;
        const int Note   = (tone & 0x0F);

        int Factor = SSGScaleFactor[Note];

        if (Octave > 0)
        {
            Factor >>= Octave - 1;

            const int Carry = Factor & 1;

            Factor = (Factor >> 1) + Carry;
        }

        channel->_Factor = (uint32_t) Factor;
    }
    else
    {
        channel->_Tone = 0xFF;

        if (channel->_HardwareLFO & 0x11)
            return;

        channel->_Factor = 0; // Don't use LFO pitch.

        return;
    }
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SSGSetVolume(channel_t * channel)
{
    if ((channel->_SSGEnvelopFlag == 3) || ((channel->_SSGEnvelopFlag == -1) && (channel->ExtendedCount == 0)))
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

    if (channel->_SSGEnvelopFlag == -1)
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
    if ((channel->_HardwareLFO & 0x22) == 0)
    {
        _OPNAW->SetReg(Register, (uint32_t) dl);
        return;
    }

    {
        int ax = (channel->_HardwareLFO & 0x02) ? channel->_LFO1Data : 0;

        if (channel->_HardwareLFO & 0x20)
            ax += channel->_LFO2Data;

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
uint8_t * pmd_driver_t::SSGDecreaseVolume(channel_t *, uint8_t * si)
{
    const int al = *(int8_t *) si++;

    if (al != 0)
        _State.SSGVolumeAdjust = std::clamp(al + _State.SSGVolumeAdjust, 0, 255);
    else
        _State.SSGVolumeAdjust = _State.DefaultSSGVolumeAdjust;

    return si;
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SetSSGPitch(channel_t * channel)
{
    if (channel->_Factor == 0)
        return;

    int Pitch = (int) (channel->_Factor + channel->_Portamento);

    {
        int dx = 0;

        // SSG Detune/LFO set
        if ((channel->_ExtendMode & 0x01) == 0)
        {
            Pitch -= channel->_DetuneValue;

            if (channel->_HardwareLFO & 0x01)
                Pitch -= channel->_LFO1Data;

            if (channel->_HardwareLFO & 0x10)
                Pitch -= channel->_LFO2Data;
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
            if (channel->_HardwareLFO & 0x11)
            {
                if (channel->_HardwareLFO & 0x01)
                    dx = channel->_LFO1Data;
                else
                    dx = 0;

                if (channel->_HardwareLFO & 0x10)
                    dx += channel->_LFO2Data;

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
    if (channel->_Tone == 0xFF)
        return;

    // Enable tone or noise mode for channel A, B or C.
    int32_t ah = (1 << (_Driver._CurrentChannel - 1)) | (1 << (_Driver._CurrentChannel + 2));

    int al = ((int32_t) _OPNAW->GetReg(0x07) | ah);

    ah = ~(ah & channel->_SSGMask);
    al &= ah;

    // Enable the SSG channels (--XXXXXX, Noise A, B and C + Tone A, B and C)
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
    if (channel->_Tone == 0xFF)
        return;

    if (channel->_SSGEnvelopFlag != -1)
        channel->_SSGEnvelopFlag = 2;
    else
        channel->ExtendedCount = 4;
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SSGSetDrumInstrument(channel_t * channel, int instrumentNumber)
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

/// <summary>
/// Set the SSG Envelope (Format 1). Command "E number1, number2, number3, number4"
/// </summary>
uint8_t * pmd_driver_t::SSGSetEnvelope1(channel_t * channel, uint8_t * si)
{
    channel->AttackDuration         = *si;
    channel->ExtendedAttackDuration = *si++;
    channel->DecayDepth             = *(int8_t *) si++;
    channel->SustainRate            = *si;
    channel->ExtendedSustainRate    = *si++;
    channel->ReleaseRate            = *si;
    channel->ExtendedReleaseRate    = *si++;

    if (channel->_SSGEnvelopFlag == -1)
    {
        channel->_SSGEnvelopFlag = 2; // RR
        channel->ExtendedAttackLevel = -15; // Volume
    }

    return si;
}

/// <summary>
/// Set the SSG Envelope (Format 2). Command "E number1, number2, number3, number4, number5 [,number6]"
/// </summary>
uint8_t * pmd_driver_t::SSGSetEnvelope2(channel_t * channel, uint8_t * si)
{
    channel->AttackDuration = *si++ & 0x1F;
    channel->DecayDepth     = *si++ & 0x1F;
    channel->SustainRate    = *si++ & 0x1F;
    channel->ReleaseRate    = *si & 0x0F;
    channel->SustainLevel   = ((*si++ >> 4) & 0x0F) ^ 0x0F;
    channel->AttackLevel    = *si++ & 0x0F;

    // Move from normal to expanded?
    if (channel->_SSGEnvelopFlag != -1)
    {
        channel->_SSGEnvelopFlag = -1;

        channel->ExtendedCount = 4; // RR
        channel->ExtendedAttackLevel = 0; // Volume
    }

    return si;
}

/// <summary>
/// 
/// </summary>
uint8_t * pmd_driver_t::SSGSetPortamento(channel_t * channel, uint8_t * si)
{
    if (channel->_PartMask != 0x00)
    {
        // Set to 'rest'.
        channel->_Factor = 0;
        channel->_Tone   = 0xFF;
        channel->_Size = si[2];
        channel->_Data   = si + 3;
        channel->_KeyOnFlag++;

        if (--_Driver._VolumeBoostCount)
            channel->VolumeBoost = 0;

        _Driver._IsTieSet = false;
        _Driver._VolumeBoostCount = 0;

        _Driver._LoopCheck &= channel->_LoopCheck;

        return si + 3; // Skip when masking
    }

    SSGSetTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

    int bx_ = (int) channel->_Factor;
    int al_ = channel->_Tone;

    SSGSetTone(channel, Transpose(channel, *si++));

    int ax = (int) channel->_Factor;   // ax = portamento destination psg_tune value

    channel->_Tone = al_;
    channel->_Factor = (uint32_t) bx_; // bx = portamento original psg_tune value

    ax -= bx_;

    channel->_Size = *si++;

    si = CalculateQ(channel, si);

    channel->_PortamentoQuotient = ax / channel->_Size;
    channel->_PortamentoRemainder = ax % channel->_Size;
    channel->_HardwareLFO |= 0x08; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->_Tone != 0xFF))
    {
        if (--_Driver._VolumeBoostCount)
        {
            channel->VolumeBoost = 0;

            _Driver._VolumeBoostCount = 0;
        }
    }

    SSGSetVolume(channel);
    SetSSGPitch(channel);
    SSGKeyOn(channel);

    channel->_KeyOnFlag++;
    channel->_Data = si;

    _Driver._IsTieSet = false;
    _Driver._VolumeBoostCount = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->_KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver._LoopCheck &= channel->_LoopCheck;

    return si;
}

/// <summary>
/// Sets the SSG effect to play. 15.6. SSG Sound Effect Playback, Command 'n number'
/// </summary>
uint8_t * pmd_driver_t::SetSSGEffect(channel_t * channel, uint8_t * si)
{
    const int EffectNumber = *si++;

    if (channel->_PartMask != 0x00)
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
uint8_t * pmd_driver_t::SSGSetNoiseFrequency(uint8_t * si)
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
uint8_t * pmd_driver_t::SSGSetChannelMask(channel_t * channel, uint8_t * si) noexcept
{
    const uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->_PartMask |= 0x40;

            if (channel->_PartMask == 0x40)
            {
                const uint32_t NewMask = ((1 << (_Driver._CurrentChannel - 1)) | (4 << _Driver._CurrentChannel));
                const uint32_t OldMask = _OPNAW->GetReg(0x07);

                // Enable the SSG channels (--XXXXXX, Noise A, B and C + Tone A, B and C)
                _OPNAW->SetReg(0x07, NewMask | OldMask);
            }
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->_PartMask &= 0xBF; // 1011 1111

    return si;
}

/// <summary>
/// Decides to stop the SSG drum and reset the SSG.
/// </summary>
bool pmd_driver_t::SSGCheckDrums(channel_t * channel, int al)
{
    // Do not stop the drum during the SSG mask. SSG drums are not playing.
    if ((channel->_PartMask & 0x01) || ((channel->_PartMask & 0x02) == 0))
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

    channel->_PartMask &= 0xFD;

    return (channel->_PartMask == 0x00);
}
