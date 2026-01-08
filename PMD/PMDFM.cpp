
/** $VER: PMDFM.cpp (2026.01.07) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

/// <summary>
/// Main FM processing
/// </summary>
void pmd_driver_t::FMMain(channel_t * channel) noexcept
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
            FMKeyOff(channel);

            channel->_KeyOffFlag = -1;
        }
    }

    if (channel->_Size == 0)
    {
        if (channel->_PartMask == 0x00)
            channel->_HardwareLFO &= 0xF7;

        while (1)
        {
            if ((*si > 0x80) && (*si != 0xDA))
            {
                si = FMExecuteCommand(channel, si);
            }
            else
            if (*si == 0x80)
            {
                channel->_Data = si;
                channel->_Tone = 0xFF;

                channel->_LoopCheck = 0x03;

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
            if (*si == 0xDA)
            {
                si = SetFMPortamentoCommand(channel, ++si);

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
            else
            if (channel->_PartMask == 0x00)
            {
                SetFMTone(channel, Transpose(channel, StartLFO(channel, *si++)));

                channel->_Size = *si++;

                si = CalculateQ(channel, si);

                if ((channel->VolumeBoost != 0) && (channel->_Tone != 0xFF))
                {
                    if (--_Driver._VolumeBoostCount != 0)
                    {
                        _Driver._VolumeBoostCount = 0;

                        channel->VolumeBoost = 0;
                    }
                }

                {
                    SetFMVolumeCommand(channel);
                    SetFMPitch(channel);
                    FMKeyOn(channel);

                    channel->_KeyOnFlag++;
                    channel->_Data = si;
                    channel->_KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00; // Don't perform Key Off if a "&" command (Tie) follows immediately.
                }

                _Driver._IsTieSet = false;
                _Driver._VolumeBoostCount = 0;

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
            else
            {
                si++;

                // Set to 'rest'.
                channel->_Factor      = 0;
                channel->_Tone        = 0xFF;
                channel->_DefaultTone = 0xFF;
                channel->_Size      = *si++;
                channel->_KeyOnFlag++;

                channel->_Data = si;

                if (--_Driver._VolumeBoostCount != 0)
                {
                    _Driver._VolumeBoostCount = 0;

                    channel->VolumeBoost = 0;
                }

                _Driver._IsTieSet = false;
                _Driver._VolumeBoostCount = 0;
                break;
            }
        }
    }

    if (channel->_PartMask == 0x00)
    {
        // Finish with LFO & Portament & Fadeout processing
        if (channel->_HardwareLFODelayCounter)
        {
            if (--channel->_HardwareLFODelayCounter == 0)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xB4 + (_Driver._CurrentChannel - 1)), (uint32_t) channel->_PanAndVolume);
        }

        if (channel->_FMSlotDelayCounter != 0)
        {
            if (--channel->_FMSlotDelayCounter == 0)
            {
                if ((channel->_KeyOffFlag & 0x01) == 0)
                    FMKeyOn(channel);
            }
        }

        if (channel->_HardwareLFO)
        {
            _Driver._HardwareLFOModulationMode = channel->_HardwareLFO & 0x08;

            if (channel->_HardwareLFO & 0x03)
            {
                if (SetLFO(channel))
                    _Driver._HardwareLFOModulationMode |= (channel->_HardwareLFO & 0x03);
            }

            if (channel->_HardwareLFO & 0x30)
            {
                LFOSwap(channel);

                if (SetLFO(channel))
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

                SetFMPitch(channel);
            }

            if (_Driver._HardwareLFOModulationMode & 0x22)
            {
                SetFMVolumeCommand(channel);

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
        }

        if (_State._FadeOutSpeed != 0)
            SetFMVolumeCommand(channel);
    }

    _Driver._LoopCheck &= channel->_LoopCheck;
}

/// <summary>
/// Executes an FM command.
/// </summary>
uint8_t * pmd_driver_t::FMExecuteCommand(channel_t * channel, uint8_t * si)
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
            channel->_EarlyKeyOffTimeout1 = *si++;
            channel->EarlyKeyOffTimeoutRandomRange = 0;
            break;
        }

        // 5.5. Relative Volume Change, Increase volume by 3dB.
        case 0xF4:
        {
            channel->_Volume += 4;

            if (channel->_Volume > 127)
                channel->_Volume = 127;
            break;
        }

        // 5.5. Relative Volume Change, Decrease volume by 3dB.
        case 0xF3:
        {
            channel->_Volume -= 4;

            if (channel->_Volume < 4)
                channel->_Volume = 0;
            break;
        }

        // 9.3. Software LFO Switch, Controls on/off and keyon synchronization of the software LFO, Command '*A number'
        case 0xF1:
        {
            si = LFO1SetSwitch(channel, si);

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

        // 9.13. Hardware LFO Delay Setting, Set hardware LFO delay. Command '#D number' / Command '#D l length[.]', Range: (number) 0–255 / (length) 1–255, divisible by the whole note length
        case 0xE4:
            channel->_HardwareLFODelay = *si++;
            break;

        // 5.5. Relative Volume Change, Command ') %number'
        case 0xE3:
        {
            channel->_Volume += *si++;

            if (channel->_Volume > 127)
                channel->_Volume = 127;
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

        // 9.10. Hardware LFO Speed/Delay Setting, Command 'H number1[, number2]'
        case 0xE1:
        {
            si = SetHardwareLFO_PMS_AMS(channel, si);
            break;
        }

        // 9.11. Hardware LFO Switch/Depth Setting (OPNA), Command "# number1, [number2]": Sets the hardware LFO on (1) or off (0). (OPNA FM sound source only). Number2 = depth. Can be omitted only when switch is 0.
        case 0xE0:
        {
            // Set the hardware LFO frequency (Speed) (LFO FREQ CONTROL, 0: 3.98 Hz, 1: 5.56 Hz, 2: 6.02 Hz, 3: 6.37 Hz, 4: 6.88 Hz, 5: 9.63 Hz, 6: 48.1 Hz, 7: 72.2 Hz)
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

        // 9.4. Software LFO Slot Setting, Sets the slot number to apply the effect of the software LFO to, Command 'MMA slotnum' (FM Sound Source only)
        case 0xC5:
        {
            si = LFO1SetSlotMask(channel, si);
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

        // 9.3. Software LFO Switch, Controls on/off and keyon synchronization of the software LFO, Command '*B number'
        case 0xBE:
        {
            si = LFO2SetSwitch(channel, si);

            SetFMChannelLFOs(channel);
            break;
        }

        // 9.4. Software LFO Slot Setting, Sets the slot number to apply the effect of the software LFO to, Commmand 'MMB slotnum' (FM Sound Source only)
        case 0xBA:
        {
            si = LFO2SetSlotMask(channel, si);
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
            si = SetFMFeedbackLoopCommand(channel, si);
            break;
        }

        // 12.3. Keyon Delay Per Slot Setting, Delays the KeyOn of specified slots, Command 'sk number1[, number2]'
        case 0xB5:
        {
            channel->_FMSlotDelayMask = (~(*si++) << 4) & 0xF0;
            channel->_FMSlotDelayCounter =
            channel->_SlotDelay = *si++;
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
int pmd_driver_t::MuteFMChannel(channel_t * channel)
{
    if (channel->_ToneMask == 0)
        return 1;

    int32_t Channel = 0x40 + (_Driver._CurrentChannel - 1);

    if (channel->_ToneMask & 0x80)
    {
        _OPNAW->SetReg((uint32_t)  (_Driver._FMSelector         + Channel), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver._FMSelector + 0x40) + Channel), 127);
    }

    Channel += 4;

    if (channel->_ToneMask & 0x40)
    {
        _OPNAW->SetReg((uint32_t)  (_Driver._FMSelector         + Channel), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver._FMSelector + 0x40) + Channel), 127);
    }

    Channel += 4;

    if (channel->_ToneMask & 0x20)
    {
        _OPNAW->SetReg((uint32_t)  (_Driver._FMSelector         + Channel), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver._FMSelector + 0x40) + Channel), 127);
    }

    Channel += 4;

    if (channel->_ToneMask & 0x10)
    {
        _OPNAW->SetReg((uint32_t)  (_Driver._FMSelector         + Channel), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver._FMSelector + 0x40) + Channel), 127);
    }

    FMKeyOff(channel);

    return 0;
}

#pragma region Commands

/// <summary>
///
/// </summary>
void pmd_driver_t::SetFMTone(channel_t * channel, int tone)
{
    if ((tone & 0x0F) != 0x0F)
    {
        channel->_Tone = tone;

        int32_t Block  = tone & 0x0F;
        int32_t Factor = _FMScaleFactor[Block];

        Factor |= (((tone >> 1) & 0x38) << 8);

        channel->_Factor = (uint32_t) Factor;
    }
    else
    {
        channel->_Tone = 0xFF; // Rest

        if ((channel->_HardwareLFO & 0x11) == 0)
            channel->_Factor = 0; // Don't use LFO pitch.
    }
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SetFMVolumeCommand(channel_t * channel)
{
    if (channel->_FMSlotMask == 0)
        return;

    int32_t Volume = (channel->VolumeBoost) ? channel->VolumeBoost - 1 : channel->_Volume;

    if (channel != &_SSGEffectChannel)
    {
        // Calculates the effect of volume down.
        if (_State._FMVolumeAdjust != 0)
            Volume = ((256 - _State._FMVolumeAdjust) * Volume) >> 8;

        // Calculates the effect of fade out.
        if (_State._FadeOutVolume >= 2)
            Volume = ((256 - (_State._FadeOutVolume >> 1)) * Volume) >> 8;
    }

    Volume = 255 - Volume;

    // Set volume to carrier & volume LFO processing.
    uint8_t SlotVolume[4] = { 0x80, 0x80, 0x80, 0x80 };

    int32_t bl = channel->_FMSlotMask;

    bl &= channel->FMCarrier;    // bl = Set volume SLOT xxxx0000b

    int32_t bh = bl;

    if (bl & 0x80) SlotVolume[0] = (uint8_t) Volume;
    if (bl & 0x40) SlotVolume[1] = (uint8_t) Volume;
    if (bl & 0x20) SlotVolume[2] = (uint8_t) Volume;
    if (bl & 0x10) SlotVolume[3] = (uint8_t) Volume;

    if (Volume != 255)
    {
        if (channel->_HardwareLFO & 0x02)
        {
            bl = channel->_LFO1Mask;
            bl &= channel->_FMSlotMask;    // bl=SLOT to set volume LFO xxxx0000b
            bh |= bl;

            CalcFMLFO(channel, channel->_LFO1Data, bl, SlotVolume);
        }

        if (channel->_HardwareLFO & 0x20)
        {
            bl = channel->_LFO2Mask;
            bl &= channel->_FMSlotMask;
            bh |= bl;

            CalcFMLFO(channel, channel->_LFO2Data, bl, SlotVolume);
        }
    }

    int32_t dh = 0x4C + (_Driver._CurrentChannel - 1); // FM Port Address

    if (bh & 0x80) CalcVolSlot(dh,      channel->FMOperator4, SlotVolume[0]);
    if (bh & 0x40) CalcVolSlot(dh -  8, channel->FMOperator3, SlotVolume[1]);
    if (bh & 0x20) CalcVolSlot(dh -  4, channel->FMOperator2, SlotVolume[2]);
    if (bh & 0x10) CalcVolSlot(dh - 12, channel->FMOperator1, SlotVolume[3]);
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SetFMPitch(channel_t * channel)
{
    if ((channel->_Factor == 0) || (channel->_FMSlotMask == 0))
        return;

    int32_t Block = (int32_t) (channel->_Factor  & 0x3800);
    int32_t Pitch = (int32_t) (channel->_Factor) & 0x07FF;

    // Portament/LFO/Detune SET
    Pitch += channel->_Portamento + channel->_DetuneValue;

    if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000) && (_State._FMChannel3Mode != 0x3F))
        SpecialFM3Processing(channel, Pitch, Block);
    else
    {
        if (channel->_HardwareLFO & 0x01)
            Pitch += channel->_LFO1Data;

        if (channel->_HardwareLFO & 0x10)
            Pitch += channel->_LFO2Data;

        CalcFMBlock(&Block, &Pitch);

        Pitch |= Block;

        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xA4 + (_Driver._CurrentChannel - 1)), (uint32_t) HIBYTE(Pitch));
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xA0 + (_Driver._CurrentChannel - 1)), (uint32_t) LOBYTE(Pitch));
    }
}

/// <summary>
///
/// </summary>
void pmd_driver_t::FMKeyOn(channel_t * channel)
{
    if (channel->_Tone == 0xFF)
        return;

    int32_t Index = _Driver._CurrentChannel - 1;

    if (_Driver._FMSelector == 0x000)
    {
        int32_t FMSlotKey = _FMSlotKey1[Index] | channel->_FMSlotMask;

        if (channel->_FMSlotDelayCounter != 0)
            FMSlotKey &= channel->_FMSlotDelayMask;

        _FMSlotKey1[Index] = FMSlotKey;

        _OPNAW->SetReg(0x28, (uint32_t) (Index | FMSlotKey));
    }
    else
    {
        int32_t FMSlotKey = _FMSlotKey2[Index] | channel->_FMSlotMask;

        if (channel->_FMSlotDelayCounter != 0)
            FMSlotKey &= channel->_FMSlotDelayMask;

        _FMSlotKey2[Index] = FMSlotKey;

        _OPNAW->SetReg(0x28, (uint32_t) (Index | FMSlotKey | 0x04));
    }
}

/// <summary>
///
/// </summary>
void pmd_driver_t::FMKeyOff(channel_t * channel)
{
    if (channel->_Tone == 0xFF)
        return;

    int32_t Index = _Driver._CurrentChannel - 1;

    if (_Driver._FMSelector == 0x000)
    {
        _FMSlotKey1[Index] &= ~channel->_FMSlotMask;

        _OPNAW->SetReg(0x28, (uint32_t) (Index | _FMSlotKey1[Index]));
    }
    else
    {
        _FMSlotKey2[Index] &= ~channel->_FMSlotMask;

        _OPNAW->SetReg(0x28, (uint32_t) (Index | _FMSlotKey2[Index]) | 0x04);
    }
}

/// <summary>
/// Command "@ number": Sets the instrument to be used. Range 0-255.
/// </summary>
uint8_t * pmd_driver_t::SetFMInstrument(channel_t * channel, uint8_t * si)
{
    int32_t IntrumentNumber = *si++;

    int32_t dl = IntrumentNumber;

    channel->InstrumentNumber = IntrumentNumber;

    if (channel->_PartMask == 0x00)
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

    if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000) && (channel->_ToneMask != 0))
    {
        if ((channel->_FMSlotMask & 0x10) == 0)
        {
            IntrumentNumber = _Driver._AlgorithmAndFeedbackLoopsFM3 & 0x38; // Feedback Loops (fb) uses previous value.
            dl = (dl & 0x07) | IntrumentNumber;
        }

        _Driver._AlgorithmAndFeedbackLoopsFM3 = dl;
        channel->AlgorithmAndFeedbackLoops = IntrumentNumber;
    }

    return si;
}

/// <summary>
/// Command "p <value>" (1: right, 2: left, 3: center (default))
/// </summary>
uint8_t * pmd_driver_t::SetFMPan1(channel_t * channel, uint8_t * si)
{
    SetFMPannningInternal(channel, *si++);

    return si;
}

/// <summary>
/// Command "px <value 1>, <value 2>" (value 1: -4 (pan to the left) to +4 (pan to the right), value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * pmd_driver_t::SetFMPan2(channel_t * channel, uint8_t * si)
{
    int32_t Value = *(int8_t *) si++;

    si++; // Skip the Phase flag

    if (Value > 0)
    {
        Value = 2; // Right
        SetFMPannningInternal(channel, Value);
    }
    else
    if (Value == 0)
    {
        Value = 3; // Center
        SetFMPannningInternal(channel, Value);
    }
    else
    {
        Value = 1; // Left
        SetFMPannningInternal(channel, Value);
    }

    return si;
}

/// <summary>
///
/// </summary>
void pmd_driver_t::SetFMPannningInternal(channel_t * channel, int value)
{
    channel->_PanAndVolume = (channel->_PanAndVolume & 0x3F) | ((value << 6) & 0xC0);

    if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000))
    {
        // For FM3, set all 4 parts.
        _FMChannels[2]._PanAndVolume = channel->_PanAndVolume;

        _FMExtensionChannels[0]._PanAndVolume = channel->_PanAndVolume;
        _FMExtensionChannels[1]._PanAndVolume = channel->_PanAndVolume;
        _FMExtensionChannels[2]._PanAndVolume = channel->_PanAndVolume;
    }

    if (channel->_PartMask == 0x00)
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xB4 + (_Driver._CurrentChannel - 1)), CalcPanOut(channel));
}

/// <summary>
///
/// </summary>
uint8_t * pmd_driver_t::DecreaseFMVolumeCommand(channel_t *, uint8_t * si)
{
    int32_t Value = *(int8_t *) si++;

    if (Value != 0)
        _State._FMVolumeAdjust = std::clamp(Value + _State._FMVolumeAdjust, 0, 255);
    else
        _State._FMVolumeAdjust = _State._FMVolumeAdjustDefault;

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * pmd_driver_t::SetFMPortamentoCommand(channel_t * channel, uint8_t * si)
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

        return si + 3;
    }

    SetFMTone(channel, Transpose(channel, StartLFO(channel, *si++)));

    int32_t cx = (int32_t) channel->_Factor;
    int32_t OldTone = channel->_Tone;

    SetFMTone(channel, Transpose(channel, *si++));

    int32_t bx = (int32_t) channel->_Factor;

    channel->_Tone = OldTone;
    channel->_Factor = (uint32_t) cx;

    int32_t bh = (int32_t) ((bx / 256) & 0x38) - ((cx / 256) & 0x38);
    int32_t ax;

    if (bh != 0)
    {
        bh /= 8;
        ax = bh * 0x26a;
    }
    else
        ax = 0;

    bx = (bx & 0x7ff) - (cx & 0x7ff);
    ax += bx;

    channel->_Size = *si++;

    si = CalculateQ(channel, si);

    channel->_PortamentoQuotient  = ax / channel->_Size;
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

    SetFMVolumeCommand(channel);
    SetFMPitch(channel);
    FMKeyOn(channel);

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
///
/// </summary>
uint8_t * pmd_driver_t::SetFMEffect(channel_t *, uint8_t * si)
{
    return si + 1;
}

/// <summary>
///
/// </summary>
uint8_t * pmd_driver_t::SetFMChannel3ModeEx(channel_t *, uint8_t * si)
{
    int16_t ax = *(int16_t *) si;
    si += 2;

    if (ax)
        InitializeFMChannel3(&_FMExtensionChannels[0], &_State._MData[ax]);

    ax = *(int16_t *) si;
    si += 2;

    if (ax)
         InitializeFMChannel3(&_FMExtensionChannels[1], &_State._MData[ax]);

    ax = *(int16_t *) si;
    si += 2;

    if (ax)
        InitializeFMChannel3(&_FMExtensionChannels[2], &_State._MData[ax]);

    return si;
}

// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// <summary>
///
/// </summary>
uint8_t * pmd_driver_t::SetFMChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept
{
    const uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->_PartMask |= 0x40;

            if (channel->_PartMask == 0x40)
                MuteFMChannel(channel);
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
    {
        if ((channel->_PartMask &= 0xBF) == 0x00)
            ResetFMInstrument(channel);
    }

    return si;
}

// Command "m": Set FM Slot. Mainly used for the 3rd FM channel, specifies the slot position (operators) to be used for performance/definition.
/// <summary>
///
/// </summary>
uint8_t * pmd_driver_t::SetFMSlotCommand(channel_t * channel, uint8_t * si)
{
    int32_t FMSlot = *si++;
    int32_t ah = FMSlot & 0xF0;

    FMSlot &= 0x0F;

    if (FMSlot != 0x00)
    {
        channel->FMCarrier = FMSlot << 4;
    }
    else
    {
        int32_t bl;

        if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000))
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

    if (channel->_FMSlotMask != ah)
    {
        channel->_FMSlotMask = ah;

        if (ah != 0x00)
            channel->_PartMask &= 0xDF;  // Unmask part when other than s0
        else
            channel->_PartMask |= 0x20;  // Part mask at s0

        if (SetFMChannelLFOs(channel))
        {
            if (channel != &_FMChannels[2])
            {
                if (_FMChannels[2]._PartMask == 0x00 && (_FMChannels[2]._KeyOffFlag & 0x01) == 0)
                    FMKeyOn(&_FMChannels[2]);

                if (channel != &_FMExtensionChannels[0])
                {
                    if (_FMExtensionChannels[0]._PartMask == 0x00 && (_FMExtensionChannels[0]._KeyOffFlag & 0x01) == 0)
                        FMKeyOn(&_FMExtensionChannels[0]);

                    if (channel != &_FMExtensionChannels[1])
                    {
                        if (_FMExtensionChannels[1]._PartMask == 0x00 && (_FMExtensionChannels[1]._KeyOffFlag & 0x01) == 0)
                            FMKeyOn(&_FMExtensionChannels[1]);
                    }
                }
            }
        }

        ah = 0x00;

        if (channel->_FMSlotMask & 0x80) ah += 0x11; // Slot 4
        if (channel->_FMSlotMask & 0x40) ah += 0x44; // Slot 3
        if (channel->_FMSlotMask & 0x20) ah += 0x22; // Slot 2
        if (channel->_FMSlotMask & 0x10) ah += 0x88; // Slot 1

        channel->_ToneMask = ah;
    }

    return si;
}

/// <summary>
/// 9.10. Hardware LFO Speed/Delay Setting, Sets PMS and AMS of hardware LFO. (OPNA/OPM FM sound source only), Command 'H number1[,number2]', PMS: 0 - 7 / AMS: 0 - 3
/// </summary>
uint8_t * pmd_driver_t::SetHardwareLFO_PMS_AMS(channel_t * channel, uint8_t * si)
{
    channel->_PanAndVolume = (channel->_PanAndVolume & 0xC0) | *si++;

    // Part_e is impossible because it is only for YM2608. For FM3, set all four parts
    if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000))
    {
        _FMChannels[2]._PanAndVolume = channel->_PanAndVolume;

        _FMExtensionChannels[0]._PanAndVolume = channel->_PanAndVolume;
        _FMExtensionChannels[1]._PanAndVolume = channel->_PanAndVolume;
        _FMExtensionChannels[2]._PanAndVolume = channel->_PanAndVolume;
    }

    // Phase Modulation Sensitivity (PMS) controls the depth of pitch modulation, measured in cents (1/100th of a semitone). Set per channel via 3 bits (D5-D3) in registers $B4–$B6. Ranges from 0 (no effect) to around 80 cents max depth.
    // Amplitude Modulation Sensitivity (AMS) controls the depth of volume modulation, measured in decibels (dB). Set per channel via 2 bits (D2-D1) in registers $B4–$B6. Ranges from 0 dB (no effect) to 11.8 dB max depth.
    if (channel->_PartMask == 0x00)
        _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + 0xB4 + (_Driver._CurrentChannel - 1)), CalcPanOut(channel));

    return si;
}

/// <summary>
/// Command "sd number": Set the detune (frequency shift) value. Range -32768-32767.
/// </summary>
uint8_t * pmd_driver_t::SetFMAbsoluteDetuneCommand(channel_t * channel, uint8_t * si)
{
    if ((_Driver._CurrentChannel != 3) || (_Driver._FMSelector != 0x000))
        return si + 3;

    int32_t SlotNumber = *si++;
    int32_t Value = *(int16_t *) si; si += 2;

    if (SlotNumber & 1)
        _State._FMSlot1Detune = Value;

    if (SlotNumber & 2)
        _State._FMSlot2Detune = Value;

    if (SlotNumber & 4)
        _State._FMSlot3Detune = Value;

    if (SlotNumber & 8)
        _State._FMSlot4Detune = Value;

    if (_State._FMSlot1Detune || _State._FMSlot2Detune || _State._FMSlot3Detune || _State._FMSlot4Detune)
        _Driver._IsFMSlotDetuneSet = true;
    else
    {
        _Driver._IsFMSlotDetuneSet = false;
        _State._FMSlot1Detune = 0;
    }

    SetFMChannel3LFOs(channel);

    return si;
}

/// <summary>
/// Command "sdd number": Set the detune (frequency shift) value to the previouse detune. Range -32768-32767.
/// </summary>
uint8_t * pmd_driver_t::SetFMRelativeDetuneCommand(channel_t * channel, uint8_t * si)
{
    if ((_Driver._CurrentChannel != 3) || (_Driver._FMSelector != 0x000))
        return si + 3;

    int32_t SlotNumber = *si++;
    int32_t Value = *(int16_t *) si; si += 2;

    if (SlotNumber & 1)
        _State._FMSlot1Detune += Value;

    if (SlotNumber & 2)
        _State._FMSlot2Detune += Value;

    if (SlotNumber & 4)
        _State._FMSlot3Detune += Value;

    if (SlotNumber & 8)
        _State._FMSlot4Detune += Value;

    if (_State._FMSlot1Detune || _State._FMSlot2Detune || _State._FMSlot3Detune || _State._FMSlot4Detune)
        _Driver._IsFMSlotDetuneSet = true;
    else
    {
        _Driver._IsFMSlotDetuneSet = false;
        _State._FMSlot1Detune = 0;
    }

    SetFMChannel3LFOs(channel);

    return si;
}

/// <summary>
/// 6.3. FM TL Setting, Sets the TL (True Level, or operator volume) value of an FM instrument.
/// </summary>
uint8_t * pmd_driver_t::SetFMTrueLevelCommand(channel_t * channel, uint8_t * si)
{
    int32_t dh = 0x40 + (_Driver._CurrentChannel - 1);   // dh=TL FM Port Address

    int32_t al = *(int8_t *) si++;
    int32_t ah = al & 0x0F;

    int32_t ch = (channel->_FMSlotMask >> 4) | ((channel->_FMSlotMask << 4) & 0xF0);

    ah &= ch; // ah = Slot to change 00004321

    int32_t dl = *(int8_t *) si++;

    if (al >= 0)
    {
        dl &= 0x7F;

        if (ah & 1)
        {
            channel->FMOperator1 = dl;

            if (channel->_PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);
        }

        dh += 8;

        if (ah & 2)
        {
            channel->FMOperator2 = dl;

            if (channel->_PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);
        }

        dh -= 4;

        if (ah & 4)
        {
            channel->FMOperator3 = dl;

            if (channel->_PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);
        }

        dh += 8;

        if (ah & 8)
        {
            channel->FMOperator4 = dl;

            if (channel->_PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);
        }
    }
    else
    {
        // Relative change
        al = dl;

        if (ah & 1)
        {
            dl = dl = (int32_t) channel->FMOperator1 + al;

            if (dl < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->_PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

            channel->FMOperator1 = dl;
        }

        dh += 8;

        if (ah & 2)
        {
            dl = (int32_t) channel->FMOperator2 + al;

            if (dl < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->_PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

            channel->FMOperator2 = dl;
        }

        dh -= 4;

        if (ah & 4)
        {
            dl = (int32_t) channel->FMOperator3 + al;

            if (dl < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->_PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

            channel->FMOperator3 = dl;
        }

        dh += 8;

        if (ah & 8)
        {
            dl = (int32_t) channel->FMOperator4 + al;

            if (dl < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->_PartMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver._FMSelector + dh), (uint32_t) dl);

            channel->FMOperator4 = dl;
        }
    }

    return si;
}

/// <summary>
/// 6.4. FM FB Setting, Sets the FB (Feedback) value of an FM instrument.
/// </summary>
uint8_t * pmd_driver_t::SetFMFeedbackLoopCommand(channel_t * channel, uint8_t * si)
{
    int32_t dl;

    int32_t dh = 0xB0 + (_Driver._CurrentChannel - 1);   // dh = Algorithm (alg) and Feedback Loops (fb) port address.
    int32_t al = *(int8_t *) si++;

    if (al >= 0)
    {
        // in  al 00000xxx FB to set
        al = ((al << 3) & 0xff) | (al >> 5);

        // in  al 00xxx000 FB to set
        if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000))
        {
            if ((channel->_FMSlotMask & 0x10) == 0)
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

        if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000))
            dl = _Driver._AlgorithmAndFeedbackLoopsFM3;
        else
            dl = channel->AlgorithmAndFeedbackLoops;

        dl = (dl >> 3) & 7;

        if ((al += dl) >= 0)
        {
            if (al >= 8)
            {
                al = 0x38;

                if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000))
                {
                    if ((channel->_FMSlotMask & 0x10) == 0)
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
                if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000))
                {
                    if ((channel->_FMSlotMask & 0x10) == 0)
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

            if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000))
            {
                if ((channel->_FMSlotMask & 0x10) == 0)
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
void pmd_driver_t::InitializeFMInstrument(channel_t * channel, int instrumentNumber, bool setFM3)
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

        if ((_Driver._CurrentChannel == 3) && (_Driver._FMSelector == 0x000))
        {
            if (setFM3)
                instrumentNumber = _Driver._AlgorithmAndFeedbackLoopsFM3;
            else
            {
                if ((channel->_FMSlotMask & 0x10) == 0)
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

        if ((channel->_LFO1Mask & 0x0F) == 0)
            channel->_LFO1Mask = channel->FMCarrier;

        if ((channel->_LFO2Mask & 0x0F) == 0)
            channel->_LFO2Mask = channel->FMCarrier;
    }

    // AH = Mask for Total Level (TL)
    int32_t ah = FMToneCarrier[instrumentNumber + 8]; // Reversed data of slot2/3 (not completed)

    // AL = Mask for other parameters.
    int32_t al = channel->_ToneMask;

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
        // Amplitude Modulation Set (AMS) / Decay Rate (dr)
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
bool pmd_driver_t::SetFMChannelLFOs(channel_t * channel)
{
    if ((_Driver._CurrentChannel != 3) || (_Driver._FMSelector != 0x000))
        return false;

    SetFMChannel3LFOs(channel);

    return true;
}

/// <summary>
/// Sets pitch and volume LFOs for FM channel 3.
/// </summary>
void pmd_driver_t::SetFMChannel3LFOs(channel_t * channel)
{
    int32_t al;

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

    int32_t Mode;

    if ((channel->_FMSlotMask & 0xF0) == 0)
        ClearFM3(Mode, al); // s0
    else
    if (channel->_FMSlotMask != 0xF0)
    {
        _Driver._Slot3Flags |= al;
        Mode = 0x7F;
    }
    else

    if ((channel->_LFO1Mask & 0x0F) == 0)
        ClearFM3(Mode, al);
    else
    if ((channel->_HardwareLFO & 0x01) != 0)
    {
        _Driver._Slot3Flags |= al;
        Mode = 0x7F;
    }
    else

    if ((channel->_LFO2Mask & 0x0F) == 0)
        ClearFM3(Mode, al);
    else
    if (channel->_HardwareLFO & 0x10)
    {
        _Driver._Slot3Flags |= al;
        Mode = 0x7F;
    }
    else
        ClearFM3(Mode, al);

    if ((uint32_t) Mode == _State._FMChannel3Mode)
        return;

    _State._FMChannel3Mode = (uint32_t) Mode;

    _OPNAW->SetReg(0x27, (uint32_t) (Mode & 0xCF)); // Don't reset.

    // When moving to sound effect mode, the pitch is rewritten with the previous FM3 part
    if (Mode == 0x3F || channel == &_FMChannels[2])
        return;

    if (_FMChannels[2]._PartMask == 0x00)
        SetFMPitch(&_FMChannels[2]);

    if (channel == &_FMExtensionChannels[0])
        return;

    if (_FMExtensionChannels[0]._PartMask == 0x00)
        SetFMPitch(&_FMExtensionChannels[0]);

    if (channel == &_FMExtensionChannels[1])
        return;

    if (_FMExtensionChannels[1]._PartMask == 0x00)
        SetFMPitch(&_FMExtensionChannels[1]);
}

/// <summary>
///
/// </summary>
void pmd_driver_t::ClearFM3(int & ah, int & al) noexcept
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
void pmd_driver_t::InitializeFMChannel3(channel_t * channel, uint8_t * data) const noexcept
{
    channel->_Data = data;
    channel->_Size = 1;
    channel->_KeyOffFlag = -1;
    channel->_LFO1DepthSpeed1 = -1; // Infinity
    channel->_LFO1DepthSpeed2 = -1; // Infinity
    channel->_LFO2DepthSpeed1 = -1; // Infinity
    channel->_LFO2DepthSpeed2 = -1; // Infinity
    channel->_Tone = 0xFF;           // Rest
    channel->_DefaultTone = 0xFF;    // Rest
    channel->_Volume = 108;
    channel->_PanAndVolume = _FMChannels[2]._PanAndVolume; // Use FM channel 3 value
    channel->_PartMask |= 0x20;
}

/// <summary>
/// Gets the definition of an FM instrument.
/// </summary>
uint8_t * pmd_driver_t::GetFMInstrumentDefinition(channel_t * channel, int instrumentNumber)
{
    if (_State._InstrumentData == nullptr)
    {
        if (channel != &_SSGEffectChannel)
            return _State._VData + ((size_t) instrumentNumber << 5);
        else
            return _State._EData;
    }

    uint8_t * Data = _State._InstrumentData;

    while (Data[0] != instrumentNumber)
    {
        Data += 26;

        if (Data > _MData + sizeof(_MData) - 26)
            return _State._InstrumentData + 1; // Return the first definition if not found.
    }

    return Data + 1;
}

// Reset the tone of the FM sound source
/// <summary>
///
/// </summary>
void pmd_driver_t::ResetFMInstrument(channel_t * channel)
{
    if (channel->_ToneMask == 0)
        return;

    int32_t s1 = channel->FMOperator1;
    int32_t s2 = channel->FMOperator2;
    int32_t s3 = channel->FMOperator3;
    int32_t s4 = channel->FMOperator4;

    InitializeFMInstrument(channel, channel->InstrumentNumber, true);

    channel->FMOperator1 = s1;
    channel->FMOperator2 = s2;
    channel->FMOperator3 = s3;
    channel->FMOperator4 = s4;

    int32_t dh;

    int32_t al = ((~channel->FMCarrier) & channel->_FMSlotMask) & 0xf0;

    if (al != 0)
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
void pmd_driver_t::SpecialFM3Processing(channel_t * channel, int ax, int cx)
{
    int32_t shiftmask = 0x80;

    int32_t si = cx;

    int32_t bh = ((channel->_LFO1Mask & 0x0F) == 0) ? 0xF0 /* All */ : channel->_LFO1Mask;  // bh=lfo1 mask 4321xxxx
    int32_t ch = ((channel->_LFO2Mask & 0x0F) == 0) ? 0xF0 /* All */ : channel->_LFO2Mask;  // ch=lfo2 mask 4321xxxx

    //  slot  4
    int32_t ax_;

    if (channel->_FMSlotMask & 0x80)
    {
        ax_ = ax;
        ax += _State._FMSlot4Detune;

        if ((bh & shiftmask) && (channel->_HardwareLFO & 0x01))  ax += channel->_LFO1Data;
        if ((ch & shiftmask) && (channel->_HardwareLFO & 0x10))  ax += channel->_LFO2Data;
        shiftmask >>= 1;

        cx = si;

        CalcFMBlock(&cx, &ax);

        ax |= cx;

        _OPNAW->SetReg(0xa6, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa2, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  3
    if (channel->_FMSlotMask & 0x40)
    {
        ax_ = ax;
        ax += _State._FMSlot3Detune;

        if ((bh & shiftmask) && (channel->_HardwareLFO & 0x01))  ax += channel->_LFO1Data;
        if ((ch & shiftmask) && (channel->_HardwareLFO & 0x10))  ax += channel->_LFO2Data;
        shiftmask >>= 1;

        cx = si;

        CalcFMBlock(&cx, &ax);

        ax |= cx;

        _OPNAW->SetReg(0xac, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa8, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  2
    if (channel->_FMSlotMask & 0x20)
    {
        ax_ = ax;
        ax += _State._FMSlot2Detune;

        if ((bh & shiftmask) && (channel->_HardwareLFO & 0x01))
            ax += channel->_LFO1Data;

        if ((ch & shiftmask) && (channel->_HardwareLFO & 0x10))
            ax += channel->_LFO2Data;

        shiftmask >>= 1;

        cx = si;

        CalcFMBlock(&cx, &ax);

        ax |= cx;

        _OPNAW->SetReg(0xae, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xaa, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  1
    if (channel->_FMSlotMask & 0x10)
    {
        ax_ = ax;
        ax += _State._FMSlot1Detune;

        if ((bh & shiftmask) && (channel->_HardwareLFO & 0x01)) 
            ax += channel->_LFO1Data;

        if ((ch & shiftmask) && (channel->_HardwareLFO & 0x10))
            ax += channel->_LFO2Data;

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
void pmd_driver_t::CalcFMBlock(int * cx, int * ax)
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
uint8_t pmd_driver_t::CalcPanOut(channel_t * channel)
{
    int32_t dl = channel->_PanAndVolume;

    if (channel->_HardwareLFODelayCounter)
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
void pmd_driver_t::CalcVolSlot(int dh, int dl, int al)
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
void pmd_driver_t::CalcFMLFO(channel_t *, int al, int bl, uint8_t * vol_tbl)
{
    if (bl & 0x80) vol_tbl[0] = (uint8_t) std::clamp(vol_tbl[0] - al, 0, 255);
    if (bl & 0x40) vol_tbl[1] = (uint8_t) std::clamp(vol_tbl[1] - al, 0, 255);
    if (bl & 0x20) vol_tbl[2] = (uint8_t) std::clamp(vol_tbl[2] - al, 0, 255);
    if (bl & 0x10) vol_tbl[3] = (uint8_t) std::clamp(vol_tbl[3] - al, 0, 255);
}
