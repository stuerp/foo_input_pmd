
// $VER: PMDADPCM.cpp (2025.12.23) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

void PMD::ADPCMMain(channel_t * channel)
{
    if (channel->_Data == nullptr)
        return;

    uint8_t * si = channel->_Data;

    channel->_Size--;

    if (channel->PartMask != 0x00)
    {
        channel->KeyOffFlag = -1;
    }
    else
    if ((channel->KeyOffFlag & 0x03) == 0)
    {
        if (channel->_Size <= channel->GateTime)
        {
            ADPCMKeyOff(channel);

            channel->KeyOffFlag = -1;
        }
    }

    if (channel->_Size == 0)
    {
        channel->HardwareLFOModulationMode &= 0xF7;

        while (1)
        {
            if ((*si != 0xDA) && (*si > 0x80))
            {
                si = ExecuteADPCMCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->_Data = si;
                channel->_LoopCheck = 0x03;

                channel->Tone = 0xFF;

                if (channel->_LoopData == nullptr)
                {
                    if (channel->PartMask != 0x00)
                    {
                        _Driver._IsTieSet = false;
                        _Driver._VolumeBoostCount = 0;

                        _Driver._LoopCheck &= channel->_LoopCheck;

                        return;
                    }
                    else
                        break;
                }

                // Start executing a loop.
                si = channel->_LoopData;

                channel->_LoopCheck = 0x01;
            }
            else
            {
                if (*si == 0xDA)
                {
                    si = SetADPCMPortamentoCommand(channel, ++si);

                    _Driver._LoopCheck &= channel->_LoopCheck;

                    return;
                }
                else
                if (channel->PartMask != 0x00)
                {
                    si++;

                    // Set to 'rest'.
                    channel->Factor = 0;
                    channel->Tone   = 0xFF;
                    channel->_Size = *si++;
                    channel->KeyOnFlag++;

                    channel->_Data = si;

                    if (--_Driver._VolumeBoostCount)
                        channel->VolumeBoost = 0;

                    _Driver._IsTieSet = false;
                    _Driver._VolumeBoostCount = 0;
                    break;
                }

                SetADPCMTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

                channel->_Size = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
                {
                    if (--_Driver._VolumeBoostCount)
                    {
                        _Driver._VolumeBoostCount = 0;
                        channel->VolumeBoost = 0;
                    }
                }

                SetADPCMVolumeCommand(channel);
                SetADPCMPitch(channel);

                if (channel->KeyOffFlag & 0x01)
                    ADPCMKeyOn(channel);

                channel->KeyOnFlag++;
                channel->_Data = si;

                _Driver._IsTieSet = false;
                _Driver._VolumeBoostCount = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
        }
    }

    _Driver._HardwareLFOModulationMode = (channel->HardwareLFOModulationMode & 0x08);

    if (channel->HardwareLFOModulationMode)
    {
        if (channel->HardwareLFOModulationMode & 0x03)
        {
            if (SetLFO(channel))
                _Driver._HardwareLFOModulationMode |= (channel->HardwareLFOModulationMode & 0x03);
        }

        if (channel->HardwareLFOModulationMode & 0x30)
        {
            SwapLFO(channel);

            if (SetSSGLFO(channel))
            {
                SwapLFO(channel);

                _Driver._HardwareLFOModulationMode |= (channel->HardwareLFOModulationMode & 0x30);
            }
            else
                SwapLFO(channel);
        }

        if (_Driver._HardwareLFOModulationMode & 0x19)
        {
            if (_Driver._HardwareLFOModulationMode & 0x08)
                CalculatePortamento(channel);

            SetADPCMPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if ((temp != 0) || _Driver._HardwareLFOModulationMode & 0x22 || (_State.FadeOutSpeed != 0))
        SetADPCMVolumeCommand(channel);

    _Driver._LoopCheck &= channel->_LoopCheck;
}

/// <summary>
/// Executes a command.
/// </summary>
uint8_t * PMD::ExecuteADPCMCommand(channel_t * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        // 6.1. Instrument Number Setting, Command '@[@] insnum' / Command '@[@] insnum[,number1[,number2[,number3]]]'
        case 0xFF:
        {
            si = SetADPCMInstrument(channel, si);
            break;
        }

        // 4.12. Sound Cut Setting 1, Command 'Q [%] numerical value' / 4.13. Sound Cut Setting 2, Command 'q [number1][-[number2]] [,number3]' / Command 'q [l length[.]][-[l length]] [,l length[.]]'
        case 0xFE:
        {
            channel->EarlyKeyOffTimeout = *si++;
            break;
        }

        // 15.1. FM Chip Direct Output, Direct register write. Writes val to address reg of the YM2608's internal memory, Command 'y number1, number2'
        case 0xEF:
        {
            _OPNAW->SetReg((uint32_t) (0x100 + si[0]), si[1]);
            si += 2;
            break;
        }

        case 0xEE: si++; break;
        case 0xED: si++; break;

        // 13.1. Pan setting 1
        case 0xEC:
            si = SetADPCMPan1(channel, si);
            break;

        // 5.5. Relative Volume Change, Command ') %number'
        case 0xE3:
        {
            channel->_Volume += *si++;

            if (channel->_Volume > 255)
                channel->_Volume = 255;
            break;
        }

        // 5.5. Relative Volume Change, Command '( %number'
        case 0xE2:
        {
            channel->_Volume -= *si++;

            if (channel->_Volume < 0)
                channel->_Volume = 0;
            break;
        }

        // 5.5. Relative Volume Change, Command ') ^%number'
        case 0xDE:
        {
            si = IncreaseVolumeForNextNote(channel, si, 255);
            break;
        }

        // 4.3. Portamento Setting
        case 0xDA:
        {
            si = SetADPCMPortamentoCommand(channel, si);
            break;
        }

        // 6.1.5. Instrument Number Setting/PCM Channels Case, Set PCM Repeat.
        case 0xCE:
        {
            si = SetADPCMRepeatCommand(channel, si);
            break;
        }

        // Set SSG Extend Mode (bit 1).
        case 0xCA:
            channel->ExtendMode = (channel->ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);
            break;

        // 8.2. Software Envelope Speed Setting, Set SSG Extend Mode (bit 2), Command 'EX number'
        case 0xC9:
        {
            channel->ExtendMode = (channel->ExtendMode & 0xFB) | ((*si++ & 0x01) << 2);
            break;
        }

        // 13.2. Pan Setting 2
        case 0xC3:
        {
            si = SetADPCMPan2(channel, si);
            break;
        }

        // 15.7. Channel Mask Control, Sets the channel mask to on or off, Command 'm number'
        case 0xC0:
        {
            si = SetADPCMChannelMaskCommand(channel, si);
            break;
        }

        // 9.11. Hardware LFO Switch/Depth Setting (OPNA), Command "# number1, [number2]": Sets the hardware LFO on (1) or off (0). (OPNA FM sound source only). Number2 = depth. Can be omitted only when switch is 0.
        case 0xBE:
        {
            si = SetHardwareLFOSwitchCommand(channel, si);
            break;
        }

        // 2.25. PPZ8 Channel Extension, Extends the PPZ8 channels with the notated channels, Command '#PPZExtend notation1[notation2[notation3]... (up to 8)]]'
        case 0xB4:
        {
            si = InitializePPZ(channel, si);
            break;
        }

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
void PMD::SetADPCMTone(channel_t * channel, int tone)
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

        if ((channel->HardwareLFOModulationMode & 0x11) == 0)
            channel->Factor = 0; // Don't use LFO pitch.
    }
}

/// <summary>
///
/// </summary>
void PMD::SetADPCMVolumeCommand(channel_t * channel)
{
    int al = channel->VolumeBoost ? channel->VolumeBoost : channel->_Volume;

    // Calculate volume down.
    al = ((256 - _State.ADPCMVolumeAdjust) * al) >> 8;

    // Calculate fade out.
    if (_State._FadeOutVolume)
        al = (((256 - _State._FadeOutVolume) * (256 - _State._FadeOutVolume) >> 8) * al) >> 8;

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

    if ((channel->HardwareLFOModulationMode & 0x22) == 0)
    {
        _OPNAW->SetReg(0x10b, (uint32_t) al);

        return;
    }

    // Calculate the LFO volume.
    int dx = (channel->HardwareLFOModulationMode & 0x02) ? channel->LFO1Data : 0;

    if (channel->HardwareLFOModulationMode & 0x20)
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
void PMD::SetADPCMPitch(channel_t * channel)
{
    if (channel->Factor == 0)
        return;

    int Pitch = (int) (channel->Factor + channel->_Portamento);

    {
        int dx = (int) (((channel->HardwareLFOModulationMode & 0x11) && (channel->HardwareLFOModulationMode & 0x01)) ? dx = channel->LFO1Data : 0);

        if (channel->HardwareLFOModulationMode & 0x10)
            dx += channel->LFO2Data;

        dx *= 4;
        dx += channel->_DetuneValue;

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
void PMD::ADPCMKeyOn(channel_t * channel)
{
    if (channel->Tone == 0xFF)
        return;

    _OPNAW->SetReg(0x101, 0x02);  // PAN=0 / x8 bit mode
    _OPNAW->SetReg(0x100, 0x21);  // PCM RESET

    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_State.PCMStart));
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_State.PCMStart));
    _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_State.PCMStop));
    _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_State.PCMStop));

    if ((_Driver._LoopBegin | _Driver._LoopEnd) == 0)
    {
        _OPNAW->SetReg(0x100, 0xa0);  // PCM PLAY (non_repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (channel->_PanAndVolume | 2));  // PAN SET / x8 bit mode
    }
    else
    {
        _OPNAW->SetReg(0x100, 0xb0);  // PCM PLAY (repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (channel->_PanAndVolume | 2));  // PAN SET / x8 bit mode

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_Driver._LoopBegin));
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_Driver._LoopBegin));
        _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_Driver._LoopEnd));
        _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_Driver._LoopEnd));
    }
}

