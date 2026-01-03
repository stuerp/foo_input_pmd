
// $VER: PMDFM.cpp (2025.12.23) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

void PMD::FMMain(channel_t * channel) noexcept
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    channel->Length--;

    if (channel->PartMask != 0x00)
    {
        channel->KeyOffFlag = -1;
    }
    else
    if ((channel->KeyOffFlag & 0x03) == 0)
    {
        if (channel->Length <= channel->GateTime)
        {
            FMKeyOff(channel);

            channel->KeyOffFlag = -1;
        }
    }

    if (channel->Length == 0)
    {
        if (channel->PartMask == 0x00)
            channel->HardwareLFOModulationMode &= 0xF7;

        while (1)
        {
            if ((*si != 0xDA) && (*si > 0x80))
            {
                si = ExecuteFMCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->Data = si;
                channel->loopcheck = 3;
                channel->Tone = 0xFF;

                if (channel->LoopData == nullptr)
                {
                    if (channel->PartMask != 0x00)
                    {
                        _Driver.TieNotesTogether = false;
                        _Driver._IsVolumeBoostSet = 0;
                        _Driver._LoopWork &= channel->loopcheck;

                        return;
                    }
                    else
                        break;
                }

                // Start executing a loop.
                si = channel->LoopData;
                channel->loopcheck = 1;
            }
            else
            if (*si == 0xDA)
            {
                si = SetFMPortamentoCommand(channel, ++si);

                _Driver._LoopWork &= channel->loopcheck;

                return;
            }
            else
            if (channel->PartMask == 0x00)
            {
                SetFMTone(channel, Transpose(channel, StartLFO(channel, *si++)));

                channel->Length = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
                {
                    if (--_Driver._IsVolumeBoostSet)
                    {
                        _Driver._IsVolumeBoostSet = 0;
                        channel->VolumeBoost = 0;
                    }
                }

                SetFMVolumeCommand(channel);
                SetFMPitch(channel);
                FMKeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

                _Driver.TieNotesTogether = false;
                _Driver._IsVolumeBoostSet = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                _Driver._LoopWork &= channel->loopcheck;

                return;
            }
            else
            {
                si++;

                // Set to 'rest'.
                channel->Factor      = 0;
                channel->Tone        = 0xFF;
                channel->DefaultTone = 0xFF;
                channel->Length      = *si++;
                channel->KeyOnFlag++;

                channel->Data = si;

                if (--_Driver._IsVolumeBoostSet)
                    channel->VolumeBoost = 0;

                _Driver.TieNotesTogether = false;
                _Driver._IsVolumeBoostSet = 0;
                break;
            }
        }
    }

    if (channel->PartMask == 0x00)
    {
        // Finish with LFO & Portament & Fadeout processing
        if (channel->HardwareLFODelayCounter)
        {
            if (--channel->HardwareLFODelayCounter == 0)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xB4 + (_Driver._CurrentChannel - 1)), (uint32_t) channel->_PanAndVolume);
        }

        if (channel->SlotDelayCounter != 0)
        {
            if (--channel->SlotDelayCounter == 0)
            {
                if ((channel->KeyOffFlag & 0x01) == 0)
                    FMKeyOn(channel);
            }
        }

        if (channel->HardwareLFOModulationMode)
        {
            _Driver._HardwareLFOModulationMode = channel->HardwareLFOModulationMode & 0x08;

            if (channel->HardwareLFOModulationMode & 0x03)
            {
                if (SetLFO(channel))
                    _Driver._HardwareLFOModulationMode |= (channel->HardwareLFOModulationMode & 0x03);
            }

            if (channel->HardwareLFOModulationMode & 0x30)
            {
                SwapLFO(channel);

                if (SetLFO(channel))
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

                SetFMPitch(channel);
            }

            if (_Driver._HardwareLFOModulationMode & 0x22)
            {
                SetFMVolumeCommand(channel);
                _Driver._LoopWork &= channel->loopcheck;

                return;
            }
        }

        if (_State.FadeOutSpeed != 0)
            SetFMVolumeCommand(channel);
    }

    _Driver._LoopWork &= channel->loopcheck;
}

