
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
        // Already KeyOff?
        if (channel->Length <= channel->qdat)
        {
            KeyOff(channel);
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
                channel->onkai = 0xFF;

                if (channel->LoopData == nullptr)
                {
                    if (channel->PartMask)
                    {
                        _DriverState.tieflag = 0;
                        _DriverState.volpush_flag = 0;
                        _DriverState.loop_work &= channel->loopcheck;

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

                    _DriverState.loop_work &= channel->loopcheck;

                    return;
                }
                else
                if (channel->PartMask == 0x00)
                {
                    // TONE SET
                    fnumset(channel, oshift(channel, lfoinit(channel, *si++)));

                    channel->Length = *si++;

                    si = calc_q(channel, si);

                    if (channel->volpush && (channel->onkai != 0xFF))
                    {
                        if (--_DriverState.volpush_flag)
                        {
                            _DriverState.volpush_flag = 0;
                            channel->volpush = 0;
                        }
                    }

                    SetFMVolumeCommand(channel);
                    Otodasi(channel);
                    KeyOn(channel);

                    channel->keyon_flag++;
                    channel->Data = si;

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;

                    if (*si == 0xfb)
                        channel->keyoff_flag = 2; // Do not key off if '&' immediately follows
                    else
                        channel->keyoff_flag = 0;

                    _DriverState.loop_work &= channel->loopcheck;

                    return;
                }
                else
                {
                    si++;

                    channel->fnum = 0; // Set to rest
                    channel->onkai = 0xFF;
                    channel->onkai_def = 0xFF;
                    channel->Length = *si++;
                    channel->keyon_flag++;
                    channel->Data = si;

                    if (--_DriverState.volpush_flag)
                        channel->volpush = 0;

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;
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
                _OPNAW->SetReg((uint32_t) (_DriverState.FMSelector + (_DriverState.CurrentChannel - 1 + 0xb4)), (uint32_t) channel->fmpan);
        }

        if (channel->sdelay_c)
        {
            if (--channel->sdelay_c == 0)
            {
                if ((channel->keyoff_flag & 1) == 0)
                    KeyOn(channel); // Already keyoffed?
            }
        }

        if (channel->lfoswi)
        {
            _DriverState.lfo_switch = channel->lfoswi & 8;

            if (channel->lfoswi & 3)
            {
                if (lfo(channel))
                    _DriverState.lfo_switch |= (channel->lfoswi & 3);
            }

            if (channel->lfoswi & 0x30)
            {
                SwapLFO(channel);

                if (lfo(channel))
                {
                    SwapLFO(channel);

                    _DriverState.lfo_switch |= (channel->lfoswi & 0x30);
                }
                else
                    SwapLFO(channel);
            }

            if (_DriverState.lfo_switch & 0x19)
            {
                if (_DriverState.lfo_switch & 8)
                    porta_calc(channel);

                Otodasi(channel);
            }

            if (_DriverState.lfo_switch & 0x22)
            {
                SetFMVolumeCommand(channel);
                _DriverState.loop_work &= channel->loopcheck;

                return;
            }
        }

        if (_State.FadeOutSpeed != 0)
            SetFMVolumeCommand(channel);
    }

    _DriverState.loop_work &= channel->loopcheck;
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

        case 0xfb: _DriverState.tieflag |= 1; break;
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
            _OPNAW->SetReg((uint32_t) (_DriverState.FMSelector + *si), (uint32_t) (*(si + 1)));
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
        channel->onkai = 255;
        channel->Length = *(si + 2);
        channel->keyon_flag++;
        channel->Data = si + 3;

        if (--_DriverState.volpush_flag)
            channel->volpush = 0;

        _DriverState.tieflag = 0;
        _DriverState.volpush_flag = 0;
        _DriverState.loop_work &= channel->loopcheck;

        return si + 3;    // 読み飛ばす  (Mask時)
    }

    fnumset(channel, oshift(channel, lfoinit(channel, *si++)));

    int cx = (int) channel->fnum;
    int cl = channel->onkai;

    fnumset(channel, oshift(channel, *si++));

    int bx = (int) channel->fnum;      // bx=ポルタメント先のfnum値

    channel->onkai = cl;
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

    si = calc_q(channel, si);

    channel->porta_num2 = ax / channel->Length;  // 商
    channel->porta_num3 = ax % channel->Length;  // 余り
    channel->lfoswi |= 8;        // Porta ON

    if (channel->volpush && channel->onkai != 255)
    {
        if (--_DriverState.volpush_flag)
        {
            _DriverState.volpush_flag = 0;
            channel->volpush = 0;
        }
    }

    SetFMVolumeCommand(channel);
    Otodasi(channel);
    KeyOn(channel);

    channel->keyon_flag++;
    channel->Data = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;

    if (*si == 0xfb)
        channel->keyoff_flag = 2;// '&'が直後にあったらKeyOffしない
    else
        channel->keyoff_flag = 0;

    _DriverState.loop_work &= channel->loopcheck;

    return si;
}

uint8_t * PMD::IncreaseFMVolumeCommand(Channel * channel, uint8_t * si)
{
    int al = (int) channel->volume + 1 + *si++;

    if (al > 128)
        al = 128;

    channel->volpush = al;

    _DriverState.volpush_flag = 1;

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
    channel->keyoff_flag = -1;      // 現在KeyOff中
    channel->mdc = -1;          // MDepth Counter (無限)
    channel->mdc2 = -1;          //
    channel->_mdc = -1;          //
    channel->_mdc2 = -1;          //
    channel->onkai = 255;        // rest
    channel->onkai_def = 255;      // rest
    channel->volume = 108;        // FM  VOLUME DEFAULT= 108
    channel->fmpan = _FMTrack[2].fmpan;  // FM PAN = CH3と同じ
    channel->PartMask |= 0x20;      // s0用 partmask
}

// FM tone generator hard LFO setting.
uint8_t * PMD::hlfo_set(Channel * channel, uint8_t * si)
{
    channel->fmpan = (channel->fmpan & 0xc0) | *si++;

    if (_DriverState.CurrentChannel == 3 && _DriverState.FMSelector == 0)
    {
        // Part_e is impossible because it is only for 2608. For FM3, set all four parts
        _FMTrack[2].fmpan = channel->fmpan;
        _ExtensionTrack[0].fmpan = channel->fmpan;
        _ExtensionTrack[1].fmpan = channel->fmpan;
        _ExtensionTrack[2].fmpan = channel->fmpan;
    }

    if (channel->PartMask == 0x00)
        _OPNAW->SetReg((uint32_t) (_DriverState.FMSelector + _DriverState.CurrentChannel + 0xb4 - 1), calc_panout(channel));

    return si;
}
