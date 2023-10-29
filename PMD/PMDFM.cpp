
// $VER: PMDFM.cpp (2023.10.23) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

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
    {
        channel->KeyOffFlag = 0xFF;
    }
    else
    if ((channel->KeyOffFlag & 0x03) == 0)
    {
        if (channel->Length <= channel->GateTime)
        {
            FMKeyOff(channel);

            channel->KeyOffFlag = 0xFF;
        }
    }

    if (channel->Length == 0)
    {
        if (channel->MuteMask == 0x00)
            channel->ModulationMode &= 0xF7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xDA)
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
                    si = SetFMPortamentoCommand(channel, ++si);

                    _Driver.loop_work &= channel->loopcheck;

                    return;
                }
                else
                if (channel->MuteMask == 0x00)
                {
                    // TONE SET
                    SetFMTone(channel, Transpose(channel, StartLFO(channel, *si++)));

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

                    SetFMVolumeCommand(channel);
                    SetFMPitch(channel);
                    FMKeyOn(channel);

                    channel->KeyOnFlag++;
                    channel->Data = si;

                    _Driver.TieNotesTogether = false;
                    _Driver.IsVolumeBoostSet = 0;

                    // Don't perform Key Off if a "&" command (Tie) follows immediately.
                    channel->KeyOffFlag = (*si == 0xFB) ? 0x02 : 0x00;

                    _Driver.loop_work &= channel->loopcheck;

                    return;
                }
                else
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

        if (channel->SlotDelayCounter)
        {
            if (--channel->SlotDelayCounter == 0)
            {
                if ((channel->KeyOffFlag & 0x01) == 0)
                    FMKeyOn(channel);
            }
        }

        if (channel->ModulationMode)
        {
            _Driver.ModulationMode = channel->ModulationMode & 0x08;

            if (channel->ModulationMode & 0x03)
            {
                if (SetLFO(channel))
                    _Driver.ModulationMode |= (channel->ModulationMode & 0x03);
            }

            if (channel->ModulationMode & 0x30)
            {
                SwapLFO(channel);

                if (SetLFO(channel))
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

                SetFMPitch(channel);
            }

            if (_Driver.ModulationMode & 0x22)
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
    uint8_t Command = *si++;

    switch (Command)
    {
        case 0xFF:
            si = SetFMInstrument(channel, si);
            break;

        // Set Early Key Off Timeout.
        case 0xFE:
            channel->EarlyKeyOffTimeout = *si++;
            channel->EarlyKeyOffTimeoutRandomRange = 0;
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
*/
        // Increase volume by 3dB.
        case 0xF4:
            channel->Volume += 4;

            if (channel->Volume > 127)
                channel->Volume = 127;
            break;

        // Decrease volume by 3dB.
        case 0xF3:
            channel->Volume -= 4;

            if (channel->Volume < 4)
                channel->Volume = 0;
            break;
/*
        case 0xF2:
            si = SetModulation(channel, si);
            break;
*/
        case 0xF1:
            si = SetModulationMask(channel, si);

            SetFMChannel3Mode(channel);
            break;

        case 0xF0: si += 4; break;

        // Set SSG envelope.
        case 0xEF:
            _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + si[0]), si[1]);
            si += 2;
            break;

        case 0xEE: si++; break;
        case 0xED: si++; break;

        case 0xEC:
            si = SetFMPanning(channel, si);
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
*/
        // Set hardware LFO delay.
        case 0xE4:
            channel->HardwareLFODelay = *si++;
            break;

        // Increase volume.
        case 0xE3:
            channel->Volume += *si++;

            if (channel->Volume > 127)
                channel->Volume = 127;
            break;

        // Decrease volume.
        case 0xE2:
            channel->Volume -= *si++;

            if (channel->Volume < 0)
                channel->Volume = 0;
            break;

        // Set hardware LFO AM/FM.
        case 0xE1:
            si = SetHardwareLFOCommand(channel, si);
            break;

        // Set hardware LFO speed.
        case 0xE0:
            _OPNAW->SetReg(0x22, *si++);
            break;
/*
        // Command "Z number": Set ticks per measure.
        case 0xDF:
            _State.BarLength = *si++;
            break;
*/
        case 0xDE:
            si = IncreaseVolumeForNextNote(channel, si, 128);
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
            si = SetFMPortamentoCommand(channel, si);
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
*/

        case 0xCF:
            si = SetFMSlotCommand(channel, si);
            break;
