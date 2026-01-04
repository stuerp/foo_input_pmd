
// $VER: PMDPPZ.cpp (2026.01.03) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

void PMD::PPZMain(channel_t * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

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
            PPZKeyOff(channel);

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
                si = ExecutePPZCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->Data = si;
                channel->Tone = 0xFF;

                channel->_LoopCheck = 0x03;

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
                    si = SetPPZPortamentoCommand(channel, ++si);

                    _Driver._LoopCheck &= channel->_LoopCheck;

                    return;
                }
                else
                if (channel->PartMask != 0x00)
                {
                    si++;

                    // Set to 'rest'.
                    channel->Factor      = 0;
                    channel->Tone        = 0xFF;
//                  channel->DefaultTone = 0xFF;
                    channel->_Size      = *si++;
                    channel->KeyOnFlag++;

                    channel->Data = si;

                    if (--_Driver._VolumeBoostCount)
                        channel->VolumeBoost = 0;

                    _Driver._IsTieSet = false;
                    _Driver._VolumeBoostCount = 0;
                    break;
                }

                SetPPZTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

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

                SetPPZVolume(channel);
                SetPPZPitch(channel);

                if (channel->KeyOffFlag & 0x01)
                    PPZKeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

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

    if (channel->HardwareLFOModulationMode != 0x00)
    {
        if (channel->HardwareLFOModulationMode & 0x03)
        {
            if (SetLFO(channel))
            {
                _Driver._HardwareLFOModulationMode |= (channel->HardwareLFOModulationMode & 0x03);
            }
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
            {
                CalculatePortamento(channel);
            }

            SetPPZPitch(channel);
        }
    }

    int temp = SSGPCMSoftwareEnvelope(channel);

    if (temp || _Driver._HardwareLFOModulationMode & 0x22 || _State.FadeOutSpeed)
        SetPPZVolume(channel);

    _Driver._LoopCheck &= channel->_LoopCheck;
}

uint8_t * PMD::ExecutePPZCommand(channel_t * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        // 6.1. Instrument Number Setting, Command '@[@] insnum' / Command '@[@] insnum[,number1[,number2[,number3]]]'
        case 0xFF:
        {
            si = SetPPZInstrument(channel, si);
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
            _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + si[0]), si[1]);
            si += 2;
            break;
        }

        case 0xEE: si++; break;
        case 0xED: si++; break;

        // 13.1. Pan setting 1
        case 0xEC:
            si = SetPPZPan1(channel, si);
            break;

        // 5.5. Relative Volume Change, Command ') %number'
        case 0xE3:
        {
            channel->Volume += *si++;

            if (channel->Volume > 255)
                channel->Volume = 255;
            break;
        }

        // 5.5. Relative Volume Change, Command '( %number'
        case 0xE2:
        {
            channel->Volume -= *si++;

            if (channel->Volume < 0)
                channel->Volume = 0;
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
            si = SetPPZPortamentoCommand(channel, si);
            break;
        }

        // 6.1.5. Instrument Number Setting/PCM Channels Case, Set PCM Repeat.
        case 0xCE:
        {
            si = SetPPZRepeatCommand(channel, si);
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
            si = SetPPZPan2(channel, si);
            break;
        }

        // 15.7. Channel Mask Control, Sets the channel mask to on or off, Command 'm number'
        case 0xC0:
        {
            si = SetPPZChannelMaskCommand(channel, si);
            break;
        }

        // 9.11. Hardware LFO Switch/Depth Setting (OPNA), Command "# number1, [number2]": Sets the hardware LFO on (1) or off (0). (OPNA FM sound source only). Number2 = depth. Can be omitted only when switch is 0.
        case 0xBE:
        {
            si = SetHardwareLFOSwitchCommand(channel, si);
            break;
        }

        default:
            si = ExecuteCommand(channel, si, Command);
    }

    return si;
}

