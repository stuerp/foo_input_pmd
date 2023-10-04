
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

    if (channel->MuteMask)
        channel->KeyOffFlag = -1;
    else
    if ((channel->KeyOffFlag & 3) == 0) // KEYOFF CHECK & Keyoff
    {
        // Already SetFMKeyOff?
        if (channel->Length <= channel->qdat)
        {
            SetFMKeyOff(channel);
            channel->KeyOffFlag = -1;
        }
    }

    if (channel->Length == 0)
    {
        if (channel->MuteMask == 0x00)
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
                    if (channel->MuteMask)
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
                if (channel->MuteMask == 0x00)
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
                        channel->KeyOffFlag = 2; // Do not key off if '&' immediately follows
                    else
                        channel->KeyOffFlag = 0;

                    _Driver.loop_work &= channel->loopcheck;

                    return;
                }
                else
                {
                    si++;

                    channel->fnum = 0; // Set to rest
                    channel->Tone = 0xFF;
                    channel->DefaultTone = 0xFF;
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

    if (channel->MuteMask == 0x00)
    {
        // Finish with LFO & Portament & Fadeout processing
        if (channel->hldelay_c)
        {
            if (--channel->hldelay_c == 0)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + (_Driver.CurrentChannel - 1 + 0xb4)), (uint32_t) channel->PanAndVolume);
        }

        if (channel->sdelay_c)
        {
            if (--channel->sdelay_c == 0)
            {
                if ((channel->KeyOffFlag & 1) == 0)
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
        case 0xf4: if ((channel->Volume += 4) > 127) channel->Volume = 127; break;
        case 0xf3: if (channel->Volume < 4) channel->Volume = 0; else channel->Volume -= 4; break;
        case 0xf2: si = SetLFOParameter(channel, si); break;
        case 0xf1: si = lfoswitch(channel, si); SetFMChannel3Mode(channel); break;
        case 0xf0: si += 4; break;

        case 0xef:
            _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + *si), (uint32_t) (*(si + 1)));
            si += 2;
            break;

        case 0xee: si++; break;
        case 0xed: si++; break;

        case 0xec: si = SetFMPanValueCommand(channel, si); break;        // FOR SB2

        case 0xeb: si = RhythmInstrumentCommand(si); break;
        case 0xea: si = SetRhythmInstrumentVolumeCommand(si); break;
        case 0xe9: si = SetRhythmPanCommand(si); break;
        case 0xe8: si = SetRhythmMasterVolumeCommand(si); break;

        case 0xe7: channel->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: channel->hldelay = *si++; break;

        case 0xe3:
            if ((channel->Volume += *si++) > 127)
                channel->Volume = 127;
            break;

        case 0xe2:
            if (channel->Volume < *si)
                channel->Volume = 0;
            else
                channel->Volume -= *si;
            si++;
            break;

        case 0xe1: si = hlfo_set(channel, si); break;
        case 0xe0: _State.port22h = *si; _OPNAW->SetReg(0x22, *si++); break;

        case 0xdf:
            _State.BarLength = *si++; // Command "Z number"
            break;

        case 0xde:
            si = IncreaseFMVolumeCommand(channel, si);
            break;

        case 0xdd: 
            si = DecreaseSoundSourceVolumeCommand(channel, si);
            break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda:
            si = SetFMPortamentoCommand(channel, si);
            break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6:
            channel->mdspd = channel->mdspd2 = *si++;
            channel->mdepth = *(int8_t *) si++;
            break;

        case 0xd5: channel->detune += *(int16_t *) si; si += 2; break;

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
        case 0xd0: si++; break;

        case 0xcf:
            si = SetFMSlotCommand(channel, si);
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
        case 0xc6: si = SetFMChannel3ModeEx(channel, si); break;
        case 0xc5: si = volmask_set(channel, si); break;
        case 0xc4: channel->qdatb = *si++; break;
        case 0xc3: si = SetFMPanValueExtendedCommand(channel, si); break;
        case 0xc2: channel->delay = channel->delay2 = *si++; lfoinit_main(channel); break;
        case 0xc1: break;
        case 0xc0:
            si = SetFMMaskCommand(channel, si);
            break;

        case 0xbf: SwapLFO(channel); si = SetLFOParameter(channel, si); SwapLFO(channel); break;
        case 0xbe: si = _lfoswitch(channel, si); SetFMChannel3Mode(channel); break;
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
    if (channel->FMSlotMask == 0)
        return;

    int cl = (channel->volpush) ? channel->volpush - 1 : channel->Volume;

    if (channel != &_EffectChannel)
    {  // 効果音の場合はvoldown/fadeout影響無し
//--------------------------------------------------------------------
//  音量down計算
//--------------------------------------------------------------------
        if (_State.FMVolumeDown)
            cl = ((256 - _State.FMVolumeDown) * cl) >> 8;

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
    int bl = channel->FMSlotMask;    // ch=SlotMask Push

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
            bl &= channel->FMSlotMask;    // bl=音量LFOを設定するSLOT xxxx0000b
            bh |= bl;

            fmlfo_sub(channel, channel->lfodat, bl, vol_tbl);
        }

        if (channel->lfoswi & 0x20)
        {
            bl = channel->VolumeMask2;
            bl &= channel->FMSlotMask;
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
    if ((channel->fnum == 0) || (channel->FMSlotMask == 0))
        return;

    int cx = (int) (channel->fnum & 0x3800);    // cx=BLOCK
    int ax = (int) (channel->fnum) & 0x7ff;    // ax=FNUM

    // Portament/LFO/Detune SET
    ax += channel->porta_num + channel->detune;

    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0) && (_State.FMChannel3Mode != 0x3F))
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
        int al = _Driver.omote_key[_Driver.CurrentChannel - 1] | channel->FMSlotMask;

        if (channel->sdelay_c)
            al &= channel->sdelay_m;

        _Driver.omote_key[_Driver.CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) ((_Driver.CurrentChannel - 1) | al));
    }
    else
    {
        int al = _Driver.ura_key[_Driver.CurrentChannel - 1] | channel->FMSlotMask;

        if (channel->sdelay_c)
            al &= channel->sdelay_m;

        _Driver.ura_key[_Driver.CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) (((_Driver.CurrentChannel - 1) | al) | 4));
    }
}

