
// $VER: PMDSSG.cpp (2023.10.29) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

void PMD::SSGMain(Channel * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    channel->Length--;

    // When using the PPS and SSG channel 3 and the SSG is playing sound effects.
    if (_Driver.UsePPS && (channel == &_SSGChannels[2]) && !_State.UseRhythmChannel && (channel->Length <= channel->GateTime))
    {
        SSGKeyOff(channel);

        _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), 0U); // Stop playing.

        channel->KeyOffFlag = 0xFF;
    }

    if (channel->MuteMask)
        channel->KeyOffFlag = 0xFF;
    else
    if ((channel->KeyOffFlag & 0x03) == 0)
    {
        if (channel->Length <= channel->GateTime)
        {
            SSGKeyOff(channel);

            channel->KeyOffFlag = 0xFF;
        }
    }

    if (channel->Length == 0)
    {
        channel->ModulationMode &= 0xF7;

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
                channel->Data = si;
                channel->loopcheck = 3;
                channel->Tone = 0xFF;

                if (channel->LoopData == nullptr)
                {
                    if (channel->MuteMask)
                    {
                        _Driver.TieNotesTogether = false;
                        _Driver.IsVolumeBoostSet = 0;
                        _Driver.loop_work &= channel->loopcheck;
                        return;
                    }
                    else
                        break;
                }

                si = channel->LoopData;

                channel->loopcheck = 1;
            }
            else
            {
                if (*si == 0xDA)
                {
                    si = SetSSGPortamentoCommand(channel, ++si);

                    _Driver.loop_work &= channel->loopcheck;

                    return;
                }
                else
                if (channel->MuteMask)
                {
                    if (!CheckSSGDrum(channel, *si))
                    {
/*
                        si++;

                        // Set to 'rest'.
                        channel->fnum = 0;
                        channel->Note = 0xFF;
                        channel->Length = *si++;

                        channel->KeyOnFlag++;
                        channel->Data = si;

                        if (--_Driver.IsVolumePushSet)
                            channel->VolumePush = 0;
*/
                        si = channel->Rest(++si, (--_Driver.IsVolumeBoostSet) != 0);

                        _Driver.TieNotesTogether = false;
                        _Driver.IsVolumeBoostSet = 0;
                        break;
                    }
                }

                //  TONE SET
                SetSSGTone(channel, TransposeSSG(channel, StartPCMLFO(channel, *si++)));

                channel->Length = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
                {
                    if (--_Driver.IsVolumeBoostSet)
                    {
                        _Driver.IsVolumeBoostSet = 0;
                        channel->VolumeBoost = 0;
                    }
                }

                SetSSGVolume(channel);
                SetSSGPitch(channel);
                SSGKeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

                _Driver.TieNotesTogether = false;
                _Driver.IsVolumeBoostSet = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    _Driver.ModulationMode = (channel->ModulationMode & 0x08);

    if (channel->ModulationMode != 0x00)
    {
        if (channel->ModulationMode & 0x03)
        {
            if (SetSSGLFO(channel))
                _Driver.ModulationMode |= (channel->ModulationMode & 0x03);
        }

        if (channel->ModulationMode & 0x30)
        {
            SwapLFO(channel);

            if (SetSSGLFO(channel))
            {
                SwapLFO(channel);

                _Driver.ModulationMode |= (channel->ModulationMode & 0x30);
            }
            else
                SwapLFO(channel);
        }

        if (_Driver.ModulationMode & 0x19)
        {
            if (_Driver.ModulationMode & 0x08)
                CalculatePortamento(channel);

            // Do not operate while using SSG channel 3 and the SSG drum is playing.
            if (!(!_Driver.UsePPS && (channel == &_SSGChannels[2]) && (channel->Tone == 0xFF) && !_State.UseRhythmChannel))
                SetSSGPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if ((temp != 0) || _Driver.ModulationMode & 0x22 || (_State.FadeOutSpeed != 0))
    {
        // Do not operate while using SSG channel 3 and the SSG drum is playing.
        if (!(!_Driver.UsePPS && (channel == &_SSGChannels[2]) && (channel->Tone == 0xFF) && !_State.UseRhythmChannel))
            SetSSGVolume(channel);
    }

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecuteSSGCommand(Channel * channel, uint8_t * si)
{
    uint8_t Command = *si++;

    switch (Command)
    {
        case 0xFF: si++; break;

        // Set early Key Off Timeout.
        case 0xFE:
            channel->EarlyKeyOffTimeout = *si++;
            channel->EarlyKeyOffTimeoutRandomRange = 0;
            break;
/*
        case 0xFD:
            channel->Volume = *si++;
            break;

        case 0xFC:
            si = ChangeTempoCommand(si);
            break;

        // Command "&": Tie notes together.
        case 0xFB:
            _Driver.TieNotesTogether = true;
            break;

        // Set detune.
        case 0xFA:
            channel->DetuneValue = *(int16_t *) si;
            si += 2;
            break;

        // Set loop start.
        case 0xF9:
            si = SetStartOfLoopCommand(channel, si);
            break;

        // Set loop end.
        case 0xF8:
            si = SetEndOfLoopCommand(channel, si);
            break;

        // Exit loop.
        case 0xF7:
            si = ExitLoopCommand(channel, si);
            break;

        // Command "L": Set the loop data.
        case 0xF6:
            channel->LoopData = si;
            break;

        // Set transposition.
        case 0xF5:
            channel->Transposition = *(int8_t *) si++;
            break;
*/
        // Increase volume by 3dB.
        case 0xF4:
            if (channel->Volume < 15)
                channel->Volume++;
            break;

        // Decrease volume by 3dB.
        case 0xF3:
            if (channel->Volume > 0)
                channel->Volume--;
            break;
/*
        case 0xF2:
            si = SetModulation(channel, si);
            break;

        case 0xF1:
            si = SetModulationMask(channel, si);
            break;

        case 0xF0:
            si = SetSSGEnvelopeFormat1Command(channel, si);
            break;
*/
        // Set SSG envelope.
        case 0xEF:
            _OPNAW->SetReg(si[0], si[1]);
            si += 2;
            break;

        case 0xEE:
            _State.SSGNoiseFrequency = *si++;
            break;

        case 0xED:
            channel->SSGMask = *si++;
            break;

        case 0xEC: si++; break;
/*
        case 0xEB:
            si = OPNARhythmKeyOn(si);
            break;

        case 0xEA:
            si = SetOPNARhythmVolumeCommand(si);
            break;

        case 0xE9:
            si = SetOPNARhythmPanningCommand(si);
            break;

        case 0xE8:
            si = SetOPNARhythmMasterVolumeCommand(si);
            break;

        // Modify transposition.
        case 0xE7:
            channel->Transposition += *(int8_t *) si++;
            break;

        case 0xE6:
            si = ModifyOPNARhythmMasterVolume(si);
            break;

        case 0xE5:
            si = ModifyOPNARhythmVolume(si);
            break;

        case 0xE4: si++; break;
*/
        // Increase volume.
        case 0xE3:
            channel->Volume += *si++;

            if (channel->Volume > 15)
                channel->Volume = 15;
            break;

        // Decrease volume.
        case 0xE2:
            channel->Volume -= *si++;

            if (channel->Volume < 0)
                channel->Volume = 0;
            break;
/*
        case 0xE1: si++; break;
        case 0xE0: si++; break;

        // Command "Z number": Set ticks per measure.
        case 0xDF:
            _State.BarLength = *si++;
            break;
*/
        case 0xDE:
            si = IncreaseVolumeForNextNote(channel, si, 15);
            break;
/*
        case 0xDD:
            si = DecreaseVolumeForNextNote(channel, si);
            break;

        // Set status.
        case 0xDC:
            _State.Status = *si++;
            break;

        // Increment status.
        case 0xDB:
            _State.Status += *si++;
            break;
*/
        // Set portamento.
        case 0xDA:
            si = SetSSGPortamentoCommand(channel, si);
            break;
/*
        case 0xD9: si++; break;
        case 0xD8: si++; break;
        case 0xD7: si++; break;

        case 0xD6:
            channel->LFO1MDepthSpeed1 = channel->LFO1MDepthSpeed2 = *si++;
            channel->LFO1MDepth = *(int8_t *) si++;
            break;

        case 0xD5:
            channel->DetuneValue += *(int16_t *) si;
            si += 2;
            break;

        case 0xD4:
            si = SetSSGEffect(channel, si);
            break;

        case 0xD3:
            si = SetFMEffect(channel, si);
            break;

        case 0xD2:
            _State.FadeOutSpeed = *si++;
            _State.FadeOutSpeedSet = true;
            break;

        case 0xD1: si++; break;
*/
        case 0xD0:
            si = SetSSGPseudoEchoCommand(si);
            break;
/*
        case 0xCF: si++; break;

        // Set PCM Repeat.
        case 0xCE: si += 6; break;

        // Set SSG Envelope (Format 2).
        case 0xCD:
            si = SetSSGEnvelopeFormat2Command(channel, si);
            break;
*/
        // Set SSG Extend Mode (bit 0).
        case 0xCC:
            channel->ExtendMode = (channel->ExtendMode & 0xFE) | (*si++ & 0x01);
            break;
/*
        case 0xCB:
            channel->LFO1Waveform = *si++;
            break;
*/
        // Set SSG Extend Mode (bit 1).
        case 0xCA:
            channel->ExtendMode = (channel->ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);
            break;

        // Set SSG Extend Mode (bit 2).
        case 0xC9:
            channel->ExtendMode = (channel->ExtendMode & 0xFB) | ((*si++ & 0x01) << 2);
            break;
/*
        case 0xC8: si += 3; break;
        case 0xC7: si += 3; break;
        case 0xC6: si += 6; break;
        case 0xC5: si++; break;

        // Set Early Key Off Timeout Percentage. Stops note (length * pp / 100h) ticks early, added to value of command FE.
        case 0xC4:
            channel->EarlyKeyOffTimeoutPercentage = *si++;
            break;
*/
        case 0xC3: si += 2; break;
/*
        case 0xC2:
            channel->Delay1 = channel->Delay2 = *si++;
            InitializeLFOMain(channel);
            break;
*/
        case 0xC1: break;

        case 0xC0:
            si = SetSSGMaskCommand(channel, si);
            break;
/*
        case 0xBF:
            SwapLFO(channel);

            si = SetModulation(channel, si);

            SwapLFO(channel);
            break;
*/
        case 0xBE:
            channel->ModulationMode = (channel->ModulationMode & 0x8F) | ((*si++ & 0x07) << 4);

            SwapLFO(channel);

            InitializeLFOMain(channel);

            SwapLFO(channel);
            break;
/*
        case 0xBD:
            SwapLFO(channel);

            channel->LFO1MDepthSpeed1 = channel->LFO1MDepthSpeed2 = *si++;
            channel->LFO1MDepth = *(int8_t *) si++;

            SwapLFO(channel);
            break;

        case 0xBC:
            SwapLFO(channel);

            channel->LFO1Waveform = *si++;

            SwapLFO(channel);
            break;

        case 0xBB:
            SwapLFO(channel);

            channel->ExtendMode = (channel->ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);

            SwapLFO(channel);
            break;
*/
        case 0xBA: si++; break;
/*
        case 0xB9:
            SwapLFO(channel);

            channel->LFO1Delay1 = channel->LFO1Delay2 = *si++;
            InitializeLFOMain(channel);

            SwapLFO(channel);
            break;

        case 0xB8: si += 2; break;

        case 0xB7:
            si = SetMDepthCountCommand(channel, si);
            break;

        case 0xB6: si++; break;
        case 0xB5: si += 2; break;
        case 0xB4: si += 16; break;

        // Set Early Key Off Timeout 2. Stop note after n ticks or earlier depending on the result of B1/C4/FE happening first.
        case 0xB3:
            channel->EarlyKeyOffTimeout2 = *si++;
            break;

        case 0xB2:
            channel->Transposition2 = *(int8_t *) si++;
            break;

        // Set Early Key Off Timeout Randomizer Range. (0..tt ticks, added to the value of command C4 and FE)
        case 0xB1:
            channel->EarlyKeyOffTimeoutRandomRange = *si++;
            break;
*/
        default:
            si = ExecuteCommand(channel, si, Command);
    }

    return si;
}

/// <summary>
/// Sets SSG Wait after register output.
/// </summary>
void PMD::SetSSGDelay(int nsec)
{
    _OPNAW->SetSSGDelay(nsec);
}

#pragma region(Commands)
/// <summary>
///
/// </summary>
void PMD::SetSSGTone(Channel * channel, int tone)
{
    if ((tone & 0x0F) != 0x0F)
    {
        channel->Tone = tone;

        int Octave = (tone & 0xF0) >> 4;
        int Note   = (tone & 0x0F);

        int Factor = SSGScaleFactor[Note];

        if (Octave > 0)
        {
            Factor >>= Octave - 1;

            int Carry = Factor & 1;

            Factor = (Factor >> 1) + Carry;
        }

        channel->Factor = (uint32_t) Factor;
    }
    else
    {
        channel->Tone = 0xFF;

        if (channel->ModulationMode & 0x11)
            return;

        channel->Factor = 0; // Don't use LFO pitch.

        return;
    }
}

/// <summary>
///
/// </summary>
void PMD::SetSSGVolume(Channel * channel)
{
    if ((channel->SSGEnvelopFlag == 3) || ((channel->SSGEnvelopFlag == -1) && (channel->ExtendedCount == 0)))
        return;

    uint32_t Register = (uint32_t) (_Driver.CurrentChannel + 8 - 1);

    int dl = (channel->VolumeBoost) ? channel->VolumeBoost - 1 : channel->Volume;

    // Volume Down calculation
    dl = ((256 - _State.SSGVolumeAdjust) * dl) >> 8;

    // Fade-out calclation
    dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

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
    if ((channel->ModulationMode & 0x22) == 0)
    {
        _OPNAW->SetReg(Register, (uint32_t) dl);
        return;
    }

    {
        int ax = (channel->ModulationMode & 0x02) ? channel->LFO1Data : 0;

        if (channel->ModulationMode & 0x20)
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
void PMD::SetSSGPitch(Channel * channel)
{
    if (channel->Factor == 0)
        return;

    int Pitch = (int) (channel->Factor + channel->Portamento);

    {
        int dx = 0;

        // SSG Detune/LFO set
        if ((channel->ExtendMode & 0x01) == 0)
        {
            Pitch -= channel->DetuneValue;

            if (channel->ModulationMode & 0x01)
                Pitch -= channel->LFO1Data;

            if (channel->ModulationMode & 0x10)
                Pitch -= channel->LFO2Data;
        }
        else
        {
            // Calculating extended DETUNE (DETUNE)
            if (channel->DetuneValue)
            {
                dx = (Pitch * channel->DetuneValue) >> 12;

                if (dx >= 0)
                    dx++;
                else
                    dx--;

                Pitch -= dx;
            }

            // Extended DETUNE (LFO) calculation
            if (channel->ModulationMode & 0x11)
            {
                if (channel->ModulationMode & 0x01)
                    dx = channel->LFO1Data;
                else
                    dx = 0;

                if (channel->ModulationMode & 0x10)
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

    _OPNAW->SetReg((uint32_t) ((_Driver.CurrentChannel - 1) * 2),     (uint32_t) LOBYTE(Pitch));
    _OPNAW->SetReg((uint32_t) ((_Driver.CurrentChannel - 1) * 2 + 1), (uint32_t) HIBYTE(Pitch));
}

/// <summary>
///
/// </summary>
void PMD::SSGKeyOn(Channel * channel)
{
    if (channel->Tone == 0xFF)
        return;

    int ah = (1 << (_Driver.CurrentChannel - 1)) | (1 << (_Driver.CurrentChannel + 2));
    int al = ((int32_t) _OPNAW->GetReg(0x07) | ah);

    ah = ~(ah & channel->SSGMask);
    al &= ah;

    _OPNAW->SetReg(7, (uint32_t) al);

    // Set the SSG noise frequency.
    if ((_State.SSGNoiseFrequency != _State.OldSSGNoiseFrequency) && (_Effect.Priority == 0))
    {
        _OPNAW->SetReg(6, (uint32_t) _State.SSGNoiseFrequency);

        _State.OldSSGNoiseFrequency = _State.SSGNoiseFrequency;
    }
}

/// <summary>
///
/// </summary>
void PMD::SSGKeyOff(Channel * channel)
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
void PMD::SetSSGInstrument(Channel * channel, int instrumentNumber)
{
    if (_Driver.UsePPS)
    {
        instrumentNumber |= 0x80;

        if (_Effect.PreviousInstrumentNumber == instrumentNumber)
            _PPS->Stop();
        else
            _Effect.PreviousInstrumentNumber = instrumentNumber;
    }

    _Effect.Flags = 0x03; // Correct the pitch and volume (K command).

    EffectMain(channel, instrumentNumber);
}
#pragma endregion

//  Command "E number1, number2, number3, number4": Set the SSG Envelope (Format 1).
uint8_t * PMD::SetSSGEnvelopeFormat1Command(Channel * channel, uint8_t * si)
{
    channel->AttackDuration = *si;
    channel->ExtendedAttackDuration = *si++;
    channel->DecayDepth = *(int8_t *) si++;
    channel->SustainRate = *si;
    channel->ExtendedSustainRate = *si++;
    channel->ReleaseRate = *si;
    channel->ExtendedReleaseRate = *si++;

    if (channel->SSGEnvelopFlag == -1)
    {
        channel->SSGEnvelopFlag = 2; // RR
        channel->ExtendedAttackLevel = -15; // Volume
    }

    return si;
}

//  Command "E number1, number2, number3, number4, number5 [,number6]": Set the SSG Envelope (Format 2).
uint8_t * PMD::SetSSGEnvelopeFormat2Command(Channel * channel, uint8_t * si)
{
    channel->AttackDuration = *si++ & 0x1F;
    channel->DecayDepth = *si++ & 0x1F;
    channel->SustainRate = *si++ & 0x1F;
    channel->ReleaseRate = *si & 0x0F;
    channel->SustainLevel = ((*si++ >> 4) & 0x0F) ^ 0x0F;
    channel->AttackLevel = *si++ & 0x0F;

    // Move from normal to expanded?
    if (channel->SSGEnvelopFlag != -1)
    {
        channel->SSGEnvelopFlag = -1;

        channel->ExtendedCount = 4; // RR
        channel->ExtendedAttackLevel = 0; // Volume
    }

    return si;
}

uint8_t * PMD::SetSSGPortamentoCommand(Channel * channel, uint8_t * si)
{
    if (channel->MuteMask)
    {
        // Set to 'rest'.
        channel->Factor = 0;
        channel->Tone = 0xFF;
        channel->Length = si[2];
        channel->KeyOnFlag++;
        channel->Data = si + 3;

        if (--_Driver.IsVolumeBoostSet)
            channel->VolumeBoost = 0;

        _Driver.TieNotesTogether = false;
        _Driver.IsVolumeBoostSet = 0;
        _Driver.loop_work &= channel->loopcheck;

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

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->PortamentoQuotient = ax / channel->Length;
    channel->PortamentoRemainder = ax % channel->Length;
    channel->ModulationMode |= 0x08; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
    {
        if (--_Driver.IsVolumeBoostSet)
        {
            channel->VolumeBoost = 0;

            _Driver.IsVolumeBoostSet = 0;
        }
    }

    SetSSGVolume(channel);
    SetSSGPitch(channel);
    SSGKeyOn(channel);

    channel->KeyOnFlag++;
    channel->Data = si;

    _Driver.TieNotesTogether = false;
    _Driver.IsVolumeBoostSet = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver.loop_work &= channel->loopcheck;

    return si;
}

// Command "n": SSG Sound Effect Playback
uint8_t * PMD::SetSSGEffect(Channel * channel, uint8_t * si)
{
    int al = *si++;

    if (channel->MuteMask)
        return si;

    if (al != 0)
    {
        _Effect.Flags = 0x01; // Correct the pitch.

        EffectMain(channel, al);
    }
    else
        StopEffect();

    return si;
}

// Command "w"
uint8_t * PMD::SetSSGPseudoEchoCommand(uint8_t * si)
{
    _State.SSGNoiseFrequency += *(int8_t *) si++;

    if (_State.SSGNoiseFrequency < 0)
        _State.SSGNoiseFrequency = 0;
    else
    if (_State.SSGNoiseFrequency > 31)
        _State.SSGNoiseFrequency = 31;

    return si;
}

/// <summary>
/// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// </summary>
uint8_t * PMD::SetSSGMaskCommand(Channel * channel, uint8_t * si)
{
    uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->MuteMask |= 0x40;

            if (channel->MuteMask == 0x40)
            {
                int ah = ((1 << (_Driver.CurrentChannel - 1)) | (4 << _Driver.CurrentChannel));
                uint32_t al = _OPNAW->GetReg(0x07);

                _OPNAW->SetReg(0x07, ah | al);
            }
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->MuteMask &= 0xBF; // 1011 1111

    return si;
}

/// <summary>
/// Decides to stop the SSG drum and reset the SSG.
/// </summary>
bool PMD::CheckSSGDrum(Channel * channel, int al)
{
    // Do not stop the drum during the SSG mask. SSG drums are not playing.
    if ((channel->MuteMask & 0x01) || ((channel->MuteMask & 0x02) == 0))
        return false;

    // Do not turn off normal sound effects.
    if (_Effect.Priority >= 2)
        return false;

    // Don't stop the drums during rests.
    if ((al & 0x0F) == 0x0F)
        return false;

    // Is the SSG drum still playing?
    if (_Effect.Priority == 1)
        StopEffect(); // Turn off the SSG drum.

    channel->MuteMask &= 0xFD;

    return (channel->MuteMask == 0x00);
}

uint8_t * PMD::DecreaseSSGVolumeCommand(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al != 0)
        _State.SSGVolumeAdjust = std::clamp(al + _State.SSGVolumeAdjust, 0, 255);
    else
        _State.SSGVolumeAdjust = _State.DefaultSSGVolumeAdjust;

    return si;
}
