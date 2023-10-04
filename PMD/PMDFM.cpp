
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
            channel->LFOSwitch &= 0xf7;

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
                channel->Note = 0xFF;

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

                    if (channel->volpush && (channel->Note != 0xFF))
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

                    channel->KeyOnFlag++;
                    channel->Data = si;

                    _Driver.TieMode = 0;
                    _Driver.volpush_flag = 0;

                    // Don't perform Key Off if a "&" command (Tie) follows immediately.
                    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                    _Driver.loop_work &= channel->loopcheck;

                    return;
                }
                else
                {
                    si++;

                    channel->fnum = 0; // Set to rest
                    channel->Note = 0xFF;
                    channel->DefaultNote = 0xFF;
                    channel->Length = *si++;
                    channel->KeyOnFlag++;
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
        if (channel->HardwareLFODelayCounter)
        {
            if (--channel->HardwareLFODelayCounter == 0)
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

        if (channel->LFOSwitch)
        {
            _Driver.lfo_switch = channel->LFOSwitch & 8;

            if (channel->LFOSwitch & 3)
            {
                if (lfo(channel))
                    _Driver.lfo_switch |= (channel->LFOSwitch & 3);
            }

            if (channel->LFOSwitch & 0x30)
            {
                SwapLFO(channel);

                if (lfo(channel))
                {
                    SwapLFO(channel);

                    _Driver.lfo_switch |= (channel->LFOSwitch & 0x30);
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
        case 0xff:
            si = SetFMInstrumentCommand(channel, si);
            break;

        case 0xfe:
            channel->qdata = *si++;
            channel->qdat3 = 0;
            break;

        case 0xFD:
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

        case 0xf9:
            si = SetStartOfLoopCommand(channel, si);
            break;

        case 0xf8:
            si = SetEndOfLoopCommand(channel, si);
            break;

        case 0xf7:
            si = ExitLoopCommand(channel, si);
            break;

        case 0xf6:
            channel->LoopData = si;
            break;

        case 0xf5:
            channel->shift = *(int8_t *) si++;
            break;

        case 0xf4:
            channel->Volume += 4;

            if (channel->Volume > 127)
                channel->Volume = 127;
            break;

        case 0xf3:
            if (channel->Volume < 4)
                channel->Volume = 0;
            else
                channel->Volume -= 4;
            break;

        case 0xf2:
            si = SetLFOParameter(channel, si);
            break;

        case 0xf1:
            si = lfoswitch(channel, si);
            SetFMChannel3Mode(channel);
            break;

        case 0xf0: si += 4; break;

        case 0xef:
            _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + *si), (uint32_t) (*(si + 1)));
            si += 2;
            break;

        case 0xee: si++; break;
        case 0xed: si++; break;

        case 0xec:
            si = SetFMPanValueCommand(channel, si); // FOR SB2
            break;

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

        case 0xe4:
            channel->HardwareLFODelay = *si++;
            break;

        case 0xe3:
            channel->Volume += *si++;

            if (channel->Volume > 127)
                channel->Volume = 127;
            break;

        case 0xe2:
            if (channel->Volume < *si)
                channel->Volume = 0;
            else
                channel->Volume -= *si;
            si++;
            break;

        case 0xe1:
            si = SetHardLFOCommand(channel, si);
            break;

        case 0xe0:
            _State.port22h = *si;
            _OPNAW->SetReg(0x22, *si++);
            break;

        case 0xdf:
            _State.BarLength = *si++; // Command "Z number"
            break;

        case 0xde:
            si = IncreaseFMVolumeCommand(channel, si);
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
            si = SetFMPortamentoCommand(channel, si);
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
        case 0xd0: si++; break;

        case 0xcf:
            si = SetFMSlotCommand(channel, si);
            break;

        case 0xce: si += 6; break;
        case 0xcd: si += 5; break;
        case 0xcc: si++; break;

        case 0xcb:
            channel->LFOWaveform = *si++;
            break;

        case 0xca:
            channel->extendmode = (channel->extendmode & 0xFD) | ((*si++ & 1) << 1);
            break;

        case 0xc9: si++; break;

        case 0xc8:
            si = SetFMAbsoluteDetuneCommand(channel, si);
            break;

        case 0xc7:
            si = SetFMRelativeDetuneCommand(channel, si);
            break;

        case 0xc6:
            si = SetFMChannel3ModeEx(channel, si);
            break;

        case 0xc5:
            si = SetFMVolumeMaskSlotCommand(channel, si);
            break;

        case 0xc4:
            channel->qdatb = *si++;
            break;

        case 0xc3:
            si = SetFMPanValueExtendedCommand(channel, si);
            break;

        case 0xc2:
            channel->delay = channel->delay2 = *si++;
            lfoinit_main(channel);
            break;

        case 0xc1: break;

        case 0xc0:
            si = SetFMMaskCommand(channel, si);
            break;

        case 0xbf:
            SwapLFO(channel);

            si = SetLFOParameter(channel, si);

            SwapLFO(channel);
            break;

        case 0xbe:
            si = SetHardwareLFOSwitchCommand(channel, si);

            SetFMChannel3Mode(channel);
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

        case 0xba:
            si = SetVolumeMask(channel, si);
            break;

        case 0xb9:
            SwapLFO(channel);

            channel->delay = channel->delay2 = *si++;
            lfoinit_main(channel);

            SwapLFO(channel);
            break;

        case 0xb8:
            si = SetFMTLSettingCommand(channel, si);
            break;

        case 0xb7:
            si = SetMDepthCountCommand(channel, si);
            break;

        case 0xb6:
            si = SetFMFBSettingCommand(channel, si);
            break;

        case 0xb5:
            channel->sdelay_m = (~(*si++) << 4) & 0xF0;
            channel->sdelay_c = channel->sdelay = *si++;
            break;

        case 0xb4: si += 16; break;

        case 0xb3:
            channel->qdat2 = *si++;
            break;

        case 0xb2:
            channel->shift_def = *(int8_t *) si++;
            break;

        case 0xb1:
            channel->qdat3 = *si++;
            break;

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
        channel->Note = al;

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
        channel->Note = 0xFF;

        if ((channel->LFOSwitch & 0x11) == 0)
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
        if (channel->LFOSwitch & 2)
        {
            bl = channel->VolumeMask1;
            bl &= channel->FMSlotMask;    // bl=音量LFOを設定するSLOT xxxx0000b
            bh |= bl;

            CalcFMLFO(channel, channel->lfodat, bl, vol_tbl);
        }

        if (channel->LFOSwitch & 0x20)
        {
            bl = channel->VolumeMask2;
            bl &= channel->FMSlotMask;
            bh |= bl;

            CalcFMLFO(channel, channel->_lfodat, bl, vol_tbl);
        }
    }

    int dh = 0x4c - 1 + _Driver.CurrentChannel;    // dh=FM Port Address

    if (bh & 0x80) CalcVolSlot(dh,      channel->slot4, vol_tbl[0]);
    if (bh & 0x40) CalcVolSlot(dh -  8, channel->slot3, vol_tbl[1]);
    if (bh & 0x20) CalcVolSlot(dh -  4, channel->slot2, vol_tbl[2]);
    if (bh & 0x10) CalcVolSlot(dh - 12, channel->slot1, vol_tbl[3]);
}

void PMD::SetFMPitch(Channel * channel)
{
    if ((channel->fnum == 0) || (channel->FMSlotMask == 0))
        return;

    int cx = (int) (channel->fnum & 0x3800);    // cx=BLOCK
    int ax = (int) (channel->fnum) & 0x7ff;    // ax=FNUM

    // Portament/LFO/Detune SET
    ax += channel->porta_num + channel->DetuneValue;

    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0) && (_State.FMChannel3Mode != 0x3F))
        SpecialFM3Processing(channel, ax, cx);
    else
    {
        if (channel->LFOSwitch & 0x01)
            ax += channel->lfodat;

        if (channel->LFOSwitch & 0x10)
            ax += channel->_lfodat;

        CalcFMBlock(&cx, &ax);

        // SET BLOCK/FNUM TO OPN

        ax |= cx;

        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xa4 - 1), (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xa4 - 5), (uint32_t) LOBYTE(ax));
    }
}

