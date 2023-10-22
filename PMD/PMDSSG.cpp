
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

void PMD::SSGMain(Channel * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    channel->Length--;

    if (_Driver.UsePPS && (channel == &_SSGChannel[2]) && (_State.kshot_dat != 0) && (channel->Length <= channel->qdat))
    {
        // When using PPS & SSG 3ch & when playing SSG sound effects.
        SetSSGKeyOff(channel);

        _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), 0U);   // Force the soundto sto.

        channel->KeyOffFlag = 0xFF;
    }

    if (channel->MuteMask)
        channel->KeyOffFlag = 0xFF;
    else
    if ((channel->KeyOffFlag & 0x03) == 0)
    {
        if (channel->Length <= channel->qdat)
        {
            SetSSGKeyOff(channel);

            channel->KeyOffFlag = 0xFF;
        }
    }

    if (channel->Length == 0)
    {
        channel->LFOSwitch &= 0xf7;

        // DATA READ
        while (1)
        {
            if ((*si == 0xda) && CheckSSGDrum(channel, *si))
            {
                si++;
            }
            else
            if (*si > 0x80 && *si != 0xda)
            {
                si = ExecuteSSGCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->Data = si;
                channel->loopcheck = 3;
                channel->Note = 0xFF;

                if (channel->LoopData == nullptr)
                {
                    if (channel->MuteMask)
                    {
                        _Driver.TieMode = 0;
                        _Driver.IsVolumePushSet = 0;
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
                if (*si == 0xda)
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
                        si = channel->Rest(++si, (--_Driver.IsVolumePushSet) != 0);

                        _Driver.TieMode = 0;
                        _Driver.IsVolumePushSet = 0;
                        break;
                    }
                }

                //  TONE SET
                SetSSGTone(channel, oshiftp(channel, StartPCMLFO(channel, *si++)));

                channel->Length = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumePush != 0) && (channel->Note != 0xFF))
                {
                    if (--_Driver.IsVolumePushSet)
                    {
                        _Driver.IsVolumePushSet = 0;
                        channel->VolumePush = 0;
                    }
                }

                SetSSGVolume(channel);
                SetSSGPitch(channel);
                SetSSGKeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

                _Driver.TieMode = 0;
                _Driver.IsVolumePushSet = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    _Driver.lfo_switch = (channel->LFOSwitch & 8);

    if (channel->LFOSwitch)
    {
        if (channel->LFOSwitch & 3)
        {
            if (SetSSGLFO(channel))
            {
                _Driver.lfo_switch |= (channel->LFOSwitch & 3);
            }
        }

        if (channel->LFOSwitch & 0x30)
        {
            SwapLFO(channel);

            if (SetSSGLFO(channel))
            {
                SwapLFO(channel);

                _Driver.lfo_switch |= (channel->LFOSwitch & 0x30);
            }
            else
                SwapLFO(channel);
        }

        if (_Driver.lfo_switch & 0x19)
        {
            if (_Driver.lfo_switch & 0x08)
                CalculatePortamento(channel);

            // Do not operate while resting on SSG channel 3 and SSG drum is sounding.
            if (!(!_Driver.UsePPS && (channel == &_SSGChannel[2]) && (channel->Note == 0xFF) && (_State.kshot_dat != 0)))
                SetSSGPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if ((temp != 0) || _Driver.lfo_switch & 0x22 || (_State.FadeOutSpeed != 0))
    {
        // Do not set volume while resting on SSG channel 3 and SSG drum is sounding.
        if (!(!_Driver.UsePPS && (channel == &_SSGChannel[2]) && (channel->Note == 0xFF) && (_State.kshot_dat != 0)))
            SetSSGVolume(channel);
    }

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecuteSSGCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si++; break;

        // Set early Key Off Timeout
        case 0xFE:
            channel->EarlyKeyOffTimeout = *si++;
            channel->EarlyKeyOffTimeoutRandomRange = 0;
            break;

        case 0xFD:
            channel->Volume = *si++;
            break;

        case 0xFC:
            si = ChangeTempoCommand(si);
            break;

        // Command "&": Tie notes together.
        case 0xFB:
            _Driver.TieMode |= 1;
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

        case 0xf5:
            channel->shift = *(int8_t *) si++;
            break;

        case 0xf4:
            if (channel->Volume < 15)
                channel->Volume++;
            break;

        case 0xf3:
            if (channel->Volume > 0)
                channel->Volume--;
            break;

        case 0xf2:
            si = SetLFOParameter(channel, si);
            break;

        case 0xf1:
            si = lfoswitch(channel, si);
            break;

        case 0xf0:
            si = SetSSGEnvelopeFormat1Command(channel, si);
            break;

        case 0xef:
            _OPNAW->SetReg(*si, *(si + 1));
            si += 2;
            break;

        case 0xee:
            _State.SSGNoiseFrequency = *si++;
            break;

        case 0xed:
            channel->SSGPattern = *si++;
            break;

        case 0xec: si++; break;

        case 0xeb:
            si = RhythmInstrumentCommand(si);
            break;

        case 0xea:
            si = SetRhythmInstrumentVolumeCommand(si);
            break;

        case 0xe9:
            si = SetRhythmPanCommand(si);
            break;

        case 0xe8:
            si = SetRhythmMasterVolumeCommand(si);
            break;

        case 0xe7:
            channel->shift += *(int8_t *) si++;
            break;

        case 0xe6:
            si = SetRhythmVolume(si);
            break;

        case 0xe5:
            si = SetRhythmPanValue(si);
            break;

        case 0xe4: si++; break;

        case 0xe3:
            channel->Volume += *si++;

            if (channel->Volume > 15)
                channel->Volume = 15;
            break;

        case 0xe2:
            channel->Volume -= *si++;

            if (channel->Volume < 0)
                channel->Volume = 0;
            break;

        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf:
            _State.BarLength = *si++; // Command "Z number"
            break;

        case 0xde:
            si = SetSSGVolumeCommand(channel, si);
            break;

        case 0xdd:
            si = DecreaseVolumeCommand(channel, si);
            break;

        case 0xdc:
            _State.status = *si++;
            break;

        case 0xdb:
            _State.status += *si++;
            break;

        case 0xda:
            si = SetSSGPortamentoCommand(channel, si);
            break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6:
            channel->MDepthSpeedA = channel->MDepthSpeedB = *si++;
            channel->MDepth = *(int8_t *) si++;
            break;

        case 0xd5:
            channel->DetuneValue += *(int16_t *) si;
            si += 2;
            break;

        case 0xd4:
            si = SetSSGEffect(channel, si);
            break;

        case 0xd3:
            si = SetFMEffect(channel, si);
            break;

        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;

        case 0xd0:
            si = SetSSGPseudoEchoCommand(si);
            break;

        case 0xcf: si++; break;
        case 0xce: si += 6; break;

        case 0xcd:
            si = SetSSGEnvelopeFormat2Command(channel, si);
            break;

        case 0xcc:
            channel->extendmode = (channel->extendmode & 0xfe) | (*si++ & 1);
            break;

        case 0xcb:
            channel->LFOWaveform = *si++;
            break;

        case 0xca:
            channel->extendmode = (channel->extendmode & 0xFD) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            channel->extendmode = (channel->extendmode & 0xFB) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;

        // Set Early Key Off Timeout Percentage. Stops note (length * pp / 100h) ticks early, added to value of command FE.
        case 0xC4:
            channel->EarlyKeyOffTimeoutPercentage = *si++;
            break;

        case 0xc3: si += 2; break;

        case 0xc2:
            channel->delay = channel->delay2 = *si++;
            lfoinit_main(channel);
            break;

        case 0xc1: break;

        case 0xc0:
            si = SetSSGMaskCommand(channel, si);
            break;

        case 0xbf:
            SwapLFO(channel);

            si = SetLFOParameter(channel, si);

            SwapLFO(channel);
            break;

        case 0xbe:
            channel->LFOSwitch = (channel->LFOSwitch & 0x8f) | ((*si++ & 7) << 4);

            SwapLFO(channel);

            lfoinit_main(channel);

            SwapLFO(channel);
            break;

        case 0xbd:
            SwapLFO(channel);

            channel->MDepthSpeedA = channel->MDepthSpeedB = *si++;
            channel->MDepth = *(int8_t *) si++;

            SwapLFO(channel);
            break;

        case 0xbc:
            SwapLFO(channel);

            channel->LFOWaveform = *si++;

            SwapLFO(channel);
            break;

        case 0xbb:
            SwapLFO(channel);

            channel->extendmode = (channel->extendmode & 0xFD) | ((*si++ & 1) << 1);

            SwapLFO(channel);
            break;

        case 0xba: si++; break;

        case 0xb9:
            SwapLFO(channel);

            channel->delay = channel->delay2 = *si++;
            lfoinit_main(channel);

// FIXME    break;

            SwapLFO(channel);
            break;

        case 0xb8: si += 2; break;

        case 0xb7:
            si = SetMDepthCountCommand(channel, si);
            break;

        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;

        case 0xb3:
            channel->qdat2 = *si++;
            break;

        case 0xb2:
            channel->shift_def = *(int8_t *) si++;
            break;

        // Set Early Key Off Timeout Randomizer Range. (0..tt ticks, added to the value of command C4 and FE)
        case 0xB1:
            channel->EarlyKeyOffTimeoutRandomRange = *si++;
            break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::DecreaseSSGVolumeCommand(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.SSGVolumeDown = Limit(al + _State.SSGVolumeDown, 255, 0);
    else
        _State.SSGVolumeDown = _State.DefaultSSGVolumeDown;

    return si;
}

/// <summary>
/// Plays an SSG drum or sound effect.
/// </summary>
/// <param name="channel"></param>
/// <param name="al">Sound effect number</param>
void PMD::SSGPlayEffect(Channel * channel, int al)
{
    if (_Driver.UsePPS)
    {
        al |= 0x80;

        if (_Effect.PreviousNumber == al)
            _PPS->Stop();
        else
            _Effect.PreviousNumber = al;
    }

    _Effect.Flags = 0x03; // Correct the pitch and volume (K part).

    EffectMain(channel, al);
}

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

    if (channel->envf == -1)
    {
        channel->envf = 2; // RR
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
    if (channel->envf != -1)
    {
        channel->envf = -1;

        channel->ExtendedCount = 4; // RR
        channel->ExtendedAttackLevel = 0; // Volume
    }

    return si;
}

// Command "v": Sets the SSG volume.
uint8_t * PMD::SetSSGVolumeCommand(Channel * channel, uint8_t * si)
{
    int al = channel->Volume + *si++;

    if (al > 15)
        al = 15;

    channel->VolumePush = ++al;
    _Driver.IsVolumePushSet = 1;

    return si;
}

uint8_t * PMD::SetSSGPortamentoCommand(Channel * channel, uint8_t * si)
{
    if (channel->MuteMask)
    {
        // Set to 'rest'.
        channel->fnum = 0;
        channel->Note = 0xFF;
        channel->Length = *(si + 2);
        channel->KeyOnFlag++;
        channel->Data = si + 3;

        if (--_Driver.IsVolumePushSet)
            channel->VolumePush = 0;

        _Driver.TieMode = 0;
        _Driver.IsVolumePushSet = 0;
        _Driver.loop_work &= channel->loopcheck;

        return si + 3; // Skip when masking
    }

    SetSSGTone(channel, oshiftp(channel, StartPCMLFO(channel, *si++)));

    int bx_ = (int) channel->fnum;
    int al_ = channel->Note;

    SetSSGTone(channel, oshiftp(channel, *si++));

    int ax = (int) channel->fnum;   // ax = portamento destination psg_tune value

    channel->Note = al_;
    channel->fnum = (uint32_t) bx_; // bx = portamento original psg_tune value

    ax -= bx_;

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->porta_num2 = ax / channel->Length;
    channel->porta_num3 = ax % channel->Length;
    channel->LFOSwitch |= 8; // Enable portamento.

    if ((channel->VolumePush != 0) && (channel->Note != 0xFF))
    {
        if (--_Driver.IsVolumePushSet)
        {
            _Driver.IsVolumePushSet = 0;
            channel->VolumePush = 0;
        }
    }

    SetSSGVolume(channel);
    SetSSGPitch(channel);
    SetSSGKeyOn(channel);

    channel->KeyOnFlag++;
    channel->Data = si;

    _Driver.TieMode = 0;
    _Driver.IsVolumePushSet = 0;

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

    if (_State.SSGNoiseFrequency > 31)
        _State.SSGNoiseFrequency = 31;

    return si;
}

// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
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

void PMD::SetSSGTone(Channel * channel, int al)
{
    if ((al & 0x0F) == 0x0F)
    {
        channel->Note = 0xFF; // Kyufu Nara FNUM Ni 0 Set

        if (channel->LFOSwitch & 0x11)
            return;

        channel->fnum = 0;  // Pitch LFO not used

        return;
    }

    channel->Note = al;

    int cl = (al >> 4) & 0x0F;  // cl=oct
    int bx = al & 0x0F;      // bx=onkai
    int ax = psg_tune_data[bx];

    if (cl > 0)
    {
        ax >>= cl - 1;

        int carry = ax & 1;

        ax = (ax >> 1) + carry;
    }

    channel->fnum = (uint32_t) ax;
}

void PMD::SetSSGVolume(Channel * channel)
{
    if (channel->envf == 3 || (channel->envf == -1 && channel->ExtendedCount == 0))
        return;

    int dl = (channel->VolumePush) ? channel->VolumePush - 1 : channel->Volume;

    //  音量down計算
    dl = ((256 - _State.SSGVolumeDown) * dl) >> 8;

    //  Fadeout計算
    dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

    //  ENVELOPE 計算
    if (dl <= 0)
    {
        _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), 0);

        return;
    }

    if (channel->envf == -1)
    {
        if (channel->ExtendedAttackLevel == 0)
        {
            _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), 0);

            return;
        }

        dl = ((((dl * (channel->ExtendedAttackLevel + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        dl += channel->ExtendedAttackLevel;

        if (dl <= 0)
        {
            _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), 0);

            return;
        }

        if (dl > 15)
            dl = 15;
    }


    //  音量LFO計算
    if ((channel->LFOSwitch & 0x22) == 0)
    {
        _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), (uint32_t) dl);
        return;
    }

    int ax = (channel->LFOSwitch & 2) ? channel->lfodat : 0;

    if (channel->LFOSwitch & 0x20)
        ax += channel->_lfodat;

    dl += ax;

    if (dl < 0)
    {
        _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), 0);
        return;
    }

    if (dl > 15)
        dl = 15;

    //  出力
    _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), (uint32_t) dl);
}

