
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

void PMD::ADPCMMain(Channel * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    channel->Length--;

    if (channel->PartMask)
    {
        channel->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((channel->keyoff_flag & 3) == 0)
        {    // 既にSetFMKeyOffしたか？
            if (channel->Length <= channel->qdat)
            {
                keyoffm(channel);
                channel->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (channel->Length == 0)
    {
        channel->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = ExecuteADPCMCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->Data = si;
                channel->loopcheck = 3;
                channel->Tone = 255;

                if (channel->LoopData == NULL)
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

                // "L"があった時
                si = channel->LoopData;
                channel->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {        // ポルタメント
                    si = portam(channel, ++si);

                    _Driver.loop_work &= channel->loopcheck;

                    return;
                }
                else
                if (channel->PartMask)
                {
                    si++;
                    channel->fnum = 0;    //休符に設定
                    channel->Tone = 255;
                    //          qq->onkai_def = 255;
                    channel->Length = *si++;
                    channel->keyon_flag++;
                    channel->Data = si;

                    if (--_Driver.volpush_flag)
                        channel->volpush = 0;

                    _Driver.TieMode = 0;
                    _Driver.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumsetm(channel, oshift(channel, StartPCMLFO(channel, *si++)));

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

                SetADPCMVolumeCommand(channel);
                SetADPCMPitch(channel);

                if (channel->keyoff_flag & 1)
                    keyonm(channel);

                channel->keyon_flag++;
                channel->Data = si;

                _Driver.TieMode = 0;
                _Driver.volpush_flag = 0;

                if (*si == 0xfb)
                {   // Do not SetFMKeyOff if '&' immediately follows
                    channel->keyoff_flag = 2;
                }
                else
                {
                    channel->keyoff_flag = 0;
                }

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
            if (lfo(channel))
                _Driver.lfo_switch |= (channel->lfoswi & 3);
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

            SetADPCMPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if ((temp != 0) || _Driver.lfo_switch & 0x22 || (_State.FadeOutSpeed != 0))
        SetADPCMVolumeCommand(channel);

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecuteADPCMCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comatm(channel, si); break;
        case 0xfe: channel->qdata = *si++; break;
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
        case 0xf4:
            if (channel->volume < (255 - 16)) channel->volume += 16;
            else channel->volume = 255;
            break;

        case 0xf3: if (channel->volume < 16) channel->volume = 0; else channel->volume -= 16; break;
        case 0xf2: si = SetLFOParameter(channel, si); break;
        case 0xf1: si = lfoswitch(channel, si); break;
        case 0xf0: si = psgenvset(channel, si); break;

        case 0xef: _OPNAW->SetReg((uint32_t) (0x100 + *si), (uint32_t) (*(si + 1))); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;

        case 0xec: si = pansetm(channel, si); break;        // FOR SB2
        case 0xeb: si = RhythmInstrumentCommand(si); break;
        case 0xea: si = SetRhythmInstrumentVolumeCommand(si); break;
        case 0xe9: si = SetRhythmOutputPosition(si); break;
        case 0xe8: si = SetRhythmMasterVolumeCommand(si); break;

        case 0xe7: channel->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: si++; break;

        case 0xe3:
            if (channel->volume < (255 - (*si))) channel->volume += (*si);
            else channel->volume = 255;
            si++;
            break;

        case 0xe2:
            if (channel->volume < *si) channel->volume = 0; else channel->volume -= *si;
            si++;
            break;

        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = vol_one_up_pcm(channel, si); break;
        case 0xdd: si = DecreaseSoundSourceVolumeCommand(channel, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si = portam(channel, si); break;

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
        case 0xd0: si++; break;

        case 0xcf: si++; break;
        case 0xce: si = pcmrepeat_set(channel, si); break;
        case 0xcd: si = SetSSGEnvelopeSpeedToExtend(channel, si); break;
        case 0xcc: si++; break;
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
        case 0xc3: si = pansetm_ex(channel, si); break;
        case 0xc2: channel->delay = channel->delay2 = *si++; lfoinit_main(channel); break;
        case 0xc1: break;
        case 0xc0: si = pcm_mml_part_mask(channel, si); break;
        case 0xbf: SwapLFO(channel); si = SetLFOParameter(channel, si); SwapLFO(channel); break;
        case 0xbe: si = _lfoswitch(channel, si); break;
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

        case 0xba: si = _volmask_set(channel, si); break;
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
        case 0xb4: si = ppz_extpartset(channel, si); break;
        case 0xb3: channel->qdat2 = *si++; break;
        case 0xb2: channel->shift_def = *(int8_t *) si++; break;
        case 0xb1: channel->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

void PMD::SetADPCMVolumeCommand(Channel * channel)
{
    int al = channel->volpush ? channel->volpush : channel->volume;

    //  音量down計算
    al = ((256 - _State.pcm_voldown) * al) >> 8;

    //  Fadeout計算
    if (_State.FadeOutVolume)
        al = (((256 - _State.FadeOutVolume) * (256 - _State.FadeOutVolume) >> 8) * al) >> 8;

    //  ENVELOPE 計算
    if (al == 0)
    {
        _OPNAW->SetReg(0x10b, 0);

        return;
    }

    if (channel->envf == -1)
    {
        //  拡張版 音量=al*(eenv_vol+1)/16
        if (channel->eenv_volume == 0)
        {
            _OPNAW->SetReg(0x10b, 0);
            return;
        }

        al = ((((al * (channel->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        if (channel->eenv_volume < 0)
        {
            int ah = -channel->eenv_volume * 16;

            if (al < ah)
            {
                _OPNAW->SetReg(0x10b, 0);
                return;
            }
            else
                al -= ah;
        }
        else
        {
            int ah = channel->eenv_volume * 16;

            if (al + ah > 255)
                al = 255;
            else
                al += ah;
        }
    }

    //  音量LFO計算
    if ((channel->lfoswi & 0x22) == 0)
    {
        _OPNAW->SetReg(0x10b, (uint32_t) al);

        return;
    }

    int dx = (channel->lfoswi & 2) ? channel->lfodat : 0;

    if (channel->lfoswi & 0x20)
        dx += channel->_lfodat;

    if (dx >= 0)
    {
        al += dx;

        if (al & 0xff00)
            _OPNAW->SetReg(0x10b, 255);
        else
            _OPNAW->SetReg(0x10b, (uint32_t) al);
    }
    else
    {
        al += dx;

        if (al < 0)
            _OPNAW->SetReg(0x10b, 0);
        else
            _OPNAW->SetReg(0x10b, (uint32_t) al);
    }
}

void PMD::SetADPCMPitch(Channel * channel)
{
    if (channel->fnum == 0)
        return;

    // Portament/LFO/Detune SET
    int bx = (int) (channel->fnum + channel->porta_num);
    int dx = (int) (((channel->lfoswi & 0x11) && (channel->lfoswi & 1)) ? dx = channel->lfodat : 0);

    if (channel->lfoswi & 0x10)
        dx += channel->_lfodat;

    dx *= 4;  // PCM ﾊ LFO ｶﾞ ｶｶﾘﾆｸｲ ﾉﾃﾞ depth ｦ 4ﾊﾞｲ ｽﾙ

    dx += channel->detune;

    if (dx >= 0)
    {
        bx += dx;

        if (bx > 0xffff)
            bx = 0xffff;
    }
    else
    {
        bx += dx;

        if (bx < 0)
            bx = 0;
    }

    // TONE SET
    _OPNAW->SetReg(0x109, (uint32_t) LOBYTE(bx));
    _OPNAW->SetReg(0x10a, (uint32_t) HIBYTE(bx));
}