#pragma region(Commands)
/// <summary>
///
/// </summary>
void PMD::SetPPZTone(channel_t * channel, int tone)
{
    if ((tone & 0x0F) != 0x0F)
    {
        channel->Tone = tone;

        int Block = tone & 0x0F;

        int cl = (tone >> 4) & 0x0F;

        uint32_t Factor = (uint32_t) PPZScaleFactor[Block];

        if ((cl -= 4) < 0)
        {
            cl = -cl;
            Factor >>= cl;
        }
        else
            Factor <<= cl;

        channel->Factor = Factor;
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
void PMD::SetPPZVolume(channel_t * channel)
{
    int al = channel->VolumeBoost ? channel->VolumeBoost : channel->Volume;

    // Calculate volume down.
    al = ((256 - _State.PPZVolumeAdjust) * al) >> 8;

    // Calculate fade out.
    if (_State._FadeOutVolume != 0)
        al = ((256 - _State._FadeOutVolume) * al) >> 8;

    if (al == 0)
    {
        _PPZ->SetVolume((size_t) _Driver._CurrentChannel, 0);
        _PPZ->Stop((size_t) _Driver._CurrentChannel);

        return;
    }

    // Calculate envelope.
    if (channel->SSGEnvelopFlag == -1)
    {
        // Extended version: Volume = al * (eenv_vol + 1) / 16
        if (channel->ExtendedAttackLevel == 0)
        {
        //  _PPZ->SetVol((Size_t) _Driver._CurrentChannel, 0);
            _PPZ->Stop((size_t) _Driver._CurrentChannel);

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
            //  _PPZ->SetVol((Size_t) _Driver._CurrentChannel, 0);
                _PPZ->Stop((size_t) _Driver._CurrentChannel);

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
    if ((channel->HardwareLFOModulationMode & 0x22))
    {
        int dx = (channel->HardwareLFOModulationMode & 0x02) ? channel->LFO1Data : 0;

        if (channel->HardwareLFOModulationMode & 0x20)
            dx += channel->LFO2Data;

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
        _PPZ->SetVolume((size_t) _Driver._CurrentChannel, al >> 4);
    else
        _PPZ->Stop((size_t) _Driver._CurrentChannel);
}

/// <summary>
///
/// </summary>
void PMD::SetPPZPitch(channel_t * channel)
{
    uint32_t Pitch = channel->Factor;

    if (Pitch == 0)
        return;

    Pitch += channel->_Portamento * 16;

    {
        int ax = (channel->HardwareLFOModulationMode & 0x01) ? channel->LFO1Data : 0;

        if (channel->HardwareLFOModulationMode & 0x10)
            ax += channel->LFO2Data;

        ax += channel->_DetuneValue;

        int64_t cx = Pitch + ((int64_t) Pitch) / 256 * ax;

        Pitch = (uint32_t) std::clamp(cx, (int64_t) 0, (int64_t) 0xFFFFFFFF);
    }

    _PPZ->SetPitch((size_t) _Driver._CurrentChannel, Pitch);
}

/// <summary>
///
/// </summary>
void PMD::PPZKeyOn(channel_t * channel)
{
    if (channel->Tone == 0xFF)
        return;

    if ((channel->InstrumentNumber & 0x80) == 0)
        _PPZ->Play((size_t) _Driver._CurrentChannel, 0, channel->InstrumentNumber,        0, 0);
    else
        _PPZ->Play((size_t) _Driver._CurrentChannel, 1, channel->InstrumentNumber & 0x7F, 0, 0);
}

/// <summary>
///
/// </summary>
void PMD::PPZKeyOff(channel_t * channel)
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

    SSGKeyOff(channel);
}

/// <summary>
/// Command "@ number": Sets the instrument to be used. Range 0-255.
/// </summary>
uint8_t * PMD::SetPPZInstrument(channel_t * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    if ((channel->InstrumentNumber & 0x80) == 0)
        _PPZ->SetInstrument((size_t) _Driver._CurrentChannel, 0, (size_t) channel->InstrumentNumber);
    else
        _PPZ->SetInstrument((size_t) _Driver._CurrentChannel, 1, (size_t) (channel->InstrumentNumber & 0x7F));

    return si;
}

/// <summary>
/// Command "p <value>" (1: right, 2: left, 3: center (default))
/// </summary>
uint8_t * PMD::SetPPZPan1(channel_t * channel, uint8_t * si)
{
    static const int PanValues[4] = { 0, 9, 1, 5 }; // { Left, Right, Leftwards, Rightwards }

    channel->_PanAndVolume = PanValues[*si++];

    _PPZ->SetPan((size_t) _Driver._CurrentChannel, channel->_PanAndVolume);

    return si;
}

/// <summary>
/// Command "px <value 1>, <value 2>" (value 1: -128 to -4 (Pan to the left), -3 to -1 (Leftwards), 0 (Center), 1 to 3 (Rightwards), 4 to 127 (Pan to the right), value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * PMD::SetPPZPan2(channel_t * channel, uint8_t * si)
{
    int al = *(int8_t *) si++;
    si++; // Skip the Phase flag.

    if (al >  4)
        al = 4;
    else
    if (al < -4)
        al = -4;

    channel->_PanAndVolume = al + 5; // Scale the value to range 1..9.

    _PPZ->SetPan((size_t) _Driver._CurrentChannel, channel->_PanAndVolume);

    return si;
}
#pragma endregion

/// <summary>
/// Command "{interval1 interval2} [length1] [.] [,length2]"
/// </summary>
uint8_t * PMD::SetPPZPortamentoCommand(channel_t * channel, uint8_t * si)
{
    if (channel->PartMask != 0x00)
    {
        // Set to 'rest'.
        channel->Factor = 0;
        channel->Tone   = 0xFF;
        channel->_Size = si[2];
        channel->Data   = si + 3;
        channel->KeyOnFlag++;

        if (--_Driver._VolumeBoostCount)
            channel->VolumeBoost = 0;

        _Driver._IsTieSet = false;
        _Driver._VolumeBoostCount = 0;

        _Driver._LoopCheck &= channel->_LoopCheck;

        return si + 3; // Skip when muted.
    }

    SetPPZTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

    int bx_ = (int) channel->Factor;
    int al_ = channel->Tone;

    SetPPZTone(channel, Transpose(channel, *si++));

    int ax = (int) channel->Factor;

    channel->Tone = al_;
    channel->Factor = (uint32_t) bx_;

    ax -= bx_;
    ax /= 16;

    channel->_Size = *si++;

    si = CalculateQ(channel, si);

    channel->PortamentoQuotient = ax / channel->_Size;
    channel->PortamentoRemainder = ax % channel->_Size;
    channel->HardwareLFOModulationMode |= 0x08; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
    {
        if (--_Driver._VolumeBoostCount)
        {
            _Driver._VolumeBoostCount = 0;
            channel->VolumeBoost = 0;
        }
    }

    SetPPZVolume(channel);
    SetPPZPitch(channel);

    if (channel->KeyOffFlag & 0x01)
        PPZKeyOn(channel);

    channel->KeyOnFlag++;
    channel->Data = si;

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
uint8_t * PMD::SetPPZRepeatCommand(channel_t * channel, uint8_t * si)
{
    int LoopBegin, LoopEnd;

    if ((channel->InstrumentNumber & 0x80) == 0)
    {
        LoopBegin = *(int16_t *) si;
        si += 2;

        LoopEnd = *(int16_t *) si;
        si += 2;

        _PPZ->SetLoop((size_t) _Driver._CurrentChannel, 0, (size_t) channel->InstrumentNumber, LoopBegin, LoopEnd);
    }
    else
    {
        LoopBegin = *(int16_t *) si;
        si += 2;

        LoopEnd = *(int16_t *) si;
        si += 2;

        _PPZ->SetLoop((size_t) _Driver._CurrentChannel, 1, (size_t) channel->InstrumentNumber & 0x7F, LoopBegin, LoopEnd);
    }

    return si + 2; // Skip the Loop Release Address.
}

/// <summary>
/// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// </summary>
uint8_t * PMD::SetPPZChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept
{
    const uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->PartMask |= 0x40;

            if (channel->PartMask == 0x40)
                _PPZ->Stop((size_t) _Driver._CurrentChannel);
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->PartMask &= 0xBF; // 1011 1111

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::InitializePPZ(channel_t *, uint8_t * si)
{
    for (size_t i = 0; i < _countof(_PPZChannels); ++i)
    {
        int16_t Offset = *(int16_t *) si;

        if (Offset != 0)
        {
            _PPZChannels[i].Data = &_State.MData[Offset];
            _PPZChannels[i]._Size = 1;
            _PPZChannels[i].KeyOffFlag = -1;
            _PPZChannels[i].LFO1MDepthCount1 = -1;        // LFO1MDepth Counter (Infinite)
            _PPZChannels[i].LFO1MDepthCount2 = -1;
            _PPZChannels[i].LFO2MDepthCount1 = -1;
            _PPZChannels[i].LFO2MDepthCount2 = -1;
            _PPZChannels[i].Tone = 0xFF;         // Rest
            _PPZChannels[i].DefaultTone = 0xFF;  // Rest
            _PPZChannels[i].Volume = 128;
            _PPZChannels[i]._PanAndVolume = 5;    // Center
        }

        si += 2;
    }

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::DecreasePPZVolumeCommand(channel_t *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al != 0)
        _State.PPZVolumeAdjust = std::clamp(al + _State.PPZVolumeAdjust, 0, 255);
    else
        _State.PPZVolumeAdjust = _State.DefaultPPZVolumeAdjust;

    return si;
}