/// <summary>
/// Executes an FM command.
/// </summary>
uint8_t * PMD::ExecuteFMCommand(channel_t * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        // 6.1. Instrument Number Setting, Command '@[@] insnum' / Command '@[@] insnum[,number1[,number2[,number3]]]'
        case 0xFF:
        {
            si = SetFMInstrument(channel, si);
            break;
        }

        // 4.12. Sound Cut Setting 1, Command 'Q [%] numerical value' / 4.13. Sound Cut Setting 2, Command 'q [number1][-[number2]] [,number3]' / Command 'q [l length[.]][-[l length]] [,l length[.]]'
        case 0xFE:
        {
            channel->EarlyKeyOffTimeout = *si++;
            channel->EarlyKeyOffTimeoutRandomRange = 0;
            break;
        }

        // 5.5. Relative Volume Change, Increase volume by 3dB.
        case 0xF4:
        {
            channel->Volume += 4;

            if (channel->Volume > 127)
                channel->Volume = 127;
            break;
        }

        // 5.5. Relative Volume Change, Decrease volume by 3dB.
        case 0xF3:
        {
            channel->Volume -= 4;

            if (channel->Volume < 4)
                channel->Volume = 0;
            break;
        }

        // 9.3. Software LFO Switch, Command '*A number'
        case 0xF1:
        {
            si = SetModulationMask(channel, si);

            SetFMChannelLFOs(channel);
            break;
        }

        case 0xF0: si += 4; break;

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
            si = SetFMPan1(channel, si);
            break;

        // 9.13. Hardware LFO Delay Setting, Set hardware LFO delay.
        case 0xE4:
            channel->HardwareLFODelay = *si++;
            break;

        // 5.5. Relative Volume Change, Command ') %number'
        case 0xE3:
        {
            channel->Volume += *si++;

            if (channel->Volume > 127)
                channel->Volume = 127;
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

        // 9.10. Hardware LFO Speed/Delay Setting, Command 'H number1[, number2]'
        case 0xE1:
        {
            si = SetHardwareLFOCommand(channel, si);
            break;
        }

        // 9.11. Hardware LFO Switch/Depth Setting (OPNA), Command "# number1, [number2]": Sets the hardware LFO on (1) or off (0). (OPNA FM sound source only). Number2 = depth. Can be omitted only when switch is 0.
        case 0xE0:
        {
            _OPNAW->SetReg(0x22, *si++);
            break;
        }

        // 5.5. Relative Volume Change, Command ') ^%number'
        case 0xDE:
        {
            si = IncreaseVolumeForNextNote(channel, si, 128);
            break;
        }

        // 4.3. Portamento Setting
        case 0xDA:
        {
            si = SetFMPortamentoCommand(channel, si);
            break;
        }

        // 6.2. FM Slot Use Setting, Specifies the slot position (operators) to be used for performance/definition, Command 's number'
        case 0xCF:
        {
            si = SetFMSlotCommand(channel, si);
            break;
        }

        case 0xCD: si += 5; break;

        // Set SSG Extend Mode (bit 1).
        case 0xCA:
            channel->ExtendMode = (channel->ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);
            break;

        case 0xC9: si++; break;

        // 7.2. FM Channel 3 Per-Slot Detune Setting, Set an absolute detune, Command 'sd slotnum, number'
        case 0xC8:
        {
            si = SetFMAbsoluteDetuneCommand(channel, si);
            break;
        }

        // 7.2. FM Channel 3 Per-Slot Detune Setting, Set a detune relative to the previous value, Command 'sdd slotnum, number'
        case 0xC7:
        {
            si = SetFMRelativeDetuneCommand(channel, si);
            break;
        }

        // 2.20. FM Channel 3 Expansion, Expands the 3rd FM channel by creating new channels with the notated letter, Command '#FM3Extend notation1[notation2[notation3]]]'
        case 0xC6:
        {
            si = SetFMChannel3ModeEx(channel, si);
            break;
        }

        // 9.4. Software LFO Slot Setting, Sets the slot number to apply the effect of the software LFO to.
        case 0xC5:
        {
            si = SetFMVolumeMaskSlotCommand(channel, si);
            break;
        }

        // 13.2. Pan Setting 2
        case 0xC3:
        {
            si = SetFMPan2(channel, si);
            break;
        }

        // 15.7. Channel Mask Control, Sets the channel mask to on or off, Command 'm number'
        case 0xC0:
        {
            si = SetFMChannelMaskCommand(channel, si);
            break;
        }

        // 9.11. Hardware LFO Switch/Depth Setting (OPNA), Command "# number1, [number2]": Sets the hardware LFO on (1) or off (0). (OPNA FM sound source only). Number2 = depth. Can be omitted only when switch is 0.
        case 0xBE:
        {
            si = SetHardwareLFOSwitchCommand(channel, si);

            SetFMChannelLFOs(channel);
            break;
        }

        // 6.3. FM TL Setting, Sets the TL (True Level, or operator volume) value of an FM instrument.
        case 0xB8:
        {
            si = SetFMTrueLevelCommand(channel, si);
            break;
        }

        // 6.4. FM FB Setting, Sets the FB (Feedback) value of an FM instrument.
        case 0xB6:
        {
            si = SetFMFeedbackLoopsCommand(channel, si);
            break;
        }

        // 12.3. Keyon Delay Per Slot Setting, Delays the KeyOn of specified slots, Command 'sk number1[, number2]'
        case 0xB5:
        {
            channel->SlotDelayMask = (~(*si++) << 4) & 0xF0;
            channel->SlotDelayCounter =
            channel->SlotDelay = *si++;
            break;
        }

        default:
            si = ExecuteCommand(channel, si, Command);
    }

    return si;
}

/// <summary>
/// Completely muting the [PartB] part (TL=127 and RR=15 and KEY-OFF). cy=1 ･･･ All slots are neiromasked
/// </summary>
int PMD::MuteFMChannel(channel_t * channel)
{
    if (channel->ToneMask == 0)
        return 1;

    int dh = 0x40 + (_Driver._CurrentChannel - 1);

    if (channel->ToneMask & 0x80)
    {
        _OPNAW->SetReg((uint32_t) ( _Driver._FMSelector         + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver._FMSelector + 0x40) + dh), 127);
    }

    dh += 4;

    if (channel->ToneMask & 0x40)
    {
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver._FMSelector + 0x40) + dh), 127);
    }

    dh += 4;

    if (channel->ToneMask & 0x20)
    {
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver._FMSelector + 0x40) + dh), 127);
    }

    dh += 4;

    if (channel->ToneMask & 0x10)
    {
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver._FMSelector + 0x40) + dh), 127);
    }

    FMKeyOff(channel);

    return 0;
}

/// <summary>
/// Sets FM Wait after register output.
/// </summary>
void PMD::SetFMDelay(int nsec)
{
    _OPNAW->SetFMDelay(nsec);
}

