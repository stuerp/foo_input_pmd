
// $VER: PMDPPZ.cpp (2023.10.23) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

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
        if (channel->Length <= channel->GateTime)
        {
            PPZKeyOff(channel);

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
                si = ExecutePPZCommand(channel, si);
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
                    {
                        break;
                    }
                }

                si = channel->LoopData;

                channel->loopcheck = 1;
            }
            else
            {
                if (*si == 0xDA)
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

                    _Driver.TieNotesTogether = false;
                    _Driver.IsVolumeBoostSet = 0;
                    break;
                }

                //  TONE SET
                SetPPZTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

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

                SetPPZVolume(channel);
                SetPPZPitch(channel);

                if (channel->KeyOffFlag & 0x01)
                    PPZKeyOn(channel);

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

    if (channel->ModulationMode != 0x00)
    {
        if (channel->ModulationMode & 0x03)
        {
            if (SetLFO(channel))
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
    uint8_t Command = *si++;

    switch (Command)
    {
        case 0xFF:
            si = SetPPZInstrument(channel, si);
            break;

        // Set early Key Off Timeout
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
*/
        // Set SSG envelope.
        case 0xEF:
            _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + si[0]), si[1]);
            si += 2;
            break;

        case 0xEE: si++; break;
        case 0xED: si++; break;

        case 0xEC:
            si = SetPPZPanning(channel, si);
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
            si = SetPPZPortamentoCommand(channel, si);
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
            si = SetPPZRepeatCommand(channel, si);
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
            si = SetPPZPanningExtend(channel, si);
            break;
/*
        case 0xC2:
            channel->Delay1 = channel->Delay2 = *si++;
            InitializeLFOMain(channel);
            break;
*/
        case 0xC1: break;

        case 0xC0:
            si = SetPPZMaskCommand(channel, si);
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
*
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

#pragma region(Commands)
/// <summary>
///
/// </summary>
void PMD::SetPPZTone(Channel * channel, int tone)
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

        if ((channel->ModulationMode & 0x11) == 0)
            channel->Factor = 0; // Don't use LFO pitch.
    }
}

