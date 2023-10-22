
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

    if (channel->MuteMask)
    {
        channel->KeyOffFlag = 0xFF;
    }
    else
    if ((channel->KeyOffFlag & 0x03) == 0)
    {
        if (channel->Length <= channel->qdat)
        {
            SetADPCMKeyOff(channel);

            channel->KeyOffFlag = 0xFF;
        }
    }

    if (channel->Length == 0)
    {
        channel->LFOSwitch &= 0xf7;

        while (1)
        {
            if ((*si > 0x80) && (*si != 0xda))
            {
                si = ExecuteADPCMCommand(channel, si);
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
                        _Driver.IsVolumePushSet = 0;
                        _Driver.loop_work &= channel->loopcheck;

                        return;
                    }
                    else
                        break;
                }

                si = channel->LoopData;

                channel->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {
                    si = SetADPCMPortamentoCommand(channel, ++si);

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
                    si = channel->Rest(++si, (--_Driver.IsVolumePushSet) != 0);

                    _Driver.TieMode = 0;
                    _Driver.IsVolumePushSet = 0;
                    break;
                }

                SetADPCMTone(channel, oshift(channel, StartPCMLFO(channel, *si++)));

                channel->Length = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumePush != 0) && (channel->Note != 0xFF))
                {
                    if (--_Driver.IsVolumePushSet)
                    {
                        _Driver.IsVolumePushSet = 0;
                        channel->VolumePush = 0;
                    }
                }

                SetADPCMVolumeCommand(channel);
                SetADPCMPitch(channel);

                if (channel->KeyOffFlag & 0x01)
                    SetADPCMKeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

                _Driver.TieMode = 0;
                _Driver.IsVolumePushSet = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    _Driver.lfo_switch = (channel->LFOSwitch & 8);

    if (channel->LFOSwitch)
    {
        if (channel->LFOSwitch & 3)
        {
            if (lfo(channel))
                _Driver.lfo_switch |= (channel->LFOSwitch & 3);
        }

        if (channel->LFOSwitch & 0x30)
        {
            SwapLFO(channel);

            if (SetSSGLFO(channel))
            {
                SwapLFO(channel);
                _Driver.lfo_switch |= (channel->LFOSwitch & 0x30);
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
        case 0xFF:
            si = SetADPCMInstrumentCommand(channel, si);
            break;

        // Set Early Key Off Timeout.
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

        case 0xf5:
            channel->shift = *(int8_t *) si++;
            break;

        case 0xf4:
            if (channel->Volume < (255 - 16))
                channel->Volume += 16;
            else
                channel->Volume = 255;
            break;

        case 0xf3:
            if (channel->Volume < 16)
                channel->Volume = 0;
            else
                channel->Volume -= 16;
            break;

        case 0xf2:
            si = SetLFOParameter(channel, si);
            break;

        case 0xf1:
            si = lfoswitch(channel, si);
            break;

        case 0xf0:
            si = SetSSGEnvelopeFormat1Command(channel, si);
            break;

        case 0xef:
            _OPNAW->SetReg((uint32_t) (0x100 + si[0]), (uint32_t) si[1]);
            si += 2;
            break;

        case 0xee: si++; break;
        case 0xed: si++; break;

        case 0xec: si = SetADPCMPanningCommand(channel, si); break;        // FOR SB2
        case 0xeb: si = RhythmInstrumentCommand(si); break;
        case 0xea: si = SetRhythmInstrumentVolumeCommand(si); break;
        case 0xe9: si = SetRhythmPanCommand(si); break;
        case 0xe8: si = SetRhythmMasterVolumeCommand(si); break;

        case 0xe7: channel->shift += *(int8_t *) si++; break;
        case 0xe6: si = SetRhythmVolume(si); break;
        case 0xe5: si = SetRhythmPanValue(si); break;

        case 0xe4: si++; break;

        case 0xe3:
            if (channel->Volume < (255 - (*si)))
                channel->Volume += (*si);
            else
                channel->Volume = 255;
            si++;
            break;

        case 0xe2:
            if (channel->Volume < *si)
                channel->Volume = 0;
            else
                channel->Volume -= *si;
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

        case 0xdc:
            _State.status = *si++;
            break;

        case 0xdb: _State.status += *si++;
            break;

        case 0xda:
            si = SetADPCMPortamentoCommand(channel, si);
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

        case 0xcf: si++; break;
        case 0xce:
            si = SetADPCMRepeatCommand(channel, si);
            break;

        case 0xcd:
            si = SetSSGEnvelopeFormat2Command(channel, si);
            break;

        case 0xcc: si++; break;

        case 0xcb:
            channel->LFOWaveform = *si++;
            break;

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
            si = SetADPCMPanningExtendCommand(channel, si);
            break;

        case 0xc2:
            channel->delay = channel->delay2 = *si++;
            lfoinit_main(channel);
            break;

        case 0xc1: break;

        case 0xC0:
            si = SetADPCMMaskCommand(channel, si);
            break;

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
// FIXME    break;

            SwapLFO(channel);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = SetMDepthCountCommand(channel, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si = InitializePPZ(channel, si); break;
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
uint8_t * PMD::SetADPCMInstrumentCommand(Channel * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    _State.PCMStart = _SampleBank.Address[channel->InstrumentNumber][0];
    _State.PCMStop = _SampleBank.Address[channel->InstrumentNumber][1];

    _Driver.LoopBegin = 0;
    _Driver.LoopEnd = 0;
    _Driver.LoopRelease = 0x8000;

    return si;
}
#pragma endregion

void PMD::SetADPCMTone(Channel * channel, int al)
{
    if ((al & 0x0F) != 0x0F)
    {
        // Music Note
        channel->Note = al;

        int bx = al & 0x0F;
        int ch = (al >> 4) & 0x0F;
        int cl = ch;

        cl = (cl > 5) ? 0 : 5 - cl;

        int ax = pcm_tune_data[bx];

        if (ch >= 6)
        {
            ch = 0x50;

            if (ax < 0x8000)
            {
                ax *= 2;
                ch = 0x60;
            }

            channel->Note = (channel->Note & 0x0F) | ch;
        }
        else
            ax >>= cl;

        channel->fnum = (uint32_t) ax;
    }
    else
    {
        // Rest
        channel->Note = 0xFF;

        if ((channel->LFOSwitch & 0x11) == 0)
            channel->fnum = 0;
    }
}

void PMD::SetADPCMVolumeCommand(Channel * channel)
{
    int al = channel->VolumePush ? channel->VolumePush : channel->Volume;

    //  音量down計算
    al = ((256 - _State.ADPCMVolumeDown) * al) >> 8;

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
        if (channel->ExtendedAttackLevel == 0)
        {
            _OPNAW->SetReg(0x10b, 0);

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
                _OPNAW->SetReg(0x10b, 0);

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

    //  音量LFO計算
    if ((channel->LFOSwitch & 0x22) == 0)
    {
        _OPNAW->SetReg(0x10b, (uint32_t) al);

        return;
    }

    int dx = (channel->LFOSwitch & 2) ? channel->lfodat : 0;

    if (channel->LFOSwitch & 0x20)
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
    int dx = (int) (((channel->LFOSwitch & 0x11) && (channel->LFOSwitch & 1)) ? dx = channel->lfodat : 0);

    if (channel->LFOSwitch & 0x10)
        dx += channel->_lfodat;

    dx *= 4;  // PCM ﾊ LFO ｶﾞ ｶｶﾘﾆｸｲ ﾉﾃﾞ depth ｦ 4ﾊﾞｲ ｽﾙ

    dx += channel->DetuneValue;

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

void PMD::SetADPCMKeyOn(Channel * channel)
{
    if (channel->Note == 0xFF)
        return;

    _OPNAW->SetReg(0x101, 0x02);  // PAN=0 / x8 bit mode
    _OPNAW->SetReg(0x100, 0x21);  // PCM RESET

    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_State.PCMStart));
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_State.PCMStart));
    _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_State.PCMStop));
    _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_State.PCMStop));

    if ((_Driver.LoopBegin | _Driver.LoopEnd) == 0)
    {
        _OPNAW->SetReg(0x100, 0xa0);  // PCM PLAY (non_repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (channel->PanAndVolume | 2));  // PAN SET / x8 bit mode
    }
    else
    {
        _OPNAW->SetReg(0x100, 0xb0);  // PCM PLAY (repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (channel->PanAndVolume | 2));  // PAN SET / x8 bit mode

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_Driver.LoopBegin));
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_Driver.LoopBegin));
        _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_Driver.LoopEnd));
        _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_Driver.LoopEnd));
    }
}

