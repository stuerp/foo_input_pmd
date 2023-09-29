
// OPNA module with waiting / Based on PMDWin code by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <cmath>
#include <algorithm>

#include "OPNAW.h"

// Declare if you want linear interpolation in this unit. Added because fmgen 007 deprecated linear interpolation.
#define USE_INTERPOLATION

/// <summary>
/// Initializes the module.
/// </summary>
bool OPNAW::Init(uint32_t c, uint32_t synthesisRate, bool ipflag, const WCHAR * directoryPath)
{
    Reset();

    _OutputRate = synthesisRate;

#ifdef USE_INTERPOLATION
    return ipflag ? OPNA::Init(c, SOUND_55K_2, false, directoryPath) : OPNA::Init(c, synthesisRate, false, directoryPath);
#else
    return OPNA::Init(c, r, ipflag, directoryPath);
#endif
}

/// <summary>
/// Sets the synthesis rate.
/// </summary>
bool OPNAW::SetRate(uint32_t c, uint32_t synthesisRate, bool useFM55kHzSynthesis)
{
    SetFMWait(_FMWait);
    SetSSGWait(_SSGWait);
    SetRhythmWait(_RhythmWait);
    SetADPCMWait(_ADPCMWait);

    interpolation2 = useFM55kHzSynthesis;
    _OutputRate = synthesisRate;

    // Sampling theorem and provisional setting of LPF.
    ffirst = true;

#ifdef USE_INTERPOLATION
    bool Result = useFM55kHzSynthesis ? OPNA::SetRate(c, SOUND_55K_2, false) : OPNA::SetRate(c, synthesisRate, false);
#else
    bool Result = OPNA::SetRate(c, r, useFM55kHzSynthesis);
#endif
/*
    _FMWaitCount = (int) (_FMWait * _SynthesisRate / 1000000);
    _SSGWaitCount = (int) (_SSGWait * _SynthesisRate / 1000000);
    _RhythmWaitCount = (int) (_RhythmWait * _SynthesisRate / 1000000);
    _ADPCMWaitCount = (int) (_ADPCMWait * _SynthesisRate / 1000000);
*/
    return Result;
}

/// <summary>
/// Sets the FM delay.
/// </summary>
void OPNAW::SetFMWait(int ns)
{
    _FMWait      = ns;
    _FMWaitCount = (int) (ns * _SynthesisRate / 1000000);
}

/// <summary>
/// Sets the SSG delay.
/// </summary>
void OPNAW::SetSSGWait(int ns)
{
    _SSGWait      = ns;
    _SSGWaitCount = (int) (ns * _SynthesisRate / 1000000);
}

/// <summary>
/// Sets the ADPCM delay.
/// </summary>
void OPNAW::SetADPCMWait(int ns)
{
    _ADPCMWait      = ns;
    _ADPCMWaitCount = (int) (ns * _SynthesisRate / 1000000);
}

/// <summary>
/// Sets the Rhythm delay.
/// </summary>
void OPNAW::SetRhythmWait(int ns)
{
    _RhythmWait      = ns;
    _RhythmWaitCount = (int) (ns * _SynthesisRate / 1000000);
}

/// <summary>
/// Sets the value of a register.
/// </summary>
void OPNAW::SetReg(uint32_t addr, uint32_t value)
{
    if (addr < 0x10)
    {   // SSG
        if (_SSGWaitCount != 0)
            CalcWaitPCM(_SSGWaitCount);
    }
    else
    if ((addr % 0x100) <= 0x10)
    {   // ADPCM
        if (_ADPCMWaitCount !=0)
            CalcWaitPCM(_ADPCMWaitCount);
    }
    else
    if (addr < 0x20)
    {   // RHYTHM
        if (_RhythmWaitCount != 0)
            CalcWaitPCM(_RhythmWaitCount);
    }
    else
    {   // FM
        if (_FMWaitCount != 0)
            CalcWaitPCM(_FMWaitCount);
    }

    OPNA::SetReg(addr, value);
}

