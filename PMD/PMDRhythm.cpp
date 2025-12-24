
// $VER: PMDRhythm.cpp (2025.12.23) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

void PMD::RhythmMain(Channel * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    if (--channel->Length == 0)
    {
        uint8_t * Data = _State.RhythmData;

        bool Success = false;
        int al;

    rhyms00:
        do
        {
            Success = true;

            al = *Data++;

            if (al != 0xFF)
            {
                if (al & 0x80)
                {
                    Data = RhythmKeyOn(channel, al, Data, &Success);

                    if (!Success)
                        continue;
                }
                else
                    _State.UseRhythmChannel = false;

                al = *Data++;

                _State.RhythmData = Data;

                channel->Length = al;
                channel->KeyOnFlag++;

                _Driver.TieNotesTogether = false;
                _Driver.IsVolumeBoostSet = 0;
                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
        while (!Success);

        while (1)
        {
            while ((al = *si++) != 0x80)
            {
                if (al < 0x80)
                {
                    channel->Data = si;

                    Data = _State.RhythmData = &_State.MData[_State.RhythmDataTable[al]];
                    goto rhyms00;
                }

                si = ExecuteRhythmCommand(channel, si - 1);
            }

            channel->Data = (uint8_t *) --si;
            channel->loopcheck = 3;

            Data = channel->LoopData;

            if (Data != nullptr)
            {
                si = Data;

                channel->loopcheck = 1;
            }
            else
            {
                _State.RhythmData = &_State.DummyRhythmData;

                _Driver.TieNotesTogether = false;
                _Driver.IsVolumeBoostSet = 0;
                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    _Driver.loop_work &= channel->loopcheck;
}

/// <summary>
/// Executes a command on the RSS.
/// </summary>
uint8_t * PMD::ExecuteRhythmCommand(Channel * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        case 0xF5: si++; break;

        // Increase volume by 3dB.
        case 0xF4:
            if (channel->Volume < 15)
                channel->Volume++;
            break;

        // Decrease volume by 3dB.
        case 0xF3:
            if (channel->Volume > 0)
                channel->Volume--;
            break;

        case 0xF2: si += 4; break;

        case 0xF1:
            si = PDRSwitchCommand(channel, si);
            break;

        case 0xF0: si += 4; break;

        // Set SSG envelope.
        case 0xEF:
            _OPNAW->SetReg(si[0], si[1]);
            si += 2;
            break;

        case 0xEE: si++; break;
        case 0xED: si++; break;
        case 0xEC: si++; break;
        case 0xE7: si++; break;

        // 5.5. Relative Volume Change, Command ') [^] % number'
        case 0xE3:
        {
            channel->Volume += *si++;

            if (channel->Volume > 16)
                channel->Volume = 16;
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
            si = IncreaseVolumeForNextNote(channel, si, 15);
            break;

        // Set portamento.
        case 0xDA: si++; break;
        case 0xD6: si += 2; break;
        case 0xCD: si += 5; break;
        case 0xCB: si++; break;
        case 0xCA: si++; break;
        case 0xC9: si++; break;
        case 0xC4: si++; break;
        case 0xC3: si += 2; break;
        case 0xC2: si++; break;

        case 0xC0:
            si = SetRhythmMaskCommand(channel, si);
            break;

        case 0xBF: si += 4; break;
        case 0xBE: si++; break;
        case 0xBD: si += 2; break;
        case 0xBC: si++; break;
        case 0xBB: si++; break;
        case 0xBA: si++; break;
        case 0xB9: si++; break;
        case 0xB7: si++; break;
        case 0xB3: si++; break;
        case 0xB2: si++; break;
        case 0xB1: si++; break;

        default:
            si = ExecuteCommand(channel, si, Command);
    }

    return si;
}

/// <summary>
/// Sets Rhythm Wait after register output.
/// </summary>
void PMD::SetRhythmDelay(int nsec)
{
    _OPNAW->SetRSSDelay(nsec);
}

#pragma region(Commands)
/// <summary>
///
/// </summary>
uint8_t * PMD::RhythmKeyOn(Channel * channel, int al, uint8_t * rhythmData, bool * success)
{
    if (al & 0x40)
    {
        rhythmData = ExecuteRhythmCommand(channel, rhythmData - 1);
        *success = false;

        return rhythmData;
    }

    *success = true;

    if (channel->MuteMask)
    {
        _State.UseRhythmChannel = false;

        return ++rhythmData;
    }

    {
        al = ((al << 8) + *rhythmData++) & 0x3fff;

        _State.UseRhythmChannel = (al != 0);

        if (al == 0)
            return rhythmData;

        _State.RhythmData = rhythmData;
    }

    if (_State.UseSSG)
    {
        for (int cl = 0; cl < 11; cl++)
        {
            if (al & (1 << cl))
            {
                _OPNAW->SetReg((uint32_t) SSGRhythmDefinitions[cl][0], (uint32_t) SSGRhythmDefinitions[cl][1]);

                int dl = SSGRhythmDefinitions[cl][2] & _State.RhythmMask;

                if (dl)
                {
                    if (dl < 0x80)
                        _OPNAW->SetReg(0x10, (uint32_t) dl);
                    else
                    {
                        _OPNAW->SetReg(0x10, 0x84);

                        dl = _State.RhythmMask & 0x08;

                        if (dl)
                            _OPNAW->SetReg(0x10, (uint32_t) dl);
                    }
                }
            }
        }
    }

    if (_State.FadeOutVolume != 0)
    {
        if (_State.UseSSG)
        {
            int dl = _State.RhythmVolume;

            dl = ((256 - _State.FadeOutVolume) * _State.RhythmVolume) >> 8;

            _OPNAW->SetReg(0x11, (uint32_t) dl);
        }

        if (!_Driver.UsePPS)
            return _State.RhythmData; // No sound during fadeout when using PPS.
    }

    {
        int Bits = al;

        int InstrumentNumber = 0;

        do
        {
            // Count the number of zero bits.
            while ((Bits & 1) == 0)
            {
                InstrumentNumber++;

                Bits >>= 1;
            }

            SetSSGInstrument(channel, InstrumentNumber);

            Bits >>= 1;
        }
        while (_Driver.UsePPS && (Bits != 0)); // If PPS is used, try playing the second or more notes.
    }

    return _State.RhythmData;
}
#pragma endregion

/// <summary>
/// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// </summary>
uint8_t * PMD::SetRhythmMaskCommand(Channel * channel, uint8_t * si)
{
    uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
            channel->MuteMask |= 0x40;
        else
            si = SpecialC0ProcessingCommand(channel, si, Value);
    }
    else
        channel->MuteMask &= 0xBF;

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::DecreaseRhythmVolumeCommand(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al != 0)
        _State.RhythmVolumeAdjust = std::clamp(al + _State.RhythmVolumeAdjust, 0, 255);
    else
        _State.RhythmVolumeAdjust = _State.DefaultRhythmVolumeAdjust;

    return si;
}

/// <summary>
///
/// </summary>
uint8_t * PMD::PDRSwitchCommand(Channel *, uint8_t * si)
{
    if (!_Driver.UsePPS)
        return si + 1;

//  ppsdrv->SetParameter((*si & 1) << 1, *si & 1); // Preliminary
    si++;

    return si;
}

#pragma region OPNA Rhythm

// Command "\b", "\s", "\c", "\h", "\t", "\i", "\bp", "\sp", "\cp", "\hp", "\tp", "\ip": Triggers the specified drum channels of the RSS (Rhythm Sound Source). 14.1. Rhythm Sound Source Shot/Dump Control
// Starts/stops each sound in the rhythm sound channels.
uint8_t * PMD::PlayOPNARhythm(uint8_t * si)
{
    int Channel = *si++ & _State.RhythmMask;

    if (Channel == 0)
        return si;

    if (_State.FadeOutVolume != 0)
    {
        int Volume = ((256 - _State.FadeOutVolume) * _State.RhythmVolume) >> 8;

        _OPNAW->SetReg(0x11, (uint32_t) Volume);
    }

    if (Channel < 0x80)
    {
        if (Channel & 0x01)
        {
            _OPNAW->SetReg(0x18, (uint32_t) _State.RhythmPanAndVolume[0]);
            _State.RhythmBassDrumOn++;
        }

        if (Channel & 0x02)
        {
            _OPNAW->SetReg(0x19, (uint32_t) _State.RhythmPanAndVolume[1]);
            _State.RhythmSnareDrumOn++;
        }

        if (Channel & 0x04)
        {
            _OPNAW->SetReg(0x1a, (uint32_t) _State.RhythmPanAndVolume[2]);
            _State.RhythmCymbalOn++;
        }

        if (Channel & 0x08)
        {
            _OPNAW->SetReg(0x1b, (uint32_t) _State.RhythmPanAndVolume[3]);
            _State.RhythmHiHatOn++;
        }

        if (Channel & 0x10)
        {
            _OPNAW->SetReg(0x1c, (uint32_t) _State.RhythmPanAndVolume[4]);
            _State.RhythmTomDrumOn++;
        }

        if (Channel & 0x20)
        {
            _OPNAW->SetReg(0x1d, (uint32_t) _State.RhythmPanAndVolume[5]);
            _State.RhythmRimShotOn++;
        }

        _State.RhythmChannelMask |= Channel;
    }
    else
    {
        if (Channel & 0x01) _State.RhythmBassDrumOff++;
        if (Channel & 0x02) _State.RhythmSnareDrumOff++;
        if (Channel & 0x04) _State.RhythmCymbalOff++;
        if (Channel & 0x08) _State.RhythmHiHatOff++;
        if (Channel & 0x10) _State.RhythmTomDrumOff++;
        if (Channel & 0x20) _State.RhythmRimShotOff++;

        _State.RhythmChannelMask &= (~Channel);
    }

    _OPNAW->SetReg(0x10, (uint32_t) Channel);

    return si;
}

// Command "\V number", 14.2. Rhythm Sound Source Master Volume Setting
uint8_t * PMD::SetOPNARhythmMasterVolumeCommand(uint8_t * si)
{
    int dl = *si++;

    if (_State.RhythmVolumeAdjust != 0)
        dl = ((256 - _State.RhythmVolumeAdjust) * dl) >> 8;

    _State.RhythmVolume = dl;

    if (_State.FadeOutVolume != 0)
        dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) dl);

    return si;
}

// Command "\V ±number", 14.2. Rhythm Sound Source Master Volume Setting
uint8_t * PMD::SetRelativeOPNARhythmMasterVolume(uint8_t * si)
{
    int dl = _State.RhythmVolume + *(int8_t *) si++;

    if (dl >= 64)
        dl = (dl & 0x80) ? 0 : 63;

    _State.RhythmVolume = dl;

    if (_State.FadeOutVolume != 0)
        dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) dl);

    return si;
}

