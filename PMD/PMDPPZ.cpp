﻿
// $VER: PMDPPZ.cpp (2023.10.22) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

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

    channel->Length--;

    if (channel->MuteMask)
    {
        channel->KeyOffFlag = 0xFF;
    }
    else
    if ((channel->KeyOffFlag & 0x03) == 0)
    {
        if (channel->Length <= channel->qdat)
        {
            SetPPZKeyOff(channel);

            channel->KeyOffFlag = 0xFF;
        }
    }

    if (channel->Length == 0)
    {
        channel->ModulationMode &= 0xF7;

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
                channel->Note = 0xFF;

                if (channel->LoopData == nullptr)
                {
                    if (channel->MuteMask)
                    {
                        _Driver.TieMode = 0;
                        _Driver.IsVolumeBoostSet = 0;
                        _Driver.loop_work &= channel->loopcheck;
                        return;
                    }
                    else
                    {
                        break;
                    }
                }

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
/*
                    si++;

                    // Set to 'rest'.
                    channel->fnum = 0;
                    channel->Note = 0xFF;
                //  channel->DefaultNote = 0xFF;

                    channel->Length = *si++;
                    channel->KeyOnFlag++;
                    channel->Data = si;

                    if (--_Driver.IsVolumePushSet)
                        channel->VolumePush = 0;
*/
                    si = channel->Rest(++si, (--_Driver.IsVolumeBoostSet) != 0);

                    _Driver.TieMode = 0;
                    _Driver.IsVolumeBoostSet = 0;
                    break;
                }

                //  TONE SET
                SetPPZTone(channel, oshift(channel, StartPCMLFO(channel, *si++)));

                channel->Length = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumeBoost != 0) && (channel->Note != 0xFF))
                {
                    if (--_Driver.IsVolumeBoostSet)
                    {
                        _Driver.IsVolumeBoostSet = 0;
                        channel->VolumeBoost = 0;
                    }
                }

                SetPPZVolume(channel);
                SetPPZPitch(channel);

                if (channel->KeyOffFlag & 0x01)
                    SetPPZKeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

                _Driver.TieMode = 0;
                _Driver.IsVolumeBoostSet = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver.loop_work &= channel->loopcheck;
                return;

            }
        }
    }

    _Driver.ModulationMode = (channel->ModulationMode & 0x08);

    if (channel->ModulationMode != 0x00)
    {
        if (channel->ModulationMode & 0x03)
        {
            if (lfo(channel))
            {
                _Driver.ModulationMode |= (channel->ModulationMode & 0x03);
            }
        }

        if (channel->ModulationMode & 0x30)
        {
            SwapLFO(channel);

            if (SetSSGLFO(channel))
            {
                SwapLFO(channel);

                _Driver.ModulationMode |= (channel->ModulationMode & 0x30);
            }
            else
                SwapLFO(channel);
        }

        if (_Driver.ModulationMode & 0x19)
        {
            if (_Driver.ModulationMode & 0x08)
            {
                CalculatePortamento(channel);
            }

            SetPPZPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if (temp || _Driver.ModulationMode & 0x22 || _State.FadeOutSpeed)
        SetPPZVolume(channel);

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecutePPZCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xFF:
            si = SetPPZInstrumentCommand(channel, si);
            break;

        // Set early Key Off Timeout
        case 0xFE:
            channel->EarlyKeyOffTimeout = *si++;
            break;

        case 0xFD:
            channel->Volume = *si++;
            break;

        case 0xFC:
            si = ChangeTempoCommand(si);
            break;

        // Command "&": Tie notes together.
        case 0xFB:
            _Driver.TieMode |= 1;
            break;

        // Set detune.
        case 0xFA:
            channel->DetuneValue = *(int16_t *) si;
            si += 2;
            break;

        // Set loop start.
        case 0xF9:
            si = SetStartOfLoopCommand(channel, si);
            break;

        // Set loop end.
        case 0xF8:
            si = SetEndOfLoopCommand(channel, si);
            break;

        // Exit loop.
        case 0xF7:
            si = ExitLoopCommand(channel, si);
            break;

        // Command "L": Set the loop data.
        case 0xF6:
            channel->LoopData = si;
            break;

        // Set transposition.
        case 0xF5:
            channel->Transposition = *(int8_t *) si++;
            break;

        // Increase volume by 3dB.
        case 0xF4:
            channel->Volume += 16;

            if (channel->Volume > 255)
                channel->Volume = 255;
            break;

        // Decrease volume by 3dB.
        case 0xF3:
            channel->Volume -= 16;

            if (channel->Volume < 0)
                channel->Volume = 0;
            break;

        case 0xF2:
            si = SetModulation(channel, si);
            break;

        case 0xF1:
            si = SetModulationMask(channel, si);
            break;

        case 0xF0:
            si = SetSSGEnvelopeFormat1Command(channel, si);
            break;

        // Set SSG envelope.
        case 0xEF:
            _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + *si), (uint32_t) *(si + 1));
            si += 2;
            break;

        case 0xEE: si++; break;
        case 0xED: si++; break;

        case 0xEC:
            si = SetPPZPanningCommand(channel, si);
            break;

        case 0xEB:
            si = OPNARhythmKeyOn(si);
            break;

        case 0xEA:
            si = SetOPNARhythmVolumeCommand(si);
            break;

        case 0xE9:
            si = SetOPNARhythmPanningCommand(si);
            break;

        case 0xE8:
            si = SetOPNARhythmMasterVolumeCommand(si);
            break;

        // Modify transposition.
        case 0xE7:
            channel->Transposition += *(int8_t *) si++;
            break;

        case 0xE6:
            si = ModifyOPNARhythmMasterVolume(si);
            break;

        case 0xE5:
            si = ModifyOPNARhythmVolume(si);
            break;

        case 0xE4: si++; break;

        // Increase volume.
        case 0xE3:
            channel->Volume += *si++;

            if (channel->Volume > 255)
                channel->Volume = 255;
            break;

        // Decrease volume.
        case 0xE2:
            channel->Volume -= *si++;

            if (channel->Volume < 0)
                channel->Volume = 0;
            break;

        case 0xE1: si++; break;
        case 0xE0: si++; break;

        // Command "Z number": Set ticks per measure.
        case 0xDF:
            _State.BarLength = *si++;
            break;

        case 0xDE:
            si = IncreaseVolumeForNextNote(channel, si, 255);
            break;

        case 0xDD:
            si = DecreaseVolumeForNextNote(channel, si);
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

        case 0xcd: si = SetSSGEnvelopeFormat2Command(channel, si); break;
        case 0xcc: si++; break;
        case 0xcb: channel->LFOWaveform = *si++; break;
        case 0xca:
            channel->extendmode = (channel->extendmode & 0xFD) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            channel->extendmode = (channel->extendmode & 0xFB) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;

        // Set Early Key Off Timeout Percentage. Stops note (length * pp / 100h) ticks early, added to value of command FE.
        case 0xC4:
            channel->EarlyKeyOffTimeoutPercentage = *si++;
            break;

        case 0xc3:
            si = SetPPZPanValueExtendedCommand(channel, si);
            break;

        case 0xc2: channel->delay = channel->delay2 = *si++; lfoinit_main(channel); break;
        case 0xc1: break;

        case 0xC0:
            si = SetPPZMaskCommand(channel, si);
            break;

        case 0xbf:
            SwapLFO(channel);

            si = SetModulation(channel, si);

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

        // Set Early Key Off Timeout Randomizer Range. (0..tt ticks, added to the value of command C4 and FE)
        case 0xB1:
            channel->EarlyKeyOffTimeoutRandomRange = *si++;
            break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

