
// Based on PMDWin code by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <cmath>
#include <algorithm>

#include "OPNAW.h"

#define M_PI 3.14159265358979323846

// Declare if you want linear interpolation in this unit. Added because fmgen 007 deprecated linear interpolation
#define INTERPOLATION_IN_THIS_UNIT

bool OPNAW::Init(uint32_t c, uint32_t r, bool ipflag, const WCHAR * directoryPath)
{
    InitializeInternal();

    rate2 = r;

#ifdef INTERPOLATION_IN_THIS_UNIT
    return ipflag ? OPNA::Init(c, SOUND_55K_2, false, directoryPath) : OPNA::Init(c, r, false, directoryPath);
#else
    return OPNA::Init(c, r, ipflag, directoryPath);
#endif
}

void OPNAW::InitializeInternal()
{
    ::memset(pre_buffer, 0, sizeof(pre_buffer));

    fmwait = 0;
    ssgwait = 0;
    rhythmwait = 0;
    adpcmwait = 0;

    fmwaitcount = 0;
    ssgwaitcount = 0;
    rhythmwaitcount = 0;
    adpcmwaitcount = 0;

    read_pos = 0;
    write_pos = 0;
    count2 = 0;

    ::memset(ip_buffer, 0, sizeof(ip_buffer));

    rate2 = 0;
    interpolation2 = false;
    delta = 0;
    delta_double = 0.0;

    // Sampling theorem and provisional setting of LPF
    ffirst = true;
    rest = 0;
    write_pos_ip = NUMOFINTERPOLATION;
}

/// <summary>
/// Sets the synthesis rate.
/// </summary>
bool OPNAW::SetRate(uint32_t c, uint32_t r, bool ipflag)
{
    bool result;

    SetFMWait(fmwait);
    SetSSGWait(ssgwait);
    SetRhythmWait(rhythmwait);
    SetADPCMWait(adpcmwait);

    interpolation2 = ipflag;
    rate2 = r;

    // Sampling theorem and provisional setting of LPF.
    ffirst = true;

#ifdef INTERPOLATION_IN_THIS_UNIT
    result = ipflag ? OPNA::SetRate(c, SOUND_55K_2, false) : OPNA::SetRate(c, r, false);
#else
    result = OPNA::SetRate(c, r, ipflag);
#endif

    fmwaitcount = fmwait * _SynthesisRate / 1000000;
    ssgwaitcount = ssgwait * _SynthesisRate / 1000000;
    rhythmwaitcount = rhythmwait * _SynthesisRate / 1000000;
    adpcmwaitcount = adpcmwait * _SynthesisRate / 1000000;

    return result;
}

void OPNAW::SetFMWait(int nsec)
{
    fmwait      = nsec;
    fmwaitcount = nsec * _SynthesisRate / 1000000;
}

void OPNAW::SetSSGWait(int nsec)
{
    ssgwait      = nsec;
    ssgwaitcount = nsec * _SynthesisRate / 1000000;
}

void OPNAW::SetRhythmWait(int nsec)
{
    rhythmwait      = nsec;
    rhythmwaitcount = nsec * _SynthesisRate / 1000000;
}

void OPNAW::SetADPCMWait(int nsec)
{
    adpcmwait      = nsec;
    adpcmwaitcount = nsec * _SynthesisRate / 1000000;
}

//Set data in register array
void OPNAW::SetReg(uint32_t addr, uint32_t data)
{
    if (addr < 0x10)
    {   // SSG
        if (ssgwaitcount)
            CalcWaitPCM(ssgwaitcount);
    }
    else
    if ((addr % 0x100) <= 0x10)
    {   // ADPCM
        if (adpcmwaitcount)
            CalcWaitPCM(adpcmwaitcount);
    }
    else
    if (addr < 0x20)
    {   // RHYTHM
        if (rhythmwaitcount)
            CalcWaitPCM(rhythmwaitcount);
    }
    else
    {   // FM
        if (fmwaitcount)
            CalcWaitPCM(fmwaitcount);
    }

    OPNA::SetReg(addr, data);
}

// Calculate PCM when SetReg() wait
void OPNAW::CalcWaitPCM(int waitcount)
{
    count2 += waitcount % 1000;
    waitcount /= 1000;

    if (count2 > 1000)
    {
        waitcount++;
        count2 -= 1000;
    }

    int outsamples;

    do
    {
        if (write_pos + waitcount > WAIT_PCM_BUFFER_SIZE)
            outsamples = WAIT_PCM_BUFFER_SIZE - write_pos;
        else
            outsamples = waitcount;

        ::memset(&pre_buffer[write_pos * 2], 0, outsamples * 2 * sizeof(Sample));

        OPNA::Mix(&pre_buffer[write_pos * 2], outsamples);

        write_pos += outsamples;

        if (write_pos == WAIT_PCM_BUFFER_SIZE)
            write_pos = 0;

        waitcount -= outsamples;
    }
    while (waitcount > 0);
}

// Sinc function
double OPNAW::Sinc(double x)
{
    return (x != 0.0) ? sin(M_PI * x) / (M_PI * x) : 1.0;
}

