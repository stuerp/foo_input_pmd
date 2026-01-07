
// $VER: PMDADPCM.cpp (2025.12.23) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

/// <summary>
/// Main ADPCM processing
/// </summary>
void pmd_driver_t::ADPCMMain(channel_t * channel)
{
    if (channel->_Data == nullptr)
        return;

    uint8_t * si = channel->_Data;

    channel->_Size--;

    if (channel->_PartMask != 0x00)
    {
        channel->_KeyOffFlag = -1;
    }
    else
    if ((channel->_KeyOffFlag & 0x03) == 0)
    {
        if (channel->_Size <= channel->_GateTime)
        {
            ADPCMKeyOff(channel);

            channel->_KeyOffFlag = -1;
        }
    }

    if (channel->_Size == 0)
    {
        channel->_HardwareLFO &= 0xF7;

        while (1)
        {
            if ((*si != 0xDA) && (*si > 0x80))
            {
                si = ADPCMExecuteCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->_Data = si;
                channel->_LoopCheck = 0x03;

                channel->_Tone = 0xFF;

                if (channel->_LoopData == nullptr)
                {
                    if (channel->_PartMask != 0x00)
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
                    si = ADPCMSetPortamento(channel, ++si);

                    _Driver._LoopCheck &= channel->_LoopCheck;

                    return;
                }
                else
                if (channel->_PartMask != 0x00)
                {
                    si++;

                    // Set to 'rest'.
                    channel->_Factor = 0;
                    channel->_Tone   = 0xFF;
                    channel->_Size = *si++;
                    channel->_KeyOnFlag++;

                    channel->_Data = si;

                    if (--_Driver._VolumeBoostCount)
                        channel->VolumeBoost = 0;

                    _Driver._IsTieSet = false;
                    _Driver._VolumeBoostCount = 0;
                    break;
                }

                ADPCMSetTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

                channel->_Size = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumeBoost != 0) && (channel->_Tone != 0xFF))
                {
                    if (--_Driver._VolumeBoostCount)
                    {
                        _Driver._VolumeBoostCount = 0;
                        channel->VolumeBoost = 0;
                    }
                }

                ADPCMSetVolume(channel);
                ADPCMSetPitch(channel);

                if (channel->_KeyOffFlag & 0x01)
                    ADPCMKeyOn(channel);

                channel->_KeyOnFlag++;
                channel->_Data = si;

                _Driver._IsTieSet = false;
                _Driver._VolumeBoostCount = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->_KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
        }
    }

    _Driver._HardwareLFOModulationMode = (channel->_HardwareLFO & 0x08);

    if (channel->_HardwareLFO != 0x00)
    {
        if (channel->_HardwareLFO & 0x03)
        {
            if (SetLFO(channel))
                _Driver._HardwareLFOModulationMode |= (channel->_HardwareLFO & 0x03);
        }

        if (channel->_HardwareLFO & 0x30)
        {
            LFOSwap(channel);

            if (SetSSGLFO(channel))
            {
                LFOSwap(channel);

                _Driver._HardwareLFOModulationMode |= (channel->_HardwareLFO & 0x30);
            }
            else
                LFOSwap(channel);
        }

        if (_Driver._HardwareLFOModulationMode & 0x19)
        {
            if (_Driver._HardwareLFOModulationMode & 0x08)
                CalculatePortamento(channel);

            ADPCMSetPitch(channel);
        }
    }

    int32_t temp = SSGPCMSoftwareEnvelope(channel);

    if ((temp != 0) || _Driver._HardwareLFOModulationMode & 0x22 || (_State._FadeOutSpeed != 0))
        ADPCMSetVolume(channel);

    _Driver._LoopCheck &= channel->_LoopCheck;
}