void PMD::SetFMKeyOn(Channel * channel)
{
    if (channel->Note == 0xFF)
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
    if (channel->Note == 0xFF)
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

// Command "@ number": Set the instrument to use
uint8_t * PMD::SetFMInstrumentCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;

    channel->InstrumentNumber = al;

    int dl = channel->InstrumentNumber;

    if (channel->MuteMask == 0x00)
    {
        ActivateFMInstrumentDefinition(channel, dl);

        return si;
    }

    uint8_t * bx = GetFMInstrumentDefinition(channel, dl);

    channel->alg_fb = dl = bx[24];
    bx += 4;

    // tl設定
    channel->slot1 = bx[0];
    channel->slot3 = bx[1];
    channel->slot2 = bx[2];
    channel->slot4 = bx[3];

    //  Set fm3_alg_fb if masked in FM3ch
    if ((_Driver.CurrentChannel == 3) && channel->ToneMask)
    {
        if (_Driver.FMSelector == 0)
        {
            // in. dl = alg/fb
            if ((channel->FMSlotMask & 0x10) == 0)
            {
                al = _Driver.fm3_alg_fb & 0x38;    // fbは前の値を使用
                dl = (dl & 7) | al;
            }

            _Driver.fm3_alg_fb = dl;
            channel->alg_fb = al;
        }
    }

    return si;
}

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
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xb4 - 1), CalcPanOut(channel));
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
        channel->Note = 255;
        channel->Length = *(si + 2);
        channel->KeyOnFlag++;
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
    int cl = channel->Note;

    SetFMTone(channel, oshift(channel, *si++));

    int bx = (int) channel->fnum;      // bx=ポルタメント先のfnum値

    channel->Note = cl;
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

    channel->porta_num2 = ax / channel->Length;
    channel->porta_num3 = ax % channel->Length;
    channel->LFOSwitch |= 8;        // Porta ON

    if (channel->volpush && channel->Note != 255)
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

    channel->KeyOnFlag++;
    channel->Data = si;

    _Driver.TieMode = 0;
    _Driver.volpush_flag = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

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
            ResetFMInstrument(channel);
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
            uint8_t * bx = GetFMInstrumentDefinition(channel, channel->InstrumentNumber);

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

