
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

int PMD::SSGPCMSoftwareEnvelope(Channel * channel)
{
    if (channel->extendmode & 4)
    {
        if (_State.TimerATime == _Driver.OldTimerATime)
            return 0;

        int cl = 0;

        for (int i = 0; i < _State.TimerATime - _Driver.OldTimerATime; ++i)
        {
            if (SSGPCMSoftwareEnvelopeMain(channel))
                cl = 1;
        }

        return cl;
    }
    else
        return SSGPCMSoftwareEnvelopeMain(channel);
}

int PMD::SSGPCMSoftwareEnvelopeMain(Channel * channel)
{
    if (channel->envf == -1)
        return ExtendedSSGPCMSoftwareEnvelopeMain(channel);

    int dl = channel->eenv_volume;

    SSGPCMSoftwareEnvelopeSub(channel);

    if (dl == channel->eenv_volume)
        return 0;

    return -1;
}

int PMD::SSGPCMSoftwareEnvelopeSub(Channel * channel)
{
    if (channel->envf == 0)
    {
        // Attack
        if (--channel->eenv_ar != 0)
            return 0;

        channel->envf = 1;
        channel->eenv_volume = channel->eenv_dr;

        return 1;
    }

    if (channel->envf != 2)
    {
        // Decay
        if (channel->eenv_sr == 0) return 0;  // No attenuation when DR=0
        if (--channel->eenv_sr != 0) return 0;

        channel->eenv_sr = channel->eenv_src;
        channel->eenv_volume--;

        if (channel->eenv_volume >= -15 || channel->eenv_volume < 15)
            return 0;

        channel->eenv_volume = -15;

        return 0;
    }

    // Release
    if (channel->eenv_rr == 0)
    {
        channel->eenv_volume = -15; // When RR = 0, immediately mute
        return 0;
    }

    if (--channel->eenv_rr != 0)
        return 0;

    channel->eenv_rr = channel->eenv_rrc;
    channel->eenv_volume--;

    if (channel->eenv_volume >= -15 && channel->eenv_volume < 15)
        return 0;

    channel->eenv_volume = -15;

    return 0;
}

int PMD::ExtendedSSGPCMSoftwareEnvelopeMain(Channel * channel)
{
    if (channel->eenv_count == 0)
        return 0;

    int dl = channel->eenv_volume;

    ExtendedSSGPCMSoftwareEnvelopeSub(channel, channel->eenv_count);

    if (dl == channel->eenv_volume)
        return 0;

    return -1;
}

void PMD::ExtendedSSGPCMSoftwareEnvelopeSub(Channel * channel, int ah)
{
    if (--ah == 0)
    {
        // Attack Rate
        if (channel->eenv_arc > 0)
        {
            channel->eenv_volume += channel->eenv_arc;

            if (channel->eenv_volume < 15)
            {
                channel->eenv_arc = channel->eenv_ar - 16;
                return;
            }

            channel->eenv_volume = 15;
            channel->eenv_count++;

            if (channel->eenv_sl != 15)
                return;    // If SL=0, immediately go to SR

            channel->eenv_count++;

            return;
        }
        else
        {
            if (channel->eenv_ar == 0)
                return;

            channel->eenv_arc++;

            return;
        }
    }

    if (--ah == 0)
    {
        // Decay Rate
        if (channel->eenv_drc > 0)
        {
            channel->eenv_volume -= channel->eenv_drc; // Count CHECK if less than 0

            if (channel->eenv_volume < 0 || channel->eenv_volume < channel->eenv_sl)
            {
                channel->eenv_volume = channel->eenv_sl;
                channel->eenv_count++;

                return;
            }

            if (channel->eenv_dr < 16)
                channel->eenv_drc = (channel->eenv_dr - 16) * 2;
            else
                channel->eenv_drc = channel->eenv_dr - 16;

            return;
        }

        if (channel->eenv_dr == 0)
            return;

        channel->eenv_drc++;

        return;
    }

    if (--ah == 0)
    {
        // Sustain Rate
        if (channel->eenv_src > 0)
        {
            // Count CHECK if less than 0
            if ((channel->eenv_volume -= channel->eenv_src) < 0)
                channel->eenv_volume = 0;

            if (channel->eenv_sr < 16)
                channel->eenv_src = (channel->eenv_sr - 16) * 2;
            else
                channel->eenv_src = channel->eenv_sr - 16;

            return;
        }

        if (channel->eenv_sr == 0)
            return;  // SR=0?

        channel->eenv_src++;

        return;
    }

    // Release Rate
    if (channel->eenv_rrc > 0)
    {
        // Count CHECK if less than 0
        if ((channel->eenv_volume -= channel->eenv_rrc) < 0)
            channel->eenv_volume = 0;

        channel->eenv_rrc = (channel->eenv_rr) * 2 - 16;

        return;
    }

    if (channel->eenv_rr == 0)
        return;

    channel->eenv_rrc++;
}
