
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

void PMD::FMMain(Channel * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    channel->Length--;

    if (channel->PartMask)
        channel->keyoff_flag = -1;
    else
    if ((channel->keyoff_flag & 3) == 0) // KEYOFF CHECK & Keyoff
    {
        // Already SetFMKeyOff?
        if (channel->Length <= channel->qdat)
        {
            SetFMKeyOff(channel);
            channel->keyoff_flag = -1;
        }
    }

    if (channel->Length == 0)
    {
        if (channel->PartMask == 0x00)
            channel->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = ExecuteFMCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->Data = si;
                channel->loopcheck = 3;
                channel->Tone = 0xFF;

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

                si = channel->LoopData; // When there was an "L"
                channel->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {
                    si = SetFMPortamentoCommand(channel, ++si);

                    _Driver.loop_work &= channel->loopcheck;

                    return;
                }
                else
                if (channel->PartMask == 0x00)
                {
                    // TONE SET
                    SetFMTone(channel, oshift(channel, StartLFO(channel, *si++)));

                    channel->Length = *si++;

                    si = CalculateQ(channel, si);

                    if (channel->volpush && (channel->Tone != 0xFF))
                    {
                        if (--_Driver.volpush_flag)
                        {
                            _Driver.volpush_flag = 0;
                            channel->volpush = 0;
                        }
                    }

                    SetFMVolumeCommand(channel);
                    SetFMPitch(channel);
                    SetFMKeyOn(channel);

                    channel->keyon_flag++;
                    channel->Data = si;

                    _Driver.TieMode = 0;
                    _Driver.volpush_flag = 0;

                    if (*si == 0xfb)
                        channel->keyoff_flag = 2; // Do not key off if '&' immediately follows
                    else
                        channel->keyoff_flag = 0;

                    _Driver.loop_work &= channel->loopcheck;

                    return;
                }
                else
                {
                    si++;

                    channel->fnum = 0; // Set to rest
                    channel->Tone = 0xFF;
                    channel->onkai_def = 0xFF;
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
        }
    }

    if (channel->PartMask == 0x00)
    {
        // Finish with LFO & Portament & Fadeout processing
        if (channel->hldelay_c)
        {
            if (--channel->hldelay_c == 0)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + (_Driver.CurrentChannel - 1 + 0xb4)), (uint32_t) channel->fmpan);
        }

        if (channel->sdelay_c)
        {
            if (--channel->sdelay_c == 0)
            {
                if ((channel->keyoff_flag & 1) == 0)
                    SetFMKeyOn(channel); // Already keyoffed?
            }
        }

        if (channel->lfoswi)
        {
            _Driver.lfo_switch = channel->lfoswi & 8;

            if (channel->lfoswi & 3)
            {
                if (lfo(channel))
                    _Driver.lfo_switch |= (channel->lfoswi & 3);
            }

            if (channel->lfoswi & 0x30)
            {
                SwapLFO(channel);

                if (lfo(channel))
                {
                    SwapLFO(channel);

                    _Driver.lfo_switch |= (channel->lfoswi & 0x30);
                }
                else
                    SwapLFO(channel);
            }

            if (_Driver.lfo_switch & 0x19)
            {
                if (_Driver.lfo_switch & 8)
                    CalculatePortamento(channel);

                SetFMPitch(channel);
            }

            if (_Driver.lfo_switch & 0x22)
            {
                SetFMVolumeCommand(channel);
                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }

        if (_State.FadeOutSpeed != 0)
            SetFMVolumeCommand(channel);
    }

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecuteFMCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = ChangeProgramCommand(channel, si); break;
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
        case 0xf4: if ((channel->volume += 4) > 127) channel->volume = 127; break;
        case 0xf3: if (channel->volume < 4) channel->volume = 0; else channel->volume -= 4; break;
        case 0xf2: si = SetLFOParameter(channel, si); break;
        case 0xf1: si = lfoswitch(channel, si); ch3_setting(channel); break;
        case 0xf0: si += 4; break;

        case 0xef:
            _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + *si), (uint32_t) (*(si + 1)));
            si += 2;
            break;

        case 0xee: si++; break;
        case 0xed: si++; break;

        case 0xec: si = panset(channel, si); break;        // FOR SB2

        case 0xeb: si = RhythmInstrumentCommand(si); break;
        case 0xea: si = SetRhythmInstrumentVolumeCommand(si); break;
        case 0xe9: si = SetRhythmOutputPosition(si); break;
        case 0xe8: si = SetRhythmMasterVolumeCommand(si); break;

        case 0xe7: channel->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: channel->hldelay = *si++; break;

        case 0xe3:
            if ((channel->volume += *si++) > 127)
                channel->volume = 127;
            break;

        case 0xe2:
            if (channel->volume < *si)
                channel->volume = 0;
            else
                channel->volume -= *si;
            si++;
            break;

        case 0xe1: si = hlfo_set(channel, si); break;
        case 0xe0: _State.port22h = *si; _OPNAW->SetReg(0x22, *si++); break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = IncreaseFMVolumeCommand(channel, si); break;
        case 0xdd: si = DecreaseSoundSourceVolumeCommand(channel, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si = SetFMPortamentoCommand(channel, si); break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6:
            channel->mdspd = channel->mdspd2 = *si++;
            channel->mdepth = *(int8_t *) si++;
            break;

        case 0xd5: channel->detune += *(int16_t *) si; si += 2; break;

        case 0xd4: si = SetSSGEffect(channel, si); break;
        case 0xd3: si = SetFMEffect(channel, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si++; break;

        case 0xcf:
            si = SetSlotMask(channel, si);
            break;

        case 0xce: si += 6; break;
        case 0xcd: si += 5; break;
        case 0xcc: si++; break;
        case 0xcb: channel->lfo_wave = *si++; break;
        case 0xca:
            channel->extendmode = (channel->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9: si++; break;
        case 0xc8: si = slotdetune_set(channel, si); break;
        case 0xc7: si = slotdetune_set2(channel, si); break;
        case 0xc6: si = fm3_extpartset(channel, si); break;
        case 0xc5: si = volmask_set(channel, si); break;
        case 0xc4: channel->qdatb = *si++; break;
        case 0xc3: si = panset_ex(channel, si); break;
        case 0xc2: channel->delay = channel->delay2 = *si++; lfoinit_main(channel); break;
        case 0xc1: break;
        case 0xc0: si = fm_mml_part_mask(channel, si); break;
        case 0xbf: SwapLFO(channel); si = SetLFOParameter(channel, si); SwapLFO(channel); break;
        case 0xbe: si = _lfoswitch(channel, si); ch3_setting(channel); break;
        case 0xbd:
            SwapLFO(channel);

            channel->mdspd = channel->mdspd2 = *si++;
            channel->mdepth = *(int8_t *) si++;

            SwapLFO(channel);
            break;

        case 0xbc: SwapLFO(channel); channel->lfo_wave = *si++; SwapLFO(channel); break;
        case 0xbb:
            SwapLFO(channel);

            channel->extendmode = (channel->extendmode & 0xfd) | ((*si++ & 1) << 1);

            SwapLFO(channel);
            break;

        case 0xba: si = _volmask_set(channel, si); break;
        case 0xb9:
            SwapLFO(channel);
            channel->delay = channel->delay2 = *si++; lfoinit_main(channel);
            SwapLFO(channel);
            break;

        case 0xb8: si = tl_set(channel, si); break;
        case 0xb7: si = mdepth_count(channel, si); break;
        case 0xb6: si = fb_set(channel, si); break;
        case 0xb5:
            channel->sdelay_m = (~(*si++) << 4) & 0xf0;
            channel->sdelay_c = channel->sdelay = *si++;
            break;

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

uint8_t * PMD::SetFMPortamentoCommand(Channel * channel, uint8_t * si)
{
    if (channel->PartMask)
    {
        channel->fnum = 0;    //休符に設定
        channel->Tone = 255;
        channel->Length = *(si + 2);
        channel->keyon_flag++;
        channel->Data = si + 3;

        if (--_Driver.volpush_flag)
            channel->volpush = 0;

        _Driver.TieMode = 0;
        _Driver.volpush_flag = 0;
        _Driver.loop_work &= channel->loopcheck;

        return si + 3;    // 読み飛ばす  (Mask時)
    }

    SetFMTone(channel, oshift(channel, StartLFO(channel, *si++)));

    int cx = (int) channel->fnum;
    int cl = channel->Tone;

    SetFMTone(channel, oshift(channel, *si++));

    int bx = (int) channel->fnum;      // bx=ポルタメント先のfnum値

    channel->Tone = cl;
    channel->fnum = (uint32_t) cx;      // cx=ポルタメント元のfnum値

    int bh = (int) ((bx / 256) & 0x38) - ((cx / 256) & 0x38);  // 先のoctarb - 元のoctarb
    int ax;

    if (bh)
    {
        bh /= 8;
        ax = bh * 0x26a;      // ax = 26ah * octarb差
    }
    else
        ax = 0;

    bx = (bx & 0x7ff) - (cx & 0x7ff);
    ax += bx;        // ax=26ah*octarb差 + 音程差

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->porta_num2 = ax / channel->Length;  // 商
    channel->porta_num3 = ax % channel->Length;  // 余り
    channel->lfoswi |= 8;        // Porta ON

    if (channel->volpush && channel->Tone != 255)
    {
        if (--_Driver.volpush_flag)
        {
            _Driver.volpush_flag = 0;
            channel->volpush = 0;
        }
    }

    SetFMVolumeCommand(channel);
    SetFMPitch(channel);
    SetFMKeyOn(channel);

    channel->keyon_flag++;
    channel->Data = si;

    _Driver.TieMode = 0;
    _Driver.volpush_flag = 0;

    if (*si == 0xfb)
        channel->keyoff_flag = 2;// '&'が直後にあったらSetFMKeyOffしない
    else
        channel->keyoff_flag = 0;

    _Driver.loop_work &= channel->loopcheck;

    return si;
}

uint8_t * PMD::IncreaseFMVolumeCommand(Channel * channel, uint8_t * si)
{
    int al = (int) channel->volume + 1 + *si++;

    if (al > 128)
        al = 128;

    channel->volpush = al;

    _Driver.volpush_flag = 1;

    return si;
}

uint8_t * PMD::SetFMEffect(Channel *, uint8_t * si)
{
    return si + 1;
}

//  FM3ch 拡張パートセット
uint8_t * PMD::fm3_extpartset(Channel *, uint8_t * si)
{
    int16_t ax = *(int16_t *) si;
    si += 2;

    if (ax)
        fm3_partinit(&_ExtensionTrack[0], &_State.MData[ax]);

    ax = *(int16_t *) si;
    si += 2;

    if (ax)
         fm3_partinit(&_ExtensionTrack[1], &_State.MData[ax]);

    ax = *(int16_t *) si;
    si += 2;

    if (ax)
        fm3_partinit(&_ExtensionTrack[2], &_State.MData[ax]);

    return si;
}

void PMD::fm3_partinit(Channel * channel, uint8_t * ax)
{
    channel->Data = ax;
    channel->Length = 1;          // ｱﾄ 1ｶｳﾝﾄ ﾃﾞ ｴﾝｿｳ ｶｲｼ
    channel->keyoff_flag = -1;      // 現在SetFMKeyOff中
    channel->mdc = -1;          // MDepth Counter (無限)
    channel->mdc2 = -1;          //
    channel->_mdc = -1;          //
    channel->_mdc2 = -1;          //
    channel->Tone = 255;        // rest
    channel->onkai_def = 255;      // rest
    channel->volume = 108;        // FM  VOLUME DEFAULT= 108
    channel->fmpan = _FMTrack[2].fmpan;  // FM PAN = CH3と同じ
    channel->PartMask |= 0x20;      // s0用 partmask
}

// FM tone generator hard LFO setting.
uint8_t * PMD::hlfo_set(Channel * channel, uint8_t * si)
{
    channel->fmpan = (channel->fmpan & 0xc0) | *si++;

    if (_Driver.CurrentChannel == 3 && _Driver.FMSelector == 0)
    {
        // Part_e is impossible because it is only for 2608. For FM3, set all four parts
        _FMTrack[2].fmpan = channel->fmpan;
        _ExtensionTrack[0].fmpan = channel->fmpan;
        _ExtensionTrack[1].fmpan = channel->fmpan;
        _ExtensionTrack[2].fmpan = channel->fmpan;
    }

    if (channel->PartMask == 0x00)
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xb4 - 1), calc_panout(channel));

    return si;
}

void PMD::SetFMTone(Channel * channel, int al)
{
    if ((al & 0x0f) != 0x0f)
    {
        // Music Note
        channel->Tone = al;

        // BLOCK/FNUM CALICULATE
        int bx = al & 0x0f;    // bx=onkai
        int ax = fnum_data[bx];

        // BLOCK SET
        ax |= (((al >> 1) & 0x38) << 8);
        channel->fnum = (uint32_t) ax;
    }
    else
    {
        // Rest
        channel->Tone = 0xFF;

        if ((channel->lfoswi & 0x11) == 0)
            channel->fnum = 0;      // 音程LFO未使用
    }
}

void PMD::SetFMVolumeCommand(Channel * channel)
{
    if (channel->SlotMask == 0)
        return;

    int cl = (channel->volpush) ? channel->volpush - 1 : channel->volume;

    if (channel != &_EffectTrack)
    {  // 効果音の場合はvoldown/fadeout影響無し
//--------------------------------------------------------------------
//  音量down計算
//--------------------------------------------------------------------
        if (_State.fm_voldown)
            cl = ((256 - _State.fm_voldown) * cl) >> 8;

        //--------------------------------------------------------------------
        //  Fadeout計算
        //--------------------------------------------------------------------
        if (_State.FadeOutVolume >= 2)
            cl = ((256 - (_State.FadeOutVolume >> 1)) * cl) >> 8;
    }

    //  音量をcarrierに設定 & 音量LFO処理
    //    input  cl to Volume[0-127]
    //      bl to SlotMask
    int bh = 0;          // Vol Slot Mask
    int bl = channel->SlotMask;    // ch=SlotMask Push

    uint8_t vol_tbl[4] = { 0x80, 0x80, 0x80, 0x80 };

    cl = 255 - cl;      // cl=carrierに設定する音量+80H(add)
    bl &= channel->carrier;    // bl=音量を設定するSLOT xxxx0000b
    bh |= bl;

    if (bl & 0x80) vol_tbl[0] = (uint8_t) cl;
    if (bl & 0x40) vol_tbl[1] = (uint8_t) cl;
    if (bl & 0x20) vol_tbl[2] = (uint8_t) cl;
    if (bl & 0x10) vol_tbl[3] = (uint8_t) cl;

    if (cl != 255)
    {
        if (channel->lfoswi & 2)
        {
            bl = channel->VolumeMask1;
            bl &= channel->SlotMask;    // bl=音量LFOを設定するSLOT xxxx0000b
            bh |= bl;

            fmlfo_sub(channel, channel->lfodat, bl, vol_tbl);
        }

        if (channel->lfoswi & 0x20)
        {
            bl = channel->VolumeMask2;
            bl &= channel->SlotMask;
            bh |= bl;

            fmlfo_sub(channel, channel->_lfodat, bl, vol_tbl);
        }
    }

    int dh = 0x4c - 1 + _Driver.CurrentChannel;    // dh=FM Port Address

    if (bh & 0x80) volset_slot(dh,      channel->slot4, vol_tbl[0]);
    if (bh & 0x40) volset_slot(dh -  8, channel->slot3, vol_tbl[1]);
    if (bh & 0x20) volset_slot(dh -  4, channel->slot2, vol_tbl[2]);
    if (bh & 0x10) volset_slot(dh - 12, channel->slot1, vol_tbl[3]);
}

void PMD::SetFMPitch(Channel * channel)
{
    if ((channel->fnum == 0) || (channel->SlotMask == 0))
        return;

    int cx = (int) (channel->fnum & 0x3800);    // cx=BLOCK
    int ax = (int) (channel->fnum) & 0x7ff;    // ax=FNUM

    // Portament/LFO/Detune SET
    ax += channel->porta_num + channel->detune;

    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0) && (_State.ch3mode != 0x3f))
        ch3_special(channel, ax, cx);
    else
    {
        if (channel->lfoswi & 1)
            ax += channel->lfodat;

        if (channel->lfoswi & 0x10)
            ax += channel->_lfodat;

        fm_block_calc(&cx, &ax);

        // SET BLOCK/FNUM TO OPN

        ax |= cx;

        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xa4 - 1), (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xa4 - 5), (uint32_t) LOBYTE(ax));
    }
}