#pragma region Commands
/// <summary>
///
/// </summary>
void PMD::SetFMTone(channel_t * channel, int tone)
{
    if ((tone & 0x0F) != 0x0F)
    {
        channel->Tone = tone;

        int Block  = tone & 0x0F;
        int Factor = FMScaleFactor[Block];

        Factor |= (((tone >> 1) & 0x38) << 8);

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
void PMD::SetFMVolumeCommand(channel_t * channel)
{
    if (channel->FMSlotMask == 0)
        return;

    int Volume = (channel->VolumeBoost) ? channel->VolumeBoost - 1 : channel->Volume;

    if (channel != &_EffectChannel)
    {
        // Calculates the effect of volume down.
        if (_State.FMVolumeAdjust != 0)
            Volume = ((256 - _State.FMVolumeAdjust) * Volume) >> 8;

        // Calculates the effect of fade out.
        if (_State._FadeOutVolume >= 2)
            Volume = ((256 - (_State._FadeOutVolume >> 1)) * Volume) >> 8;
    }

    Volume = 255 - Volume;

    // Set volume to carrier & volume LFO processing.
    uint8_t SlotVolume[4] = { 0x80, 0x80, 0x80, 0x80 };

    int bl = channel->FMSlotMask;

    bl &= channel->FMCarrier;    // bl=Set volume SLOT xxxx0000b

    int bh = bl;

    if (bl & 0x80) SlotVolume[0] = (uint8_t) Volume;
    if (bl & 0x40) SlotVolume[1] = (uint8_t) Volume;
    if (bl & 0x20) SlotVolume[2] = (uint8_t) Volume;
    if (bl & 0x10) SlotVolume[3] = (uint8_t) Volume;

    if (Volume != 255)
    {
        if (channel->HardwareLFOModulationMode & 0x02)
        {
            bl = channel->VolumeMask1;
            bl &= channel->FMSlotMask;    // bl=SLOT to set volume LFO xxxx0000b
            bh |= bl;

            CalcFMLFO(channel, channel->LFO1Data, bl, SlotVolume);
        }

        if (channel->HardwareLFOModulationMode & 0x20)
        {
            bl = channel->VolumeMask2;
            bl &= channel->FMSlotMask;
            bh |= bl;

            CalcFMLFO(channel, channel->LFO2Data, bl, SlotVolume);
        }
    }

    int dh = 0x4C + (_Driver._CurrentChannel - 1); // FM Port Address

    if (bh & 0x80) CalcVolSlot(dh,      channel->FMOperator4, SlotVolume[0]);
    if (bh & 0x40) CalcVolSlot(dh -  8, channel->FMOperator3, SlotVolume[1]);
    if (bh & 0x20) CalcVolSlot(dh -  4, channel->FMOperator2, SlotVolume[2]);
    if (bh & 0x10) CalcVolSlot(dh - 12, channel->FMOperator1, SlotVolume[3]);
}

/// <summary>
///
/// </summary>
void PMD::SetFMPitch(channel_t * channel)
{
    if ((channel->Factor == 0) || (channel->FMSlotMask == 0))
        return;

    int Block = (int) (channel->Factor  & 0x3800);
    int Pitch = (int) (channel->Factor) & 0x07ff;

    // Portament/LFO/Detune SET
    Pitch += channel->_Portamento + channel->_DetuneValue;

    if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0) && (_State.FMChannel3Mode != 0x3F))
        SpecialFM3Processing(channel, Pitch, Block);
    else
    {
        if (channel->HardwareLFOModulationMode & 0x01)
            Pitch += channel->LFO1Data;

        if (channel->HardwareLFOModulationMode & 0x10)
            Pitch += channel->LFO2Data;

        CalcFMBlock(&Block, &Pitch);

        Pitch |= Block;

        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xA4 + (_Driver._CurrentChannel - 1)), (uint32_t) HIBYTE(Pitch));
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xA0 + (_Driver._CurrentChannel - 1)), (uint32_t) LOBYTE(Pitch));
    }
}

/// <summary>
///
/// </summary>
void PMD::FMKeyOn(channel_t * channel)
{
    if (channel->Tone == 0xFF)
        return;

    if (_Driver._FMSelector == 0)
    {
        int al = _Driver.omote_key[_Driver._CurrentChannel - 1] | channel->FMSlotMask;

        if (channel->SlotDelayCounter)
            al &= channel->SlotDelayMask;

        _Driver.omote_key[_Driver._CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) ((_Driver._CurrentChannel - 1) | al));
    }
    else
    {
        int al = _Driver.ura_key[_Driver._CurrentChannel - 1] | channel->FMSlotMask;

        if (channel->SlotDelayCounter)
            al &= channel->SlotDelayMask;

        _Driver.ura_key[_Driver._CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) (((_Driver._CurrentChannel - 1) | al) | 4));
    }
}

/// <summary>
///
/// </summary>
void PMD::FMKeyOff(channel_t * channel)
{
    if (channel->Tone == 0xFF)
        return;

    if (_Driver._FMSelector == 0)
    {
        _Driver.omote_key[_Driver._CurrentChannel - 1] = (~channel->FMSlotMask) & (_Driver.omote_key[_Driver._CurrentChannel - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) ((_Driver._CurrentChannel - 1) | _Driver.omote_key[_Driver._CurrentChannel - 1]));
    }
    else
    {
        _Driver.ura_key[_Driver._CurrentChannel - 1] = (~channel->FMSlotMask) & (_Driver.ura_key[_Driver._CurrentChannel - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) (((_Driver._CurrentChannel - 1) | _Driver.ura_key[_Driver._CurrentChannel - 1]) | 4));
    }
}

/// <summary>
/// Command "@ number": Sets the instrument to be used. Range 0-255.
/// </summary>
uint8_t * PMD::SetFMInstrument(channel_t * channel, uint8_t * si)
{
    int al = *si++;
    int dl = al;

    channel->InstrumentNumber = al;

    if (channel->PartMask == 0x00)
    {
        InitializeFMInstrument(channel, dl);

        return si;
    }

    {
        uint8_t * Data = GetFMInstrumentDefinition(channel, dl);

        channel->AlgorithmAndFeedbackLoops = dl = Data[24];
        Data += 4;

        // Set the Total Level.
        channel->FMOperator1 = Data[0];
        channel->FMOperator3 = Data[1];
        channel->FMOperator2 = Data[2];
        channel->FMOperator4 = Data[3];
    }

    if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0) && (channel->ToneMask != 0))
    {
        if ((channel->FMSlotMask & 0x10) == 0)
        {
            al = _Driver._AlgorithmAndFeedbackLoopsFM3 & 0x38; // Feedback Loops (fb) uses previous value.
            dl = (dl & 0x07) | al;
        }

        _Driver._AlgorithmAndFeedbackLoopsFM3 = dl;
        channel->AlgorithmAndFeedbackLoops = al;
    }

    return si;
}