/*
        // Set PCM Repeat.
        case 0xCE: si += 6; break;
*/
        // Set SSG Envelope (Format 2).
        case 0xCD: si += 5; break;
/*
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
        case 0xC9: si++; break;

        case 0xC8:
            si = SetFMAbsoluteDetuneCommand(channel, si);
            break;

        case 0xC7:
            si = SetFMRelativeDetuneCommand(channel, si);
            break;

        case 0xC6:
            si = SetFMChannel3ModeEx(channel, si);
            break;

        case 0xC5:
            si = SetFMVolumeMaskSlotCommand(channel, si);
            break;
/*
        // Set Early Key Off Timeout Percentage. Stops note (length * pp / 100h) ticks early, added to value of command FE.
        case 0xC4:
            channel->EarlyKeyOffTimeoutPercentage = *si++;
            break;
*/
        case 0xC3:
            si = SetFMPanningExtend(channel, si);
            break;
/*
        case 0xC2:
            channel->Delay1 = channel->Delay2 = *si++;
            InitializeLFOMain(channel);
            break;
*/
        case 0xC1: break;

        case 0xC0:
            si = SetFMMaskCommand(channel, si);
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

            SetFMChannel3Mode(channel);
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
*/
        case 0xB8:
            si = SetFMTLSettingCommand(channel, si);
            break;
/*
        case 0xB7:
            si = SetMDepthCountCommand(channel, si);
            break;
*/
        case 0xB6:
            si = SetFMFeedbackLoops(channel, si);
            break;

        case 0xB5:
            channel->SlotDelayMask = (~(*si++) << 4) & 0xF0;
            channel->SlotDelayCounter = channel->SlotDelay = *si++;
            break;
