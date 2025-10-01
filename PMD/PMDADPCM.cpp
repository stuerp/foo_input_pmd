
// $VER: PMDADPCM.cpp (2023.10.22) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

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
        if (channel->Length <= channel->GateTime)
        {
            ADPCMKeyOff(channel);

            channel->KeyOffFlag = 0xFF;
        }
    }

    if (channel->Length == 0)
    {
        channel->ModulationMode &= 0xF7;

        while (1)
        {
            if ((*si != 0xDA) && (*si > 0x80))
            {
                si = ExecuteADPCMCommand(channel, si);
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
                        _Driver.TieNotesTogether = false;
                        _Driver.IsVolumeBoostSet = 0;
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
                if (*si == 0xDA)
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
                    si = channel->Rest(++si, (--_Driver.IsVolumeBoostSet) != 0);

                    _Driver.TieNotesTogether = false;
                    _Driver.IsVolumeBoostSet = 0;
                    break;
                }

                SetADPCMTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

                channel->Length = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
                {
                    if (--_Driver.IsVolumeBoostSet)
                    {
                        _Driver.IsVolumeBoostSet = 0;
                        channel->VolumeBoost = 0;
                    }
                }

                SetADPCMVolumeCommand(channel);
                SetADPCMPitch(channel);

                if (channel->KeyOffFlag & 0x01)
                    ADPCMKeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

                _Driver.TieNotesTogether = false;
                _Driver.IsVolumeBoostSet = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    _Driver.ModulationMode = (channel->ModulationMode & 0x08);

    if (channel->ModulationMode)
    {
        if (channel->ModulationMode & 0x03)
        {
            if (SetLFO(channel))
                _Driver.ModulationMode |= (channel->ModulationMode & 0x03);
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
                CalculatePortamento(channel);

            SetADPCMPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if ((temp != 0) || _Driver.ModulationMode & 0x22 || (_State.FadeOutSpeed != 0))
        SetADPCMVolumeCommand(channel);

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecuteADPCMCommand(Channel * channel, uint8_t * si)
{
    uint8_t Command = *si++;

    switch (Command)
    {
        case 0xFF:
            si = SetADPCMInstrument(channel, si);
            break;

        // Set Early Key Off Timeout.
        case 0xFE:
            channel->EarlyKeyOffTimeout = *si++;
            break;
/*
        case 0xFD:
            channel->Volume = *si++;
            break;

        case 0xFC:
            si = ChangeTempoCommand(si);
            break;

        // Command "&": Tie notes together.
        case 0xFB:
            _Driver.TieNotesTogether = true;
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

            if (channel->Volume < 16)
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
*/
        // Set SSG envelope.
        case 0xEF:
            _OPNAW->SetReg((uint32_t) (0x100 + si[0]), si[1]);
            si += 2;
            break;

        case 0xEE: si++; break;
        case 0xED: si++; break;

        case 0xEC:
            si = SetADPCMPanning(channel, si);
            break;
/*
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
*/
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
/*
        case 0xE1: si++; break;
        case 0xE0: si++; break;

        // Command "Z number": Set ticks per measure.
        case 0xDF:
            _State.BarLength = *si++;
            break;
*/
        case 0xDE:
            si = IncreaseVolumeForNextNote(channel, si, 255);
            break;
/*
        case 0xDD:
            si = DecreaseVolumeForNextNote(channel, si);
            break;

        // Set status.
        case 0xDC:
            _State.Status = *si++;
            break;

        // Increment status.
        case 0xDB:
            _State.Status += *si++;
            break;
*/
        // Set portamento.
        case 0xDA:
            si = SetADPCMPortamentoCommand(channel, si);
            break;
/*
        case 0xD9: si++; break;
        case 0xD8: si++; break;
        case 0xD7: si++; break;

        case 0xD6:
            channel->LFO1MDepthSpeed1 = channel->LFO1MDepthSpeed2 = *si++;
            channel->LFO1MDepth = *(int8_t *) si++;
            break;

        case 0xD5:
            channel->DetuneValue += *(int16_t *) si;
            si += 2;
            break;

        case 0xD4:
            si = SetSSGEffect(channel, si);
            break;

        case 0xD3:
            si = SetFMEffect(channel, si);
            break;

        case 0xD2:
            _State.FadeOutSpeed = *si++;
            _State.FadeOutSpeedSet = true;
            break;

        case 0xD1: si++; break;
        case 0xD0: si++; break;
        case 0xCF: si++; break;
*/
        // Set PCM Repeat.
        case 0xCE:
            si = SetADPCMRepeatCommand(channel, si);
            break;
/*
        case 0xCD:
            si = SetSSGEnvelopeFormat2Command(channel, si);
            break;

        // Set SSG Extend Mode (bit 0).
        case 0xCC: si++; break;

        case 0xCB:
            channel->LFO1Waveform = *si++;
            break;
*/
        // Set SSG Extend Mode (bit 1).
        case 0xCA:
            channel->ExtendMode = (channel->ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);
            break;

        // Set SSG Extend Mode (bit 2).
        case 0xC9:
            channel->ExtendMode = (channel->ExtendMode & 0xFB) | ((*si++ & 0x01) << 2);
            break;
/*
        case 0xC8: si += 3; break;
        case 0xC7: si += 3; break;
        case 0xC6: si += 6; break;
        case 0xC5: si++; break;

        // Set Early Key Off Timeout Percentage. Stops note (length * pp / 100h) ticks early, added to value of command FE.
        case 0xC4:
            channel->EarlyKeyOffTimeoutPercentage = *si++;
            break;
*/
        case 0xC3:
            si = SetADPCMPanningExtend(channel, si);
            break;
/*
        case 0xC2:
            channel->Delay1 = channel->Delay2 = *si++;
            InitializeLFOMain(channel);
            break;
*/
        case 0xC1: break;

        case 0xC0:
            si = SetADPCMMaskCommand(channel, si);
            break;
/*
        case 0xBF:
            SwapLFO(channel);

            si = SetModulation(channel, si);

            SwapLFO(channel);
            break;
*/
        case 0xBE:
            si = SetHardwareLFOSwitchCommand(channel, si);
            break;
/*
        case 0xBD:
            SwapLFO(channel);

            channel->LFO1MDepthSpeed1 = channel->LFO1MDepthSpeed2 = *si++;
            channel->LFO1MDepth = *(int8_t *) si++;

            SwapLFO(channel);
            break;

        case 0xBC:
            SwapLFO(channel);

            channel->LFO1Waveform = *si++;

            SwapLFO(channel);
            break;

        case 0xBB:
            SwapLFO(channel);

            channel->ExtendMode = (channel->ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);

            SwapLFO(channel);
            break;

        case 0xBA:
            si = SetVolumeMask(channel, si);
            break;

        case 0xB9:
            SwapLFO(channel);

            channel->LFO1Delay1 = channel->LFO1Delay2 = *si++;
            InitializeLFOMain(channel);

            SwapLFO(channel);
            break;

        case 0xB8: si += 2; break;

        case 0xB7:
            si = SetMDepthCountCommand(channel, si);
            break;

        case 0xB6: si++; break;
        case 0xB5: si += 2; break;
*/
        case 0xB4:
            si = InitializePPZ(channel, si);
            break;
/*
        // Set Early Key Off Timeout 2. Stop note after n ticks or earlier depending on the result of B1/C4/FE happening first.
        case 0xB3:
            channel->EarlyKeyOffTimeout2 = *si++;
            break;

        case 0xB2:
            channel->Transposition2 = *(int8_t *) si++;
            break;

        // Set Early Key Off Timeout Randomizer Range. (0..tt ticks, added to the value of command C4 and FE)
        case 0xB1:
            channel->EarlyKeyOffTimeoutRandomRange = *si++;
            break;
*/
        default:
            si = ExecuteCommand(channel, si, Command);
    }

    return si;
}

/// <summary>
/// Sets ADPCM Wait after register output.
/// </summary>
void PMD::SetADPCMDelay(int nsec)
{
    _OPNAW->SetADPCMDelay(nsec);
}

#pragma region(Commands)
/// <summary>
///
/// </summary>
void PMD::SetADPCMTone(Channel * channel, int tone)
{
    if ((tone & 0x0F) != 0x0F)
    {
        channel->Tone = tone;

        int Block = tone & 0x0F;

        int ch = (tone >> 4) & 0x0F;
        int cl = ch;

        cl = (cl > 5) ? 0 : 5 - cl;

        int Factor = PCMScaleFactor[Block];

        if (ch >= 6)
        {
            ch = 0x50;

            if (Factor < 0x8000)
            {
                Factor *= 2;
                ch = 0x60;
            }

            channel->Tone = (channel->Tone & 0x0F) | ch;
        }
        else
            Factor >>= cl;

        channel->Factor = (uint32_t) Factor;
    }
    else
    {
        // Rest
        channel->Tone = 0xFF;

        if ((channel->ModulationMode & 0x11) == 0)
            channel->Factor = 0; // Don't use LFO pitch.
    }
}

/// <summary>
///
/// </summary>
void PMD::SetADPCMVolumeCommand(Channel * channel)
{
    int al = channel->VolumeBoost ? channel->VolumeBoost : channel->Volume;

    // Calculate volume down.
    al = ((256 - _State.ADPCMVolumeAdjust) * al) >> 8;

    // Calculate fade out.
    if (_State.FadeOutVolume)
        al = (((256 - _State.FadeOutVolume) * (256 - _State.FadeOutVolume) >> 8) * al) >> 8;

    if (al == 0)
    {
        _OPNAW->SetReg(0x10b, 0);

        return;
    }

    // Calculate envelope.
    if (channel->SSGEnvelopFlag == -1)
    {
        // Extended version: Volume = al * (eenv_vol + 1) / 16
        if (channel->ExtendedAttackLevel == 0)
        {
            _OPNAW->SetReg(0x10b, 0);

            return;
        }

        al = ((((al * (channel->ExtendedAttackLevel + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        // Extended envelope volume
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

    if ((channel->ModulationMode & 0x22) == 0)
    {
        _OPNAW->SetReg(0x10b, (uint32_t) al);

        return;
    }

    // Calculate the LFO volume.
    int dx = (channel->ModulationMode & 0x02) ? channel->LFO1Data : 0;

    if (channel->ModulationMode & 0x20)
        dx += channel->LFO2Data;

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

/// <summary>
///
/// </summary>
void PMD::SetADPCMPitch(Channel * channel)
{
    if (channel->Factor == 0)
        return;

    int Pitch = (int) (channel->Factor + channel->Portamento);

    {
        int dx = (int) (((channel->ModulationMode & 0x11) && (channel->ModulationMode & 0x01)) ? dx = channel->LFO1Data : 0);

        if (channel->ModulationMode & 0x10)
            dx += channel->LFO2Data;

        dx *= 4;
        dx += channel->DetuneValue;

        if (dx >= 0)
        {
            Pitch += dx;

            if (Pitch > 0xffff)
                Pitch = 0xffff;
        }
        else
        {
            Pitch += dx;

            if (Pitch < 0)
                Pitch = 0;
        }
    }

    _OPNAW->SetReg(0x109, (uint32_t) LOBYTE(Pitch));
    _OPNAW->SetReg(0x10a, (uint32_t) HIBYTE(Pitch));
}

/// <summary>
///
/// </summary>
void PMD::ADPCMKeyOn(Channel * channel)
{
    if (channel->Tone == 0xFF)
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

/// <summary>
///
/// </summary>
void PMD::ADPCMKeyOff(Channel * channel)
{
    if (channel->SSGEnvelopFlag != -1)
    {
        if (channel->SSGEnvelopFlag == 2)
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

    SSGKeyOff(channel);
}

/// <summary>
/// Command "@ number": Sets the instrument to be used. Range 0-255.
/// </summary>
uint8_t * PMD::SetADPCMInstrument(Channel * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    _State.PCMStart = _SampleBank.Address[channel->InstrumentNumber][0];
    _State.PCMStop = _SampleBank.Address[channel->InstrumentNumber][1];

    _Driver.LoopBegin = 0;
    _Driver.LoopEnd = 0;
    _Driver.LoopRelease = 0x8000;

    return si;
}

/// <summary>
/// Command "p <value>" (1: right, 2: left, 3: center (default))
/// </summary>
uint8_t * PMD::SetADPCMPanning(Channel * channel, uint8_t * si)
{
    channel->PanAndVolume = (*si << 6) & 0xC0;

    return si + 1;  // Skip the Phase flag
}

/// <summary>
/// Command "px <value 1>, <value 2>" (value 1: < 0 (pan to the right), 0 (Center), > 0 (pan to the left), value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * PMD::SetADPCMPanningExtend(Channel * channel, uint8_t * si)
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
#pragma endregion

/// <summary>
///
/// </summary>
uint8_t * PMD::SetADPCMPortamentoCommand(Channel * channel, uint8_t * si)
{
    if (channel->MuteMask)
    {
        channel->Factor = 0;
        channel->Tone = 0xFF;
        channel->Length = si[2];
        channel->KeyOnFlag++;
        channel->Data = si + 3;

        if (--_Driver.IsVolumeBoostSet)
            channel->VolumeBoost = 0;

        _Driver.TieNotesTogether = false;
        _Driver.IsVolumeBoostSet = 0;
        _Driver.loop_work &= channel->loopcheck;

        return si + 3; // Skip when muted
    }

    SetADPCMTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

    int bx_ = (int) channel->Factor;
    int al_ = (int) channel->Tone;

    SetADPCMTone(channel, Transpose(channel, *si++));

    int ax = (int) channel->Factor;

    channel->Tone = al_;
    channel->Factor = (uint32_t) bx_;

    ax -= bx_;

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->PortamentoQuotient = ax / channel->Length;
    channel->PortamentoRemainder = ax % channel->Length;
    channel->ModulationMode |= 0x08; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
    {
        if (--_Driver.IsVolumeBoostSet)
        {
            channel->VolumeBoost = 0;

            _Driver.IsVolumeBoostSet = 0;
        }
    }

    SetADPCMVolumeCommand(channel);
    SetADPCMPitch(channel);

    if (channel->KeyOffFlag & 0x01)
        ADPCMKeyOn(channel);

    channel->KeyOnFlag++;
    channel->Data = si;

    _Driver.TieNotesTogether = false;
    _Driver.IsVolumeBoostSet = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver.loop_work &= channel->loopcheck;

    return si;
}

/// <summary>
/// Command "@[@] insnum[,number1[,number2[,number3]]]"
/// </summary>
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

/// <summary>
/// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// </summary>
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

/// <summary>
///
/// </summary>
uint8_t * PMD::DecreaseADPCMVolumeCommand(Channel *, uint8_t * si)
{
    int  al = *(int8_t *) si++;

    if (al != 0)
        _State.ADPCMVolumeAdjust = Limit(al + _State.ADPCMVolumeAdjust, 255, 0);
    else
        _State.ADPCMVolumeAdjust = _State.DefaultADPCMVolumeAdjust;

    return si;
}