uint8_t * PMD::SetHardLFOCommand(Channel * channel, uint8_t * si)
{
    channel->PanAndVolume = (channel->PanAndVolume & 0xC0) | *si++;

    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
    {
        // Part_e is impossible because it is only for 2608. For FM3, set all four parts
        _FMChannel[2].PanAndVolume = channel->PanAndVolume;

        _FMExtensionChannel[0].PanAndVolume = channel->PanAndVolume;
        _FMExtensionChannel[1].PanAndVolume = channel->PanAndVolume;
        _FMExtensionChannel[2].PanAndVolume = channel->PanAndVolume;
    }

    if (channel->MuteMask == 0x00)
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xb4 - 1), CalcPanOut(channel));

    return si;
}

// Command "D number": Set the detune (frequency shift) value. Range -32768-32767.
uint8_t * PMD::SetFMAbsoluteDetuneCommand(Channel * channel, uint8_t * si)
{
    if ((_Driver.CurrentChannel != 3) || (_Driver.FMSelector != 0))
        return si + 3;

    int bl = *si++;
    int ax = *(int16_t *) si;

    si += 2;

    if (bl & 1)
        _State.slot_detune1 = ax;

    if (bl & 2)
        _State.slot_detune2 = ax;

    if (bl & 4)
        _State.slot_detune3 = ax;

    if (bl & 8)
        _State.slot_detune4 = ax;

    if (_State.slot_detune1 || _State.slot_detune2 || _State.slot_detune3 || _State.slot_detune4)
        _Driver.slotdetune_flag = 1;
    else
    {
        _Driver.slotdetune_flag = 0;
        _State.slot_detune1 = 0;
    }

    SetFMChannel3Mode2(channel);

    return si;
}