void PMD::SetFMKeyOff(Channel * channel)
{
    if (channel->Tone == 0xFF)
        return;

    if (_Driver.FMSelector == 0)
    {
        _Driver.omote_key[_Driver.CurrentChannel - 1] = (~channel->FMSlotMask) & (_Driver.omote_key[_Driver.CurrentChannel - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) ((_Driver.CurrentChannel - 1) | _Driver.omote_key[_Driver.CurrentChannel - 1]));
    }
    else
    {
        _Driver.ura_key[_Driver.CurrentChannel - 1] = (~channel->FMSlotMask) & (_Driver.ura_key[_Driver.CurrentChannel - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) (((_Driver.CurrentChannel - 1) | _Driver.ura_key[_Driver.CurrentChannel - 1]) | 4));
    }
}

// Sets FM Wait after register output.
void PMD::SetFMDelay(int nsec)
{
    _OPNAW->SetFMDelay(nsec);
}

#pragma region(Commands)
// Command "p <value>" (1: right, 2: left, 3: center (default))
uint8_t * PMD::SetFMPanValueCommand(Channel * channel, uint8_t * si)
{
    SetFMPanValueInternal(channel, *si++);

    return si;
}

// Command "px <value 1>, <value 2>" (value 1: -4 (pan to the left) to +4 (pan to the right), value 2: 0 (In phase) or 1 (Reverse phase)).
uint8_t * PMD::SetFMPanValueExtendedCommand(Channel * channel, uint8_t * si)
{
    int al = *(int8_t *) si++;

    si++; // Skip the Phase flag

    if (al > 0)
    {
        al = 2; // Right
        SetFMPanValueInternal(channel, al);
    }
    else
    if (al == 0)
    {
        al = 3; // Center
        SetFMPanValueInternal(channel, al);
    }
    else
    {
        al = 1; // Left
        SetFMPanValueInternal(channel, al);
    }

    return si;
}

