
// $VER: PMDP86.cpp (2026.01.03) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

/// <summary>
/// Main P86 processing
/// </summary>
void pmd_driver_t::P86Main(channel_t * channel)
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
            P86KeyOff(channel);

            channel->_KeyOffFlag = -1;
        }
    }

    if (channel->_Size == 0)
    {
        while (1)
        {
            if (*si > 0x80)
            {
                si = P86ExecuteCommand(channel, si);
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
            {
                if (channel->_PartMask != 0x00)
                {
                    si++;

                    // Set to 'rest'.
                    channel->_Factor      = 0;
                    channel->_Tone        = 0xFF;
//                  channel->DefaultTone = 0xFF;
                    channel->_Size      = *si++;
                    channel->_KeyOnFlag++;

                    channel->_Data = si;

                    if (--_Driver._VolumeBoostCount)
                        channel->VolumeBoost = 0;

                    _Driver._IsTieSet = false;
                    _Driver._VolumeBoostCount = 0;
                    break;
                }

                P86SetTone(channel, Transpose(channel, StartPCMLFO(channel, *si++)));

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

                P86SetVolume(channel);
                P86SetPitch(channel);

                if (channel->_KeyOffFlag & 0x01)
                    P86KeyOn(channel);

                channel->_KeyOnFlag++;
                channel->_Data = si;

                _Driver._IsTieSet = false;
                _Driver._VolumeBoostCount = 0;

                // Don't perform Key Off if a "&" command (Tie) follows immediately.
                channel->_KeyOffFlag = (*si == 0xFB) ? 0x02: 0x00;

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
        }
    }

    if (channel->_HardwareLFO & 0x22)
    {
        _Driver._HardwareLFOModulationMode = 0;

        if (channel->_HardwareLFO & 0x02)
        {
            SetLFO(channel);

            _Driver._HardwareLFOModulationMode |= (channel->_HardwareLFO & 0x02);
        }

        if (channel->_HardwareLFO & 0x20)
        {
            LFOSwap(channel);

            if (SetLFO(channel))
            {
                LFOSwap(channel);

                _Driver._HardwareLFOModulationMode |= (channel->_HardwareLFO & 0x20);
            }
            else
                LFOSwap(channel);
        }

        int temp = SSGPCMSoftwareEnvelope(channel);

        if (temp || _Driver._HardwareLFOModulationMode & 0x22 || _State.FadeOutSpeed)
            P86SetVolume(channel);
    }
    else
    {
        int temp = SSGPCMSoftwareEnvelope(channel);

        if (temp || _State.FadeOutSpeed)
            P86SetVolume(channel);
    }

    _Driver._LoopCheck &= channel->_LoopCheck;
}

uint8_t * pmd_driver_t::P86ExecuteCommand(channel_t * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        // 6.1. Instrument Number Setting, Command '@[@] insnum' / Command '@[@] insnum[,number1[,number2[,number3]]]'
        case 0xFF:
        {
            si = P86SetInstrument(channel, si);
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
            si = P86SetPan1(channel, si);
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

        case 0xDA: si++; break;

        // 15.5. SSG Sound Effect Playback, Play SSG sound effect, Command 'n number'
        case 0xD4:
        {
            si = SetSSGEffect(channel, si);
            break;
        }

        // 15.5. FM Sound Effect Playback, Play FM sound effect, Command 'N number'
        case 0xD3:
        {
            si = SetFMEffect(channel, si);
            break;
        }

        // 15.3. Fade Out Setting, Fades out from the specified position, Command 'F number'
        case 0xD2:
        {
            _State.FadeOutSpeed = *si++;
            _State.IsFadeOutSpeedSet = true;
            break;
        }

        // 6.1.5. Instrument Number Setting/PCM Channels Case, Set PCM Repeat.
        case 0xCE:
        {
            si = P86SetRepeat(channel, si);
            break;
        }

        // Set SSG Extend Mode (bit 1).
        case 0xCA:
            channel->_ExtendMode = (channel->_ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);
            break;

        // 8.2. Software Envelope Speed Setting, Set SSG Extend Mode (bit 2), Command 'EX number'
        case 0xC9:
        {
            channel->_ExtendMode = (channel->_ExtendMode & 0xFB) | ((*si++ & 0x01) << 2);
            break;
        }

        // 13.2. Pan Setting 2
        case 0xC3:
        {
            si = P86SetPan2(channel, si);
            break;
        }

        // 15.7. Channel Mask Control, Sets the channel mask to on or off, Command 'm number'
        case 0xC0:
        {
            si = P86SetChannelMask(channel, si);
            break;
        }

        case 0xBF: si += 4; break;
        case 0xBE: si++; break;
        case 0xBD: si += 2; break;
        case 0xBC: si++; break;
        case 0xBB: si++; break;
        case 0xBA: si++; break;
        case 0xB9: si++; break;

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

#pragma region(Commands)
/// <summary>
///
/// </summary>
void pmd_driver_t::P86SetTone(channel_t * channel, int tone)
{
    int ah = tone & 0x0F;

    if (ah != 0x0F)
    {
        if (_State.PMDB2CompatibilityMode && (tone >= 0x65))
        {
            tone = (ah < 5) ? 0x60 /* o7 */ : 0x50 /* o6 */;

            tone |= ah;
        }

        channel->_Tone = tone;

        int Index = ((tone & 0xF0) >> 4) * 12 + ah;

        channel->_Factor = P86ScaleFactor[Index];
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
void pmd_driver_t::P86SetVolume(channel_t * channel)
{
    int al = channel->VolumeBoost ? channel->VolumeBoost : channel->_Volume;

    // Calculate volume down.
    al = ((256 - _State.ADPCMVolumeAdjust) * al) >> 8;

    // Calculate fade out.
    if (_State._FadeOutVolume != 0)
        al = ((256 - _State._FadeOutVolume) * al) >> 8;

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
    int dx = (channel->_HardwareLFO & 0x02) ? channel->_LFO1Data : 0;

    if (channel->_HardwareLFO & 0x20)
        dx += channel->_LFO2Data;

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

    _P86->SetVolume(al);
}

/// <summary>
///
/// </summary>
void pmd_driver_t::P86SetPitch(channel_t * channel)
{
    if (channel->_Factor == 0)
        return;

    int SampleRateIndex = (int) ((channel->_Factor & 0x0E00000) >> (16 + 5));
    int Pitch           = (int) ( channel->_Factor & 0x01FFFFF);

    if (!_State.PMDB2CompatibilityMode && (channel->_DetuneValue != 0))
        Pitch = std::clamp((Pitch >> 5) + channel->_DetuneValue, 1, 65535) << 5;

    _P86->SetPitch(SampleRateIndex, (uint32_t) Pitch);
}

/// <summary>
///
/// </summary>
void pmd_driver_t::P86KeyOn(channel_t * channel)
{
    if (channel->_Tone == 0xFF)
        return;

    _P86->Start();
}

/// <summary>
///
/// </summary>
void pmd_driver_t::P86KeyOff(channel_t * channel)
{
    _P86->Keyoff();

    if (channel->_SSGEnvelopFlag != -1)
    {
        if (channel->_SSGEnvelopFlag != 2)
            SSGKeyOff(channel);

        return;
    }

    if (channel->ExtendedCount != 4)
        SSGKeyOff(channel);
}

/// <summary>
/// Command "@ number": Sets the instrument to be used. Range 0-255.
/// </summary>
uint8_t * pmd_driver_t::P86SetInstrument(channel_t * channel, uint8_t * si)
{
    channel->InstrumentNumber = *si++;

    _P86->SelectSample(channel->InstrumentNumber);

    return si;
}

/// <summary>
/// Command "p value" (1: right, 2: left, 3: center (default), 0: Reverse Phase)
/// </summary>
uint8_t * pmd_driver_t::P86SetPan1(channel_t *, uint8_t * si)
{
    switch (*si++)
    {
        case 0: // Reverse Phase
            _P86->SetPan(3 | 4, 0);
            break;

        case 1: // Right
            _P86->SetPan(2, 1);
            break;

        case 2: // Left
            _P86->SetPan(1, 0);
            break;

        case 3: // Center
            _P86->SetPan(3, 0);
            break;

        default:
            break;
    }

    return si;
}

/// <summary>
/// Command "px ±value1 [, value2]" (value 1: < 0 (Pan to the right), > 0 (Pan to the left), 0 (Center) / value 2: 0 (In phase) or 1 (Reverse phase)).
/// </summary>
uint8_t * pmd_driver_t::P86SetPan2(channel_t * channel, uint8_t * si)
{
    int Flags = 0;
    int Value = 0;

    channel->_PanAndVolume = (int8_t) *si++;

    const bool ReversePhase = (*si++ == 1);

    if (channel->_PanAndVolume == 0)
    {
        Flags = 0x03; // Center
        Value = 0;
    }
    else
    if (channel->_PanAndVolume > 0)
    {
        Flags = 0x02; // Right
        Value = 128 - channel->_PanAndVolume;
    }
    else
    {
        Flags = 0x01; // Left
        Value = 128 + channel->_PanAndVolume;
    }

    if (ReversePhase)
        Flags |= 0x04; // Reverse the phase

    _P86->SetPan(Flags, Value);

    return si;
}
#pragma endregion

// Command "@[@] insnum[,number1[,number2[,number3]]]"
uint8_t * pmd_driver_t::P86SetRepeat(channel_t *, uint8_t * si)
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
uint8_t * pmd_driver_t::P86SetChannelMask(channel_t * channel, uint8_t * si) noexcept
{
    const uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
        {
            channel->_PartMask |= 0x40;

            if (channel->_PartMask == 0x40)
                _P86->Stop();
        }
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->_PartMask &= 0xBF; // 1011 1111

    return si;
}