// Command "DD number": Set the detune (frequency shift) value relative to the previouse detune. Range -32768-32767.
uint8_t * PMD::SetFMRelativeDetuneCommand(Channel * channel, uint8_t * si)
{
    if ((_Driver.CurrentChannel != 3) || (_Driver.FMSelector != 0))
        return si + 3;

    int bl = *si++;
    int ax = *(int16_t *) si;

    si += 2;

    if (bl & 1)
        _State.slot_detune1 += ax;

    if (bl & 2)
        _State.slot_detune2 += ax;

    if (bl & 4)
        _State.slot_detune3 += ax;

    if (bl & 8)
        _State.slot_detune4 += ax;

    if (_State.slot_detune1 || _State.slot_detune2 || _State.slot_detune3 || _State.slot_detune4)
        _Driver.slotdetune_flag = 1;
    else
    {
        _Driver.slotdetune_flag = 0;
        _State.slot_detune1 = 0;
    }

    SetFMChannel3Mode2(channel);

    return si;
}

// Command: Sets the volume mask slot.
uint8_t * PMD::SetFMVolumeMaskSlotCommand(Channel * channel, uint8_t * si)
{
    int al = *si++ & 0x0F;

    if (al != 0)
    {
        al = (al << 4) | 0x0f;

        channel->VolumeMask1 = al;
    }
    else
        channel->VolumeMask1 = channel->carrier;

    SetFMChannel3Mode(channel);

    return si;
}