/// <summary>
/// Command "p <value>" (1: right, 2: left, 3: center (default))
/// </summary>
uint8_t * PMD::SetFMPan1(channel_t * channel, uint8_t * si)
{
    SetFMPannningInternal(channel, *si++);

    return si;
}

/// <summary>
/// Command "px <value 1>, <value 2>" (value 1: -4 (pan to the left) to +4 (pan to the right), value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * PMD::SetFMPan2(channel_t * channel, uint8_t * si)
{
    int al = *(int8_t *) si++;

    si++; // Skip the Phase flag

    if (al > 0)
    {
        al = 2; // Right
        SetFMPannningInternal(channel, al);
    }
    else
    if (al == 0)
    {
        al = 3; // Center
        SetFMPannningInternal(channel, al);
    }
    else
    {
        al = 1; // Left
        SetFMPannningInternal(channel, al);
    }

    return si;
}

/// <summary>
///
/// </summary>
void PMD::SetFMPannningInternal(channel_t * channel, int value)
{
    channel->_PanAndVolume = (channel->_PanAndVolume & 0x3F) | ((value << 6) & 0xC0);

    if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0))
    {
        // For FM3, set all 4 parts.
        _FMChannels[2]._PanAndVolume = channel->_PanAndVolume;

        _FMExtensionChannels[0]._PanAndVolume = channel->_PanAndVolume;
        _FMExtensionChannels[1]._PanAndVolume = channel->_PanAndVolume;
        _FMExtensionChannels[2]._PanAndVolume = channel->_PanAndVolume;
    }

    if (channel->PartMask == 0x00)
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xB4 + (_Driver._CurrentChannel - 1)), CalcPanOut(channel));
}

