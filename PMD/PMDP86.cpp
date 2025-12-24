
// $VER: PMDP86.cpp (2025.12.23) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

void PMD::P86Main(Channel * channel)
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
            P86KeyOff(channel);

            channel->KeyOffFlag = 0xFF;
        }
    }

    if (channel->Length == 0)
    {
        while (1)
        {
            if (*si > 0x80)
            {
                si = ExecuteP86Command(channel, si);
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

                    _Driver.TieNotesTogether = false;
                    _Driver.IsVolumeBoostSet = 0;

                    break;
                }

                SetP86Tone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

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

                SetP86Volume(channel);
                SetP86Pitch(channel);

                if (channel->KeyOffFlag & 0x01)
                    P86KeyOn(channel);

                channel->KeyOnFlag++;
                channel->Data = si;

                _Driver.TieNotesTogether = false;
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
            SetLFO(channel);

            _Driver.ModulationMode |= (channel->ModulationMode & 0x02);
        }

        if (channel->ModulationMode & 0x20)
        {
            SwapLFO(channel);

            if (SetLFO(channel))
            {
                SwapLFO(channel);

                _Driver.ModulationMode |= (channel->ModulationMode & 0x20);
            }
            else
                SwapLFO(channel);
        }

        int temp = SSGPCMSoftwareEnvelope(channel);

        if (temp || _Driver.ModulationMode & 0x22 || _State.FadeOutSpeed)
            SetP86Volume(channel);
    }
    else
    {
        int temp = SSGPCMSoftwareEnvelope(channel);

        if (temp || _State.FadeOutSpeed)
            SetP86Volume(channel);
    }

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecuteP86Command(Channel * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        // 6.1. Instrument Number Setting, Command '@[@] insnum' / Command '@[@] insnum[,number1[,number2[,number3]]]'
        case 0xFF:
        {
            si = SetP86Instrument(channel, si);
            break;
        }

        // Set Early Key Off Timeout.
        case 0xFE:
            channel->EarlyKeyOffTimeout = *si++;
            break;

        // Set SSG envelope.
        case 0xEF:
            _OPNAW->SetReg((uint32_t) (0x100 + si[0]), si[1]);
            si += 2;
            break;

        case 0xEE: si++; break;
        case 0xED: si++; break;

        // 13.1. Pan setting 1
        case 0xEC:
            si = SetP86Pan1(channel, si);
            break;

        // 5.5. Relative Volume Change, Command ') [^] % number'
        case 0xE3:
        {
            channel->Volume += *si++;

            if (channel->Volume > 255)
                channel->Volume = 255;
            break;
        }

        // 5.5. Relative Volume Change, Command ') [^] % number'
        case 0xE2:
        {
            channel->Volume -= *si++;

            if (channel->Volume < 0)
                channel->Volume = 0;
            break;
        }

        case 0xDE:
            si = IncreaseVolumeForNextNote(channel, si, 255);
            break;

        // Set portamento.
        case 0xDA: si++; break;

        case 0xD4:
            si = SetSSGEffect(channel, si);
            break;

        case 0xD3:
            si = SetFMEffect(channel, si);
            break;

        case 0xD2:
            _State.FadeOutSpeed = *si++;
            _State.IsFadeOutSpeedSet = true;
            break;

        // Set PCM Repeat.
        case 0xCE:
            si = SetP86RepeatCommand(channel, si);
            break;

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
            si = SetP86Pan2(channel, si);
            break;

        case 0xC0:
            si = SetP86MaskCommand(channel, si);
            break;

        case 0xBF: si += 4; break;
        case 0xBE: si++; break;
        case 0xBD: si += 2; break;
        case 0xBC: si++; break;
        case 0xBB: si++; break;
        case 0xBA: si++; break;
        case 0xB9: si++; break;

        case 0xB4:
            si = InitializePPZ(channel, si);
            break;

        default:
            si = ExecuteCommand(channel, si, Command);
    }

    return si;
}

#pragma region(Commands)
/// <summary>
///
/// </summary>
void PMD::SetP86Tone(Channel * channel, int tone)
{
    int ah = tone & 0x0F;

    if (ah != 0x0F)
    {
        if (_State.PMDB2CompatibilityMode && (tone >= 0x65))
        {
            tone = (ah < 5) ? 0x60 /* o7 */ : 0x50 /* o6 */;

            tone |= ah;
        }

        channel->Tone = tone;

        int Index = ((tone & 0xF0) >> 4) * 12 + ah;

        channel->Factor = P86ScaleFactor[Index];
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
void PMD::SetP86Volume(Channel * channel)
{
    int al = channel->VolumeBoost ? channel->VolumeBoost : channel->Volume;

    // Calculate volume down.
    al = ((256 - _State.ADPCMVolumeAdjust) * al) >> 8;

    // Calculate fade out.
    if (_State.FadeOutVolume != 0)
        al = ((256 - _State.FadeOutVolume) * al) >> 8;

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

    // Calculate the LFO volume.
    int dx = (channel->ModulationMode & 0x02) ? channel->LFO1Data : 0;

    if (channel->ModulationMode & 0x20)
        dx += channel->LFO2Data;

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

/// <summary>
///
/// </summary>
void PMD::SetP86Pitch(Channel * channel)
{
    if (channel->Factor == 0)
        return;

    int SampleRateIndex = (int) ((channel->Factor & 0x0E00000) >> (16 + 5));
    int Pitch           = (int) ( channel->Factor & 0x01FFFFF);

    if (!_State.PMDB2CompatibilityMode && (channel->DetuneValue != 0))
        Pitch = std::clamp((Pitch >> 5) + channel->DetuneValue, 1, 65535) << 5;

    _P86->SetPitch(SampleRateIndex, (uint32_t) Pitch);
}

/// <summary>
///
/// </summary>
void PMD::P86KeyOn(Channel * channel)
{
    if (channel->Tone == 0xFF)
        return;

    _P86->Play();
}

/// <summary>
///
/// </summary>
void PMD::P86KeyOff(Channel * channel)
{
    _P86->Keyoff();

    if (channel->SSGEnvelopFlag != -1)
    {
        if (channel->SSGEnvelopFlag != 2)
            SSGKeyOff(channel);

        return;
    }

    if (channel->ExtendedCount != 4)
        SSGKeyOff(channel);
}

/// <summary>
/// Command "@ number": Sets the instrument to be used. Range 0-255.
/// </summary>
uint8_t * PMD::SetP86Instrument(Channel * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    _P86->SelectSample(channel->InstrumentNumber);

    return si;
}

/// <summary>
/// Command "p <value>" (1: right, 2: left, 3: center (default), 0: Reverse Phase)
/// </summary>
uint8_t * PMD::SetP86Pan1(Channel *, uint8_t * si)
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

/// <summary>
/// Command "px <value 1>, <value 2>" (value 1: < 0 (Pan to the right), > 0 (Pan to the left), 0 (Center), value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * PMD::SetP86Pan2(Channel * channel, uint8_t * si)
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
#pragma endregion

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
