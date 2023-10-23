
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

void PMD::PCM86Main(Channel * channel)
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
            SetP86KeyOff(channel);

            channel->KeyOffFlag = 0xFF;
        }
    }

    if (channel->Length == 0)
    {
        while (1)
        {
//          if (*si > 0x80 && *si != 0xda) {
            if (*si > 0x80)
            {
                si = ExecuteP86Command(channel, si);
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
                if (channel->MuteMask)
                {
/*
                    si++;

                    // Set to "rest".
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

                SetP86Tone(channel, oshift(channel, StartPCMLFO(channel, *si++)));

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

                SetPCM86Volume(channel);
                SetPCM86Pitch(channel);

                if (channel->KeyOffFlag & 0x01)
                    SetP86KeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

                _Driver.TieMode = 0;
                _Driver.IsVolumeBoostSet = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->KeyOffFlag = (*si == 0xFB) ? 0x02: 0x00;

                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    if (channel->ModulationMode & 0x22)
    {
        _Driver.ModulationMode = 0;

        if (channel->ModulationMode & 0x02)
        {
            lfo(channel);

            _Driver.ModulationMode |= (channel->ModulationMode & 0x02);
        }

        if (channel->ModulationMode & 0x20)
        {
            SwapLFO(channel);

            if (lfo(channel))
            {
                SwapLFO(channel);

                _Driver.ModulationMode |= (channel->ModulationMode & 0x20);
            }
            else
                SwapLFO(channel);
        }

        int temp = SSGPCMSoftwareEnvelope(channel);

        if (temp || _Driver.ModulationMode & 0x22 || _State.FadeOutSpeed)
            SetPCM86Volume(channel);
    }
    else
    {
        int temp = SSGPCMSoftwareEnvelope(channel);

        if (temp || _State.FadeOutSpeed)
            SetPCM86Volume(channel);
    }

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecuteP86Command(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xFF:
            si = SetP86InstrumentCommand(channel, si);
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

        // Set SSG envelope.
        case 0xEF:
            _OPNAW->SetReg((uint32_t) (0x100 + si[0]), (uint32_t) si[1]);
            si += 2;
            break;

        case 0xEE: si++; break;
        case 0xED: si++; break;

        case 0xEC:
            si = SetP86PanningCommand(channel, si);
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

        case 0xda: si++; break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        // Command "MD", "MDA", "MDB": Set LFO Depth Temporal Change
        case 0xd6:
            channel->MDepthSpeedA = channel->MDepthSpeedB = *si++;
            channel->MDepth = *(int8_t *) si++;
            break;

        case 0xd5:
            channel->DetuneValue += *(int16_t *) si;
            si += 2;
            break;

        case 0xd4: si = SetSSGEffect(channel, si); break;
        case 0xd3: si = SetFMEffect(channel, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si++; break;
        case 0xcf: si++; break;

        case 0xce: si = SetP86RepeatCommand(channel, si); break;
        case 0xcd: si = SetSSGEnvelopeFormat2Command(channel, si); break;
        case 0xcc: si++; break;
        case 0xcb:
            channel->LFOWaveform = *si++;
            break;

        case 0xca:
            channel->extendmode = (channel->extendmode & 0xFD) | ((*si++ & 0x01) << 1);
            break;

        case 0xc9:
            channel->extendmode = (channel->extendmode & 0xFB) | ((*si++ & 0x01) << 2);
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
            si = SetP86PanValueExtendedCommand(channel, si);
            break;

        case 0xc2:
            channel->delay = channel->delay2 = *si++;
            lfoinit_main(channel);
            break;

        case 0xc1: break;

        case 0xC0:
            si = SetP86MaskCommand(channel, si);
            break;

        case 0xbf: si += 4; break;
        case 0xbe: si++; break;
        case 0xbd: si += 2; break;
        case 0xbc: si++; break;
        case 0xbb: si++; break;
        case 0xba: si++; break;
        case 0xb9: si++; break;
        case 0xb8: si += 2; break;
        case 0xb7:
            si = SetMDepthCountCommand(channel, si);
            break;

        case 0xb6: si++; break;
        case 0xb5: si += 2; break;

        case 0xb4:
            si = InitializePPZ(channel, si);
            break;

        case 0xb3:
            channel->qdat2 = *si++;
            break;

        case 0xb2:
            channel->shift_def = *(int8_t *) si++;
            break;

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
uint8_t * PMD::SetP86InstrumentCommand(Channel * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    _P86->SelectSample(channel->InstrumentNumber);

    return si;
}
#pragma endregion

void PMD::SetP86Tone(Channel * channel, int al)
{
    int ah = al & 0x0F;

    if (ah != 0x0F)
    {
        // Music Note
        if (_State.PMDB2CompatibilityMode && (al >= 0x65))
        {
            al = (ah < 5) ? 0x60 /* o7 */ : 0x50 /* o6 */;

            al |= ah;
        }

        channel->Note = al;

        int bl = ((al & 0xF0) >> 4) * 12 + ah;

        channel->fnum = p86_tune_data[bl];
    }
    else
    {
        // Rest
        channel->Note = 0xFF;

        if ((channel->ModulationMode & 0x11) == 0)
            channel->fnum = 0; // Don't use LFO pitch.
    }
}

void PMD::SetPCM86Volume(Channel * channel)
{
    int al = channel->VolumeBoost ? channel->VolumeBoost : channel->Volume;

    //  Calculate Volume Down
    al = ((256 - _State.ADPCMVolumeDown) * al) >> 8;

    //  Calculate Fade Out
    if (_State.FadeOutVolume != 0)
        al = ((256 - _State.FadeOutVolume) * al) >> 8;

    if (al == 0)
    {
        _OPNAW->SetReg(0x10b, 0);

        return;
    }

    //  Calculate Envelope.
    if (channel->envf == -1)
    {
        // Extended Envelope Volume
        if (channel->ExtendedAttackLevel == 0)
        {
            _OPNAW->SetReg(0x10b, 0);

            return;
        }

        al = ((((al * (channel->ExtendedAttackLevel + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        // Extended Envelope Volume
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

    // Calculate Volume LFO.
    int dx = (channel->ModulationMode & 0x02) ? channel->LFODat1 : 0;

    if (channel->ModulationMode & 0x20)
        dx += channel->LFODat2;

    if (dx >= 0)
    {
        if ((al += dx) > 255)
            al = 255;
    }
    else
    {
        if ((al += dx) < 0)
            al = 0;
    }

    if (!_State.PMDB2CompatibilityMode)
        al >>= 4;
    else
        al = (int) ::sqrt(al); // Make the volume NEC Speaker Board-compatible.

    _P86->SelectVolume(al);
}

void PMD::SetPCM86Pitch(Channel * channel)
{
    if (channel->fnum == 0)
        return;

    int bl = (int) ((channel->fnum & 0x0e00000) >> (16 + 5));
    int cx = (int) ( channel->fnum & 0x01fffff);

    if (!_State.PMDB2CompatibilityMode && (channel->DetuneValue != 0))
        cx = Limit((cx >> 5) + channel->DetuneValue, 65535, 1) << 5;

    _P86->SetPitch(bl, (uint32_t) cx);
}

void PMD::SetP86KeyOn(Channel * channel)
{
    if (channel->Note == 0xFF)
        return;

    _P86->Play();
}

void PMD::SetP86KeyOff(Channel * channel)
{
    _P86->Keyoff();

    if (channel->envf != -1)
    {
        if (channel->envf != 2)
            SetSSGKeyOff(channel);

        return;
    }

    if (channel->ExtendedCount != 4)
        SetSSGKeyOff(channel);
}

// Command "p <value>" (1: right, 2: left, 3: center (default), 0: Reverse Phase)
uint8_t * PMD::SetP86PanningCommand(Channel *, uint8_t * si)
{
    switch (*si++)
    {
        case 1: // Right
            _P86->SetPan(2, 1);
            break;

        case 2: // Left
            _P86->SetPan(1, 0);
            break;

        case 3: // Center
            _P86->SetPan(3, 0);
            break;

        default: // Reverse Phase
            _P86->SetPan(3 | 4, 0);
    }

    return si;
}

// Command "px <value 1>, <value 2>" (value 1: < 0 (Pan to the right), > 0 (Pan to the left), 0 (Center), value 2: 0 (In phase) or 1 (Reverse phase)).
uint8_t * PMD::SetP86PanValueExtendedCommand(Channel * channel, uint8_t * si)
{
    int flag, value;

    channel->PanAndVolume = (int8_t) *si++;
    bool ReversePhase = (*si++ == 1);

    if (channel->PanAndVolume == 0)
    {
        flag = 3; // Center
        value = 0;
    }
    else
    if (channel->PanAndVolume > 0)
    {
        flag = 2; // Right
        value = 128 - channel->PanAndVolume;
    }
    else
    {
        flag = 1; // Left
        value = 128 + channel->PanAndVolume;
    }

    if (ReversePhase != 1)
        flag |= 4; // Reverse the phase

    _P86->SetPan(flag, value);

    return si;
}

// Command "@[@] insnum[,number1[,number2[,number3]]]"
uint8_t * PMD::SetP86RepeatCommand(Channel *, uint8_t * si)
{
    int16_t LoopBegin = *(int16_t *) si;
    si += 2;

    int16_t LoopEnd = *(int16_t *) si;
    si += 2;

    int16_t ReleaseStart = *(int16_t *) si;

    _P86->SetLoop(LoopBegin, LoopEnd, ReleaseStart, _State.PMDB2CompatibilityMode);

    return si + 2;
}

// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
uint8_t * PMD::SetP86MaskCommand(Channel * channel, uint8_t * si)
{
    uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->MuteMask |= 0x40;

            if (channel->MuteMask == 0x40)
                _P86->Stop();
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->MuteMask &= 0xBF; // 1011 1111

    return si;
}