/// <summary>
///
/// </summary>
void PMD::ADPCMKeyOff(channel_t * channel)
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

    if (_Driver._LoopRelease != 0x8000)
    {
        // PCM RESET
        _OPNAW->SetReg(0x100, 0x21);

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_Driver._LoopRelease));
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_Driver._LoopRelease));

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
uint8_t * PMD::SetADPCMInstrument(channel_t * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    _State.PCMStart = _SampleBank.Address[channel->InstrumentNumber][0];
    _State.PCMStop = _SampleBank.Address[channel->InstrumentNumber][1];

    _Driver._LoopBegin = 0;
    _Driver._LoopEnd = 0;
    _Driver._LoopRelease = 0x8000;

    return si;
}

/// <summary>
/// Command "p <value>" (1: right, 2: left, 3: center (default))
/// </summary>
uint8_t * PMD::SetADPCMPan1(channel_t * channel, uint8_t * si)
{
    channel->_PanAndVolume = (*si << 6) & 0xC0;

    return si + 1;  // Skip the Phase flag
}

/// <summary>
/// Command "px <value 1>, <value 2>" (value 1: < 0 (pan to the right), 0 (Center), > 0 (pan to the left), value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * PMD::SetADPCMPan2(channel_t * channel, uint8_t * si)
{
    if (*si == 0)
        channel->_PanAndVolume = 0xC0; // Center
    else
    if (*si < 0x80)
        channel->_PanAndVolume = 0x80; // Left
    else
        channel->_PanAndVolume = 0x40; // Right

    return si + 2; // Skip the Phase flag.
}
#pragma endregion