void PMD::SetADPCMKeyOff(Channel * channel)
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

    if (_Driver.LoopRelease != 0x8000)
    {
        // PCM RESET
        _OPNAW->SetReg(0x100, 0x21);

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_Driver.LoopRelease));
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_Driver.LoopRelease));

        // Stop ADDRESS for Release
        _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_State.PCMStop));
        _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_State.PCMStop));

        // PCM PLAY(non_repeat)
        _OPNAW->SetReg(0x100, 0xa0);
    }

    SetSSGKeyOff(channel);
}

// Sets ADPCM Wait after register output.
void PMD::SetADPCMDelay(int nsec)
{
    _OPNAW->SetADPCMDelay(nsec);
}

// Command "p <value>" (1: right, 2: left, 3: center (default))
uint8_t * PMD::SetADPCMPanningCommand(Channel * channel, uint8_t * si)
{
    channel->PanAndVolume = (*si << 6) & 0xC0;

    return si + 1;  // Skip the Phase flag
}

// Command "px <value 1>, <value 2>" (value 1: < 0 (pan to the right), 0 (Center), > 0 (pan to the left), value 2: 0 (In phase) or 1 (Reverse phase)).
uint8_t * PMD::SetADPCMPanningExtendCommand(Channel * channel, uint8_t * si)
{
    if (*si == 0)
        channel->PanAndVolume = 0xC0; // Center
    else
    if (*si < 0x80)
        channel->PanAndVolume = 0x80; // Left
    else
        channel->PanAndVolume = 0x40; // Right

    return si + 2; // Skip the Phase flag.
}

