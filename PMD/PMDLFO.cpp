﻿
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

void PMD::LFOMain(Channel * channel)
{
    if (channel->speed != 1)
    {
        if (channel->speed != 255)
            channel->speed--;

        return;
    }

    channel->speed = channel->speed2;

    int al, ax;

    if (channel->lfo_wave == 0 || channel->lfo_wave == 4 || channel->lfo_wave == 5)
    {
        // Triangle wave (lfowave = 0,4,5)
        if (channel->lfo_wave == 5)
            ax = ::abs(channel->step) * channel->step;
        else
            ax = channel->step;

        if ((channel->lfodat += ax) == 0)
            md_inc(channel);

        al = channel->time;

        if (al != 255)
        {
            if (--al == 0)
            {
                al = channel->time2;

                if (channel->lfo_wave != 4)
                    al += al;  // When lfowave = 0 or 5, double the time when inverting.

                channel->time = al;
                channel->step = -channel->step;

                return;
            }
        }

        channel->time = al;
    }
    else
    if (channel->lfo_wave == 2)
    {
        // Square wave (lfowave = 2)
        channel->lfodat = (channel->step * channel->time);

        md_inc(channel);

        channel->step = -channel->step;

    }
    else
    if (channel->lfo_wave == 6)
    {
        // One shot (lfowave = 6)
        if (channel->time)
        {
            if (channel->time != 255)
                channel->time--;

            channel->lfodat += channel->step;
        }
    }
    else
    if (channel->lfo_wave == 1)
    {
        // Sawtooth wave (lfowave = 1)
        channel->lfodat += channel->step;

        al = channel->time;

        if (al != -1)
        {
            al--;

            if (al == 0)
            {
                channel->lfodat = -channel->lfodat;

                md_inc(channel);

                al = (channel->time2) * 2;
            }
        }

        channel->time = al;
    }
    else
    {
        // Random wave (lfowave = 3)
        ax = abs(channel->step) * channel->time;

        channel->lfodat = ax - rnd(ax * 2);

        md_inc(channel);
    }
}

int PMD::StartLFO(Channel * channel, int al)
{
    int ah = al & 0x0F;

    if (ah == 0x0C)
    {
        al = channel->DefaultTone;
        ah = al & 0x0F;
    }

    channel->DefaultTone = al;

    if (ah == 0x0F)
    {
        StopLFO(channel);

        return al;
    }

    channel->porta_num = 0; // Initialize the portamento.

    if (_Driver.TieMode & 1)
        StopLFO(channel);
    else
        lfin1(channel);

    return al;
}

// Entry for SSG/PCM sound source
int PMD::StartPCMLFO(Channel * channel, int al)
{
    int ah = al & 0x0F;

    if (ah == 0x0C)
    {
        al = channel->DefaultTone;
        ah = al & 0x0F;
    }

    channel->DefaultTone = al;

    if (ah == 0x0F)
    {
        SSGPCMSoftwareEnvelope(channel);
        StopLFO(channel);

        return al;
    }

    channel->porta_num = 0; // Initialize the portamento.

    if (_Driver.TieMode & 1)
    {
        SSGPCMSoftwareEnvelope(channel); // Only execute the software envelope once when preceded by a "&" command (Tie).
        StopLFO(channel);

        return al;
    }

    //  Initialize the software envelope.
    if (channel->envf != -1)
    {
        channel->envf = 0;
        channel->eenv_volume = 0;
        channel->eenv_ar = channel->eenv_arc;

        if (channel->eenv_ar == 0)
        {
            channel->envf = 1;  // ATTACK=0 ... ｽｸﾞ Decay ﾆ
            channel->eenv_volume = channel->eenv_dr;
        }

        channel->eenv_sr = channel->eenv_src;
        channel->eenv_rr = channel->eenv_rrc;

        lfin1(channel);
    }
    else
    {
        // Extended SSG envelope processing
        channel->eenv_arc = channel->eenv_ar - 16;

        if (channel->eenv_dr < 16)
            channel->eenv_drc = (channel->eenv_dr - 16) * 2;
        else
            channel->eenv_drc = channel->eenv_dr - 16;

        if (channel->eenv_sr < 16)
            channel->eenv_src = (channel->eenv_sr - 16) * 2;
        else
            channel->eenv_src = channel->eenv_sr - 16;

        channel->eenv_rrc = (channel->eenv_rr) * 2 - 16;
        channel->eenv_volume = channel->eenv_al;
        channel->eenv_count = 1;

        ExtendedSSGPCMSoftwareEnvelopeMain(channel);

        lfin1(channel);
    }

    return al;
}

void PMD::StopLFO(Channel * track)
{
    if ((track->lfoswi & 0x03) != 0)
        lfo(track); // Only execute the LFO once when preceded by a "&" command (Tie).

    if ((track->lfoswi & 0x30) != 0)
    {
        // Only execute the LFO once when preceded by a "&" command (Tie).
        SwapLFO(track);

        lfo(track);

        SwapLFO(track);
    }
}

//  ＬＦＯ初期化
void PMD::lfin1(Channel * track)
{
    track->hldelay_c = track->hldelay;

    if (track->hldelay)
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + _Driver.CurrentChannel + 0xb4 - 1), (uint32_t) (track->PanAndVolume & 0xc0));

    track->sdelay_c = track->sdelay;

    if (track->lfoswi & 0x03)
    {   // LFO not used
        if ((track->lfoswi & 4) == 0)
            lfoinit_main(track); // Is keyon asynchronous?

        lfo(track);
    }

    if (track->lfoswi & 0x30)
    {   // LFO not used
        if ((track->lfoswi & 0x40) == 0)
        {
            SwapLFO(track); // Is keyon asynchronous?

            lfoinit_main(track);

            SwapLFO(track);
        }

        SwapLFO(track);

        lfo(track);

        SwapLFO(track);
    }
}

void PMD::lfoinit_main(Channel * track)
{
    track->lfodat = 0;
    track->delay = track->delay2;
    track->speed = track->speed2;
    track->step = track->step2;
    track->time = track->time2;
    track->mdc = track->mdc2;

    if (track->lfo_wave == 2 || track->lfo_wave == 3)
    {   // Square wave or random wave?
        track->speed = 1;  // Make the LFO apply immediately after the delay
    }
    else
        track->speed++; // Otherwise, +1 to the speed value immediately after the delay
}

//  ＬＦＯ処理
//    Don't Break cl
//    output    cy=1  変化があった
int PMD::lfo(Channel * track)
{
    return SetSSGLFO(track);
}

int PMD::SetSSGLFO(Channel * track)
{
    if (track->delay)
    {
        track->delay--;

        return 0;
    }

    int ax, ch;

    if (track->extendmode & 2)
    {
        // Match with TimerA? If not, unconditionally process lfo
        ch = _State.TimerATime - _Driver.OldTimerATime;

        if (ch == 0)
            return 0;

        ax = track->lfodat;

        for (; ch > 0; ch--)
            LFOMain(track);
    }
    else
    {
        ax = track->lfodat;

        LFOMain(track);
    }

    return (ax == track->lfodat) ? 0 : 1;
}

uint8_t * PMD::lfoswitch(Channel * channel, uint8_t * si)
{
    int al = *si++;

    if (al & 0xf8)
        al = 1;

    al &= 7;

    channel->lfoswi = (channel->lfoswi & 0xf8) | al;

    lfoinit_main(channel);

    return si;
}
