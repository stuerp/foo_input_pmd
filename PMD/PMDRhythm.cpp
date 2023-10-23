
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

void PMD::RhythmMain(Channel * channel)
{
    if (channel->Data == nullptr)
        return;

    uint8_t * si = channel->Data;

    if (--channel->Length == 0)
    {
        uint8_t * bx = _State.RhythmData;

        bool Success = false;
        int al;

    rhyms00:
        do
        {
            Success = 1;

            al = *bx++;

            if (al != 0xFF)
            {
                if (al & 0x80)
                {
                    bx = RhythmOn(channel, al, bx, &Success);

                    if (!Success)
                        continue;
                }
                else
                    _State.kshot_dat = 0;  //rest

                al = *bx++;

                _State.RhythmData = bx;

                channel->Length = al;
                channel->KeyOnFlag++;

                _Driver.TieMode = 0;
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
                    channel->Data = (uint8_t *) si;

                    bx = _State.RhythmData = &_State.MData[_State.RhythmDataTable[al]];
                    goto rhyms00;
                }

                si = ExecuteRhythmCommand(channel, si - 1);
            }

            channel->Data = (uint8_t *) --si;
            channel->loopcheck = 3;

            bx = channel->LoopData;

            if (bx != nullptr)
            {
                si = bx;

                channel->loopcheck = 1;
            }
            else
            {
                _State.RhythmData = &_State.DummyRhythmData;

                _Driver.TieMode = 0;
                _Driver.IsVolumeBoostSet = 0;
                _Driver.loop_work &= channel->loopcheck;

                return;
            }
        }
    }

    _Driver.loop_work &= channel->loopcheck;
}