uint8_t * PMD::SetFMTLSettingCommand(Channel * channel, uint8_t * si)
{
    int dh = 0x40 - 1 + _Driver.CurrentChannel;    // dh=TL FM Port Address

    int al = *(int8_t *) si++;
    int ah = al & 0x0F;

    int ch = (channel->FMSlotMask >> 4) | ((channel->FMSlotMask << 4) & 0xF0);

    ah &= ch; // ah=slot to change 00004321

    int dl = *(int8_t *) si++;

    if (al >= 0)
    {
        dl &= 127;

        if (ah & 1)
        {
            channel->slot1 = dl;

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
        }

        dh += 8;

        if (ah & 2)
        {
            channel->slot2 = dl;

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
        }

        dh -= 4;

        if (ah & 4)
        {
            channel->slot3 = dl;

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
        }

        dh += 8;

        if (ah & 8)
        {
            channel->slot4 = dl;

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
        }
    }
    else
    {
        // Relative change
        al = dl;

        if (ah & 1)
        {
            if ((dl = (int) channel->slot1 + al) < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

            channel->slot1 = dl;
        }

        dh += 8;

        if (ah & 2)
        {
            if ((dl = (int) channel->slot2 + al) < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

            channel->slot2 = dl;
        }

        dh -= 4;

        if (ah & 4)
        {
            if ((dl = (int) channel->slot3 + al) < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

            channel->slot3 = dl;
        }

        dh += 8;

        if (ah & 8)
        {
            if ((dl = (int) channel->slot4 + al) < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

            channel->slot4 = dl;
        }
    }

    return si;
}

uint8_t * PMD::SetFMFBSettingCommand(Channel * channel, uint8_t * si)
{
    int dl;

    int dh = _Driver.CurrentChannel + 0xb0 - 1;  // dh=ALG/FB port address
    int al = *(int8_t *) si++;

    if (al >= 0)
    {
        // in  al 00000xxx FB to set
        al = ((al << 3) & 0xff) | (al >> 5);

        // in  al 00xxx000 FB to set
        if (_Driver.CurrentChannel == 3 && _Driver.FMSelector == 0)
        {
            if ((channel->FMSlotMask & 0x10) == 0) return si;
            dl = (_Driver.fm3_alg_fb & 7) | al;
            _Driver.fm3_alg_fb = dl;
        }
        else
            dl = (channel->alg_fb & 7) | al;

        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
        channel->alg_fb = dl;

        return si;
    }
    else
    {
        if ((al & 0x40) == 0)
            al &= 7;

        if (_Driver.CurrentChannel == 3 && _Driver.FMSelector == 0)
            dl = _Driver.fm3_alg_fb;
        else
            dl = channel->alg_fb;

        dl = (dl >> 3) & 7;

        if ((al += dl) >= 0)
        {
            if (al >= 8)
            {
                al = 0x38;
                if (_Driver.CurrentChannel == 3 && _Driver.FMSelector == 0)
                {
                    if ((channel->FMSlotMask & 0x10) == 0) return si;

                    dl = (_Driver.fm3_alg_fb & 7) | al;
                    _Driver.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (channel->alg_fb & 7) | al;
                }
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
                channel->alg_fb = dl;
                return si;
            }
            else
            {
                // in  al 00000xxx 設定するFB
                al = ((al << 3) & 0xff) | (al >> 5);

                // in  al 00xxx000 設定するFB
                if (_Driver.CurrentChannel == 3 && _Driver.FMSelector == 0)
                {
                    if ((channel->FMSlotMask & 0x10) == 0) return si;
                    dl = (_Driver.fm3_alg_fb & 7) | al;
                    _Driver.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (channel->alg_fb & 7) | al;
                }
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
                channel->alg_fb = dl;
                return si;
            }
        }
        else
        {
            al = 0;

            if (_Driver.CurrentChannel == 3 && _Driver.FMSelector == 0)
            {
                if ((channel->FMSlotMask & 0x10) == 0) return si;

                dl = (_Driver.fm3_alg_fb & 7) | al;
                _Driver.fm3_alg_fb = dl;
            }
            else
            {
                dl = (channel->alg_fb & 7) | al;
            }

            _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
            channel->alg_fb = dl;
            return si;
        }
    }
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
        ClearFM3(ah, al); // s0
    else
    if (channel->FMSlotMask != 0xF0)
    {
        _Driver.slot3_flag |= al;
        ah = 0x7F;
    }
    else

    if ((channel->VolumeMask1 & 0x0F) == 0)
        ClearFM3(ah, al);
    else
    if ((channel->LFOSwitch & 1) != 0)
    {
        _Driver.slot3_flag |= al;
        ah = 0x7F;
    }
    else

    if ((channel->VolumeMask2 & 0x0F) == 0)
        ClearFM3(ah, al);
    else
    if (channel->LFOSwitch & 0x10)
    {
        _Driver.slot3_flag |= al;
        ah = 0x7F;
    }
    else
        ClearFM3(ah, al);

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

void PMD::ClearFM3(int& ah, int& al)
{
    al ^= 0xFF;

    if ((_Driver.slot3_flag &= al) == 0)
        ah = (_Driver.slotdetune_flag != 1) ? 0x3F : 0x7F;
    else
        ah = 0x7F;
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
    channel->Note = 255;        // rest
    channel->DefaultNote = 255;      // rest
    channel->Volume = 108;        // FM  VOLUME DEFAULT= 108
    channel->PanAndVolume = _FMChannel[2].PanAndVolume;  // FM PAN = CH3と同じ
    channel->MuteMask |= 0x20;      // s0用 partmask
}

/// <summary>
/// Sets the various parameters of an FM instrument.
/// </summary>
void PMD::ActivateFMInstrumentDefinition(Channel * channel, int dl)
{
    uint8_t * bx = GetFMInstrumentDefinition(channel, dl);

    if (MuteFMChannel(channel))
    {
        // When _ToneMask=0 (Only TL work is set)
        bx += 4;

        // tl setting
        channel->slot1 = bx[0];
        channel->slot3 = bx[1];
        channel->slot2 = bx[2];
        channel->slot4 = bx[3];

        return;
    }

    // Set AL/FB
    int dh = 0xB0 - 1 + _Driver.CurrentChannel;

    if (_Driver.af_check)
        dl = channel->alg_fb; // Is the mode not setting ALG/FB?
    else
        dl = bx[24];

    if (_Driver.CurrentChannel == 3 && _Driver.FMSelector == 0)
    {
        if (_Driver.af_check != 0)
            dl = _Driver.fm3_alg_fb; // Is the mode not setting ALG/FB?
        else
        {
            if ((channel->FMSlotMask & 0x10) == 0)
                dl = (_Driver.fm3_alg_fb & 0x38) | (dl & 7); // Are you using slot1?

            _Driver.fm3_alg_fb = dl;
        }
    }

    _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

    channel->alg_fb = dl;
    dl &= 7;    // dl = algo

    // Check the position of Carrier (also set in VolMask)
    if ((channel->VolumeMask1 & 0x0f) == 0)
        channel->VolumeMask1 = carrier_table[dl];

    if ((channel->VolumeMask2 & 0x0f) == 0)
        channel->VolumeMask2 = carrier_table[dl];

    channel->carrier = carrier_table[dl];

    int ah = carrier_table[dl + 8];  // Reversed data of slot2/3 (not completed)
    int al = channel->ToneMask;

    ah &= al; // AH = mask for TL / AL = mask for others

    // Set each tone parameter (TL is modulator only)
    dh = 0x30 - 1 + _Driver.CurrentChannel;

    dl = *bx++;        // DT/ML
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // TL
    if (ah & 0x80) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x40) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh),(uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x20) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x10) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // KS/AR
    if (al & 0x08) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // AM/DR
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // SR
    if (al & 0x08) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // SL/RR
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
    dh += 4;
/*
    dl = *bx++;        // SL/RR
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_Driver.fmsel + dh), (uint32_t) dl);
    dh+=4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_Driver.fmsel + dh), (uint32_t) dl);
    dh+=4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_Driver.fmsel + dh), (uint32_t) dl);
    dh+=4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_Driver.fmsel + dh), (uint32_t) dl);
    dh+=4;
*/
    // Save TL for each SLOT in workpiece
    bx -= 20;
    channel->slot1 = bx[0];
    channel->slot3 = bx[1];
    channel->slot2 = bx[2];
    channel->slot4 = bx[3];
}

/// <summary>
/// Gets the definition of an FM instrument.
/// </summary>
uint8_t * PMD::GetFMInstrumentDefinition(Channel * channel, int instrumentNumber)
{
    if (_State.InstrumentDefinitions == nullptr)
    {
        if (channel != &_EffectChannel)
            return _State.VData + ((size_t) instrumentNumber << 5);
        else
            return _State.EData;
    }

    uint8_t * bx = _State.InstrumentDefinitions;

    while (*bx != instrumentNumber)
    {
        bx += 26;

        if (bx > _MData + sizeof(_MData) - 26)
            return _State.InstrumentDefinitions + 1; // Return the first definition if not found.
    }

    return bx + 1;
}

// Reset the tone of the FM sound source
void PMD::ResetFMInstrument(Channel * channel)
{
    if (channel->ToneMask == 0)
        return;

    int s1 = channel->slot1;
    int s2 = channel->slot2;
    int s3 = channel->slot3;
    int s4 = channel->slot4;

    _Driver.af_check = 1;

    ActivateFMInstrumentDefinition(channel, channel->InstrumentNumber);

    _Driver.af_check = 0;

    channel->slot1 = s1;
    channel->slot2 = s2;
    channel->slot3 = s3;
    channel->slot4 = s4;

    int dh;

    int al = ((~channel->carrier) & channel->FMSlotMask) & 0xf0;

    // al<- TLを再設定していいslot 4321xxxx
    if (al)
    {
        dh = 0x4c - 1 + _Driver.CurrentChannel;  // dh=TL FM Port Address

        if (al & 0x80) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) s4);

        dh -= 8;

        if (al & 0x40) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) s3);

        dh += 4;

        if (al & 0x20) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) s2);

        dh -= 8;

        if (al & 0x10) _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) s1);
    }

    dh = _Driver.CurrentChannel + 0xb4 - 1;

    _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), CalcPanOut(channel));
}

