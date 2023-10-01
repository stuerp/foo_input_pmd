
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

    // KEYOFF CHECK & Keyoff
    if (_Driver.UsePPS && (channel == &_SSGTrack[2]) && (_State.kshot_dat != 0) && (channel->Length <= channel->qdat))
    {
        // When using PPS & SSG 3ch & when playing SSG sound effects.
        SetSSGKeyOff(channel);

        _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), 0U);   // Force the soundto sto.

        channel->keyoff_flag = -1;
    }

    if (channel->PartMask)
        channel->keyoff_flag = -1;
    else
    if ((channel->keyoff_flag & 3) == 0) // Already keyed-off?
    {
        if (channel->Length <= channel->qdat)
        {
            SetSSGKeyOff(channel);
            channel->keyoff_flag = -1;
        }
    }

    if (channel->Length == 0)
    {
        channel->lfoswi &= 0xf7;

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
                channel->Tone = 255;

                if (channel->LoopData == nullptr)
                {
                    if (channel->PartMask)
                    {
                        _Driver.TieMode = 0;
                        _Driver.volpush_flag = 0;
                        _Driver.loop_work &= channel->loopcheck;
                        return;
                    }
                    else
                        break;
                }

                // In case there was an "L" command.
                si = channel->LoopData;
                channel->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {
                    si = SetSSGPortamento(channel, ++si);

                    _Driver.loop_work &= channel->loopcheck;

                    return;
                }
                else
                if (channel->PartMask)
                {
                    if (!CheckSSGDrum(channel, *si))
                    {
                        si++;

                        channel->fnum = 0;      // Set to "rest".
                        channel->Tone = 255;
                        channel->Length = *si++;
                        channel->keyon_flag++;
                        channel->Data = si;

                        if (--_Driver.volpush_flag)
                            channel->volpush = 0;

                        _Driver.TieMode = 0;
                        _Driver.volpush_flag = 0;
                        break;
                    }
                }

                //  TONE SET
                SetSSGTone(channel, oshiftp(channel, StartPCMLFO(channel, *si++)));

                channel->Length = *si++;
                si = CalculateQ(channel, si);

                if (channel->volpush && channel->Tone != 255)
                {
                    if (--_Driver.volpush_flag)
                    {
                        _Driver.volpush_flag = 0;
                        channel->volpush = 0;
                    }
                }

                SetSSGVolume2(channel);
                SetSSGPitch(channel);
                SetSSGKeyOn(channel);

                channel->keyon_flag++;
                channel->Data = si;

                _Driver.TieMode = 0;
                _Driver.volpush_flag = 0;
                channel->keyoff_flag = 0;

                if (*si == 0xfb)
                    channel->keyoff_flag = 2; // If "&" command is immediately followed, SetFMKeyOff is not performed.

                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    _Driver.lfo_switch = (channel->lfoswi & 8);

    if (channel->lfoswi)
    {
        if (channel->lfoswi & 3)
        {
            if (SetSSGLFO(channel))
            {
                _Driver.lfo_switch |= (channel->lfoswi & 3);
            }
        }

        if (channel->lfoswi & 0x30)
        {
            SwapLFO(channel);

            if (SetSSGLFO(channel))
            {
                SwapLFO(channel);

                _Driver.lfo_switch |= (channel->lfoswi & 0x30);
            }
            else
                SwapLFO(channel);
        }

        if (_Driver.lfo_switch & 0x19)
        {
            if (_Driver.lfo_switch & 0x08)
                CalculatePortamento(channel);

            // Do not operate while resting on SSG channel 3 and SSG drum is sounding.
            if (!(!_Driver.UsePPS && (channel == &_SSGTrack[2]) && (channel->Tone == 255) && (_State.kshot_dat != 0)))
                SetSSGPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if ((temp != 0) || _Driver.lfo_switch & 0x22 || (_State.FadeOutSpeed != 0))
    {
        // Do not set volume while resting on SSG channel 3 and SSG drum is sounding.
        if (!(!_Driver.UsePPS && (channel == &_SSGTrack[2]) && (channel->Tone == 255) && (_State.kshot_dat != 0)))
            SetSSGVolume2(channel);
    }

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecuteSSGCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si++; break;
        case 0xfe: channel->qdata = *si++; channel->qdat3 = 0; break;
        case 0xfd: channel->volume = *si++; break;
        case 0xfc: si = ChangeTempoCommand(si); break;

        case 0xfb:
            _Driver.TieMode |= 1;
            break;

        case 0xfa: channel->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = SetStartOfLoopCommand(channel, si); break;
        case 0xf8: si = SetEndOfLoopCommand(channel, si); break;
        case 0xf7: si = ExitLoopCommand(channel, si); break;
        case 0xf6: channel->LoopData = si; break;
        case 0xf5: channel->shift = *(int8_t *) si++; break;
        case 0xf4: if (channel->volume < 15) channel->volume++; break;
        case 0xf3: if (channel->volume > 0) channel->volume--; break;
        case 0xf2: si = SetLFOParameter(channel, si); break;
        case 0xf1: si = lfoswitch(channel, si); break;
        case 0xf0: si = psgenvset(channel, si); break;

        case 0xef: _OPNAW->SetReg(*si, *(si + 1)); si += 2; break;
        case 0xee: _State.SSGNoiseFrequency = *si++; break;
        case 0xed: channel->psgpat = *si++; break;

        case 0xec: si++; break;
        case 0xeb: si = RhythmInstrumentCommand(si); break;
        case 0xea: si = SetRhythmInstrumentVolumeCommand(si); break;
        case 0xe9: si = SetRhythmOutputPosition(si); break;
        case 0xe8: si = SetRhythmMasterVolumeCommand(si); break;

        case 0xe7: channel->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: si++; break;

        case 0xe3: channel->volume += *si++; if (channel->volume > 15) channel->volume = 15; break;
        case 0xe2: channel->volume -= *si++; if (channel->volume < 0) channel->volume = 0; break;


        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = SetSSGVolumeCommand(channel, si); break;
        case 0xdd: si = DecreaseSoundSourceVolumeCommand(channel, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si = SetSSGPortamento(channel, si); break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6: channel->mdspd = channel->mdspd2 = *si++; channel->mdepth = *(int8_t *) si++; break;
        case 0xd5: channel->detune += *(int16_t *) si; si += 2; break;

        case 0xd4: si = SetSSGEffect(channel, si); break;
        case 0xd3: si = SetFMEffect(channel, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si = SetSSGPseudoEchoCommand(si); break;

        case 0xcf: si++; break;
        case 0xce: si += 6; break;
        case 0xcd: si = SetSSGEnvelopeSpeedToExtend(channel, si); break;
        case 0xcc:
            channel->extendmode = (channel->extendmode & 0xfe) | (*si++ & 1);
            break;

        case 0xcb: channel->lfo_wave = *si++; break;
        case 0xca:
            channel->extendmode = (channel->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            channel->extendmode = (channel->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: channel->qdatb = *si++; break;
        case 0xc3: si += 2; break;
        case 0xc2: channel->delay = channel->delay2 = *si++; lfoinit_main(channel); break;
        case 0xc1: break;
        case 0xc0: si = ssg_mml_part_mask(channel, si); break;

        case 0xbf:
            SwapLFO(channel);

            si = SetLFOParameter(channel, si);

            SwapLFO(channel);
            break;

        case 0xbe:
            channel->lfoswi = (channel->lfoswi & 0x8f) | ((*si++ & 7) << 4);

            SwapLFO(channel);

            lfoinit_main(channel);

            SwapLFO(channel);
            break;

        case 0xbd:
            SwapLFO(channel);

            channel->mdspd = channel->mdspd2 = *si++;
            channel->mdepth = *(int8_t *) si++;

            SwapLFO(channel);
            break;

        case 0xbc:
            SwapLFO(channel);

            channel->lfo_wave = *si++;

            SwapLFO(channel);
            break;

        case 0xbb:
            SwapLFO(channel);

            channel->extendmode = (channel->extendmode & 0xfd) | ((*si++ & 1) << 1);

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
        case 0xb7: si = mdepth_count(channel, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;

        case 0xb3: channel->qdat2 = *si++; break;
        case 0xb2: channel->shift_def = *(int8_t *) si++; break;
        case 0xb1: channel->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

// Command "v": Sets the SSG volume.
uint8_t * PMD::SetSSGVolumeCommand(Channel * channel, uint8_t * si)
{
    int al = channel->volume + *si++;

    if (al > 15)
        al = 15;

    channel->volpush = ++al;
    _Driver.volpush_flag = 1;

    return si;
}

uint8_t * PMD::SetSSGPortamento(Channel * channel, uint8_t * si)
{
    if (channel->PartMask)
    {
        channel->fnum = 0;    // Set to "rest"
        channel->Tone = 255;
        channel->Length = *(si + 2);
        channel->keyon_flag++;
        channel->Data = si + 3;

        if (--_Driver.volpush_flag)
            channel->volpush = 0;

        _Driver.TieMode = 0;
        _Driver.volpush_flag = 0;
        _Driver.loop_work &= channel->loopcheck;

        return si + 3; // Skip when masking
    }

    SetSSGTone(channel, oshiftp(channel, StartPCMLFO(channel, *si++)));

    int bx_ = (int) channel->fnum;
    int al_ = channel->Tone;

    SetSSGTone(channel, oshiftp(channel, *si++));

    int ax = (int) channel->fnum;   // ax = portamento destination psg_tune value

    channel->Tone = al_;
    channel->fnum = (uint32_t) bx_; // bx = portamento original psg_tune value

    ax -= bx_;

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->porta_num2 = ax / channel->Length;    // Quotient
    channel->porta_num3 = ax % channel->Length;    // Remainder
    channel->lfoswi |= 8;        // Porta ON

    if (channel->volpush && channel->Tone != 255)
    {
        if (--_Driver.volpush_flag)
        {
            _Driver.volpush_flag = 0;
            channel->volpush = 0;
        }
    }

    SetSSGVolume2(channel);
    SetSSGPitch(channel);
    SetSSGKeyOn(channel);

    channel->keyon_flag++;
    channel->Data = si;

    _Driver.TieMode = 0;
    _Driver.volpush_flag = 0;
    channel->keyoff_flag = 0;

    if (*si == 0xfb) // If there is '&' immediately after, SetFMKeyOff will not be done.
        channel->keyoff_flag = 2;

    _Driver.loop_work &= channel->loopcheck;

    return si;
}

//  SSG Envelope Speed (Extend)
uint8_t * PMD::SetSSGEnvelopeSpeedToExtend(Channel * channel, uint8_t * si)
{
    channel->eenv_ar = *si++ & 0x1f;
    channel->eenv_dr = *si++ & 0x1f;
    channel->eenv_sr = *si++ & 0x1f;
    channel->eenv_rr = *si & 0x0f;
    channel->eenv_sl = ((*si++ >> 4) & 0x0f) ^ 0x0f;
    channel->eenv_al = *si++ & 0x0f;

    if (channel->envf != -1)
    {  // Did you move from normal to expanded?
        channel->envf = -1;
        channel->eenv_count = 4;    // RR
        channel->eenv_volume = 0;  // Volume
    }

    return si;
}

// Command "n": SSG Sound Effect Playback
uint8_t * PMD::SetSSGEffect(Channel * channel, uint8_t * si)
{
    int al = *si++;

    if (channel->PartMask)
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

/// <summary>
/// Decides to stop the SSG drum and reset the SSG.
/// </summary>
bool PMD::CheckSSGDrum(Channel * channel, int al)
{
    // Do not stop the drum during the SSG mask. SSG drums are not playing.
    if ((channel->PartMask & 0x01) || ((channel->PartMask & 0x02) == 0))
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

    channel->PartMask &= 0xFD;

    return (channel->PartMask == 0x00);
}

void PMD::SetSSGTone(Channel * channel, int al)
{
    if ((al & 0x0f) == 0x0f)
    {
        channel->Tone = 255; // Kyufu Nara FNUM Ni 0 Set

        if (channel->lfoswi & 0x11)
            return;

        channel->fnum = 0;  // Pitch LFO not used

        return;
    }

    channel->Tone = al;

    int cl = (al >> 4) & 0x0f;  // cl=oct
    int bx = al & 0x0f;      // bx=onkai
    int ax = psg_tune_data[bx];

    if (cl > 0)
    {
        ax >>= cl - 1;

        int carry = ax & 1;

        ax = (ax >> 1) + carry;
    }

    channel->fnum = (uint32_t) ax;
}

void PMD::SetSSGVolume2(Channel * channel)
{
    if (channel->envf == 3 || (channel->envf == -1 && channel->eenv_count == 0))
        return;

    int dl = (channel->volpush) ? channel->volpush - 1 : channel->volume;

    //  音量down計算
    dl = ((256 - _State.ssg_voldown) * dl) >> 8;

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
        if (channel->eenv_volume == 0)
        {
            _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), 0);

            return;
        }

        dl = ((((dl * (channel->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        dl += channel->eenv_volume;

        if (dl <= 0)
        {
            _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), 0);

            return;
        }

        if (dl > 15)
            dl = 15;
    }


    //  音量LFO計算
    if ((channel->lfoswi & 0x22) == 0)
    {
        _OPNAW->SetReg((uint32_t) (_Driver.CurrentChannel + 8 - 1), (uint32_t) dl);
        return;
    }

    int ax = (channel->lfoswi & 2) ? channel->lfodat : 0;

    if (channel->lfoswi & 0x20)
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
    if ((channel->extendmode & 1) == 0)
    {
        ax -= channel->detune;

        if (channel->lfoswi & 1)
            ax -= channel->lfodat;

        if (channel->lfoswi & 0x10)
            ax -= channel->_lfodat;
    }
    else
    {
        // 拡張DETUNE(DETUNE)の計算
        if (channel->detune)
        {
            dx = (ax * channel->detune) >> 12;    // dx:ax=ax * qq->detune

            if (dx >= 0)
                dx++;
            else
                dx--;

            ax -= dx;
        }

        // 拡張DETUNE(LFO)の計算
        if (channel->lfoswi & 0x11)
        {
            if (channel->lfoswi & 1)
                dx = channel->lfodat;
            else
                dx = 0;

            if (channel->lfoswi & 0x10)
                dx += channel->_lfodat;

            if (dx)
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
    if (channel->Tone == 255)
        return;

    int ah = (1 << (_Driver.CurrentChannel - 1)) | (1 << (_Driver.CurrentChannel + 2));
    int al = ((int32_t) _OPNAW->GetReg(0x07) | ah);

    ah = ~(ah & channel->psgpat);
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
    if (channel->Tone == 255)
        return;

    if (channel->envf != -1)
        channel->envf = 2;
    else
        channel->eenv_count = 4;
}