void PMD::SetSSGPitch(Channel * channel)
{
    if (channel->fnum == 0)
        return;

    // SSG Portamento set
    int ax = (int) (channel->fnum + channel->porta_num);
    int dx = 0;

    // SSG Detune/LFO set
    if ((channel->extendmode & 0x01) == 0)
    {
        ax -= channel->DetuneValue;

        if (channel->LFOSwitch & 1)
            ax -= channel->lfodat;

        if (channel->LFOSwitch & 0x10)
            ax -= channel->_lfodat;
    }
    else
    {
        // 拡張DETUNE(DETUNE)の計算
        if (channel->DetuneValue)
        {
            dx = (ax * channel->DetuneValue) >> 12;

            if (dx >= 0)
                dx++;
            else
                dx--;

            ax -= dx;
        }

        // 拡張DETUNE(LFO)の計算
        if (channel->LFOSwitch & 0x11)
        {
            if (channel->LFOSwitch & 1)
                dx = channel->lfodat;
            else
                dx = 0;

            if (channel->LFOSwitch & 0x10)
                dx += channel->_lfodat;

            if (dx != 0)
            {
                dx = (ax * dx) >> 12;

                if (dx >= 0)
                    dx++;
                else
                    dx--;
            }

            ax -= dx;
        }
    }

    // TONE SET
    if (ax >= 0x1000)
    {
        if (ax >= 0)
            ax = 0xfff;
        else
            ax = 0;
    }

    _OPNAW->SetReg((uint32_t) ((_Driver.CurrentChannel - 1) * 2),     (uint32_t) LOBYTE(ax));
    _OPNAW->SetReg((uint32_t) ((_Driver.CurrentChannel - 1) * 2 + 1), (uint32_t) HIBYTE(ax));
}

void PMD::SetSSGKeyOn(Channel * channel)
{
    if (channel->Note == 0xFF)
        return;

    int ah = (1 << (_Driver.CurrentChannel - 1)) | (1 << (_Driver.CurrentChannel + 2));
    int al = ((int32_t) _OPNAW->GetReg(0x07) | ah);

    ah = ~(ah & channel->SSGPattern);
    al &= ah;

    _OPNAW->SetReg(7, (uint32_t) al);

    // Set the SSG noise frequency.
    if ((_State.SSGNoiseFrequency != _State.OldSSGNoiseFrequency) && (_Effect.Priority == 0))
    {
        _OPNAW->SetReg(6, (uint32_t) _State.SSGNoiseFrequency);

        _State.OldSSGNoiseFrequency = _State.SSGNoiseFrequency;
    }
}

void PMD::SetSSGKeyOff(Channel * channel)
{
    if (channel->Note == 0xFF)
        return;

    if (channel->envf != -1)
        channel->envf = 2;
    else
        channel->ExtendedCount = 4;
}

// Sets SSG Wait after register output.
void PMD::SetSSGDelay(int nsec)
{
    _OPNAW->SetSSGDelay(nsec);
}