// Command "\vb", "\vs", "\vc", "\vh", "\vt", "\vi", 14.3. Rhythm Sound Source Individual Volume Setting
// b: Bass Drum, s: Snare Drum, c: Cymbal, h: Hi-Hat, t: Tom, i: Rim Shot
uint8_t * PMD::SetOPNARhythmVolumeCommand(uint8_t * si)
{
    int dl = *si & 0x1f;
    int dh = *si++ >> 5;

    int * bx = &_State.RhythmPanAndVolume[dh - 1];

    dh = 0x18 - 1 + dh;
    dl |= (*bx & 0xc0);
    *bx = dl;

    _OPNAW->SetReg((uint32_t) dh, (uint32_t) dl);

    return si;
}

// Command "\vb ±number", "\vs ±number", "\vc ±number", "\vh ±number", "\vt ±number", "\vi ±number", 14.3. Rhythm Sound Source Individual Volume Setting
// b: Bass Drum, s: Snare Drum, c: Cymbal, h: Hi-Hat, t: Tom, i: Rim Shot
uint8_t * PMD::SetRelativeOPNARhythmVolume(uint8_t * si)
{
    int * bx = &_State.RhythmPanAndVolume[*si - 1];

    int dh = *si++ + 0x18 - 1;

    int dl = *bx & 0x1F;
    int al = (*(int8_t *) si++ + dl);

    if (al > 31)
        al = 31;
    else
    if (al < 0)
        al = 0;

    dl = (al &= 0x1F);
    dl = *bx = ((*bx & 0xE0) | dl);

    _OPNAW->SetReg((uint32_t) dh, (uint32_t) dl);

    return si;
}

// Command "\p?"
uint8_t * PMD::SetOPNARhythmPanningCommand(uint8_t * si)
{
    int dl = (*si & 0x03) << 6;     // Pan value
    int dh = (*si++ >> 5) & 0x07;   // Instrument

    int * bx = &_State.RhythmPanAndVolume[dh - 1];

    dl |= (*bx & 0x1F);

    *bx = dl;

    dh += 0x18 - 1;

    _OPNAW->SetReg((uint32_t) dh, (uint32_t) dl);

    return si;
}

#pragma endregion