/// <summary>
///
/// </summary>
void PMD::SetPPZVolume(Channel * channel)
{
    int al = channel->VolumeBoost ? channel->VolumeBoost : channel->Volume;

    // Calculate volume down.
    al = ((256 - _State.PPZVolumeAdjust) * al) >> 8;

    // Calculate fade out.
    if (_State.FadeOutVolume != 0)
        al = ((256 - _State.FadeOutVolume) * al) >> 8;

    if (al == 0)
    {
        _PPZ->SetVolume((size_t) _Driver.CurrentChannel, 0);
        _PPZ->Stop((size_t) _Driver.CurrentChannel);

        return;
    }

    // Calculate envelope.
    if (channel->SSGEnvelopFlag == -1)
    {
        // Extended version: Volume = al * (eenv_vol + 1) / 16
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
        // Extended envelope volume
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
        int dx = (channel->ModulationMode & 0x02) ? channel->LFO1Data : 0;

        if (channel->ModulationMode & 0x20)
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
        _PPZ->SetVolume((size_t) _Driver.CurrentChannel, al >> 4);
    else
        _PPZ->Stop((size_t) _Driver.CurrentChannel);
}

/// <summary>
///
/// </summary>
void PMD::SetPPZPitch(Channel * channel)
{
    uint32_t Pitch = channel->Factor;

    if (Pitch == 0)
        return;

    Pitch += channel->Portamento * 16;

    {
        int ax = (channel->ModulationMode & 0x01) ? channel->LFO1Data : 0;

        if (channel->ModulationMode & 0x10)
            ax += channel->LFO2Data;

        ax += channel->DetuneValue;

        int64_t cx2 = Pitch + ((int64_t) Pitch) / 256 * ax;

        if (cx2 > 0xffffffff)
            Pitch = 0xffffffff;
        else
        if (cx2 < 0)
            Pitch = 0;
        else
            Pitch = (uint32_t) cx2;
    }

    _PPZ->SetPitch((size_t) _Driver.CurrentChannel, Pitch);
}

/// <summary>
///
/// </summary>
void PMD::PPZKeyOn(Channel * channel)
{
    if (channel->Tone == 0xFF)
        return;

    if ((channel->InstrumentNumber & 0x80) == 0)
        _PPZ->Play((size_t) _Driver.CurrentChannel, 0, channel->InstrumentNumber,        0, 0);
    else
        _PPZ->Play((size_t) _Driver.CurrentChannel, 1, channel->InstrumentNumber & 0x7F, 0, 0);
}

/// <summary>
///
/// </summary>
void PMD::PPZKeyOff(Channel * channel)
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
uint8_t * PMD::SetPPZInstrument(Channel * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    if ((channel->InstrumentNumber & 0x80) == 0)
        _PPZ->SetInstrument((size_t) _Driver.CurrentChannel, 0, (size_t) channel->InstrumentNumber);
    else
        _PPZ->SetInstrument((size_t) _Driver.CurrentChannel, 1, (size_t) (channel->InstrumentNumber & 0x7F));

    return si;
}

/// <summary>
/// Command "p <value>" (1: right, 2: left, 3: center (default))
/// </summary>
uint8_t * PMD::SetPPZPanning(Channel * channel, uint8_t * si)
{
    static const int PanValues[4] = { 0, 9, 1, 5 }; // { Left, Right, Leftwards, Rightwards }

    channel->PanAndVolume = PanValues[*si++];

    _PPZ->SetPan((size_t) _Driver.CurrentChannel, channel->PanAndVolume);

    return si;
}

/// <summary>
/// Command "px <value 1>, <value 2>" (value 1: -128 to -4 (Pan to the left), -3 to -1 (Leftwards), 0 (Center), 1 to 3 (Rightwards), 4 to 127 (Pan to the right), value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * PMD::SetPPZPanningExtend(Channel * channel, uint8_t * si)
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
#pragma endregion

/// <summary>
/// Command "{interval1 interval2} [length1] [.] [,length2]"
/// </summary>
uint8_t * PMD::SetPPZPortamentoCommand(Channel * channel, uint8_t * si)
{
    if (channel->MuteMask)
    {
        // Set to 'rest'.
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

    channel->Length = *si++;

    si = CalculateQ(channel, si);

    channel->PortamentoQuotient = ax / channel->Length;
    channel->PortamentoRemainder = ax % channel->Length;
    channel->ModulationMode |= 0x08; // Enable portamento.

    if ((channel->VolumeBoost != 0) && (channel->Tone != 0xFF))
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
        PPZKeyOn(channel);

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

/// <summary>
/// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// </summary>
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

/// <summary>
///
/// </summary>
uint8_t * PMD::InitializePPZ(Channel *, uint8_t * si)
{
    for (size_t i = 0; i < _countof(_PPZChannel); ++i)
    {
        int16_t Offset = *(int16_t *) si;

        if (Offset != 0)
        {
            _PPZChannel[i].Data = &_State.MData[Offset];
            _PPZChannel[i].Length = 1;
            _PPZChannel[i].KeyOffFlag = 0xFF;
            _PPZChannel[i].LFO1MDepthCount1 = -1;        // LFO1MDepth Counter (Infinite)
            _PPZChannel[i].LFO1MDepthCount2 = -1;
            _PPZChannel[i].LFO2MDepthCount1 = -1;
            _PPZChannel[i].LFO2MDepthCount2 = -1;
            _PPZChannel[i].Tone = 0xFF;         // Rest
            _PPZChannel[i].DefaultTone = 0xFF;  // Rest
            _PPZChannel[i].Volume = 128;
            _PPZChannel[i].PanAndVolume = 5;    // Center
        }

        si += 2;
    }

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::DecreasePPZVolumeCommand(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.PPZVolumeAdjust = Limit(al + _State.PPZVolumeAdjust, 255, 0);
    else
        _State.PPZVolumeAdjust = _State.DefaultPPZVolumeAdjust;

    return si;
}