/// <summary>
///
/// </summary>
uint8_t * PMD::SetADPCMPortamentoCommand(channel_t * channel, uint8_t * si)
{
    if (channel->PartMask != 0x00)
    {
        // Set to 'rest'.
        channel->Factor = 0;
        channel->Tone   = 0xFF;
        channel->_Size = si[2];
        channel->_Data   = si + 3;
        channel->KeyOnFlag++;

        if (--_Driver._VolumeBoostCount)
            channel->VolumeBoost = 0;

        _Driver._IsTieSet = false;
        _Driver._VolumeBoostCount = 0;

        _Driver._LoopCheck &= channel->_LoopCheck;

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

    channel->_Size = *si++;

    si = CalculateQ(channel, si);

    channel->PortamentoQuotient = ax / channel->_Size;
    channel->PortamentoRemainder = ax % channel->_Size;
    channel->HardwareLFOModulationMode |= 0x08; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
    {
        if (--_Driver._VolumeBoostCount)
        {
            channel->VolumeBoost = 0;

            _Driver._VolumeBoostCount = 0;
        }
    }

    SetADPCMVolumeCommand(channel);
    SetADPCMPitch(channel);

    if (channel->KeyOffFlag & 0x01)
        ADPCMKeyOn(channel);

    channel->KeyOnFlag++;
    channel->_Data = si;

    _Driver._IsTieSet = false;
    _Driver._VolumeBoostCount = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver._LoopCheck &= channel->_LoopCheck;

    return si;
}

/// <summary>
/// Command "@[@] insnum[,number1[,number2[,number3]]]"
/// </summary>
uint8_t * PMD::SetADPCMRepeatCommand(channel_t *, uint8_t * si)
{
    int ax = *(int16_t *) si;

    {
        si += 2;

        ax += (ax >= 0) ? _State.PCMStart : _State.PCMStop;

        _Driver._LoopBegin = ax;
    }

    {
        ax = *(int16_t *) si;
        si += 2;

        ax += (ax > 0) ? _State.PCMStart : _State.PCMStop;

        _Driver._LoopEnd = ax;
    }

    {
        ax = *(uint16_t *) si;
        si += 2;

        if (ax < 0x8000)
            ax += _State.PCMStart;
        else
        if (ax > 0x8000)
            ax += _State.PCMStop;

        _Driver._LoopRelease = ax;
    }

    return si;
}

/// <summary>
/// 15.7. Channel Mask Control, Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// </summary>
uint8_t * PMD::SetADPCMChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept
{
    const uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->PartMask |= 0x40;

            if (channel->PartMask == 0x40)
            {
                _OPNAW->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
                _OPNAW->SetReg(0x100, 0x01);    // PCM RESET
            }
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->PartMask &= 0xBF;

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::DecreaseADPCMVolumeCommand(channel_t *, uint8_t * si)
{
    int  al = *(int8_t *) si++;

    if (al != 0)
        _State.ADPCMVolumeAdjust = std::clamp(al + _State.ADPCMVolumeAdjust, 0, 255);
    else
        _State.ADPCMVolumeAdjust = _State.DefaultADPCMVolumeAdjust;

    return si;
}