void PMD::SetFMPanValueInternal(Channel * channel, int value)
{
    channel->PanAndVolume = (channel->PanAndVolume & 0x3F) | ((value << 6) & 0xC0);

    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
    {
        // For FM3, set all 4 parts.
        _FMChannel[2].PanAndVolume = channel->PanAndVolume;

        _FMExtensionChannel[0].PanAndVolume = channel->PanAndVolume;
        _FMExtensionChannel[1].PanAndVolume = channel->PanAndVolume;
        _FMExtensionChannel[2].PanAndVolume = channel->PanAndVolume;
    }

    if (channel->MuteMask == 0x00)
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xb4 - 1), calc_panout(channel));
}

uint8_t * PMD::IncreaseFMVolumeCommand(Channel * channel, uint8_t * si)
{
    int al = (int) channel->Volume + 1 + *si++;

    if (al > 128)
        al = 128;

    channel->volpush = al;

    _Driver.volpush_flag = 1;

    return si;
}

uint8_t * PMD::DecreaseFMVolumeCommand(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.FMVolumeDown = Limit(al + _State.FMVolumeDown, 255, 0);
    else
        _State.FMVolumeDown = _State.DefaultFMVolumeDown;

    return si;
}

uint8_t * PMD::SetFMPortamentoCommand(Channel * channel, uint8_t * si)
{
    if (channel->MuteMask)
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
        channel->KeyOffFlag = 2;// '&'が直後にあったらSetFMKeyOffしない
    else
        channel->KeyOffFlag = 0;

    _Driver.loop_work &= channel->loopcheck;

    return si;
}

uint8_t * PMD::SetFMEffect(Channel *, uint8_t * si)
{
    return si + 1;
}

uint8_t * PMD::SetFMChannel3ModeEx(Channel *, uint8_t * si)
{
    int16_t ax = *(int16_t *) si;
    si += 2;

    if (ax)
        InitializeFMChannel3(&_FMExtensionChannel[0], &_State.MData[ax]);

    ax = *(int16_t *) si;
    si += 2;

    if (ax)
         InitializeFMChannel3(&_FMExtensionChannel[1], &_State.MData[ax]);

    ax = *(int16_t *) si;
    si += 2;

    if (ax)
        InitializeFMChannel3(&_FMExtensionChannel[2], &_State.MData[ax]);

    return si;
}

// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
uint8_t * PMD::SetFMMaskCommand(Channel * channel, uint8_t * si)
{
    uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->MuteMask |= 0x40;

            if (channel->MuteMask == 0x40)
                MuteFMChannel(channel);
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
    {
        if ((channel->MuteMask &= 0xBF) == 0x00)
            ResetTone(channel);
    }

    return si;
}

// Command "m": Set FM Slot. Mainly used for the 3rd FM channel, specifies the slot position (operators) to be used for performance/definition.
uint8_t * PMD::SetFMSlotCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;
    int ah = al & 0xF0;

    al &= 0x0F;

    if (al != 0x00)
    {
        channel->carrier = al << 4;
    }
    else
    {
        int bl;

        if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
        {
            bl = _Driver.fm3_alg_fb;
        }
        else
        {
            uint8_t * bx = GetToneData(channel, channel->InstrumentNumber);

            bl = bx[24];
        }

        channel->carrier = carrier_table[bl & 7];
    }

    if (channel->FMSlotMask != ah)
    {
        channel->FMSlotMask = ah;

        if (ah != 0x00)
            channel->MuteMask &= 0xDF;  // Unmask part when other than s0
        else
            channel->MuteMask |= 0x20;  // Part mask at s0

        if (SetFMChannel3Mode(channel))
        {
            if (channel != &_FMChannel[2])
            {
                if (_FMChannel[2].MuteMask == 0x00 && (_FMChannel[2].KeyOffFlag & 1) == 0)
                    SetFMKeyOn(&_FMChannel[2]);

                if (channel != &_FMExtensionChannel[0])
                {
                    if (_FMExtensionChannel[0].MuteMask == 0x00 && (_FMExtensionChannel[0].KeyOffFlag & 1) == 0)
                        SetFMKeyOn(&_FMExtensionChannel[0]);

                    if (channel != &_FMExtensionChannel[1])
                    {
                        if (_FMExtensionChannel[1].MuteMask == 0x00 && (_FMExtensionChannel[1].KeyOffFlag & 1) == 0)
                            SetFMKeyOn(&_FMExtensionChannel[1]);
                    }
                }
            }
        }

        ah = 0x00;

        if (channel->FMSlotMask & 0x80) ah += 0x11; // Slot 4
        if (channel->FMSlotMask & 0x40) ah += 0x44; // Slot 3
        if (channel->FMSlotMask & 0x20) ah += 0x22; // Slot 2
        if (channel->FMSlotMask & 0x10) ah += 0x88; // Slot 1

        channel->ToneMask = ah;
    }

    return si;
}
#pragma endregion