// Synthesis (Ver. without primary interpolation)
void OPNAW::_Mix(Sample * buffer, int nsamples)
{
    if (read_pos != write_pos)
    {
        int bufsamples;
        int outsamples;

        // Output from buffer
        if (read_pos < write_pos)
            bufsamples = write_pos - read_pos;
        else
            bufsamples = write_pos - read_pos + WAIT_PCM_BUFFER_SIZE;

        if (bufsamples > nsamples)
            bufsamples = nsamples;

        do
        {
            if (read_pos + bufsamples > WAIT_PCM_BUFFER_SIZE)
                outsamples = WAIT_PCM_BUFFER_SIZE - read_pos;
            else
                outsamples = bufsamples;

            for (int i = 0; i < outsamples * 2; i++)
                *buffer++ += pre_buffer[read_pos * 2 + i];

        //  memcpy(buffer, &pre_buffer[read_pos * 2], outsamples * 2 * sizeof(Sample));

            read_pos += outsamples;

            if (read_pos == WAIT_PCM_BUFFER_SIZE)
                read_pos = 0;

            nsamples -= outsamples;
            bufsamples -= outsamples;
        }
        while (bufsamples > 0);
    }

    OPNA::Mix(buffer, nsamples);
}

// Least nonnegative remainder
double OPNAW::Fmod2(double x, double y)
{
    return x - std::floor((double) x / y) * y;
}

// Synthesis
void OPNAW::Mix(Sample * buffer, int nsamples)
{
#ifdef INTERPOLATION_IN_THIS_UNIT

    if (interpolation2 && rate2 != SOUND_55K_2)
    {
    #if 0  
        int  nmixdata2;

        while (nsamples > 0)
        {
            int nmixdata = (int) (delta + ((int64_t) nsamples) * (SOUND_55K_2 * 16384 / rate2)) / 16384;

            if (nmixdata > (IP_PCM_BUFFER_SIZE - 1))
            {
                int snsamples = (IP_PCM_BUFFER_SIZE - 2) * rate2 / SOUND_55K_2;
                nmixdata = (delta + (snsamples) * (SOUND_55K_2 * 16384 / rate2)) / 16384;
            }

            ::memset(&ip_buffer[2], 0, sizeof(Sample) * 2 * nmixdata);
            _Mix(&ip_buffer[2], nmixdata);

            nmixdata2 = 0;

            while (nmixdata > nmixdata2)
            {
                *buffer++ += (ip_buffer[nmixdata2 * 2] * (16384 - delta) + ip_buffer[nmixdata2 * 2 + 2] * delta) / 16384;
                *buffer++ += (ip_buffer[nmixdata2 * 2 + 1] * (16384 - delta) + ip_buffer[nmixdata2 * 2 + 3] * delta) / 16384;
                delta += SOUND_55K_2 * 16384 / rate2;
                nmixdata2 += delta / 16384;
                delta %= 16384;
                nsamples--;
            }

            ip_buffer[0] = ip_buffer[nmixdata * 2];
            ip_buffer[1] = ip_buffer[nmixdata * 2 + 1];
        }
    #endif

        for (int i = 0; i < nsamples; i++)
        {
            int irest = (int) rest;

            if (write_pos_ip - (irest + NUMOFINTERPOLATION) < 0)
            {
                int nrefill = (int) (rest + (nsamples - i - 1) * ((double) SOUND_55K_2 / rate2)) + NUMOFINTERPOLATION - write_pos_ip;

                if (write_pos_ip + nrefill - IP_PCM_BUFFER_SIZE > irest)
                    nrefill = irest + IP_PCM_BUFFER_SIZE - write_pos_ip;

                // Replenishment
                int nrefill1 = (std::min) (IP_PCM_BUFFER_SIZE - (write_pos_ip % IP_PCM_BUFFER_SIZE), nrefill);
                int nrefill2 = nrefill - nrefill1;

                ::memset(&ip_buffer[(write_pos_ip % IP_PCM_BUFFER_SIZE) * 2], 0, sizeof(Sample) * 2 * nrefill1);
                _Mix(&ip_buffer[(write_pos_ip % IP_PCM_BUFFER_SIZE) * 2], nrefill1);

                ::memset(&ip_buffer[0 * 2], 0, sizeof(Sample) * 2 * nrefill2);
                _Mix(&ip_buffer[0], nrefill2);

                write_pos_ip += nrefill;
            }

            double tempL = 0;
            double tempR = 0;

            for (int j = irest; j < irest + NUMOFINTERPOLATION; j++)
            {
                double temps;

                temps = Sinc((double) j - rest - NUMOFINTERPOLATION / 2 + 1);
                tempL += temps * ip_buffer[(j % IP_PCM_BUFFER_SIZE) * 2];
                tempR += temps * ip_buffer[(j % IP_PCM_BUFFER_SIZE) * 2 + 1];
            }

            *buffer++ += Limit((int) tempL, 32767, -32768);
            *buffer++ += Limit((int) tempR, 32767, -32768);

            rest += (double) SOUND_55K_2 / rate2;
        }

    }
    else
        _Mix(buffer, nsamples);
#else
    _Mix(buffer, nsamples);
#endif
}

// Clear internal buffer
void OPNAW::ClearBuffer()
{
    read_pos = write_pos = 0;

    ::memset(pre_buffer, 0, sizeof(pre_buffer));
    ::memset(ip_buffer, 0, sizeof(ip_buffer));

    rest = 0;
    write_pos_ip = NUMOFINTERPOLATION;
}