/// <summary>
/// Executes a command.
/// </summary>
uint8_t * pmd_driver_t::ADPCMExecuteCommand(channel_t * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        // 6.1. Instrument Number Setting, Command '@[@] insnum' / Command '@[@] insnum[,number1[,number2[,number3]]]'
        case 0xFF:
        {
            si = ADPCMSetInstrument(channel, si);
            break;
        }

        // 4.12. Sound Cut Setting 1, Command 'Q [%] numerical value' / 4.13. Sound Cut Setting 2, Command 'q [number1][-[number2]] [,number3]' / Command 'q [l length[.]][-[l length]] [,l length[.]]'
        case 0xFE:
        {
            channel->_EarlyKeyOffTimeout1 = *si++;
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
            si = ADPCMSetPan1(channel, si);
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
            si = ADPCMSetPortamento(channel, si);
            break;
        }

        // 6.1.5. Instrument Number Setting/PCM Channels Case, Set PCM Repeat.
        case 0xCE:
        {
            si = ADPCMSetRepeat(channel, si);
            break;
        }

        // 8.2. Software Envelope Speed Setting, Set SSG Extend Mode (bit 2), Command 'EX number'
        case 0xC9:
        {
            channel->_ExtendMode = (channel->_ExtendMode & 0xFB) | ((*si++ & 0x01) << 2);
            break;
        }

        // 13.2. Pan Setting 2
        case 0xC3:
        {
            si = ADPCMSetPan2(channel, si);
            break;
        }

        // 15.7. Channel Mask Control, Sets the channel mask to on or off, Command 'm number'
        case 0xC0:
        {
            si = ADPCMSetChannelMask(channel, si);
            break;
        }

        // 9.3. Software LFO Switch, Controls on/off and keyon synchronization of the software LFO, Command '*B number'
        case 0xBE:
        {
            si = LFO2SetSwitch(channel, si);
            break;
        }

        // 2.25. PPZ8 Channel Extension, Extends the PPZ8 channels with the notated channels, Command '#PPZExtend notation1[notation2[notation3]... (up to 8)]]'
        case 0xB4:
        {
            si = PPZ8Initialize(channel, si);
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
void pmd_driver_t::ADPCMSetDelay(int32_t nsec)
{
    _OPNAW->SetADPCMDelay(nsec);
}

/// <summary>
///
/// </summary>
void pmd_driver_t::ADPCMSetTone(channel_t * channel, int32_t tone)
{
    if ((tone & 0x0F) != 0x0F)
    {
        channel->_Tone = tone;

        int32_t Block = tone & 0x0F;

        int32_t ch = (tone >> 4) & 0x0F;
        int32_t cl = ch;

        cl = (cl > 5) ? 0 : 5 - cl;

        int32_t Factor = PCMScaleFactor[Block];

        if (ch >= 6)
        {
            ch = 0x50;

            if (Factor < 0x8000)
            {
                Factor *= 2;
                ch = 0x60;
            }

            channel->_Tone = (channel->_Tone & 0x0F) | ch;
        }
        else
            Factor >>= cl;

        channel->_Factor = (uint32_t) Factor;
    }
    else
    {
        // Rest
        channel->_Tone = 0xFF;

        if ((channel->_HardwareLFO & 0x11) == 0)
            channel->_Factor = 0; // Don't use LFO pitch.
    }
}

/// <summary>
///
/// </summary>
void pmd_driver_t::ADPCMSetVolume(channel_t * channel)
{
    int32_t al = channel->VolumeBoost ? channel->VolumeBoost : channel->_Volume;

    // Calculate volume down.
    al = ((256 - _State._ADPCMVolumeAdjust) * al) >> 8;

    // Calculate fade out.
    if (_State._FadeOutVolume)
        al = (((256 - _State._FadeOutVolume) * (256 - _State._FadeOutVolume) >> 8) * al) >> 8;

    if (al == 0)
    {
        _OPNAW->SetReg(0x10b, 0);

        return;
    }

    // Calculate envelope.
    if (channel->_SSGEnvelopFlag == -1)
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
            int32_t ah = -channel->ExtendedAttackLevel * 16;

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
            int32_t ah = channel->ExtendedAttackLevel * 16;

            if (al + ah > 255)
                al = 255;
            else
                al += ah;
        }
    }

    if ((channel->_HardwareLFO & 0x22) == 0)
    {
        _OPNAW->SetReg(0x10b, (uint32_t) al);

        return;
    }

    // Calculate the LFO volume.
    int32_t dx = (channel->_HardwareLFO & 0x02) ? channel->_LFO1Data : 0;

    if (channel->_HardwareLFO & 0x20)
        dx += channel->_LFO2Data;

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
void pmd_driver_t::ADPCMSetPitch(channel_t * channel)
{
    if (channel->_Factor == 0)
        return;

    int32_t Pitch = (int32_t) (channel->_Factor + channel->_Portamento);

    {
        int32_t dx = (int32_t) (((channel->_HardwareLFO & 0x11) && (channel->_HardwareLFO & 0x01)) ? dx = channel->_LFO1Data : 0);

        if (channel->_HardwareLFO & 0x10)
            dx += channel->_LFO2Data;

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
void pmd_driver_t::ADPCMKeyOn(channel_t * channel)
{
    if (channel->_Tone == 0xFF)
        return;

    _OPNAW->SetReg(0x101, 0x02); // Set ADPCM Control 2: PAN UNSET / x8 bit mode
    _OPNAW->SetReg(0x100, 0x21); // Set ADPCM Control 1

    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_State._PCMBegin)); // Set ADPCM Start Address (Lo)
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_State._PCMBegin)); // Set ADPCM Start Address (Hi)
    _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_State._PCMEnd));   // Set ADPCM Stop Address (Lo)
    _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_State._PCMEnd));   // Set ADPCM Stop Address (Hi)

    if ((_Driver._LoopBegin | _Driver._LoopEnd) == 0)
    {
        _OPNAW->SetReg(0x100, 0xA0);                                    // Set ADPCM Control 1: PCM PLAY (non_repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (channel->_PanAndVolume | 2)); // Set ADPCM Control 2: PAN SET / x8 bit mode
    }
    else
    {
        _OPNAW->SetReg(0x100, 0xB0);                                    // Set ADPCM Control 1: PCM PLAY (repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (channel->_PanAndVolume | 2)); // Set ADPCM Control 2: PAN SET / x8 bit mode

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_Driver._LoopBegin));   // Set ADPCM Start Address (Lo)
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_Driver._LoopBegin));   // Set ADPCM Start Address (Hi)
        _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_Driver._LoopEnd));     // Set ADPCM Stop Address (Lo)
        _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_Driver._LoopEnd));     // Set ADPCM Stop Address (Hi)
    }
}