int PMD::SetFMChannel3Mode(Channel * channel)
{
    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
    {
        SetFMChannel3Mode2(channel);

        return 1;
    }

    return 0;
}

void PMD::SetFMChannel3Mode2(Channel * channel)
{
    int al;

    if (channel == &_FMChannel[2])
        al = 1;
    else
    if (channel == &_FMExtensionChannel[0])
        al = 2;
    else
    if (channel == &_FMExtensionChannel[1])
        al = 4;
    else
        al = 8;

    int ah;

    if ((channel->FMSlotMask & 0xF0) == 0)
    {
        cm_clear(&ah, &al); // s0
    }
    else
    if (channel->FMSlotMask != 0xF0)
    {
        _Driver.slot3_flag |= al;
        ah = 0x7F;
    }
    else

    if ((channel->VolumeMask1 & 0x0F) == 0)
    {
        cm_clear(&ah, &al);
    }
    else
    if ((channel->lfoswi & 1) != 0)
    {
        _Driver.slot3_flag |= al;
        ah = 0x7F;
    }
    else

    if ((channel->VolumeMask2 & 0x0F) == 0)
    {
        cm_clear(&ah, &al);
    }
    else
    if (channel->lfoswi & 0x10)
    {
        _Driver.slot3_flag |= al;
        ah = 0x7F;
    }
    else
    {
        cm_clear(&ah, &al);
    }

    if ((uint32_t) ah == _State.FMChannel3Mode)
        return;

    _State.FMChannel3Mode = (uint32_t) ah;

    _OPNAW->SetReg(0x27, (uint32_t) (ah & 0xCF)); // Don't reset.

    // When moving to sound effect mode, the pitch is rewritten with the previous FM3 part
    if (ah == 0x3F || channel == &_FMChannel[2])
        return;

    if (_FMChannel[2].MuteMask == 0x00)
        SetFMPitch(&_FMChannel[2]);

    if (channel == &_FMExtensionChannel[0])
        return;

    if (_FMExtensionChannel[0].MuteMask == 0x00)
        SetFMPitch(&_FMExtensionChannel[0]);

    if (channel == &_FMExtensionChannel[1])
        return;

    if (_FMExtensionChannel[1].MuteMask == 0x00)
        SetFMPitch(&_FMExtensionChannel[1]);
}

void PMD::InitializeFMChannel3(Channel * channel, uint8_t * ax)
{
    channel->Data = ax;
    channel->Length = 1;          // ｱﾄ 1ｶｳﾝﾄ ﾃﾞ ｴﾝｿｳ ｶｲｼ
    channel->KeyOffFlag = -1;      // 現在SetFMKeyOff中
    channel->mdc = -1;          // MDepth Counter (無限)
    channel->mdc2 = -1;          //
    channel->_mdc = -1;          //
    channel->_mdc2 = -1;          //
    channel->Tone = 255;        // rest
    channel->DefaultTone = 255;      // rest
    channel->Volume = 108;        // FM  VOLUME DEFAULT= 108
    channel->PanAndVolume = _FMChannel[2].PanAndVolume;  // FM PAN = CH3と同じ
    channel->MuteMask |= 0x20;      // s0用 partmask
}

// FM tone generator hard LFO setting.
uint8_t * PMD::hlfo_set(Channel * channel, uint8_t * si)
{
    channel->PanAndVolume = (channel->PanAndVolume & 0xc0) | *si++;

    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
    {
        // Part_e is impossible because it is only for 2608. For FM3, set all four parts
        _FMChannel[2].PanAndVolume = channel->PanAndVolume;
        _FMExtensionChannel[0].PanAndVolume = channel->PanAndVolume;
        _FMExtensionChannel[1].PanAndVolume = channel->PanAndVolume;
        _FMExtensionChannel[2].PanAndVolume = channel->PanAndVolume;
    }

    if (channel->MuteMask == 0x00)
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xb4 - 1), calc_panout(channel));

    return si;
}
