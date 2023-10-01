
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
        channel->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((channel->keyoff_flag & 3) == 0)
        {    // 既にKeyOffしたか？
            if (channel->Length <= channel->qdat)
            {
                keyoff8(channel);
                channel->keyoff_flag = -1;
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
                    channel->onkai = 255;
                    //          qq->onkai_def = 255;
                    channel->Length = *si++;
                    channel->keyon_flag++;
                    channel->Data = si;

                    if (--_DriverState.volpush_flag)
                        channel->volpush = 0;

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumset8(channel, oshift(channel, lfoinitp(channel, *si++)));

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

                volset8(channel);
                Otodasi8(channel);

                if (channel->keyoff_flag & 1)
                    keyon8(channel);

                channel->keyon_flag++;
                channel->Data = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;

                if (*si == 0xfb)
                    channel->keyoff_flag = 2; // '&'が直後にあったらKeyOffしない
                else
                    channel->keyoff_flag = 0;

                _DriverState.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    if (channel->lfoswi & 0x22)
    {
        _DriverState.lfo_switch = 0;

        if (channel->lfoswi & 2)
        {
            lfo(channel);

            _DriverState.lfo_switch |= (channel->lfoswi & 2);
        }

        if (channel->lfoswi & 0x20)
        {
            SwapLFO(channel);

            if (lfo(channel))
            {
                SwapLFO(channel);

                _DriverState.lfo_switch |= (channel->lfoswi & 0x20);
            }
            else
                SwapLFO(channel);
        }

        temp = soft_env(channel);

        if (temp || _DriverState.lfo_switch & 0x22 || _State.FadeOutSpeed)
            volset8(channel);
    }
    else
    {
        temp = soft_env(channel);

        if (temp || _State.FadeOutSpeed)
            volset8(channel);
    }

    _DriverState.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecutePCM86Command(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comat8(channel, si); break;
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

        case 0xec: si = panset8(channel, si); break;        // FOR SB2
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
        case 0xc3: si = panset8_ex(channel, si); break;
        case 0xc2: channel->delay = channel->delay2 = *si++; lfoinit_main(channel); break;
        case 0xc1: break;
        case 0xc0: si = pcm_mml_part_mask8(channel, si); break;
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
