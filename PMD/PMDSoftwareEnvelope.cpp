
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
    if (channel->ExtendMode & 0x04)
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
    if (channel->SSGEnvelopFlag == -1)
        return ExtendedSSGPCMSoftwareEnvelopeMain(channel);

    int dl = channel->ExtendedAttackLevel;

    SSGPCMSoftwareEnvelopeSub(channel);

    if (dl == channel->ExtendedAttackLevel)
        return 0;

    return -1;
}

int PMD::SSGPCMSoftwareEnvelopeSub(Channel * channel)
{
    if (channel->SSGEnvelopFlag == 0)
    {
        // Attack
        if (--channel->AttackDuration != 0)
            return 0;

        channel->SSGEnvelopFlag = 1;
        channel->ExtendedAttackLevel = channel->DecayDepth;

        return 1;
    }

    if (channel->SSGEnvelopFlag != 2)
    {
        // Decay
        if (channel->SustainRate == 0)
            return 0;  // No attenuation when DR=0

        if (--channel->SustainRate != 0)
            return 0;

        channel->SustainRate = channel->ExtendedSustainRate;
        channel->ExtendedAttackLevel--;

        if (channel->ExtendedAttackLevel >= -15 || channel->ExtendedAttackLevel < 15)
            return 0;

        channel->ExtendedAttackLevel = -15;

        return 0;
    }

    // Release
    if (channel->ReleaseRate == 0)
    {
        channel->ExtendedAttackLevel = -15; // When RR = 0, immediately mute
        return 0;
    }

    if (--channel->ReleaseRate != 0)
        return 0;

    channel->ReleaseRate = channel->ExtendedReleaseRate;
    channel->ExtendedAttackLevel--;

    if (channel->ExtendedAttackLevel >= -15 && channel->ExtendedAttackLevel < 15)
        return 0;

    channel->ExtendedAttackLevel = -15;

    return 0;
}

int PMD::ExtendedSSGPCMSoftwareEnvelopeMain(Channel * channel)
{
    if (channel->ExtendedCount == 0)
        return 0;

    int dl = channel->ExtendedAttackLevel;

    ExtendedSSGPCMSoftwareEnvelopeSub(channel, channel->ExtendedCount);

    if (dl == channel->ExtendedAttackLevel)
        return 0;

    return -1;
}

void PMD::ExtendedSSGPCMSoftwareEnvelopeSub(Channel * channel, int ah)
{
    if (--ah == 0)
    {
        // Attack Rate
        if (channel->ExtendedAttackDuration > 0)
        {
            channel->ExtendedAttackLevel += channel->ExtendedAttackDuration;

            if (channel->ExtendedAttackLevel < 15)
            {
                channel->ExtendedAttackDuration = channel->AttackDuration - 16;
                return;
            }

            channel->ExtendedAttackLevel = 15;
            channel->ExtendedCount++;

            if (channel->SustainLevel != 15)
                return;    // If SL=0, immediately go to SR

            channel->ExtendedCount++;

            return;
        }
        else
        {
            if (channel->AttackDuration == 0)
                return;

            channel->ExtendedAttackDuration++;

            return;
        }
    }

    if (--ah == 0)
    {
        // Decay Rate
        if (channel->ExtendedDecayDepth > 0)
        {
            channel->ExtendedAttackLevel -= channel->ExtendedDecayDepth; // Count CHECK if less than 0

            if (channel->ExtendedAttackLevel < 0 || channel->ExtendedAttackLevel < channel->SustainLevel)
            {
                channel->ExtendedAttackLevel = channel->SustainLevel;
                channel->ExtendedCount++;

                return;
            }

            if (channel->DecayDepth < 16)
                channel->ExtendedDecayDepth = (channel->DecayDepth - 16) * 2;
            else
                channel->ExtendedDecayDepth = channel->DecayDepth - 16;

            return;
        }

        if (channel->DecayDepth == 0)
            return;

        channel->ExtendedDecayDepth++;

        return;
    }

    if (--ah == 0)
    {
        // Sustain Rate
        if (channel->ExtendedSustainRate > 0)
        {
            // Count CHECK if less than 0
            if ((channel->ExtendedAttackLevel -= channel->ExtendedSustainRate) < 0)
                channel->ExtendedAttackLevel = 0;

            if (channel->SustainRate < 16)
                channel->ExtendedSustainRate = (channel->SustainRate - 16) * 2;
            else
                channel->ExtendedSustainRate = channel->SustainRate - 16;

            return;
        }

        if (channel->SustainRate == 0)
            return;  // SR=0?

        channel->ExtendedSustainRate++;

        return;
    }

    // Release Rate
    if (channel->ExtendedReleaseRate > 0)
    {
        // Count CHECK if less than 0
        if ((channel->ExtendedAttackLevel -= channel->ExtendedReleaseRate) < 0)
            channel->ExtendedAttackLevel = 0;

        channel->ExtendedReleaseRate = (channel->ReleaseRate) * 2 - 16;

        return;
    }

    if (channel->ReleaseRate == 0)
        return;

    channel->ExtendedReleaseRate++;
}
