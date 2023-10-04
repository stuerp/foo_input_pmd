
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

void PMD::PPZMain(Channel * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    int    temp;

    channel->Length--;

    if (channel->MuteMask)
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
                SetPPZKeyOff(channel);
                channel->KeyOffFlag = -1;
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
                si = ExecutePPZCommand(channel, si);
            }
            else if (*si == 0x80)
            {
                channel->Data = si;
                channel->loopcheck = 3;
                channel->Tone = 255;

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
                {
                    si = SetPPZPortamentoCommand(channel, ++si);

                    _Driver.loop_work &= channel->loopcheck;
                    return;
                }
                else
                if (channel->MuteMask)
                {
                    si++;
                    channel->fnum = 0;    //休符に設定
                    channel->Tone = 255;
                    //          qq->DefaultTone = 255;
                    channel->Length = *si++;
                    channel->KeyOnFlag++;
                    channel->Data = si;

                    if (--_Driver.volpush_flag)
                    {
                        channel->volpush = 0;
                    }

                    _Driver.TieMode = 0;
                    _Driver.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                SetPPZTone(channel, oshift(channel, StartPCMLFO(channel, *si++)));

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

                SetPPZVolume(channel);
                SetPPZPitch(channel);

                if (channel->KeyOffFlag & 1)
                    SetPPZKeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

                _Driver.TieMode = 0;
                _Driver.volpush_flag = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

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
            {
                SwapLFO(channel);
            }
        }

        if (_Driver.lfo_switch & 0x19)
        {
            if (_Driver.lfo_switch & 0x08)
            {
                CalculatePortamento(channel);
            }
            SetPPZPitch(channel);
        }
    }

    temp = SSGPCMSoftwareEnvelope(channel);
    if (temp || _Driver.lfo_switch & 0x22 || _State.FadeOutSpeed)
    {
        SetPPZVolume(channel);
    }

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecutePPZCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff:
            si = SetPPZInstrument(channel, si);
            break;

        case 0xfe:
            channel->qdata = *si++;
            break;

        case 0xfd:
            channel->Volume = *si++;
            break;

        case 0xfc:
            si = ChangeTempoCommand(si);
            break;

        // Command "&": Tie notes together.
        case 0xFB:
            _Driver.TieMode |= 1;
            break;

        case 0xfa:
            channel->DetuneValue = *(int16_t *) si;
            si += 2;
            break;

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

        case 0xef: _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + *si), (uint32_t) *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;

        case 0xec: si = SetPPZPanValueCommand(channel, si); break;        // FOR SB2
        case 0xeb: si = RhythmInstrumentCommand(si); break;
        case 0xea: si = SetRhythmInstrumentVolumeCommand(si); break;
        case 0xe9: si = SetRhythmPanCommand(si); break;
        case 0xe8: si = SetRhythmMasterVolumeCommand(si); break;

        case 0xe7: channel->shift += *(int8_t *) si++; break;
        case 0xe6: si = SetRhythmVolume(si); break;
        case 0xe5: si = SetRhythmPanValue(si); break;

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

        case 0xdf:
            _State.BarLength = *si++; // Command "Z number"
            break;

        case 0xde:
             si = IncreasePCMVolumeCommand(channel, si);
            break;

        case 0xdd:
            si = DecreaseVolumeCommand(channel, si);
            break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si = SetPPZPortamentoCommand(channel, si); break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6:
            channel->MDepthSpeedA = channel->MDepthSpeedB = *si++;
            channel->MDepth = *(int8_t *) si++;
            break;

        case 0xd5:
            channel->DetuneValue += *(int16_t *) si; si += 2; break;

        case 0xd4: si = SetSSGEffect(channel, si); break;
        case 0xd3: si = SetFMEffect(channel, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si++; break;

        case 0xcf: si++; break;
        case 0xce:
            si = SetPPZRepeatCommand(channel, si);
            break;

        case 0xcd: si = SetSSGEnvelopeSpeedToExtend(channel, si); break;
        case 0xcc: si++; break;
        case 0xcb: channel->lfo_wave = *si++; break;
        case 0xca:
            channel->extendmode = (channel->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            channel->extendmode = (channel->extendmode & 0xFB) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: channel->qdatb = *si++; break;
        case 0xc3:
            si = SetPPZPanValueExtendedCommand(channel, si);
            break;

        case 0xc2: channel->delay = channel->delay2 = *si++; lfoinit_main(channel); break;
        case 0xc1: break;
        case 0xc0: si = SetPPZMaskCommand(channel, si); break;
        case 0xbf:
            SwapLFO(channel);

            si = SetLFOParameter(channel, si);

            SwapLFO(channel);
            break;

        case 0xbe:
            si = SetHardwareLFOSwitchCommand(channel, si);
            break;

        case 0xbd:
            SwapLFO(channel);

            channel->MDepthSpeedA = channel->MDepthSpeedB = *si++;
            channel->MDepth = *(int8_t *) si++;

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

        case 0xba:
            si = SetVolumeMask(channel, si);
            break;

        case 0xb9:
            SwapLFO(channel);

            channel->delay = channel->delay2 = *si++;
            lfoinit_main(channel);
// FIXME     break;

            SwapLFO(channel);
            break;

        case 0xb8: si += 2; break;

        case 0xb7:
            si = SetMDepthCountCommand(channel, si);
            break;

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

#pragma region(Commands)
// Command "@ number": Sets the number of the instrument to be used. Range 0-255.
uint8_t * PMD::SetPPZInstrument(Channel * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    if ((channel->InstrumentNumber & 0x80) == 0)
    {
        _PPZ->SetLoop(_Driver.CurrentChannel, _PPZ->PCME_WORK[0].PZIItem[channel->InstrumentNumber].LoopStart, _PPZ->PCME_WORK[0].PZIItem[channel->InstrumentNumber].LoopEnd);
        _PPZ->SetSourceRate(_Driver.CurrentChannel, _PPZ->PCME_WORK[0].PZIItem[channel->InstrumentNumber].SampleRate);
    }
    else
    {
        int i = channel->InstrumentNumber & 0x7F;

        _PPZ->SetLoop(_Driver.CurrentChannel, _PPZ->PCME_WORK[1].PZIItem[i].LoopStart, _PPZ->PCME_WORK[1].PZIItem[i].LoopEnd);
        _PPZ->SetSourceRate(_Driver.CurrentChannel, _PPZ->PCME_WORK[1].PZIItem[i].SampleRate);
    }
    return si;
}
#pragma endregion

void PMD::SetPPZTone(Channel * channel, int al)
{
    if ((al & 0x0f) != 0x0f)
    {
        // Music Note
        channel->Tone = al;

        int bx = al & 0x0f;          // bx=onkai
        int cl = (al >> 4) & 0x0f;    // cl = octarb

        uint32_t ax = (uint32_t) ppz_tune_data[bx];

        if ((cl -= 4) < 0)
        {
            cl = -cl;
            ax >>= cl;
        }
        else
            ax <<= cl;

        channel->fnum = ax;
    }
    else
    {
        // Rest
        channel->Tone = 0xFF;

        if ((channel->lfoswi & 0x11) == 0)
            channel->fnum = 0;      // 音程LFO未使用
    }
}

void PMD::SetPPZVolume(Channel * channel)
{
    int al = channel->volpush ? channel->volpush : channel->Volume;

    //  音量down計算
    al = ((256 - _State.PPZVolumeDown) * al) >> 8;

    //  Fadeout計算
    if (_State.FadeOutVolume != 0)
        al = ((256 - _State.FadeOutVolume) * al) >> 8;

    //  ENVELOPE 計算
    if (al == 0)
    {
        _PPZ->SetVolume(_Driver.CurrentChannel, 0);
        _PPZ->Stop(_Driver.CurrentChannel);
        return;
    }

    if (channel->envf == -1)
    {
        //  拡張版 音量=al*(eenv_vol+1)/16
        if (channel->eenv_volume == 0)
        {
            //*@    ppz8->SetVol(pmdwork._CurrentPart, 0);
            _PPZ->Stop(_Driver.CurrentChannel);
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
                //*@      ppz8->SetVol(pmdwork._CurrentPart, 0);
                _PPZ->Stop(_Driver.CurrentChannel);
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

    // Calculate the LFO volume.
    if ((channel->lfoswi & 0x22))
    {
        int dx = (channel->lfoswi & 2) ? channel->lfodat : 0;

        if (channel->lfoswi & 0x20)
            dx += channel->_lfodat;

        al += dx;

        if (dx >= 0)
        {
            if (al & 0xff00)
                al = 255;
        }
        else
        {
            if (al < 0)
                al = 0;
        }
    }

    if (al != 0)
        _PPZ->SetVolume(_Driver.CurrentChannel, al >> 4);
    else
        _PPZ->Stop(_Driver.CurrentChannel);
}

void PMD::SetPPZPitch(Channel * channel)
{
    uint32_t cx = channel->fnum;

    if (cx == 0)
        return;

    cx += channel->porta_num * 16;

    int ax = (channel->lfoswi & 1) ? channel->lfodat : 0;

    if (channel->lfoswi & 0x10)
        ax += channel->_lfodat;

    ax += channel->DetuneValue;

    int64_t cx2 = cx + ((int64_t) cx) / 256 * ax;

    if (cx2 > 0xffffffff)
        cx = 0xffffffff;
    else
    if (cx2 < 0)
        cx = 0;
    else
        cx = (uint32_t) cx2;

    _PPZ->SetPitch(_Driver.CurrentChannel, cx);
}

void PMD::SetPPZKeyOn(Channel * channel)
{
    if (channel->Tone == 0xFF)
        return;

    if ((channel->InstrumentNumber & 0x80) == 0)
        _PPZ->Play(_Driver.CurrentChannel, 0, channel->InstrumentNumber,        0, 0);
    else
        _PPZ->Play(_Driver.CurrentChannel, 1, channel->InstrumentNumber & 0x7F, 0, 0);
}

void PMD::SetPPZKeyOff(Channel * channel)
{
    if (channel->envf != -1)
    {
        if (channel->envf == 2)
            return;
    }
    else
    {
        if (channel->eenv_count == 4)
            return;
    }

    SetSSGKeyOff(channel);
}

// Command "{interval1 interval2} [length1] [.] [,length2]"
uint8_t * PMD::SetPPZPortamentoCommand(Channel * channel, uint8_t * si)
{
    if (channel->MuteMask)
    {
        // Set to 'rest'.
        channel->fnum = 0;
        channel->Tone = 255;
        channel->Length = si[2];
        channel->KeyOnFlag++;
        channel->Data = si + 3;

        if (--_Driver.volpush_flag)
            channel->volpush = 0;

        _Driver.TieMode = 0;
        _Driver.volpush_flag = 0;
        _Driver.loop_work &= channel->loopcheck;

        return si + 3; // Skip when muted.
    }

    SetPPZTone(channel, oshift(channel, StartPCMLFO(channel, *si++)));

    int bx_ = (int) channel->fnum;
    int al_ = channel->Tone;

    SetPPZTone(channel, oshift(channel, *si++));

    int ax = (int) channel->fnum;       // ax = ポルタメント先のdelta_n値

    channel->Tone = al_;
    channel->fnum = (uint32_t) bx_;      // bx = ポルタメント元のdekta_n値

    ax -= bx_;        // ax = delta_n差
    ax /= 16;

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->porta_num2 = ax / channel->Length;    // 商
    channel->porta_num3 = ax % channel->Length;    // 余り
    channel->lfoswi |= 8;        // Porta ON

    if (channel->volpush && channel->Tone != 255)
    {
        if (--_Driver.volpush_flag)
        {
            _Driver.volpush_flag = 0;
            channel->volpush = 0;
        }
    }

    SetPPZVolume(channel);
    SetPPZPitch(channel);

    if (channel->KeyOffFlag & 0x01)
        SetPPZKeyOn(channel);

    channel->KeyOnFlag++;
    channel->Data = si;

    _Driver.TieMode = 0;
    _Driver.volpush_flag = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver.loop_work &= channel->loopcheck;

    return si;
}

// Command "p <value>" (1: right, 2: left, 3: center (default))
uint8_t * PMD::SetPPZPanValueCommand(Channel * channel, uint8_t * si)
{
    static const int PanValues[4] = { 0, 9, 1, 5 }; // { Left, Right, Leftwards, Rightwards }

    channel->PanAndVolume = PanValues[*si++];

    _PPZ->SetPan(_Driver.CurrentChannel, channel->PanAndVolume);

    return si;
}

// Command "px <value 1>, <value 2>" (value 1: -128 to -4 (Pan to the left), -3 to -1 (Leftwards), 0 (Center), 1 to 3 (Rightwards), 4 to 127 (Pan to the right), value 2: 0 (In phase) or 1 (Reverse phase)).
uint8_t * PMD::SetPPZPanValueExtendedCommand(Channel * channel, uint8_t * si)
{
    int al = *(int8_t *) si++;
    si++; // Skip the Phase flag.

    if (al >  4)
        al = 4;
    else
    if (al < -4)
        al = -4;

    channel->PanAndVolume = al + 5; // Scale the value to range 1..9.

    _PPZ->SetPan(_Driver.CurrentChannel, channel->PanAndVolume);

    return si;
}

uint8_t * PMD::InitializePPZ(Channel *, uint8_t * si)
{
    for (size_t i = 0; i < _countof(_PPZChannel); ++i)
    {
        int16_t ax = *(int16_t *) si;

        if (ax)
        {
            _PPZChannel[i].Data = &_State.MData[ax];
            _PPZChannel[i].Length = 1;
            _PPZChannel[i].KeyOffFlag = -1;
            _PPZChannel[i].mdc = -1;            // MDepth Counter (無限)
            _PPZChannel[i].mdc2 = -1;
            _PPZChannel[i]._mdc = -1;
            _PPZChannel[i]._mdc2 = -1;
            _PPZChannel[i].Tone = 0xFF;         // Rest
            _PPZChannel[i].DefaultTone = 0xFF;  // Rest
            _PPZChannel[i].Volume = 128;
            _PPZChannel[i].PanAndVolume = 5;         // Center
        }

        si += 2;
    }

    return si;
}

// Command "@[@] insnum[,number1[,number2[,number3]]]"
uint8_t * PMD::SetPPZRepeatCommand(Channel * channel, uint8_t * si)
{
    int LoopBegin, LoopEnd;

    if ((channel->InstrumentNumber & 0x80) == 0)
    {
        LoopBegin = *(int16_t *) si;
        si += 2;

        if (LoopBegin < 0)
            LoopBegin = (int) (_PPZ->PCME_WORK[0].PZIItem[channel->InstrumentNumber].Size - LoopBegin);

        LoopEnd = *(int16_t *) si;
        si += 2;

        if (LoopEnd < 0)
            LoopEnd = (int) (_PPZ->PCME_WORK[0].PZIItem[channel->InstrumentNumber].Size - LoopBegin);
    }
    else
    {
        LoopBegin = *(int16_t *) si;
        si += 2;

        if (LoopBegin < 0)
            LoopBegin = (int) (_PPZ->PCME_WORK[1].PZIItem[channel->InstrumentNumber & 0x7f].Size - LoopBegin);

        LoopEnd = *(int16_t *) si;
        si += 2;

        if (LoopEnd < 0)
            LoopEnd = (int) (_PPZ->PCME_WORK[1].PZIItem[channel->InstrumentNumber & 0x7f].Size - LoopEnd);
    }

    _PPZ->SetLoop(_Driver.CurrentChannel, (uint32_t) LoopBegin, (uint32_t) LoopEnd);

    return si + 2; // Skip the Loop Release Address.
}

// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
uint8_t * PMD::SetPPZMaskCommand(Channel * channel, uint8_t * si)
{
    uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->MuteMask |= 0x40;

            if (channel->MuteMask == 0x40)
                _PPZ->Stop(_Driver.CurrentChannel);
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->MuteMask &= 0xBF; // 1011 1111

    return si;
}

uint8_t * PMD::DecreasePPZVolumeCommand(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.PPZVolumeDown = Limit(al + _State.PPZVolumeDown, 255, 0);
    else
        _State.PPZVolumeDown = _State.DefaultPPZVolumeDown;

    return si;
}