#pragma region(Commands)
// Command "@ number": Sets the number of the instrument to be used. Range 0-255.
uint8_t * PMD::SetPPZInstrumentCommand(Channel * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    if ((channel->InstrumentNumber & 0x80) == 0)
        _PPZ->SetInstrument((size_t) _Driver.CurrentChannel, 0, (size_t) channel->InstrumentNumber);
    else
        _PPZ->SetInstrument((size_t) _Driver.CurrentChannel, 1, (size_t) (channel->InstrumentNumber & 0x7F));

    return si;
}
#pragma endregion

void PMD::SetPPZTone(Channel * channel, int al)
{
    if ((al & 0x0f) != 0x0f)
    {
        // Music Note
        channel->Note = al;

        int bx = al & 0x0F;
        int cl = (al >> 4) & 0x0F;

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
        channel->Note = 0xFF;

        if ((channel->ModulationMode & 0x11) == 0)
            channel->fnum = 0;
    }
}

void PMD::SetPPZVolume(Channel * channel)
{
    int al = channel->VolumeBoost ? channel->VolumeBoost : channel->Volume;

    //  音量down計算
    al = ((256 - _State.PPZVolumeDown) * al) >> 8;

    //  Fadeout計算
    if (_State.FadeOutVolume != 0)
        al = ((256 - _State.FadeOutVolume) * al) >> 8;

    //  ENVELOPE 計算
    if (al == 0)
    {
        _PPZ->SetVolume((size_t) _Driver.CurrentChannel, 0);
        _PPZ->Stop((size_t) _Driver.CurrentChannel);

        return;
    }

    if (channel->envf == -1)
    {
        // Extended version: Volume = al*(eenv_vol+1)/16
        if (channel->ExtendedAttackLevel == 0)
        {
        //  _PPZ->SetVol((Size_t) _Driver._CurrentChannel, 0);
            _PPZ->Stop((size_t) _Driver.CurrentChannel);

            return;
        }

        al = ((((al * (channel->ExtendedAttackLevel + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        if (channel->ExtendedAttackLevel < 0)
        {
            int ah = -channel->ExtendedAttackLevel * 16;

            if (al < ah)
            {
            //  _PPZ->SetVol((Size_t) _Driver._CurrentChannel, 0);
                _PPZ->Stop((size_t) _Driver.CurrentChannel);

                return;
            }
            else
                al -= ah;
        }
        else
        {
            int ah = channel->ExtendedAttackLevel * 16;

            if (al + ah > 255)
                al = 255;
            else
                al += ah;
        }
    }

    // Calculate the LFO volume.
    if ((channel->ModulationMode & 0x22))
    {
        int dx = (channel->ModulationMode & 0x02) ? channel->LFODat1 : 0;

        if (channel->ModulationMode & 0x20)
            dx += channel->LFODat2;

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
        _PPZ->SetVolume((size_t) _Driver.CurrentChannel, al >> 4);
    else
        _PPZ->Stop((size_t) _Driver.CurrentChannel);
}

void PMD::SetPPZPitch(Channel * channel)
{
    uint32_t cx = channel->fnum;

    if (cx == 0)
        return;

    cx += channel->porta_num * 16;

    int ax = (channel->ModulationMode & 0x01) ? channel->LFODat1 : 0;

    if (channel->ModulationMode & 0x10)
        ax += channel->LFODat2;

    ax += channel->DetuneValue;

    int64_t cx2 = cx + ((int64_t) cx) / 256 * ax;

    if (cx2 > 0xffffffff)
        cx = 0xffffffff;
    else
    if (cx2 < 0)
        cx = 0;
    else
        cx = (uint32_t) cx2;

    _PPZ->SetPitch((size_t) _Driver.CurrentChannel, cx);
}

void PMD::SetPPZKeyOn(Channel * channel)
{
    if (channel->Note == 0xFF)
        return;

    if ((channel->InstrumentNumber & 0x80) == 0)
        _PPZ->Play((size_t) _Driver.CurrentChannel, 0, channel->InstrumentNumber,        0, 0);
    else
        _PPZ->Play((size_t) _Driver.CurrentChannel, 1, channel->InstrumentNumber & 0x7F, 0, 0);
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
        if (channel->ExtendedCount == 4)
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
        channel->Note = 0xFF;
        channel->Length = si[2];
        channel->KeyOnFlag++;
        channel->Data = si + 3;

        if (--_Driver.IsVolumeBoostSet)
            channel->VolumeBoost = 0;

        _Driver.TieMode = 0;
        _Driver.IsVolumeBoostSet = 0;
        _Driver.loop_work &= channel->loopcheck;

        return si + 3; // Skip when muted.
    }

    SetPPZTone(channel, oshift(channel, StartPCMLFO(channel, *si++)));

    int bx_ = (int) channel->fnum;
    int al_ = channel->Note;

    SetPPZTone(channel, oshift(channel, *si++));

    int ax = (int) channel->fnum;

    channel->Note = al_;
    channel->fnum = (uint32_t) bx_;

    ax -= bx_;
    ax /= 16;

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->porta_num2 = ax / channel->Length;
    channel->porta_num3 = ax % channel->Length;
    channel->ModulationMode |= 8; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->Note != 0xFF))
    {
        if (--_Driver.IsVolumeBoostSet)
        {
            _Driver.IsVolumeBoostSet = 0;
            channel->VolumeBoost = 0;
        }
    }

    SetPPZVolume(channel);
    SetPPZPitch(channel);

    if (channel->KeyOffFlag & 0x01)
        SetPPZKeyOn(channel);

    channel->KeyOnFlag++;
    channel->Data = si;

    _Driver.TieMode = 0;
    _Driver.IsVolumeBoostSet = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver.loop_work &= channel->loopcheck;

    return si;
}

// Command "p <value>" (1: right, 2: left, 3: center (default))
uint8_t * PMD::SetPPZPanningCommand(Channel * channel, uint8_t * si)
{
    static const int PanValues[4] = { 0, 9, 1, 5 }; // { Left, Right, Leftwards, Rightwards }

    channel->PanAndVolume = PanValues[*si++];

    _PPZ->SetPan((size_t) _Driver.CurrentChannel, channel->PanAndVolume);

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

    _PPZ->SetPan((size_t) _Driver.CurrentChannel, channel->PanAndVolume);

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
            _PPZChannel[i].KeyOffFlag = 0xFF;
            _PPZChannel[i].mdc = -1;            // MDepth Counter (無限)
            _PPZChannel[i].mdc2 = -1;
            _PPZChannel[i]._mdc = -1;
            _PPZChannel[i]._mdc2 = -1;
            _PPZChannel[i].Note = 0xFF;         // Rest
            _PPZChannel[i].DefaultNote = 0xFF;  // Rest
            _PPZChannel[i].Volume = 128;
            _PPZChannel[i].PanAndVolume = 5;    // Center
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

        LoopEnd = *(int16_t *) si;
        si += 2;

        _PPZ->SetLoop((size_t) _Driver.CurrentChannel, 0, (size_t) channel->InstrumentNumber, LoopBegin, LoopEnd);
    }
    else
    {
        LoopBegin = *(int16_t *) si;
        si += 2;

        LoopEnd = *(int16_t *) si;
        si += 2;

        _PPZ->SetLoop((size_t) _Driver.CurrentChannel, 1, (size_t) channel->InstrumentNumber & 0x7F, LoopBegin, LoopEnd);
    }

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
                _PPZ->Stop((size_t) _Driver.CurrentChannel);
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