/// <summary>
///
/// </summary>
uint8_t * PMD::DecreaseFMVolumeCommand(channel_t *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.FMVolumeAdjust = std::clamp(al + _State.FMVolumeAdjust, 0, 255);
    else
        _State.FMVolumeAdjust = _State.DefaultFMVolumeAdjust;

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::SetFMPortamentoCommand(channel_t * channel, uint8_t * si)
{
    if (channel->PartMask != 0x00)
    {
        // Set to 'rest'.
        channel->Factor = 0;
        channel->Tone   = 0xFF;
        channel->Length = si[2];
        channel->Data   = si + 3;
        channel->KeyOnFlag++;

        if (--_Driver._IsVolumeBoostSet)
            channel->VolumeBoost = 0;

        _Driver.TieNotesTogether = false;
        _Driver._IsVolumeBoostSet = 0;
        _Driver._LoopWork &= channel->loopcheck;

        return si + 3;
    }

    SetFMTone(channel, Transpose(channel, StartLFO(channel, *si++)));

    int cx = (int) channel->Factor;
    int OldTone = channel->Tone;

    SetFMTone(channel, Transpose(channel, *si++));

    int bx = (int) channel->Factor;

    channel->Tone = OldTone;
    channel->Factor = (uint32_t) cx;

    int bh = (int) ((bx / 256) & 0x38) - ((cx / 256) & 0x38);
    int ax;

    if (bh)
    {
        bh /= 8;
        ax = bh * 0x26a;
    }
    else
        ax = 0;

    bx = (bx & 0x7ff) - (cx & 0x7ff);
    ax += bx;

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->PortamentoQuotient  = ax / channel->Length;
    channel->PortamentoRemainder = ax % channel->Length;

    channel->HardwareLFOModulationMode |= 0x08; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
    {
        if (--_Driver._IsVolumeBoostSet)
        {
            channel->VolumeBoost = 0;
            _Driver._IsVolumeBoostSet = 0;
        }
    }

    SetFMVolumeCommand(channel);
    SetFMPitch(channel);
    FMKeyOn(channel);

    channel->KeyOnFlag++;
    channel->Data = si;

    _Driver.TieNotesTogether = false;
    _Driver._IsVolumeBoostSet = 0;

    // Don't perform Key Off if a "&" command (Tie) follows immediately.
    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

    _Driver._LoopWork &= channel->loopcheck;

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::SetFMEffect(channel_t *, uint8_t * si)
{
    return si + 1;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::SetFMChannel3ModeEx(channel_t *, uint8_t * si)
{
    int16_t ax = *(int16_t *) si;
    si += 2;

    if (ax)
        InitializeFMChannel3(&_FMExtensionChannels[0], &_State.MData[ax]);

    ax = *(int16_t *) si;
    si += 2;

    if (ax)
         InitializeFMChannel3(&_FMExtensionChannels[1], &_State.MData[ax]);

    ax = *(int16_t *) si;
    si += 2;

    if (ax)
        InitializeFMChannel3(&_FMExtensionChannels[2], &_State.MData[ax]);

    return si;
}

// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// <summary>
///
/// </summary>
uint8_t * PMD::SetFMChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept
{
    const uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->PartMask |= 0x40;

            if (channel->PartMask == 0x40)
                MuteFMChannel(channel);
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
    {
        if ((channel->PartMask &= 0xBF) == 0x00)
            ResetFMInstrument(channel);
    }

    return si;
}

// Command "m": Set FM Slot. Mainly used for the 3rd FM channel, specifies the slot position (operators) to be used for performance/definition.
/// <summary>
///
/// </summary>
uint8_t * PMD::SetFMSlotCommand(channel_t * channel, uint8_t * si)
{
    int al = *si++;
    int ah = al & 0xF0;

    al &= 0x0F;

    if (al != 0x00)
    {
        channel->FMCarrier = al << 4;
    }
    else
    {
        int bl;

        if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0))
        {
            bl = _Driver._AlgorithmAndFeedbackLoopsFM3;
        }
        else
        {
            uint8_t * bx = GetFMInstrumentDefinition(channel, channel->InstrumentNumber);

            bl = bx[24];
        }

        channel->FMCarrier = FMToneCarrier[bl & 7];
    }

    if (channel->FMSlotMask != ah)
    {
        channel->FMSlotMask = ah;

        if (ah != 0x00)
            channel->PartMask &= 0xDF;  // Unmask part when other than s0
        else
            channel->PartMask |= 0x20;  // Part mask at s0

        if (SetFMChannelLFOs(channel))
        {
            if (channel != &_FMChannels[2])
            {
                if (_FMChannels[2].PartMask == 0x00 && (_FMChannels[2].KeyOffFlag & 0x01) == 0)
                    FMKeyOn(&_FMChannels[2]);

                if (channel != &_FMExtensionChannels[0])
                {
                    if (_FMExtensionChannels[0].PartMask == 0x00 && (_FMExtensionChannels[0].KeyOffFlag & 0x01) == 0)
                        FMKeyOn(&_FMExtensionChannels[0]);

                    if (channel != &_FMExtensionChannels[1])
                    {
                        if (_FMExtensionChannels[1].PartMask == 0x00 && (_FMExtensionChannels[1].KeyOffFlag & 0x01) == 0)
                            FMKeyOn(&_FMExtensionChannels[1]);
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

/// <summary>
/// Sets the hardware LFO AM/FM value.
/// </summary>
uint8_t * PMD::SetHardwareLFOCommand(channel_t * channel, uint8_t * si)
{
    channel->_PanAndVolume = (channel->_PanAndVolume & 0xC0) | *si++;

    // Part_e is impossible because it is only for YM2608. For FM3, set all four parts
    if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0))
    {
        _FMChannels[2]._PanAndVolume = channel->_PanAndVolume;

        _FMExtensionChannels[0]._PanAndVolume = channel->_PanAndVolume;
        _FMExtensionChannels[1]._PanAndVolume = channel->_PanAndVolume;
        _FMExtensionChannels[2]._PanAndVolume = channel->_PanAndVolume;
    }

    if (channel->PartMask == 0x00)
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xB4 + (_Driver._CurrentChannel - 1)), CalcPanOut(channel));

    return si;
}

/// <summary>
/// Command "sd number": Set the detune (frequency shift) value. Range -32768-32767.
/// </summary>
uint8_t * PMD::SetFMAbsoluteDetuneCommand(channel_t * channel, uint8_t * si)
{
    if ((_Driver._CurrentChannel != 3) || (_Driver._FMSelector != 0))
        return si + 3;

    int SlotNumber = *si++;
    int Value = *(int16_t *) si; si += 2;

    if (SlotNumber & 1)
        _State.FMSlot1Detune = Value;

    if (SlotNumber & 2)
        _State.FMSlot2Detune = Value;

    if (SlotNumber & 4)
        _State.FMSlot3Detune = Value;

    if (SlotNumber & 8)
        _State.FMSlot4Detune = Value;

    if (_State.FMSlot1Detune || _State.FMSlot2Detune || _State.FMSlot3Detune || _State.FMSlot4Detune)
        _Driver._IsFMSlotDetuneSet = true;
    else
    {
        _Driver._IsFMSlotDetuneSet = false;
        _State.FMSlot1Detune = 0;
    }

    SetFMChannel3LFOs(channel);

    return si;
}

/// <summary>
/// Command "sdd number": Set the detune (frequency shift) value to the previouse detune. Range -32768-32767.
/// </summary>
uint8_t * PMD::SetFMRelativeDetuneCommand(channel_t * channel, uint8_t * si)
{
    if ((_Driver._CurrentChannel != 3) || (_Driver._FMSelector != 0))
        return si + 3;

    int SlotNumber = *si++;
    int Value = *(int16_t *) si; si += 2;

    if (SlotNumber & 1)
        _State.FMSlot1Detune += Value;

    if (SlotNumber & 2)
        _State.FMSlot2Detune += Value;

    if (SlotNumber & 4)
        _State.FMSlot3Detune += Value;

    if (SlotNumber & 8)
        _State.FMSlot4Detune += Value;

    if (_State.FMSlot1Detune || _State.FMSlot2Detune || _State.FMSlot3Detune || _State.FMSlot4Detune)
        _Driver._IsFMSlotDetuneSet = true;
    else
    {
        _Driver._IsFMSlotDetuneSet = false;
        _State.FMSlot1Detune = 0;
    }

    SetFMChannel3LFOs(channel);

    return si;
}

/// <summary>
/// 9.4. Software LFO Slot Setting, Sets the slot number to apply the effect of the software LFO to.
/// </summary>
uint8_t * PMD::SetFMVolumeMaskSlotCommand(channel_t * channel, uint8_t * si)
{
    int al = *si++ & 0x0F; // Slot number

    if (al != 0)
    {
        al = (al << 4) | 0x0f;

        channel->VolumeMask1 = al;
    }
    else
        channel->VolumeMask1 = channel->FMCarrier;

    SetFMChannelLFOs(channel);

    return si;
}

/// <summary>
/// 6.3. FM TL Setting, Sets the TL (True Level, or operator volume) value of an FM instrument.
/// </summary>
uint8_t * PMD::SetFMTrueLevelCommand(channel_t * channel, uint8_t * si)
{
    int dh = 0x40 + (_Driver._CurrentChannel - 1);   // dh=TL FM Port Address

    int al = *(int8_t *) si++;
    int ah = al & 0x0F;

    int ch = (channel->FMSlotMask >> 4) | ((channel->FMSlotMask << 4) & 0xF0);

    ah &= ch; // ah = Slot to change 00004321

    int dl = *(int8_t *) si++;

    if (al >= 0)
    {
        dl &= 0x7F;

        if (ah & 1)
        {
            channel->FMOperator1 = dl;

            if (channel->PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);
        }

        dh += 8;

        if (ah & 2)
        {
            channel->FMOperator2 = dl;

            if (channel->PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);
        }

        dh -= 4;

        if (ah & 4)
        {
            channel->FMOperator3 = dl;

            if (channel->PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);
        }

        dh += 8;

        if (ah & 8)
        {
            channel->FMOperator4 = dl;

            if (channel->PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);
        }
    }
    else
    {
        // Relative change
        al = dl;

        if (ah & 1)
        {
            dl = dl = (int) channel->FMOperator1 + al;

            if (dl < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

            channel->FMOperator1 = dl;
        }

        dh += 8;

        if (ah & 2)
        {
            dl = (int) channel->FMOperator2 + al;

            if (dl < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

            channel->FMOperator2 = dl;
        }

        dh -= 4;

        if (ah & 4)
        {
            dl = (int) channel->FMOperator3 + al;

            if (dl < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

            channel->FMOperator3 = dl;
        }

        dh += 8;

        if (ah & 8)
        {
            dl = (int) channel->FMOperator4 + al;

            if (dl < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

            channel->FMOperator4 = dl;
        }
    }

    return si;
}

/// <summary>
/// 6.4. FM FB Setting, Sets the FB (Feedback) value of an FM instrument.
/// </summary>
uint8_t * PMD::SetFMFeedbackLoopsCommand(channel_t * channel, uint8_t * si)
{
    int dl;

    int dh = 0xB0 + (_Driver._CurrentChannel - 1);   // dh = Algorithm (alg) and Feedback Loops (fb) port address.
    int al = *(int8_t *) si++;

    if (al >= 0)
    {
        // in  al 00000xxx FB to set
        al = ((al << 3) & 0xff) | (al >> 5);

        // in  al 00xxx000 FB to set
        if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0))
        {
            if ((channel->FMSlotMask & 0x10) == 0)
                return si;

            dl = (_Driver._AlgorithmAndFeedbackLoopsFM3 & 7) | al;

            _Driver._AlgorithmAndFeedbackLoopsFM3 = dl;
        }
        else
            dl = (channel->AlgorithmAndFeedbackLoops & 7) | al;

        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

        channel->AlgorithmAndFeedbackLoops = dl;

        return si;
    }
    else
    {
        if ((al & 0x40) == 0)
            al &= 7;

        if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0))
            dl = _Driver._AlgorithmAndFeedbackLoopsFM3;
        else
            dl = channel->AlgorithmAndFeedbackLoops;

        dl = (dl >> 3) & 7;

        if ((al += dl) >= 0)
        {
            if (al >= 8)
            {
                al = 0x38;

                if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0))
                {
                    if ((channel->FMSlotMask & 0x10) == 0)
                        return si;

                    dl = (_Driver._AlgorithmAndFeedbackLoopsFM3 & 7) | al;

                    _Driver._AlgorithmAndFeedbackLoopsFM3 = dl;
                }
                else
                    dl = (channel->AlgorithmAndFeedbackLoops & 7) | al;

                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

                channel->AlgorithmAndFeedbackLoops = dl;

                return si;
            }
            else
            {
                // in al 00000xxx FB to set
                al = ((al << 3) & 0xff) | (al >> 5);

                // in al 00xxx000 FB to set
                if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0))
                {
                    if ((channel->FMSlotMask & 0x10) == 0)
                        return si;

                    dl = (_Driver._AlgorithmAndFeedbackLoopsFM3 & 7) | al;

                    _Driver._AlgorithmAndFeedbackLoopsFM3 = dl;
                }
                else
                    dl = (channel->AlgorithmAndFeedbackLoops & 7) | al;

                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

                channel->AlgorithmAndFeedbackLoops = dl;

                return si;
            }
        }
        else
        {
            al = 0;

            if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0))
            {
                if ((channel->FMSlotMask & 0x10) == 0)
                    return si;

                dl = (_Driver._AlgorithmAndFeedbackLoopsFM3 & 7) | al;

                _Driver._AlgorithmAndFeedbackLoopsFM3 = dl;
            }
            else
                dl = (channel->AlgorithmAndFeedbackLoops & 7) | al;

            _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

            channel->AlgorithmAndFeedbackLoops = dl;

            return si;
        }
    }
}