void PMD::SetFMKeyOn(Channel * channel)
{
    if (channel->Tone == 0xFF)
        return; // ｷｭｳﾌ ﾉ ﾄｷ

    if (_Driver.FMSelector == 0)
    {
        int al = _Driver.omote_key[_Driver.CurrentChannel - 1] | channel->SlotMask;

        if (channel->sdelay_c)
            al &= channel->sdelay_m;

        _Driver.omote_key[_Driver.CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) ((_Driver.CurrentChannel - 1) | al));
    }
    else
    {
        int al = _Driver.ura_key[_Driver.CurrentChannel - 1] | channel->SlotMask;

        if (channel->sdelay_c)
            al &= channel->sdelay_m;

        _Driver.ura_key[_Driver.CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) (((_Driver.CurrentChannel - 1) | al) | 4));
    }
}

void PMD::SetFMKeyOff(Channel * track)
{
    if (track->Tone == 0xFF)
        return;

    SetFMKeyOffEx(track);
}

void PMD::SetFMKeyOffEx(Channel * track)
{
    if (_Driver.FMSelector == 0)
    {
        _Driver.omote_key[_Driver.CurrentChannel - 1] = (~track->SlotMask) & (_Driver.omote_key[_Driver.CurrentChannel - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) ((_Driver.CurrentChannel - 1) | _Driver.omote_key[_Driver.CurrentChannel - 1]));
    }
    else
    {
        _Driver.ura_key[_Driver.CurrentChannel - 1] = (~track->SlotMask) & (_Driver.ura_key[_Driver.CurrentChannel - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) (((_Driver.CurrentChannel - 1) | _Driver.ura_key[_Driver.CurrentChannel - 1]) | 4));
    }
}