/// <summary>
///
/// </summary>
void pmd_driver_t::ADPCMKeyOff(channel_t * channel)
{
    if (channel->_SSGEnvelopFlag != -1)
    {
        if (channel->_SSGEnvelopFlag == 2)
            return;
    }
    else
    {
        if (channel->ExtendedCount == 4)
            return;
    }

    if (_Driver._LoopRelease != 0x8000)
    {
        _OPNAW->SetReg(0x100, 0x21); // Set ADPCM Control 1

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_Driver._LoopRelease)); // Set ADPCM Start Address (Lo)
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_Driver._LoopRelease)); // Set ADPCM Start Address (Hi)

        // Stop ADDRESS for Release
        _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_State._PCMEnd));       // Set ADPCM Stop Address (Lo)
        _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_State._PCMEnd));       // Set ADPCM Stop Address (Hi)

        // PCM PLAY(non_repeat)
        _OPNAW->SetReg(0x100, 0xA0); // Set ADPCM Control 1: PCM PLAY (Non-repeat)
    }

    SSGKeyOff(channel);
}

/// <summary>
/// Command "@ number": Sets the instrument to be used. Range 0-255.
/// </summary>
uint8_t * pmd_driver_t::ADPCMSetInstrument(channel_t * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    _State._PCMBegin = _SampleBank.Address[channel->InstrumentNumber][0];
    _State._PCMEnd = _SampleBank.Address[channel->InstrumentNumber][1];

    _Driver._LoopBegin = 0;
    _Driver._LoopEnd = 0;
    _Driver._LoopRelease = 0x8000;

    return si;
}

/// <summary>
/// Command "p <value>" (1: right, 2: left, 3: center (default))
/// </summary>
uint8_t * pmd_driver_t::ADPCMSetPan1(channel_t * channel, uint8_t * si)
{
    channel->_PanAndVolume = (*si << 6) & 0xC0;

    return si + 1;  // Skip the Phase flag
}

/// <summary>
/// Command "px <value 1>, <value 2>" (value 1: < 0 (pan to the right), 0 (Center), > 0 (pan to the left), value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * pmd_driver_t::ADPCMSetPan2(channel_t * channel, uint8_t * si)
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

