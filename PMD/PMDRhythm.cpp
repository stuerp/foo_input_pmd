
/** $VER: PMDRhythm.cpp (2026.01.07) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

/// <summary>
/// Main Rhythm processing
/// </summary>
void pmd_driver_t::RhythmMain(channel_t * channel)
{
    if (channel->_Data == nullptr)
        return;

    uint8_t * si = channel->_Data;

    channel->_Size--;

    if (channel->_Size == 0)
    {
        uint8_t * Data = _State._RhythmData;

        bool EndOfRhythmData = false;
        int32_t Note;

    rhyms00:
        do
        {
            EndOfRhythmData = true;

            Note = *Data++;

            if (Note != 0xFF)
            {
                if (Note & 0x80)
                {
                    Data = RhythmKeyOn(channel, Note, Data, &EndOfRhythmData);

                    if (!EndOfRhythmData)
                        continue;
                }
                else
                    _State._UseRhythmChannel = false; // Rest

                Note = *Data++;

                _State._RhythmData = Data;

                channel->_Size = Note;
                channel->_KeyOnFlag++;

                _Driver._IsTieSet = false;
                _Driver._VolumeBoostCount = 0;

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
        }
        while (!EndOfRhythmData);

        while (1)
        {
            while ((Note = *si++) != 0x80)
            {
                if (Note < 0x80)
                {
                    channel->_Data = si;

                    Data = _State._RhythmData = &_State._MData[_State._RhythmDataTable[Note]];
                    goto rhyms00;
                }

                si = RhythmExecuteCommand(channel, si - 1);
            }

            channel->_Data = (uint8_t *) --si;

            channel->_LoopCheck = 0x03;

            Data = channel->_LoopData;

            if (Data != nullptr)
            {
                // Start executing a loop.
                si = (uint8_t *) Data;

                channel->_LoopCheck = 0x01;
            }
            else
            {
                _State._RhythmData = (uint8_t *) &_DummyRhythmData;

                _Driver._IsTieSet = false;
                _Driver._VolumeBoostCount = 0;

                _Driver._LoopCheck &= channel->_LoopCheck;

                return;
            }
        }
    }

    _Driver._LoopCheck &= channel->_LoopCheck;
}

/// <summary>
/// Executes a command on the Rhythm Sound Source.
/// </summary>
uint8_t * pmd_driver_t::RhythmExecuteCommand(channel_t * channel, uint8_t * si)
{
    const uint8_t Command = *si++;

    switch (Command)
    {
        case 0xF5: si++; break;

        // 5.5. Relative Volume Change, Increase volume by 3dB.
        case 0xF4:
        {
            if (channel->_Volume < 15)
                channel->_Volume++;
            break;
        }

        // 5.5. Relative Volume Change, Decrease volume by 3dB.
        case 0xF3:
        {
            if (channel->_Volume > 0)
                channel->_Volume--;
            break;
        }

        case 0xF2: si += 4; break;

        // 9.3. Software LFO Switch, Command '*A number'
        case 0xF1:
        {
            si = RhythmSetLFOControl(channel, si);
            break;
        }

        case 0xF0: si += 4; break;

        // 15.1. FM Chip Direct Output, Direct register write. Writes val to address reg of the YM2608's internal memory, Command 'y number1, number2'
        case 0xEF:
        {
            _OPNAW->SetReg(si[0], si[1]);
            si += 2;
            break;
        }

        case 0xEE: si++; break;
        case 0xED: si++; break;
        case 0xEC: si++; break;
        case 0xE7: si++; break;

        // 5.5. Relative Volume Change, Command ') %number'
        case 0xE3:
        {
            channel->_Volume += *si++;

            if (channel->_Volume > 16)
                channel->_Volume = 16;
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
            si = IncreaseVolumeForNextNote(channel, si, 15);
            break;
        }

        case 0xDA: si++; break;
        case 0xD6: si += 2; break;
        case 0xCD: si += 5; break;
        case 0xCB: si++; break;
        case 0xCA: si++; break;
        case 0xC9: si++; break;
        case 0xC4: si++; break;
        case 0xC3: si += 2; break;
        case 0xC2: si++; break;

        // 15.7. Channel Mask Control, Sets the channel mask to on or off, Command 'm number'
        case 0xC0:
        {
            si = RhythmSetChannelMask(channel, si);
            break;
        }

        case 0xBF: si += 4; break;
        case 0xBE: si++; break;
        case 0xBD: si += 2; break;
        case 0xBC: si++; break;
        case 0xBB: si++; break;
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
///
/// </summary>
uint8_t * pmd_driver_t::RhythmKeyOn(channel_t * channel, int32_t note, uint8_t * rhythmData, bool * success) noexcept
{
    if (note & 0x40)
    {
        rhythmData = RhythmExecuteCommand(channel, rhythmData - 1);
        *success = false;

        return rhythmData;
    }

    *success = true;

    if (channel->_PartMask != 0x00)
    {
        _State._UseRhythmChannel = false;

        return ++rhythmData;
    }

    {
        note = ((note << 8) + *rhythmData++) & 0x3FFF;

        _State._UseRhythmChannel = (note != 0);

        if (note == 0)
            return rhythmData;

        _State._RhythmData = rhythmData;
    }

    if (_UseSSGForDrums)
    {
        for (size_t i = 0; i < 11; ++i)
        {
            if (note & (1 << i))
            {
                auto & SSGRhythm = SSGRhythms[i];

                _OPNAW->SetReg(SSGRhythm.Register, SSGRhythm.Value);

                uint32_t Mask = SSGRhythm.Mask & _RhythmMask;

                if (Mask != 0x00)
                {
                    if (Mask < 0x80)
                        _OPNAW->SetReg(0x10, Mask);         // Rhythm Part: Set KON
                    else
                    {
                        _OPNAW->SetReg(0x10, 0x84);         // Rhythm Part: Set KON

                        Mask = _RhythmMask & 0x08;

                        if (Mask != 00)
                            _OPNAW->SetReg(0x10, Mask);     // Rhythm Part: Set KON
                    }
                }
            }
        }
    }

    if (_State._FadeOutVolume != 0)
    {
        if (_UseSSGForDrums)
        {
            const int32_t Value = (_State._FadeOutVolume != 0) ? ((256 - _State._FadeOutVolume) * _RhythmVolume) >> 8 : _RhythmVolume;

            _OPNAW->SetReg(0x11, (uint32_t) Value);         // Rhythm Part: Set RTL (Total Level)
        }

        if (!_UsePPSForDrums)
            return _State._RhythmData; // No sound during fadeout when using PPS.
    }

    {
        int32_t Bits = note;
        int32_t InstrumentNumber = 0;

        do
        {
            // Count the number of zero bits.
            while ((Bits & 1) == 0)
            {
                InstrumentNumber++;

                Bits >>= 1;
            }

            SSGSetDrumInstrument(channel, InstrumentNumber);

            Bits >>= 1;
        }
        while (_UsePPSForDrums && (Bits != 0)); // If PPS is used, try playing the second or more notes.
    }

    return _State._RhythmData;
}
/// <summary>
/// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
/// </summary>
uint8_t * pmd_driver_t::RhythmSetChannelMask(channel_t * channel, uint8_t * si) noexcept
{
    const uint8_t Value = *si++;

    if (Value != 0)
    {
        if (Value < 2)
            channel->_PartMask |= 0x40;
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
uint8_t * pmd_driver_t::RhythmDecreaseVolume(channel_t *, uint8_t * si)
{
    int32_t Value = *(int8_t *) si++;

    if (Value != 0)
        _State._RhythmVolumeAdjust = std::clamp(Value + _State._RhythmVolumeAdjust, 0, 255);
    else
        _State._RhythmVolumeAdjust = _State._RhythmVolumeAdjustDefault;

    return si;
}

/// <summary>
/// Starts/stops the drum channels of the RSS (Rhythm Sound Source).
/// 14.1. Rhythm Sound Source Shot/Dump Control, Command "\b", "\s", "\c", "\h", "\t", "\i", "\bp", "\sp", "\cp", "\hp", "\tp", "\ip"
/// </summary>
uint8_t * pmd_driver_t::RhythmControl(uint8_t * si)
{
    const uint32_t ChannelMask = (uint8_t) (*si++ & _RhythmMask);

    if (ChannelMask == 0)
        return si;

    if (_State._FadeOutVolume != 0)
    {
        const int32_t Volume = (_State._FadeOutVolume != 0) ? ((256 - _State._FadeOutVolume) * _RhythmVolume) >> 8 : _RhythmVolume;

        _OPNAW->SetReg(0x11, (uint32_t) Volume); // Rhythm Part: Set RTL (Total Level)
    }

    if (ChannelMask < 0x80)
    {
        if (ChannelMask & 0x01)
        {
            _OPNAW->SetReg(0x18, (uint32_t) _State._RhythmPanAndVolumes[0]); // Rhytm Part: Set Output Select / Instrument Level
            _State._RhythmBassDrumOn++;
        }

        if (ChannelMask & 0x02)
        {
            _OPNAW->SetReg(0x19, (uint32_t) _State._RhythmPanAndVolumes[1]); // Rhytm Part: Set Output Select / Instrument Level
            _State._RhythmSnareDrumOn++;
        }

        if (ChannelMask & 0x04)
        {
            _OPNAW->SetReg(0x1A, (uint32_t) _State._RhythmPanAndVolumes[2]); // Rhytm Part: Set Output Select / Instrument Level
            _State._RhythmCymbalOn++;
        }

        if (ChannelMask & 0x08)
        {
            _OPNAW->SetReg(0x1B, (uint32_t) _State._RhythmPanAndVolumes[3]); // Rhytm Part: Set Output Select / Instrument Level
            _State._RhythmHiHatOn++;
        }

        if (ChannelMask & 0x10)
        {
            _OPNAW->SetReg(0x1C, (uint32_t) _State._RhythmPanAndVolumes[4]); // Rhytm Part: Set Output Select / Instrument Level
            _State._RhythmTomDrumOn++;
        }

        if (ChannelMask & 0x20)
        {
            _OPNAW->SetReg(0x1D, (uint32_t) _State._RhythmPanAndVolumes[5]); // Rhytm Part: Set Output Select / Instrument Level
            _State._RhythmRimShotOn++;
        }

        _RhythmChannelMask |= ChannelMask;
    }
    else
    {
        if (ChannelMask & 0x01) _State._RhythmBassDrumOff++;
        if (ChannelMask & 0x02) _State._RhythmSnareDrumOff++;
        if (ChannelMask & 0x04) _State._RhythmCymbalOff++;
        if (ChannelMask & 0x08) _State._RhythmHiHatOff++;
        if (ChannelMask & 0x10) _State._RhythmTomDrumOff++;
        if (ChannelMask & 0x20) _State._RhythmRimShotOff++;

        _RhythmChannelMask &= (~ChannelMask);
    }

    _OPNAW->SetReg(0x10, (uint32_t) ChannelMask); // Rhythm Part: Set KON (Control)

    return si;
}

/// <summary>
/// Sets the Rhythm volume. Command "\V number", 14.2. Rhythm Sound Source Master Volume Setting
/// </summary>
uint8_t * pmd_driver_t::RhythmSetMasterVolume(uint8_t * si)
{
    int Volume = *si++;

    if (_State._RhythmVolumeAdjust != 0)
        Volume = ((256 - _State._RhythmVolumeAdjust) * Volume) >> 8;

    _RhythmVolume = Volume;

    if (_State._FadeOutVolume != 0)
        Volume = ((256 - _State._FadeOutVolume) * Volume) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) Volume); // Rhythm Part: Set RTL (Total Level)

    return si;
}

/// <summary>
/// Sets the Rhythm volume relative to the current value. Command "\V ±number", 14.2. Rhythm Sound Source Master Volume Setting
/// </summary>
uint8_t * pmd_driver_t::RhythmSetRelativeMasterVolume(uint8_t * si)
{
    int Volume = _RhythmVolume + *(int8_t *) si++;

    if (Volume >= 64)
        Volume = (Volume & 0x80) ? 0 : 63;

    _RhythmVolume = Volume;

    if (_State._FadeOutVolume != 0)
        Volume = ((256 - _State._FadeOutVolume) * Volume) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) Volume); // Rhythm Part: Set RTL (Total Level)

    return si;
}

// Command "\vb", "\vs", "\vc", "\vh", "\vt", "\vi", 14.3. Rhythm Sound Source Individual Volume Setting
// b: Bass Drum, s: Snare Drum, c: Cymbal, h: Hi-Hat, t: Tom, i: Rim Shot
uint8_t * pmd_driver_t::RhythmSetVolume(uint8_t * si)
{
    int32_t Value    = *si & 0x1f;
    int32_t Register = *si++ >> 5;

    int32_t * bx = &_State._RhythmPanAndVolumes[Register - 1];

    Register = 0x18 - 1 + Register;
    Value |= (*bx & 0xc0);
    *bx = Value;

    _OPNAW->SetReg((uint32_t) Register, (uint32_t) Value);

    return si;
}

// Command "\vb ±number", "\vs ±number", "\vc ±number", "\vh ±number", "\vt ±number", "\vi ±number", 14.3. Rhythm Sound Source Individual Volume Setting
// b: Bass Drum, s: Snare Drum, c: Cymbal, h: Hi-Hat, t: Tom, i: Rim Shot
uint8_t * pmd_driver_t::RhythmSetRelativeVolume(uint8_t * si)
{
    int32_t * bx = &_State._RhythmPanAndVolumes[*si - 1];

    int32_t Register = *si++ + 0x18 - 1;
    int32_t Value = *bx & 0x1F;

    int32_t al = (*(int8_t *) si++ + Value);

    if (al > 31)
        al = 31;
    else
    if (al < 0)
        al = 0;

    Value = (al &= 0x1F);
    Value = *bx = ((*bx & 0xE0) | Value);

    _OPNAW->SetReg((uint32_t) Register, (uint32_t) Value);

    return si;
}

uint8_t * pmd_driver_t::RhythmSetPan(uint8_t * si)
{
    int32_t Value = (*si & 0x03) << 6;     // Pan value
    int32_t Register = (*si++ >> 5) & 0x07;   // Instrument

    int * bx = &_State._RhythmPanAndVolumes[Register - 1];

    Value |= (*bx & 0x1F);

    *bx = Value;

    Register += 0x18 - 1;

    _OPNAW->SetReg((uint32_t) Register, (uint32_t) Value);

    return si;
}

/// <summary>
/// 9.3. Software LFO Switch, Command '*A number'
/// </summary>
uint8_t * pmd_driver_t::RhythmSetLFOControl(channel_t *, uint8_t * si)
{
    if (!_UsePPSForDrums)
        return si + 1;

//  ppsdrv->SetParameter((*si & 1) << 1, *si & 1); // Preliminary
    si++;

    return si;
}