#pragma endregion

/// <summary>
/// Initializes the FM operators.
/// </summary>
void PMD::InitializeFMInstrument(channel_t * channel, int instrumentNumber, bool setFM3)
{
    uint8_t * Data = GetFMInstrumentDefinition(channel, instrumentNumber);

    if (MuteFMChannel(channel))
    {
        // When _ToneMask=0 (Only TL work is set)
        Data += 4;

        // Set the Total Level (tl) in each operator slot.
        channel->FMOperator1 = Data[0];
        channel->FMOperator3 = Data[1];
        channel->FMOperator2 = Data[2];
        channel->FMOperator4 = Data[3];

        return;
    }

    {
        // Set the algorithm (alg) and the feedback loops (fbl).
        if (setFM3)
            instrumentNumber = channel->AlgorithmAndFeedbackLoops;
        else
            instrumentNumber = Data[24];

        if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0))
        {
            if (setFM3)
                instrumentNumber = _Driver._AlgorithmAndFeedbackLoopsFM3;
            else
            {
                if ((channel->FMSlotMask & 0x10) == 0)
                    instrumentNumber = (_Driver._AlgorithmAndFeedbackLoopsFM3 & 0x38) | (instrumentNumber & 7);

                _Driver._AlgorithmAndFeedbackLoopsFM3 = instrumentNumber;
            }
        }
    }

    uint32_t Register = (uint32_t) (_Driver._FMSelector + 0xB0 + (_Driver._CurrentChannel - 1));

    _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);

    {
        channel->AlgorithmAndFeedbackLoops = instrumentNumber;

        instrumentNumber &= 0x07;

        channel->FMCarrier = FMToneCarrier[instrumentNumber];

        if ((channel->VolumeMask1 & 0x0F) == 0)
            channel->VolumeMask1 = channel->FMCarrier;

        if ((channel->VolumeMask2 & 0x0F) == 0)
            channel->VolumeMask2 = channel->FMCarrier;
    }

    // AH = Mask for Total Level (TL)
    int ah = FMToneCarrier[instrumentNumber + 8]; // Reversed data of slot2/3 (not completed)

    // AL = Mask for other parameters.
    int al = channel->ToneMask;

    ah &= al;

    // Set each tone parameter (TL is modulator only)
    Register = (uint32_t) (_Driver._FMSelector + 0x30 + (_Driver._CurrentChannel - 1));

    {
        // Detune (dt) / Multiplier (ml)
        instrumentNumber = *Data++;
        if (al & 0x80) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x40) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x20) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x10) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;
    }

    {
        // Total Level (tl)
        instrumentNumber = *Data++;
        if (ah & 0x80) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (ah & 0x40) _OPNAW->SetReg(Register,(uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (ah & 0x20) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (ah & 0x10) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;
    }

    {
        // Key Scale (ks) / Attack Rate (ar)
        instrumentNumber = *Data++;
        if (al & 0x08) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x04) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x02) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x01) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;
    }

    {
        // Amplitude Modulation Set (ams) / Decay Rate (dr)
        instrumentNumber = *Data++;
        if (al & 0x80) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x40) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x20) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x10) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;
    }

    {
        // Sustain Rate (sr)
        instrumentNumber = *Data++;
        if (al & 0x08) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x04) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x02) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x01) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;
    }

    {
        // Sustain Level (sl) / Release Rate (rr)
        instrumentNumber = *Data++;
        if (al & 0x80) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x40) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x20) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;

        instrumentNumber = *Data++;
        if (al & 0x10) _OPNAW->SetReg(Register, (uint32_t) instrumentNumber);
        Register += 4;
    }

    // Store the Total Level (TL) in each operator slot.
    Data -= 20;

    channel->FMOperator1 = Data[0];
    channel->FMOperator3 = Data[1];
    channel->FMOperator2 = Data[2];
    channel->FMOperator4 = Data[3];
}