//  Pitch setting when using ch3=sound effect mode (input CX:block AX:fnum)
void PMD::SpecialFM3Processing(Channel * channel, int ax, int cx)
{
    int shiftmask = 0x80;

    int si = cx;

    int bh = ((channel->VolumeMask1 & 0x0F) == 0) ? 0xF0 /* All */ : channel->VolumeMask1;  // bh=lfo1 mask 4321xxxx
    int ch = ((channel->VolumeMask2 & 0x0F) == 0) ? 0xF0 /* All */ : channel->VolumeMask2;  // ch=lfo2 mask 4321xxxx

    //  slot  4
    int ax_;

    if (channel->FMSlotMask & 0x80)
    {
        ax_ = ax;
        ax += _State.slot_detune4;

        if ((bh & shiftmask) && (channel->LFOSwitch & 0x01))  ax += channel->lfodat;
        if ((ch & shiftmask) && (channel->LFOSwitch & 0x10))  ax += channel->_lfodat;
        shiftmask >>= 1;

        cx = si;
        CalcFMBlock(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xa6, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa2, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  3
    if (channel->FMSlotMask & 0x40)
    {
        ax_ = ax;
        ax += _State.slot_detune3;

        if ((bh & shiftmask) && (channel->LFOSwitch & 0x01))  ax += channel->lfodat;
        if ((ch & shiftmask) && (channel->LFOSwitch & 0x10))  ax += channel->_lfodat;
        shiftmask >>= 1;

        cx = si;
        CalcFMBlock(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xac, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa8, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  2
    if (channel->FMSlotMask & 0x20)
    {
        ax_ = ax;
        ax += _State.slot_detune2;

        if ((bh & shiftmask) && (channel->LFOSwitch & 0x01))
            ax += channel->lfodat;

        if ((ch & shiftmask) && (channel->LFOSwitch & 0x10))
            ax += channel->_lfodat;

        shiftmask >>= 1;

        cx = si;
        CalcFMBlock(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xae, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xaa, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  1
    if (channel->FMSlotMask & 0x10)
    {
        ax_ = ax;
        ax += _State.slot_detune1;

        if ((bh & shiftmask) && (channel->LFOSwitch & 0x01)) 
            ax += channel->lfodat;

        if ((ch & shiftmask) && (channel->LFOSwitch & 0x10))
            ax += channel->_lfodat;

        cx = si;
        CalcFMBlock(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xad, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa9, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }
}

// Fixed when the octave changes with detune of FM sound source.
//  input   CX: block / AX: fnum + detune
//  output  CX: block / AX: fnum
void PMD::CalcFMBlock(int * cx, int * ax)
{
    while (*ax >= 0x26a)
    {
        if (*ax < (0x26a * 2))
            return;

        *cx += 0x800;      // oct.up

        if (*cx != 0x4000)
        {
            *ax -= 0x26a;    // 4d2h-26ah
        }
        else
        {        // ﾓｳ ｺﾚｲｼﾞｮｳ ｱｶﾞﾝﾅｲﾖﾝ
            *cx = 0x3800;

            if (*ax >= 0x800)
                *ax = 0x7ff;  // 4d2h

            return;
        }
    }

    while (*ax < 0x26a)
    {
        *cx -= 0x800;      // oct.down

        if (*cx >= 0)
        {
            *ax += 0x26a;    // 4d2h-26ah
        }
        else
        {        // ﾓｳ ｺﾚｲｼﾞｮｳ ｻｶﾞﾝﾅｲﾖﾝ
            *cx = 0;

            if (*ax < 8)
                *ax = 8;

            return;
        }
    }
}

// Get the data to set to 0b4h? out.dl
uint8_t PMD::CalcPanOut(Channel * channel)
{
    int  dl = channel->PanAndVolume;

    if (channel->HardwareLFODelayCounter)
        dl &= 0xc0;  // If HLFO Delay remains, set only pan.

    return (uint8_t) dl;
}

//  Calculate and output macro for each slot.
//      in. dl  元のTL値
//          dh  Outするレジスタ
//          al  音量変動値 中心=80h
void PMD::CalcVolSlot(int dh, int dl, int al)
{
    if ((al += dl) > 255)
        al = 255;
    
    if ((al -= 0x80) < 0)
        al = 0;

    _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) al);
}

void PMD::CalcFMLFO(Channel *, int al, int bl, uint8_t * vol_tbl)
{
    if (bl & 0x80) vol_tbl[0] = (uint8_t) Limit(vol_tbl[0] - al, 255, 0);
    if (bl & 0x40) vol_tbl[1] = (uint8_t) Limit(vol_tbl[1] - al, 255, 0);
    if (bl & 0x20) vol_tbl[2] = (uint8_t) Limit(vol_tbl[2] - al, 255, 0);
    if (bl & 0x10) vol_tbl[3] = (uint8_t) Limit(vol_tbl[3] - al, 255, 0);
}
