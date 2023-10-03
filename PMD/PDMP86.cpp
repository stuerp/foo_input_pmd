
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

void PMD::PCM86Main(Channel * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    int    temp;

    channel->Length--;

    if (channel->PartMask)
    {
        channel->KeyOffFlag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((channel->KeyOffFlag & 3) == 0)
        {    // 既にSetFMKeyOffしたか？
            if (channel->Length <= channel->qdat)
            {
                SetP86KeyOff(channel);
                channel->KeyOffFlag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (channel->Length == 0)
    {
        while (1)
        {
//          if(*si > 0x80 && *si != 0xda) {
            if (*si > 0x80)
            {
                si = ExecutePCM86Command(channel, si);
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

                // "L"があった時
                si = channel->LoopData;
                channel->loopcheck = 1;
            }
            else
            {
                if (channel->PartMask)
                {
                    si++;
                    channel->fnum = 0;    //休符に設定
                    channel->Tone = 255;
                    //          qq->DefaultTone = 255;
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
                SetP86Tone(channel, oshift(channel, StartPCMLFO(channel, *si++)));

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

                SetPCM86Volume(channel);
                SetPCM86Pitch(channel);

                if (channel->KeyOffFlag & 1)
                    SetP86KeyOn(channel);

                channel->keyon_flag++;
                channel->Data = si;

                _Driver.TieMode = 0;
                _Driver.volpush_flag = 0;

                if (*si == 0xFB)
                    channel->KeyOffFlag = 2; // If '&' command (Tie) is immediately followed, Key Off is not performed.
                else
                    channel->KeyOffFlag = 0;

                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    if (channel->lfoswi & 0x22)
    {
        _Driver.lfo_switch = 0;

        if (channel->lfoswi & 2)
        {
            lfo(channel);

            _Driver.lfo_switch |= (channel->lfoswi & 2);
        }

        if (channel->lfoswi & 0x20)
        {
            SwapLFO(channel);

            if (lfo(channel))
            {
                SwapLFO(channel);

                _Driver.lfo_switch |= (channel->lfoswi & 0x20);
            }
            else
                SwapLFO(channel);
        }

        temp = SSGPCMSoftwareEnvelope(channel);

        if (temp || _Driver.lfo_switch & 0x22 || _State.FadeOutSpeed)
            SetPCM86Volume(channel);
    }
    else
    {
        temp = SSGPCMSoftwareEnvelope(channel);

        if (temp || _State.FadeOutSpeed)
            SetPCM86Volume(channel);
    }

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecutePCM86Command(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comat8(channel, si); break;
        case 0xfe: channel->qdata = *si++; break;
        case 0xfd: channel->Volume = *si++; break;
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
            if (channel->Volume < (255 - 16))
                channel->Volume += 16;
            else
                channel->Volume = 255;
            break;

        case 0xf3: if (channel->Volume < 16) channel->Volume = 0; else channel->Volume -= 16; break;
        case 0xf2: si = SetLFOParameter(channel, si); break;
        case 0xf1: si = lfoswitch(channel, si); break;
        case 0xf0: si = psgenvset(channel, si); break;

        case 0xef: _OPNAW->SetReg((uint32_t) (0x100 + *si), (uint32_t) (*(si + 1))); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;

        case 0xec: si = SetP86Panning(channel, si); break;        // FOR SB2
        case 0xeb: si = RhythmInstrumentCommand(si); break;
        case 0xea: si = SetRhythmInstrumentVolumeCommand(si); break;
        case 0xe9: si = SetRhythmOutputPosition(si); break;
        case 0xe8: si = SetRhythmMasterVolumeCommand(si); break;

        case 0xe7: channel->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: si++; break;

        case 0xe3:
            if (channel->Volume < (255 - (*si))) channel->Volume += (*si);
            else channel->Volume = 255;
            si++;
            break;

        case 0xe2:
            if (channel->Volume < *si) channel->Volume = 0; else channel->Volume -= *si;
            si++;
            break;

        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = vol_one_up_pcm(channel, si); break;
        case 0xdd: si = DecreaseSoundSourceVolumeCommand(channel, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si++; break;

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
        case 0xce: si = pcmrepeat_set8(channel, si); break;
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
        case 0xc3: si = SetP86PanningExtend(channel, si); break;
        case 0xc2:
            channel->delay = channel->delay2 = *si++;
            lfoinit_main(channel);
            break;

        case 0xc1: break;
        case 0xc0:
            si = pcm_mml_part_mask8(channel, si);
            break;

        case 0xbf: si += 4; break;
        case 0xbe: si++; break;
        case 0xbd: si += 2; break;
        case 0xbc: si++; break;
        case 0xbb: si++; break;
        case 0xba: si++; break;
        case 0xb9: si++; break;
        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(channel, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si = InitializePPZ(channel, si); break;

        case 0xb3: channel->qdat2 = *si++; break;
        case 0xb2: channel->shift_def = *(int8_t *) si++; break;
        case 0xb1: channel->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

void PMD::SetP86Tone(Channel * channel, int al)
{
    int ah = al & 0x0F;

    if (ah != 0x0F)
    {
        // Music Note
        if (_State.IsPMDB2Compatible && (al >= 0x65))
        {
            al = (ah < 5) ? 0x60 /* o7 */ : 0x50 /* o6 */;

            al |= ah;
        }

        channel->Tone = al;

        int bl = ((al & 0xF0) >> 4) * 12 + ah;

        channel->fnum = p86_tune_data[bl];
    }
    else
    {
        // Rest
        channel->Tone = 0xFF;

        if ((channel->lfoswi & 0x11) == 0)
            channel->fnum = 0; // Don't use LFO pitch.
    }
}

void PMD::SetPCM86Volume(Channel * channel)
{
    int al = channel->volpush ? channel->volpush : channel->Volume;

    //  音量down計算
    al = ((256 - _State.pcm_voldown) * al) >> 8;

    //  Fadeout計算
    if (_State.FadeOutVolume != 0)
        al = ((256 - _State.FadeOutVolume) * al) >> 8;

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
    int dx = (channel->lfoswi & 2) ? channel->lfodat : 0;

    if (channel->lfoswi & 0x20)
        dx += channel->_lfodat;

    if (dx >= 0)
    {
        if ((al += dx) > 255)
            al = 255;
    }
    else
    {
        if ((al += dx) < 0)
            al = 0;
    }

    if (!_State.IsPMDB2Compatible)
        al >>= 4;
    else
        al = (int) ::sqrt(al); // Make the volume Speakerboard-compatible.

    _P86->SetVol(al);
}

void PMD::SetPCM86Pitch(Channel * track)
{
    if (track->fnum == 0)
        return;

    int bl = (int) ((track->fnum & 0x0e00000) >> (16 + 5));
    int cx = (int) ( track->fnum & 0x01fffff);

    if (!_State.IsPMDB2Compatible && track->detune)
        cx = Limit((cx >> 5) + track->detune, 65535, 1) << 5;

    _P86->SetPitch(bl, (uint32_t) cx);
}

void PMD::SetP86KeyOn(Channel * channel)
{
    if (channel->Tone == 0xFF)
        return;

    _P86->Play();
}

void PMD::SetP86KeyOff(Channel * channel)
{
    _P86->Keyoff();

    if (channel->envf != -1)
    {
        if (channel->envf != 2)
            SetSSGKeyOff(channel);

        return;
    }

    if (channel->eenv_count != 4)
        SetSSGKeyOff(channel);
}

// Command "p"
uint8_t * PMD::SetP86Panning(Channel *, uint8_t * si)
{
    switch (*si++)
    {
        case 1: // Right
            _P86->SetPan(2, 1);
            break;

        case 2: // Left
            _P86->SetPan(1, 0);
            break;

        case 3: // Center
            _P86->SetPan(3, 0);
            break;

        default: // Reverse
            _P86->SetPan(3 | 4, 0);
    }

    return si;
}

uint8_t * PMD::SetP86PanningExtend(Channel * track, uint8_t * si)
{
    int    flag, data;

    track->Panning = (int8_t) *si++;
    _State.revpan = *si++;

    if (track->Panning == 0)
    {
        flag = 3;        // Center
        data = 0;
    }
    else
    if (track->Panning > 0)
    {
        flag = 2;        // Right
        data = 128 - track->Panning;
    }
    else
    {
        flag = 1;        // Left
        data = 128 + track->Panning;
    }

    if (_State.revpan != 1)
    {
        flag |= 4;        // 逆相
    }

    _P86->SetPan(flag, data);

    return si;
}