/// <summary>
/// Sets pitch and volume LFOs for FM channel 3.
/// </summary>
bool PMD::SetFMChannelLFOs(channel_t * channel)
{
    if ((_Driver._CurrentChannel != 3) || (_Driver._FMSelector != 0))
        return false;

    SetFMChannel3LFOs(channel);

    return true;
}

/// <summary>
/// Sets pitch and volume LFOs for FM channel 3.
/// </summary>
void PMD::SetFMChannel3LFOs(channel_t * channel)
{
    int al;

    if (channel == &_FMChannels[2])
        al = 1;
    else
    if (channel == &_FMExtensionChannels[0])
        al = 2;
    else
    if (channel == &_FMExtensionChannels[1])
        al = 4;
    else
        al = 8;

    int Mode;

    if ((channel->FMSlotMask & 0xF0) == 0)
        ClearFM3(Mode, al); // s0
    else
    if (channel->FMSlotMask != 0xF0)
    {
        _Driver._Slot3Flags |= al;
        Mode = 0x7F;
    }
    else

    if ((channel->VolumeMask1 & 0x0F) == 0)
        ClearFM3(Mode, al);
    else
    if ((channel->HardwareLFOModulationMode & 0x01) != 0)
    {
        _Driver._Slot3Flags |= al;
        Mode = 0x7F;
    }
    else

    if ((channel->VolumeMask2 & 0x0F) == 0)
        ClearFM3(Mode, al);
    else
    if (channel->HardwareLFOModulationMode & 0x10)
    {
        _Driver._Slot3Flags |= al;
        Mode = 0x7F;
    }
    else
        ClearFM3(Mode, al);

    if ((uint32_t) Mode == _State.FMChannel3Mode)
        return;

    _State.FMChannel3Mode = (uint32_t) Mode;

    _OPNAW->SetReg(0x27, (uint32_t) (Mode & 0xCF)); // Don't reset.

    // When moving to sound effect mode, the pitch is rewritten with the previous FM3 part
    if (Mode == 0x3F || channel == &_FMChannels[2])
        return;

    if (_FMChannels[2].PartMask == 0x00)
        SetFMPitch(&_FMChannels[2]);

    if (channel == &_FMExtensionChannels[0])
        return;

    if (_FMExtensionChannels[0].PartMask == 0x00)
        SetFMPitch(&_FMExtensionChannels[0]);

    if (channel == &_FMExtensionChannels[1])
        return;

    if (_FMExtensionChannels[1].PartMask == 0x00)
        SetFMPitch(&_FMExtensionChannels[1]);
}

/// <summary>
///
/// </summary>
void PMD::ClearFM3(int & ah, int & al) noexcept
{
    al ^= 0xFF;

    if ((_Driver._Slot3Flags &= al) == 0)
        ah = _Driver._IsFMSlotDetuneSet ? 0x7F : 0x3F;
    else
        ah = 0x7F;
}