/// <summary>
///
/// </summary>
uint8_t * pmd_driver_t::ADPCMSetPortamento(channel_t * channel, uint8_t * si)
{
    if (channel->_PartMask != 0x00)
    {
        // Set to 'rest'.
        channel->_Factor = 0;
        channel->_Tone   = 0xFF;
        channel->_Size = si[2];
        channel->_Data   = si + 3;
        channel->_KeyOnFlag++;

        if (--_Driver._VolumeBoostCount)
            channel->VolumeBoost = 0;

        _Driver._IsTieSet = false;
        _Driver._VolumeBoostCount = 0;

        _Driver._LoopCheck &= channel->_LoopCheck;

        return si + 3; // Skip when muted
    }

    ADPCMSetTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

    int32_t bx_ = (int32_t) channel->_Factor;
    int32_t al_ = (int32_t) channel->_Tone;

    ADPCMSetTone(channel, Transpose(channel, *si++));

    int32_t ax = (int32_t) channel->_Factor;

    channel->_Tone = al_;
    channel->_Factor = (uint32_t) bx_;

    ax -= bx_;

    channel->_Size = *si++;

    si = CalculateQ(channel, si);

    channel->_PortamentoQuotient = ax / channel->_Size;
    channel->_PortamentoRemainder = ax % channel->_Size;
    channel->_HardwareLFO |= 0x08; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->_Tone != 0xFF))
    {
        if (--_Driver._VolumeBoostCount)
        {
            channel->VolumeBoost = 0;

            _Driver._VolumeBoostCount = 0;
        }
    }

    ADPCMSetVolume(channel);
    ADPCMSetPitch(channel);

    if (channel->_KeyOffFlag & 0x01)
        ADPCMKeyOn(channel);

    channel->_KeyOnFlag++;
    channel->_Data = si;

    _Driver._IsTieSet = false;
    _Driver._VolumeBoostCount = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->_KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver._LoopCheck &= channel->_LoopCheck;

    return si;
}

/// <summary>
/// 6.1.5 Instrument Number Setting, Command "@[@] insnum[,number1[,number2[,number3]]]"
/// </summary>
uint8_t * pmd_driver_t::ADPCMSetRepeat(channel_t *, uint8_t * si)
{
    {
        int32_t LoopBegin = *(int16_t *) si;
        si += 2;

        LoopBegin += (LoopBegin >= 0) ? _State._PCMBegin : _State._PCMEnd;

        _Driver._LoopBegin = LoopBegin;
    }

    {
        int32_t LoopEnd = *(int16_t *) si; // Default: 0
        si += 2;

        LoopEnd += (LoopEnd > 0) ? _State._PCMBegin : _State._PCMEnd;

        _Driver._LoopEnd = LoopEnd;
    }

    {
        int32_t LoopRelease = *(uint16_t *) si; // Range -32768 - 32767, Default: -32768 (Loop continuously)
        si += 2;

        if (LoopRelease < 0x8000)
            LoopRelease += _State._PCMBegin;
        else
        if (LoopRelease > 0x8000)
            LoopRelease += _State._PCMEnd;

        _Driver._LoopRelease = LoopRelease;
    }

    return si;
}

/// <summary>
/// 15.7. Channel Mask Control, Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// </summary>
uint8_t * pmd_driver_t::ADPCMSetChannelMask(channel_t * channel, uint8_t * si) noexcept
{
    const uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->_PartMask |= 0x40;

            if (channel->_PartMask == 0x40)
            {
                _OPNAW->SetReg(0x101, 0x02); // Set ADPCM Control 2: PAN UNSET / x8 bit mode
                _OPNAW->SetReg(0x100, 0x01); // Set ADPCM Control 1
            }
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->_PartMask &= 0xBF;

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * pmd_driver_t::ADPCMDecreaseVolume(channel_t *, uint8_t * si)
{
    const int32_t  Value = *(int8_t *) si++;

    if (Value != 0)
        _State._ADPCMVolumeAdjust = std::clamp(Value + _State._ADPCMVolumeAdjust, 0, 255);
    else
        _State._ADPCMVolumeAdjust = _State._ADPCMVolumeAdjustDefault;

    return si;
}