/*
        case 0xB4: si += 16; break;

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
/// Sets FM Wait after register output.
/// </summary>
void PMD::SetFMDelay(int nsec)
{
    _OPNAW->SetFMDelay(nsec);
}

#pragma region(Commands)
/// <summary>
///
/// </summary>
void PMD::SetFMTone(Channel * channel, int tone)
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

        if ((channel->ModulationMode & 0x11) == 0)
            channel->Factor = 0; // Don't use LFO pitch.
    }
}

/// <summary>
///
/// </summary>
void PMD::SetFMVolumeCommand(Channel * channel)
{
    if (channel->FMSlotMask == 0)
        return;

    int cl = (channel->VolumeBoost) ? channel->VolumeBoost - 1 : channel->Volume;

    if (channel != &_EffectChannel)
    {
        // Calculate volume down.
        if (_State.FMVolumeDown)
            cl = ((256 - _State.FMVolumeDown) * cl) >> 8;

        // Calculate fade out.
        if (_State.FadeOutVolume >= 2)
            cl = ((256 - (_State.FadeOutVolume >> 1)) * cl) >> 8;
    }

    // Set volume to carrier & volume LFO processing.
    int bh = 0;          // Vol Slot Mask
    int bl = channel->FMSlotMask;

    uint8_t SlotVolume[4] = { 0x80, 0x80, 0x80, 0x80 };

    cl = 255 - cl;      // Volume set to cl=carrier+80H(add)
    bl &= channel->FMCarrier;    // bl=Set volume SLOT xxxx0000b
    bh |= bl;

    if (bl & 0x80) SlotVolume[0] = (uint8_t) cl;
    if (bl & 0x40) SlotVolume[1] = (uint8_t) cl;
    if (bl & 0x20) SlotVolume[2] = (uint8_t) cl;
    if (bl & 0x10) SlotVolume[3] = (uint8_t) cl;

    if (cl != 255)
    {
        if (channel->ModulationMode & 0x02)
        {
            bl = channel->VolumeMask1;
            bl &= channel->FMSlotMask;    // bl=SLOT to set volume LFO xxxx0000b
            bh |= bl;

            CalcFMLFO(channel, channel->LFO1Data, bl, SlotVolume);
        }

        if (channel->ModulationMode & 0x20)
        {
            bl = channel->VolumeMask2;
            bl &= channel->FMSlotMask;
            bh |= bl;

            CalcFMLFO(channel, channel->LFO2Data, bl, SlotVolume);
        }
    }

    int dh = 0x4c - 1 + _Driver.CurrentChannel; // FM Port Address

    if (bh & 0x80) CalcVolSlot(dh,      channel->FMOperator4, SlotVolume[0]);
    if (bh & 0x40) CalcVolSlot(dh -  8, channel->FMOperator3, SlotVolume[1]);
    if (bh & 0x20) CalcVolSlot(dh -  4, channel->FMOperator2, SlotVolume[2]);
    if (bh & 0x10) CalcVolSlot(dh - 12, channel->FMOperator1, SlotVolume[3]);
}

/// <summary>
///
/// </summary>
void PMD::SetFMPitch(Channel * channel)
{
    if ((channel->Factor == 0) || (channel->FMSlotMask == 0))
        return;

    int Block = (int) (channel->Factor  & 0x3800);
    int Pitch = (int) (channel->Factor) & 0x07ff;

    // Portament/LFO/Detune SET
    Pitch += channel->Portamento + channel->DetuneValue;

    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0) && (_State.FMChannel3Mode != 0x3F))
        SpecialFM3Processing(channel, Pitch, Block);
    else
    {
        if (channel->ModulationMode & 0x01)
            Pitch += channel->LFO1Data;

        if (channel->ModulationMode & 0x10)
            Pitch += channel->LFO2Data;

        CalcFMBlock(&Block, &Pitch);

        Pitch |= Block;

        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xa4 - 1), (uint32_t) HIBYTE(Pitch));
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xa4 - 5), (uint32_t) LOBYTE(Pitch));
    }
}

/// <summary>
///
/// </summary>
void PMD::FMKeyOn(Channel * channel)
{
    if (channel->Tone == 0xFF)
        return;

    if (_Driver.FMSelector == 0)
    {
        int al = _Driver.omote_key[_Driver.CurrentChannel - 1] | channel->FMSlotMask;

        if (channel->SlotDelayCounter)
            al &= channel->SlotDelayMask;

        _Driver.omote_key[_Driver.CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) ((_Driver.CurrentChannel - 1) | al));
    }
    else
    {
        int al = _Driver.ura_key[_Driver.CurrentChannel - 1] | channel->FMSlotMask;

        if (channel->SlotDelayCounter)
            al &= channel->SlotDelayMask;

        _Driver.ura_key[_Driver.CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) (((_Driver.CurrentChannel - 1) | al) | 4));
    }
}

/// <summary>
///
/// </summary>
void PMD::FMKeyOff(Channel * channel)
{
    if (channel->Tone == 0xFF)
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

/// <summary>
/// Command "@ number": Sets the instrument to be used. Range 0-255.
/// </summary>
uint8_t * PMD::SetFMInstrument(Channel * channel, uint8_t * si)
{
    int al = *si++;
    int dl = al;

    channel->InstrumentNumber = al;

    if (channel->MuteMask == 0x00)
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

    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0) && (channel->ToneMask != 0))
    {
        if ((channel->FMSlotMask & 0x10) == 0)
        {
            al = _Driver.AlgorithmAndFeedbackLoopsFM3 & 0x38; // Feedback Loops (fb) uses previous value.
            dl = (dl & 0x07) | al;
        }

        _Driver.AlgorithmAndFeedbackLoopsFM3 = dl;
        channel->AlgorithmAndFeedbackLoops = al;
    }

    return si;
}

/// <summary>
/// Command "p <value>" (1: right, 2: left, 3: center (default))
/// </summary>
uint8_t * PMD::SetFMPanning(Channel * channel, uint8_t * si)
{
    SetFMPannningInternal(channel, *si++);

    return si;
}

/// <summary>
/// Command "px <value 1>, <value 2>" (value 1: -4 (pan to the left) to +4 (pan to the right), value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * PMD::SetFMPanningExtend(Channel * channel, uint8_t * si)
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
void PMD::SetFMPannningInternal(Channel * channel, int value)
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

/// <summary>
///
/// </summary>
uint8_t * PMD::DecreaseFMVolumeCommand(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.FMVolumeDown = Limit(al + _State.FMVolumeDown, 255, 0);
    else
        _State.FMVolumeDown = _State.DefaultFMVolumeDown;

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::SetFMPortamentoCommand(Channel * channel, uint8_t * si)
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

    SetFMVolumeCommand(channel);
    SetFMPitch(channel);
    FMKeyOn(channel);

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
///
/// </summary>
uint8_t * PMD::SetFMEffect(Channel *, uint8_t * si)
{
    return si + 1;
}

/// <summary>
///
/// </summary>
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
/// <summary>
///
/// </summary>
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
/// <summary>
///
/// </summary>
uint8_t * PMD::SetFMSlotCommand(Channel * channel, uint8_t * si)
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

        if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
        {
            bl = _Driver.AlgorithmAndFeedbackLoopsFM3;
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
            channel->MuteMask &= 0xDF;  // Unmask part when other than s0
        else
            channel->MuteMask |= 0x20;  // Part mask at s0

        if (SetFMChannel3Mode(channel))
        {
            if (channel != &_FMChannel[2])
            {
                if (_FMChannel[2].MuteMask == 0x00 && (_FMChannel[2].KeyOffFlag & 0x01) == 0)
                    FMKeyOn(&_FMChannel[2]);

                if (channel != &_FMExtensionChannel[0])
                {
                    if (_FMExtensionChannel[0].MuteMask == 0x00 && (_FMExtensionChannel[0].KeyOffFlag & 0x01) == 0)
                        FMKeyOn(&_FMExtensionChannel[0]);

                    if (channel != &_FMExtensionChannel[1])
                    {
                        if (_FMExtensionChannel[1].MuteMask == 0x00 && (_FMExtensionChannel[1].KeyOffFlag & 0x01) == 0)
                            FMKeyOn(&_FMExtensionChannel[1]);
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
uint8_t * PMD::SetHardwareLFOCommand(Channel * channel, uint8_t * si)
{
    channel->PanAndVolume = (channel->PanAndVolume & 0xC0) | *si++;

    // Part_e is impossible because it is only for YM2608. For FM3, set all four parts
    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
    {
        _FMChannel[2].PanAndVolume = channel->PanAndVolume;

        _FMExtensionChannel[0].PanAndVolume = channel->PanAndVolume;
        _FMExtensionChannel[1].PanAndVolume = channel->PanAndVolume;
        _FMExtensionChannel[2].PanAndVolume = channel->PanAndVolume;
    }

    if (channel->MuteMask == 0x00)
        _OPNAW->SetReg((uint32_t) (0xB4 + _Driver.FMSelector + _Driver.CurrentChannel - 1), CalcPanOut(channel));

    return si;
}

// Command "D number": Set the detune (frequency shift) value. Range -32768-32767.
/// <summary>
///
/// </summary>
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
/// <summary>
///
/// </summary>
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
/// <summary>
///
/// </summary>
uint8_t * PMD::SetFMVolumeMaskSlotCommand(Channel * channel, uint8_t * si)
{
    int al = *si++ & 0x0F;

    if (al != 0)
    {
        al = (al << 4) | 0x0f;

        channel->VolumeMask1 = al;
    }
    else
        channel->VolumeMask1 = channel->FMCarrier;

    SetFMChannel3Mode(channel);

    return si;
}

/// <summary>
///
/// </summary>
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
            channel->FMOperator1 = dl;

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
        }

        dh += 8;

        if (ah & 2)
        {
            channel->FMOperator2 = dl;

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
        }

        dh -= 4;

        if (ah & 4)
        {
            channel->FMOperator3 = dl;

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);
        }

        dh += 8;

        if (ah & 8)
        {
            channel->FMOperator4 = dl;

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
            if ((dl = (int) channel->FMOperator1 + al) < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

            channel->FMOperator1 = dl;
        }

        dh += 8;

        if (ah & 2)
        {
            if ((dl = (int) channel->FMOperator2 + al) < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

            channel->FMOperator2 = dl;
        }

        dh -= 4;

        if (ah & 4)
        {
            if ((dl = (int) channel->FMOperator3 + al) < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

            channel->FMOperator3 = dl;
        }

        dh += 8;

        if (ah & 8)
        {
            if ((dl = (int) channel->FMOperator4 + al) < 0)
            {
                dl = 0;

                if (al >= 0)
                    dl = 127;
            }

            if (channel->MuteMask == 0x00)
                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

            channel->FMOperator4 = dl;
        }
    }

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::SetFMFeedbackLoops(Channel * channel, uint8_t * si)
{
    int dl;

    int dh = _Driver.CurrentChannel + 0xb0 - 1;  // dh = Algorithm (alg) and Feedback Loops (fb) port address.
    int al = *(int8_t *) si++;

    if (al >= 0)
    {
        // in  al 00000xxx FB to set
        al = ((al << 3) & 0xff) | (al >> 5);

        // in  al 00xxx000 FB to set
        if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
        {
            if ((channel->FMSlotMask & 0x10) == 0)
                return si;

            dl = (_Driver.AlgorithmAndFeedbackLoopsFM3 & 7) | al;

            _Driver.AlgorithmAndFeedbackLoopsFM3 = dl;
        }
        else
            dl = (channel->AlgorithmAndFeedbackLoops & 7) | al;

        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

        channel->AlgorithmAndFeedbackLoops = dl;

        return si;
    }
    else
    {
        if ((al & 0x40) == 0)
            al &= 7;

        if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
            dl = _Driver.AlgorithmAndFeedbackLoopsFM3;
        else
            dl = channel->AlgorithmAndFeedbackLoops;

        dl = (dl >> 3) & 7;

        if ((al += dl) >= 0)
        {
            if (al >= 8)
            {
                al = 0x38;

                if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
                {
                    if ((channel->FMSlotMask & 0x10) == 0)
                        return si;

                    dl = (_Driver.AlgorithmAndFeedbackLoopsFM3 & 7) | al;

                    _Driver.AlgorithmAndFeedbackLoopsFM3 = dl;
                }
                else
                    dl = (channel->AlgorithmAndFeedbackLoops & 7) | al;

                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

                channel->AlgorithmAndFeedbackLoops = dl;

                return si;
            }
            else
            {
                // in al 00000xxx FB to set
                al = ((al << 3) & 0xff) | (al >> 5);

                // in al 00xxx000 FB to set
                if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
                {
                    if ((channel->FMSlotMask & 0x10) == 0)
                        return si;

                    dl = (_Driver.AlgorithmAndFeedbackLoopsFM3 & 7) | al;

                    _Driver.AlgorithmAndFeedbackLoopsFM3 = dl;
                }
                else
                    dl = (channel->AlgorithmAndFeedbackLoops & 7) | al;

                _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

                channel->AlgorithmAndFeedbackLoops = dl;

                return si;
            }
        }
        else
        {
            al = 0;

            if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
            {
                if ((channel->FMSlotMask & 0x10) == 0)
                    return si;

                dl = (_Driver.AlgorithmAndFeedbackLoopsFM3 & 7) | al;

                _Driver.AlgorithmAndFeedbackLoopsFM3 = dl;
            }
            else
                dl = (channel->AlgorithmAndFeedbackLoops & 7) | al;

            _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) dl);

            channel->AlgorithmAndFeedbackLoops = dl;

            return si;
        }
    }
}
#pragma endregion

/// <summary>
/// Initializes the FM operators.
/// </summary>
void PMD::InitializeFMInstrument(Channel * channel, int instrumentNumber, bool setFM3)
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

        if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
        {
            if (setFM3)
                instrumentNumber = _Driver.AlgorithmAndFeedbackLoopsFM3;
            else
            {
                if ((channel->FMSlotMask & 0x10) == 0)
                    instrumentNumber = (_Driver.AlgorithmAndFeedbackLoopsFM3 & 0x38) | (instrumentNumber & 7);

                _Driver.AlgorithmAndFeedbackLoopsFM3 = instrumentNumber;
            }
        }
    }

    uint32_t Register = (uint32_t) (0xB0 - 1 + _Driver.FMSelector + _Driver.CurrentChannel);

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

    // AH = Mask for Total Level (tl)
    int ah = FMToneCarrier[instrumentNumber + 8]; // Reversed data of slot2/3 (not completed)

    // AL = Mask for other parameters.
    int al = channel->ToneMask;

    ah &= al;

    // Set each tone parameter (TL is modulator only)
    Register = (uint32_t) (0x30 - 1 + _Driver.FMSelector + _Driver.CurrentChannel);

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

    // Store the Total Level (tl) in each operator slot.
    Data -= 20;

    channel->FMOperator1 = Data[0];
    channel->FMOperator3 = Data[1];
    channel->FMOperator2 = Data[2];
    channel->FMOperator4 = Data[3];
}

/// <summary>
///
/// </summary>
bool PMD::SetFMChannel3Mode(Channel * channel)
{
    if ((_Driver.CurrentChannel == 3) && (_Driver.FMSelector == 0))
    {
        SetFMChannel3Mode2(channel);

        return true;
    }

    return false;
}

/// <summary>
///
/// </summary>
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

    int Mode;

    if ((channel->FMSlotMask & 0xF0) == 0)
        ClearFM3(Mode, al); // s0
    else
    if (channel->FMSlotMask != 0xF0)
    {
        _Driver.slot3_flag |= al;
        Mode = 0x7F;
    }
    else

    if ((channel->VolumeMask1 & 0x0F) == 0)
        ClearFM3(Mode, al);
    else
    if ((channel->ModulationMode & 0x01) != 0)
    {
        _Driver.slot3_flag |= al;
        Mode = 0x7F;
    }
    else

    if ((channel->VolumeMask2 & 0x0F) == 0)
        ClearFM3(Mode, al);
    else
    if (channel->ModulationMode & 0x10)
    {
        _Driver.slot3_flag |= al;
        Mode = 0x7F;
    }
    else
        ClearFM3(Mode, al);

    if ((uint32_t) Mode == _State.FMChannel3Mode)
        return;

    _State.FMChannel3Mode = (uint32_t) Mode;

    _OPNAW->SetReg(0x27, (uint32_t) (Mode & 0xCF)); // Don't reset.

    // When moving to sound effect mode, the pitch is rewritten with the previous FM3 part
    if (Mode == 0x3F || channel == &_FMChannel[2])
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

/// <summary>
///
/// </summary>
void PMD::ClearFM3(int& ah, int& al)
{
    al ^= 0xFF;

    if ((_Driver.slot3_flag &= al) == 0)
        ah = (_Driver.slotdetune_flag != 1) ? 0x3F : 0x7F;
    else
        ah = 0x7F;
}

/// <summary>
///
/// </summary>
void PMD::InitializeFMChannel3(Channel * channel, uint8_t * ax)
{
    channel->Data = ax;
    channel->Length = 1;
    channel->KeyOffFlag = 0xFF;
    channel->LFO1MDepthCount1 = -1; // Infinity
    channel->LFO1MDepthCount2 = -1; // Infinity
    channel->LFO2MDepthCount1 = -1; // Infinity
    channel->LFO2MDepthCount2 = -1; // Infinity
    channel->Tone = 0xFF;           // Rest
    channel->DefaultTone = 0xFF;    // Rest
    channel->Volume = 108;
    channel->PanAndVolume = _FMChannel[2].PanAndVolume; // Use FM channel 3 value
    channel->MuteMask |= 0x20;
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
void PMD::ResetFMInstrument(Channel * channel)
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
/// <summary>
///
/// </summary>
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

        if ((bh & shiftmask) && (channel->ModulationMode & 0x01))  ax += channel->LFO1Data;
        if ((ch & shiftmask) && (channel->ModulationMode & 0x10))  ax += channel->LFO2Data;
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

        if ((bh & shiftmask) && (channel->ModulationMode & 0x01))  ax += channel->LFO1Data;
        if ((ch & shiftmask) && (channel->ModulationMode & 0x10))  ax += channel->LFO2Data;
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

        if ((bh & shiftmask) && (channel->ModulationMode & 0x01))
            ax += channel->LFO1Data;

        if ((ch & shiftmask) && (channel->ModulationMode & 0x10))
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
        ax += _State.slot_detune1;

        if ((bh & shiftmask) && (channel->ModulationMode & 0x01)) 
            ax += channel->LFO1Data;

        if ((ch & shiftmask) && (channel->ModulationMode & 0x10))
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
uint8_t PMD::CalcPanOut(Channel * channel)
{
    int  dl = channel->PanAndVolume;

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
    if ((al += dl) > 255)
        al = 255;
    
    if ((al -= 0x80) < 0)
        al = 0;

    _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) al);
}

/// <summary>
///
/// </summary>
void PMD::CalcFMLFO(Channel *, int al, int bl, uint8_t * vol_tbl)
{
    if (bl & 0x80) vol_tbl[0] = (uint8_t) Limit(vol_tbl[0] - al, 255, 0);
    if (bl & 0x40) vol_tbl[1] = (uint8_t) Limit(vol_tbl[1] - al, 255, 0);
    if (bl & 0x20) vol_tbl[2] = (uint8_t) Limit(vol_tbl[2] - al, 255, 0);
    if (bl & 0x10) vol_tbl[3] = (uint8_t) Limit(vol_tbl[3] - al, 255, 0);
}