/// <summary>
///
/// </summary>
void PMD::InitializeFMChannel3(channel_t * channel, uint8_t * ax)
{
    channel->Data = ax;
    channel->Length = 1;
    channel->KeyOffFlag = -1;
    channel->LFO1MDepthCount1 = -1; // Infinity
    channel->LFO1MDepthCount2 = -1; // Infinity
    channel->LFO2MDepthCount1 = -1; // Infinity
    channel->LFO2MDepthCount2 = -1; // Infinity
    channel->Tone = 0xFF;           // Rest
    channel->DefaultTone = 0xFF;    // Rest
    channel->Volume = 108;
    channel->_PanAndVolume = _FMChannels[2]._PanAndVolume; // Use FM channel 3 value
    channel->PartMask |= 0x20;
}

/// <summary>
/// Gets the definition of an FM instrument.
/// </summary>
uint8_t * PMD::GetFMInstrumentDefinition(channel_t * channel, int instrumentNumber)
{
    if (_State.InstrumentDefinitions == nullptr)
    {
        if (channel != &_EffectChannel)
            return _State.VData + ((size_t) instrumentNumber << 5);
        else
            return _State.EData;
    }

    uint8_t * Data = _State.InstrumentDefinitions;

    while (Data[0] != instrumentNumber)
    {
        Data += 26;

        if (Data > _MData + sizeof(_MData) - 26)
            return _State.InstrumentDefinitions + 1; // Return the first definition if not found.
    }

    return Data + 1;
}

// Reset the tone of the FM sound source
/// <summary>
///
/// </summary>
void PMD::ResetFMInstrument(channel_t * channel)
{
    if (channel->ToneMask == 0)
        return;

    int s1 = channel->FMOperator1;
    int s2 = channel->FMOperator2;
    int s3 = channel->FMOperator3;
    int s4 = channel->FMOperator4;

    InitializeFMInstrument(channel, channel->InstrumentNumber, true);

    channel->FMOperator1 = s1;
    channel->FMOperator2 = s2;
    channel->FMOperator3 = s3;
    channel->FMOperator4 = s4;

    int dh;

    int al = ((~channel->FMCarrier) & channel->FMSlotMask) & 0xf0;

    // al<- TLを再設定していいslot 4321xxxx
    if (al)
    {
        dh = 0x4C + (_Driver._CurrentChannel - 1); // dh=TL FM Port Address

        if (al & 0x80) _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) s4);

        dh -= 8;

        if (al & 0x40) _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) s3);

        dh += 4;

        if (al & 0x20) _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) s2);

        dh -= 8;

        if (al & 0x10) _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) s1);
    }

    dh = 0xB4 + (_Driver._CurrentChannel - 1);

    _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), CalcPanOut(channel));
}

//  Pitch setting when using ch3=sound effect mode (input CX:block AX:fnum)
/// <summary>
///
/// </summary>
void PMD::SpecialFM3Processing(channel_t * channel, int ax, int cx)
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
        ax += _State.FMSlot4Detune;

        if ((bh & shiftmask) && (channel->HardwareLFOModulationMode & 0x01))  ax += channel->LFO1Data;
        if ((ch & shiftmask) && (channel->HardwareLFOModulationMode & 0x10))  ax += channel->LFO2Data;
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
        ax += _State.FMSlot3Detune;

        if ((bh & shiftmask) && (channel->HardwareLFOModulationMode & 0x01))  ax += channel->LFO1Data;
        if ((ch & shiftmask) && (channel->HardwareLFOModulationMode & 0x10))  ax += channel->LFO2Data;
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
        ax += _State.FMSlot2Detune;

        if ((bh & shiftmask) && (channel->HardwareLFOModulationMode & 0x01))
            ax += channel->LFO1Data;

        if ((ch & shiftmask) && (channel->HardwareLFOModulationMode & 0x10))
            ax += channel->LFO2Data;

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
        ax += _State.FMSlot1Detune;

        if ((bh & shiftmask) && (channel->HardwareLFOModulationMode & 0x01)) 
            ax += channel->LFO1Data;

        if ((ch & shiftmask) && (channel->HardwareLFOModulationMode & 0x10))
            ax += channel->LFO2Data;

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
/// <summary>
///
/// </summary>
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
        { // ﾓｳ ｺﾚｲｼﾞｮｳ ｱｶﾞﾝﾅｲﾖﾝ
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
        { // ﾓｳ ｺﾚｲｼﾞｮｳ ｻｶﾞﾝﾅｲﾖﾝ
            *cx = 0;

            if (*ax < 8)
                *ax = 8;

            return;
        }
    }
}

// Get the data to set to 0b4h? out.dl
/// <summary>
///
/// </summary>
uint8_t PMD::CalcPanOut(channel_t * channel)
{
    int  dl = channel->_PanAndVolume;

    if (channel->HardwareLFODelayCounter)
        dl &= 0xC0; // If HLFO Delay remains, set only pan.

    return (uint8_t) dl;
}

//  Calculate and output macro for each slot.
//      in. dl  Original TL value
//          dh  Register to Out
//          al  Volume fluctuation value center = 80h
/// <summary>
///
/// </summary>
void PMD::CalcVolSlot(int dh, int dl, int al)
{
    al += dl;

    if (al > 255)
        al = 255;

    al -= 0x80;

    if (al < 0)
        al = 0;

    _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) al);
}

/// <summary>
///
/// </summary>
void PMD::CalcFMLFO(channel_t *, int al, int bl, uint8_t * vol_tbl)
{
    if (bl & 0x80) vol_tbl[0] = (uint8_t) std::clamp(vol_tbl[0] - al, 0, 255);
    if (bl & 0x40) vol_tbl[1] = (uint8_t) std::clamp(vol_tbl[1] - al, 0, 255);
    if (bl & 0x20) vol_tbl[2] = (uint8_t) std::clamp(vol_tbl[2] - al, 0, 255);
    if (bl & 0x10) vol_tbl[3] = (uint8_t) std::clamp(vol_tbl[3] - al, 0, 255);
}