uint8_t * PMD::SetADPCMPortamentoCommand(Channel * channel, uint8_t * si)
{
    if (channel->MuteMask)
    {
        channel->fnum = 0;
        channel->Note = 0xFF;
        channel->Length = *(si + 2);
        channel->KeyOnFlag++;
        channel->Data = si + 3;

        if (--_Driver.IsVolumePushSet)
            channel->VolumePush = 0;

        _Driver.TieMode = 0;
        _Driver.IsVolumePushSet = 0;
        _Driver.loop_work &= channel->loopcheck;

        return si + 3; // Skip when muted
    }

    SetADPCMTone(channel, oshift(channel, StartPCMLFO(channel, *si++)));

    int bx_ = (int) channel->fnum;
    int al_ = (int) channel->Note;

    SetADPCMTone(channel, oshift(channel, *si++));

    int ax = (int) channel->fnum;

    channel->Note = al_;
    channel->fnum = (uint32_t) bx_;

    ax -= bx_;

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->porta_num2 = ax / channel->Length;
    channel->porta_num3 = ax % channel->Length;
    channel->LFOSwitch |= 8; // Portamento on

    if ((channel->VolumePush != 0) && (channel->Note != 0xFF))
    {
        if (--_Driver.IsVolumePushSet)
        {
            channel->VolumePush = 0;

            _Driver.IsVolumePushSet = 0;
        }
    }

    SetADPCMVolumeCommand(channel);
    SetADPCMPitch(channel);

    if (channel->KeyOffFlag & 0x01)
        SetADPCMKeyOn(channel);

    channel->KeyOnFlag++;
    channel->Data = si;

    _Driver.TieMode = 0;
    _Driver.IsVolumePushSet = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver.loop_work &= channel->loopcheck;

    return si;
}

// Command "@[@] insnum[,number1[,number2[,number3]]]"
uint8_t * PMD::SetADPCMRepeatCommand(Channel *, uint8_t * si)
{
    int ax = *(int16_t *) si;

    {
        si += 2;

        ax += (ax >= 0) ? _State.PCMStart : _State.PCMStop;

        _Driver.LoopBegin = ax;
    }

    {
        ax = *(int16_t *) si;
        si += 2;

        ax += (ax > 0) ? _State.PCMStart : _State.PCMStop;

        _Driver.LoopEnd = ax;
    }

    {
        ax = *(uint16_t *) si;
        si += 2;

        if (ax < 0x8000)
            ax += _State.PCMStart;
        else
        if (ax > 0x8000)
            ax += _State.PCMStop;

        _Driver.LoopRelease = ax;
    }

    return si;
}

// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
uint8_t * PMD::SetADPCMMaskCommand(Channel * channel, uint8_t * si)
{
    uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->MuteMask |= 0x40;

            if (channel->MuteMask == 0x40)
            {
                _OPNAW->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
                _OPNAW->SetReg(0x100, 0x01);    // PCM RESET
            }
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->MuteMask &= 0xBF;

    return si;
}

uint8_t * PMD::DecreaseADPCMVolumeCommand(Channel *, uint8_t * si)
{
    int  al = *(int8_t *) si++;

    if (al != 0)
        _State.ADPCMVolumeDown = Limit(al + _State.ADPCMVolumeDown, 255, 0);
    else
        _State.ADPCMVolumeDown = _State.DefaultADPCMVolumeDown;

    return si;
}
