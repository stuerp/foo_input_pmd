
// PMD driver (Based on PMDWin code by C60)

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4710 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "PMD.h"

#include "Utility.h"
#include "Table.h"

#include "OPNAW.h"
#include "PPZ8.h"
#include "PPS.h"
#include "P86.h"

void PMD::PPZ8Main(Channel * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    int    temp;

    channel->Length--;

    if (channel->PartMask)
    {
        channel->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((channel->keyoff_flag & 3) == 0)
        {    // 既にKeyOffしたか？
            if (channel->Length <= channel->qdat)
            {
                keyoffz(channel);
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
                si = ExecutePPZ8Command(channel, si);
            }
            else if (*si == 0x80)
            {
                channel->Data = si;
                channel->loopcheck = 3;
                channel->onkai = 255;

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
                    {
                        break;
                    }
                }
                // "L"があった時
                si = channel->LoopData;
                channel->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {        // ポルタメント
                    si = SetPPZPortamento(channel, ++si);
                    _DriverState.loop_work &= channel->loopcheck;
                    return;
                }
                else
                if (channel->PartMask)
                {
                    si++;
                    channel->fnum = 0;    //休符に設定
                    channel->onkai = 255;
                    //          qq->onkai_def = 255;
                    channel->Length = *si++;
                    channel->keyon_flag++;
                    channel->Data = si;

                    if (--_DriverState.volpush_flag)
                    {
                        channel->volpush = 0;
                    }

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumsetz(channel, oshift(channel, lfoinitp(channel, *si++)));

                channel->Length = *si++;
                si = calc_q(channel, si);

                if (channel->volpush && channel->onkai != 255)
                {
                    if (--_DriverState.volpush_flag)
                    {
                        _DriverState.volpush_flag = 0;
                        channel->volpush = 0;
                    }
                }

                volsetz(channel);
                OtodasiZ(channel);
                if (channel->keyoff_flag & 1)
                {
                    keyonz(channel);
                }

                channel->keyon_flag++;
                channel->Data = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらKeyOffしない
                    channel->keyoff_flag = 2;
                }
                else
                {
                    channel->keyoff_flag = 0;
                }
                _DriverState.loop_work &= channel->loopcheck;
                return;

            }
        }
    }

    _DriverState.lfo_switch = (channel->lfoswi & 8);
    if (channel->lfoswi)
    {
        if (channel->lfoswi & 3)
        {
            if (lfo(channel))
            {
                _DriverState.lfo_switch |= (channel->lfoswi & 3);
            }
        }

        if (channel->lfoswi & 0x30)
        {
            SwapLFO(channel);
            if (lfop(channel))
            {
                SwapLFO(channel);
                _DriverState.lfo_switch |= (channel->lfoswi & 0x30);
            }
            else
            {
                SwapLFO(channel);
            }
        }

        if (_DriverState.lfo_switch & 0x19)
        {
            if (_DriverState.lfo_switch & 0x08)
            {
                porta_calc(channel);
            }
            OtodasiZ(channel);
        }
    }

    temp = soft_env(channel);
    if (temp || _DriverState.lfo_switch & 0x22 || _State.FadeOutSpeed)
    {
        volsetz(channel);
    }

    _DriverState.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecutePPZ8Command(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comatz(channel, si); break;
        case 0xfe: channel->qdata = *si++; break;
        case 0xfd: channel->volume = *si++; break;
        case 0xfc: si = ChangeTempoCommand(si); break;

        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: channel->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = SetStartOfLoopCommand(channel, si); break;
        case 0xf8: si = SetEndOfLoopCommand(channel, si); break;
        case 0xf7: si = ExitLoopCommand(channel, si); break;
        case 0xf6: channel->LoopData = si; break;
        case 0xf5: channel->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (channel->volume < (255 - 16))
                channel->volume += 16;
            else
                channel->volume = 255;
            break;

        case 0xf3: if (channel->volume < 16) channel->volume = 0; else channel->volume -= 16; break;
        case 0xf2: si = SetLFOParameter(channel, si); break;
        case 0xf1: si = lfoswitch(channel, si); break;
        case 0xf0: si = SetSSGEnvelopeCommand(channel, si); break;

        case 0xef: _OPNAW->SetReg((uint32_t) (_DriverState.FMSelector + *si), (uint32_t) *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;

        case 0xec: si = SetPPZPanCommand(channel, si); break;

        case 0xeb: si = SetRhythmInstrumentCommand(si); break;
        case 0xea: si = SetRhythmInstrumentVolumeCommand(si); break;
        case 0xe9: si = SetRhythmOutputPositionCommand(si); break;
        case 0xe8: si = SetRhythmMasterVolumeCommand(si); break;

        case 0xe7: channel->shift += *(int8_t *) si++; break;
        case 0xe6: si = IncreaseRhythmVolumeCommand(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: si++; break;

        case 0xe3:
            if (channel->volume < (255 - (*si)))
                channel->volume += (*si);
            else
                channel->volume = 255;
            si++;
            break;

        case 0xe2:
            if (channel->volume < *si)
                channel->volume = 0;
            else
                channel->volume -= *si;
            si++;
            break;

        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = IncreasePCMVolumeCommand(channel, si); break;
        case 0xdd: si = DecreaseSoundSourceVolumeCommand(channel, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si = SetPPZPortamento(channel, si); break;

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
        case 0xce: si = ppzrepeat_set(channel, si); break;
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
        case 0xc3: si = SetPPZPanExtendCommand(channel, si); break;
        case 0xc2: channel->delay = channel->delay2 = *si++; lfoinit_main(channel); break;
        case 0xc1: break;
        case 0xc0: si = ppz_mml_part_mask(channel, si); break;
        case 0xbf: SwapLFO(channel); si = SetLFOParameter(channel, si); SwapLFO(channel); break;
        case 0xbe: si = _lfoswitch(channel, si); break;
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

            channel->delay = channel->delay2 = *si++;
            lfoinit_main(channel);
// FIXME     break;
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

// Command "p"
uint8_t * PMD::SetPPZPanCommand(Channel * channel, uint8_t * si)
{
    channel->fmpan = ppzpandata[*si++];

    _PPZ8->SetPan(_DriverState.CurrentChannel, channel->fmpan);

    return si;
}

// Command "px":  -4?+4
uint8_t * PMD::SetPPZPanExtendCommand(Channel * channel, uint8_t * si)
{
    int al = *(int8_t *) si++;

    si++; // Skip the reverse phase flag

    if (al >= 5)
        al = 4;
    else
    if (al < -4)
        al = -4;

    channel->fmpan = al + 5;

    _PPZ8->SetPan(_DriverState.CurrentChannel, channel->fmpan);

    return si;
}

uint8_t * PMD::SetPPZPortamento(Channel * qq, uint8_t * si)
{
    int    ax, al_, bx_;

    if (qq->PartMask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->Length = *(si + 2);
        qq->keyon_flag++;
        qq->Data = si + 3;

        if (--_DriverState.volpush_flag)
        {
            qq->volpush = 0;
        }

        _DriverState.tieflag = 0;
        _DriverState.volpush_flag = 0;
        _DriverState.loop_work &= qq->loopcheck;
        return si + 3;    // 読み飛ばす  (Mask時)
    }

    fnumsetz(qq, oshift(qq, lfoinitp(qq, *si++)));

    bx_ = (int) qq->fnum;
    al_ = qq->onkai;
    fnumsetz(qq, oshift(qq, *si++));
    ax = (int) qq->fnum;       // ax = ポルタメント先のdelta_n値

    qq->onkai = al_;
    qq->fnum = (uint32_t) bx_;      // bx = ポルタメント元のdekta_n値
    ax -= bx_;        // ax = delta_n差
    ax /= 16;

    qq->Length = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->Length;    // 商
    qq->porta_num3 = ax % qq->Length;    // 余り
    qq->lfoswi |= 8;        // Porta ON

    if (qq->volpush && qq->onkai != 255)
    {
        if (--_DriverState.volpush_flag)
        {
            _DriverState.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volsetz(qq);
    OtodasiZ(qq);
    if (qq->keyoff_flag & 1)
    {
        keyonz(qq);
    }

    qq->keyon_flag++;
    qq->Data = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {      // '&'が直後にあったらKeyOffしない
        qq->keyoff_flag = 2;
    }

    _DriverState.loop_work &= qq->loopcheck;

    return si;
}