/// <summary>
/// Synthesizes a buffer with samples.
/// </summary>
void OPNAW::Mix(Sample * sampleData, int sampleCount)
{
#ifdef USE_INTERPOLATION

    if (interpolation2 && (_OutputRate != SOUND_55K_2))
    {
    #if 0  
        int  nmixdata2;

        while (sampleCount > 0)
        {
            int nmixdata = (int) (delta + ((int64_t) sampleCount) * (SOUND_55K_2 * 16384 / _OutputRate)) / 16384;

            if (nmixdata > (IP_PCM_BUFFER_SIZE - 1))
            {
                int snsamples = (IP_PCM_BUFFER_SIZE - 2) * _OutputRate / SOUND_55K_2;
                nmixdata = (delta + (snsamples) * (SOUND_55K_2 * 16384 / _OutputRate)) / 16384;
            }

            ::memset(&ip_buffer[2], 0, sizeof(Sample) * 2 * nmixdata);
            MixInternal(&ip_buffer[2], nmixdata);

            nmixdata2 = 0;

            while (nmixdata > nmixdata2)
            {
                *sampleData++ += (ip_buffer[nmixdata2 * 2] * (16384 - delta) + ip_buffer[nmixdata2 * 2 + 2] * delta) / 16384;
                *sampleData++ += (ip_buffer[nmixdata2 * 2 + 1] * (16384 - delta) + ip_buffer[nmixdata2 * 2 + 3] * delta) / 16384;
                delta += SOUND_55K_2 * 16384 / _OutputRate;
                nmixdata2 += delta / 16384;
                delta %= 16384;
                sampleCount--;
            }

            ip_buffer[0] = ip_buffer[nmixdata * 2];
            ip_buffer[1] = ip_buffer[nmixdata * 2 + 1];
        }
    #endif

        for (int i = 0; i < sampleCount; i++)
        {
            int irest = (int) rest;

            if (write_pos_ip - (irest + NUMOFINTERPOLATION) < 0)
            {
                int nrefill = (int) (rest + (sampleCount - i - 1) * ((double) SOUND_55K_2 / _OutputRate)) + NUMOFINTERPOLATION - write_pos_ip;

                if (write_pos_ip + nrefill - IP_PCM_BUFFER_SIZE > irest)
                    nrefill = irest + IP_PCM_BUFFER_SIZE - write_pos_ip;

                // Replenishment
                int nrefill1 = (std::min) (IP_PCM_BUFFER_SIZE - (write_pos_ip % IP_PCM_BUFFER_SIZE), nrefill);
                int nrefill2 = nrefill - nrefill1;

                ::memset(&ip_buffer[(write_pos_ip % IP_PCM_BUFFER_SIZE) * 2], 0, sizeof(Sample) * 2 * nrefill1);
                MixInternal(&ip_buffer[(write_pos_ip % IP_PCM_BUFFER_SIZE) * 2], nrefill1);

                ::memset(&ip_buffer[0 * 2], 0, sizeof(Sample) * 2 * nrefill2);
                MixInternal(&ip_buffer[0], nrefill2);

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

            *sampleData++ += Limit((int) tempL, 32767, -32768);
            *sampleData++ += Limit((int) tempR, 32767, -32768);

            rest += (double) SOUND_55K_2 / _OutputRate;
        }

    }
    else
        MixInternal(sampleData, sampleCount);
#else
    MixInternal(sampleData, sampleCount);
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

#pragma region("Private")
/// <summary>
/// Resets the module.
/// </summary>
void OPNAW::Reset() noexcept
{
    ::memset(pre_buffer, 0, sizeof(pre_buffer));

    _FMWait = 0;
    _SSGWait = 0;
    _RhythmWait = 0;
    _ADPCMWait = 0;

    _FMWaitCount = 0;
    _SSGWaitCount = 0;
    _RhythmWaitCount = 0;
    _ADPCMWaitCount = 0;

    read_pos = 0;
    write_pos = 0;
    count2 = 0;

    ::memset(ip_buffer, 0, sizeof(ip_buffer));

    _OutputRate = 0;
    interpolation2 = false;
    delta = 0;
    delta_double = 0.0;

    // Sampling theorem and provisional setting of LPF
    ffirst = true;
    rest = 0;
    write_pos_ip = NUMOFINTERPOLATION;
}

/// <summary>
/// Calculates the PCM delay whenever a register gets set.
/// </summary>
void OPNAW::CalcWaitPCM(int value)
{
    count2 += value % 1000;
    value /= 1000;

    if (count2 > 1000)
    {
        value++;
        count2 -= 1000;
    }

    int SampleCount;

    do
    {
        if (write_pos + value > WAIT_PCM_BUFFER_SIZE)
            SampleCount = WAIT_PCM_BUFFER_SIZE - write_pos;
        else
            SampleCount = value;

        ::memset(&pre_buffer[write_pos * 2], 0, (size_t) SampleCount * 2 * sizeof(Sample));

        OPNA::Mix(&pre_buffer[write_pos * 2], SampleCount);

        write_pos += SampleCount;

        if (write_pos == WAIT_PCM_BUFFER_SIZE)
            write_pos = 0;

        value -= SampleCount;
    }
    while (value > 0);
}

/// <summary>
/// Sinc function
/// </summary>
double OPNAW::Sinc(double x)
{
#define M_PI 3.14159265358979323846

    return (x != 0.0) ? sin(M_PI * x) / (M_PI * x) : 1.0;
}

/// <summary>
/// Least nonnegative remainder
/// </summary>
double OPNAW::Fmod2(double x, double y)
{
    return x - std::floor((double) x / y) * y;
}

/// <summary>
/// Synthesizes a buffer without primary interpolation.
/// </summary>
void OPNAW::MixInternal(Sample * sampleData, int sampleCount)
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

        if (bufsamples > sampleCount)
            bufsamples = sampleCount;

        do
        {
            if (read_pos + bufsamples > WAIT_PCM_BUFFER_SIZE)
                outsamples = WAIT_PCM_BUFFER_SIZE - read_pos;
            else
                outsamples = bufsamples;

            for (int i = 0; i < outsamples * 2; i++)
                *sampleData++ += pre_buffer[read_pos * 2 + i];

        //  memcpy(buffer, &pre_buffer[read_pos * 2], outsamples * 2 * sizeof(Sample));

            read_pos += outsamples;

            if (read_pos == WAIT_PCM_BUFFER_SIZE)
                read_pos = 0;

            sampleCount -= outsamples;
            bufsamples -= outsamples;
        }
        while (bufsamples > 0);
    }

    OPNA::Mix(sampleData, sampleCount);
}
#pragma endregion