uint8_t * PMD::ExecuteRhythmCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xFF: si++; break;
        case 0xFE: si++; break;

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
            _OPNAW->SetReg(*si, *(si + 1));
            si += 2;
            break;

        case 0xEE: si++; break;
        case 0xED: si++; break;
        case 0xEC: si++; break;

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

        case 0xE7: si++; break;

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

            if (channel->Volume > 16)
                channel->Volume = 16;
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
            si = IncreaseVolumeForNextNote(channel, si, 15);
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

        case 0xd6: si += 2; break;
        case 0xd5: channel->DetuneValue += *(int16_t *) si; si += 2; break;

        case 0xd4: si = SetSSGEffect(channel, si); break;
        case 0xd3: si = SetFMEffect(channel, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si++; break;

        case 0xcf: si++; break;
        case 0xce: si += 6; break;
        case 0xcd: si += 5; break;
        case 0xcc: si++; break;
        case 0xcb: si++; break;
        case 0xca: si++; break;
        case 0xc9: si++; break;
        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: si++; break;
        case 0xc3: si += 2; break;
        case 0xc2: si++; break;
        case 0xc1: break;

        case 0xc0:
            si = SetRhythmMaskCommand(channel, si);
            break;

        case 0xbf: si += 4; break;
        case 0xbe: si++; break;
        case 0xbd: si += 2; break;
        case 0xbc: si++; break;
        case 0xbb: si++; break;
        case 0xba: si++; break;
        case 0xb9: si++; break;
        case 0xb8: si += 2; break;
        case 0xb7: si++; break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;
        case 0xb3: si++; break;
        case 0xb2: si++; break;
        case 0xB1: si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

/// <summary>
/// Start playing a sound using the Rhythm channel.
/// </summary>
uint8_t * PMD::RhythmOn(Channel * channel, int al, uint8_t * bx, bool * success)
{
    if (al & 0x40)
    {
        bx = ExecuteRhythmCommand(channel, bx - 1);
        *success = false;

        return bx;
    }

    *success = true;

    if (channel->MuteMask)
    {
        _State.kshot_dat = 0x00;

        return ++bx;
    }

    al = ((al << 8) + *bx++) & 0x3fff;

    _State.kshot_dat = al;

    if (al == 0)
        return bx;

    _State.RhythmData = bx;

    if (_State.UseRhythm)
    {
        for (int cl = 0; cl < 11; cl++)
        {
            if (al & (1 << cl))
            {
                _OPNAW->SetReg((uint32_t) rhydat[cl][0], (uint32_t) rhydat[cl][1]);

                int dl = rhydat[cl][2] & _State.RhythmMask;

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
        if (_State.UseRhythm)
        {
            int dl = _State.RhythmVolume;

            dl = ((256 - _State.FadeOutVolume) * _State.RhythmVolume) >> 8;

            _OPNAW->SetReg(0x11, (uint32_t) dl);
        }

        if (!_Driver.UsePPS)
            return _State.RhythmData; // No sound during fadeout when using PPS.
    }

    {
        int bx_ = al;

        al = 0;

        do
        {
            // Count the number of zero bits.
            while ((bx_ & 1) == 0)
            {
                bx_ >>= 1;
                al++;
            }

            SSGPlayEffect(channel, al);

            bx_ >>= 1;
        }
        while (_Driver.UsePPS && (bx_ != 0)); // If PPS is used, try playing the second or more notes.
    }

    return _State.RhythmData;
}

// Sets Rhythm Wait after register output.
void PMD::SetRhythmDelay(int nsec)
{
    _OPNAW->SetRhythmDelay(nsec);
}

// Command "m <number>": Channel Mask Control (0 = off (Channel plays) / 1 = on (channel does not play))
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

uint8_t * PMD::DecreaseRhythmVolumeCommand(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.RhythmVolumeDown = Limit(al + _State.RhythmVolumeDown, 255, 0);
    else
        _State.RhythmVolumeDown = _State.DefaultRhythmVolumeDown;

    return si;
}

#pragma region(OPNA Rhythm)
// Command "\?" / "\?p"
uint8_t * PMD::OPNARhythmKeyOn(uint8_t * si)
{
    int dl = *si++ & _State.RhythmMask;

    if (dl == 0)
        return si;

    if (_State.FadeOutVolume != 0)
    {
        int al = ((256 - _State.FadeOutVolume) * _State.RhythmVolume) >> 8;

        _OPNAW->SetReg(0x11, (uint32_t) al);
    }

    if (dl < 0x80)
    {
        if (dl & 0x01) _OPNAW->SetReg(0x18, (uint32_t) _State.RhythmPanAndVolume[0]);
        if (dl & 0x02) _OPNAW->SetReg(0x19, (uint32_t) _State.RhythmPanAndVolume[1]);
        if (dl & 0x04) _OPNAW->SetReg(0x1a, (uint32_t) _State.RhythmPanAndVolume[2]);
        if (dl & 0x08) _OPNAW->SetReg(0x1b, (uint32_t) _State.RhythmPanAndVolume[3]);
        if (dl & 0x10) _OPNAW->SetReg(0x1c, (uint32_t) _State.RhythmPanAndVolume[4]);
        if (dl & 0x20) _OPNAW->SetReg(0x1d, (uint32_t) _State.RhythmPanAndVolume[5]);
    }

    _OPNAW->SetReg(0x10, (uint32_t) dl);

    if (dl >= 0x80)
    {
        if (dl & 0x01) _State.rdump_bd++;
        if (dl & 0x02) _State.rdump_sd++;
        if (dl & 0x04) _State.rdump_sym++;
        if (dl & 0x08) _State.rdump_hh++;
        if (dl & 0x10) _State.rdump_tom++;
        if (dl & 0x20) _State.rdump_rim++;

        _State.rshot_dat &= (~dl);
    }
    else
    {
        if (dl & 0x01) _State.rshot_bd++;
        if (dl & 0x02) _State.rshot_sd++;
        if (dl & 0x04) _State.rshot_sym++;
        if (dl & 0x08) _State.rshot_hh++;
        if (dl & 0x10) _State.rshot_tom++;
        if (dl & 0x20) _State.rshot_rim++;

        _State.rshot_dat |= dl;
    }

    return si;
}

// Command "\V"
uint8_t * PMD::SetOPNARhythmMasterVolumeCommand(uint8_t * si)
{
    int dl = *si++;

    if (_State.RhythmVolumeDown != 0)
        dl = ((256 - _State.RhythmVolumeDown) * dl) >> 8;

    _State.RhythmVolume = dl;

    if (_State.FadeOutVolume != 0)
        dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) dl);

    return si;
}

// Command "\v?"
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

uint8_t * PMD::ModifyOPNARhythmMasterVolume(uint8_t * si)
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

uint8_t * PMD::ModifyOPNARhythmVolume(uint8_t * si)
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
#pragma endregion
