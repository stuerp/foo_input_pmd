
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
bool OPNAW::Init(uint32_t clock, uint32_t synthesisRate, bool useInterpolation, const WCHAR * directoryPath)
{
    Reset();

    _OutputRate = synthesisRate;

#ifdef USE_INTERPOLATION
    return OPNA::Init(clock, useInterpolation ? SOUND_55K_2 : synthesisRate, false, directoryPath);
#else
    return OPNA::Init(clock, synthesisRate, useInterpolation, directoryPath);
#endif
}

/// <summary>
/// Sets the synthesis rate.
/// </summary>
bool OPNAW::SetRate(uint32_t clock, uint32_t synthesisRate, bool useFM55kHzSynthesis)
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
    bool Result =  OPNA::SetRate(clock, useFM55kHzSynthesis ? SOUND_55K_2 : synthesisRate, false);
#else
    bool Result = OPNA::SetRate(clock, synthesisRate, useFM55kHzSynthesis);
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
void OPNAW::Mix(Sample * sampleData, size_t sampleCount) noexcept
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

        for (size_t i = 0; i < sampleCount; i++)
        {
            size_t irest = (size_t) rest;

            if ((irest + NUMOFINTERPOLATION) > _InterpolationIndex)
            {
                size_t nrefill = (size_t) (rest + (double) (sampleCount - i - 1) * ((double) SOUND_55K_2 / _OutputRate)) + NUMOFINTERPOLATION - _InterpolationIndex;

                if (_InterpolationIndex + nrefill - IP_PCM_BUFFER_SIZE > irest)
                    nrefill = (size_t) irest + IP_PCM_BUFFER_SIZE - _InterpolationIndex;

                // Replenishment
                size_t nrefill1 = (std::min)((size_t) IP_PCM_BUFFER_SIZE - (_InterpolationIndex % IP_PCM_BUFFER_SIZE), nrefill);

                size_t nrefill2 = nrefill - nrefill1;

                ::memset(&_InterpolationBuffer[(_InterpolationIndex % IP_PCM_BUFFER_SIZE) * 2], 0, sizeof(Sample) * 2 * nrefill1);
                MixInternal(&_InterpolationBuffer[(_InterpolationIndex % IP_PCM_BUFFER_SIZE) * 2], nrefill1);

                ::memset(&_InterpolationBuffer[0 * 2], 0, sizeof(Sample) * 2 * nrefill2);
                MixInternal(&_InterpolationBuffer[0], nrefill2);

                _InterpolationIndex += nrefill;
            }

            double tempL = 0;
            double tempR = 0;

            for (size_t j = irest; j < irest + NUMOFINTERPOLATION; ++j)
            {
                double temps;

                temps = Sinc((double) j - rest - NUMOFINTERPOLATION / 2 + 1);

                tempL += temps * _InterpolationBuffer[(j % IP_PCM_BUFFER_SIZE) * 2];
                tempR += temps * _InterpolationBuffer[(j % IP_PCM_BUFFER_SIZE) * 2 + 1];
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
    _ReadIndex = _WriteIndex = 0;

    ::memset(_PreBuffer, 0, sizeof(_PreBuffer));
    ::memset(_InterpolationBuffer, 0, sizeof(_InterpolationBuffer));

    rest = 0.;
    _InterpolationIndex = NUMOFINTERPOLATION;
}

#pragma region("Private")
/// <summary>
/// Resets the module.
/// </summary>
void OPNAW::Reset() noexcept
{
    ::memset(_PreBuffer, 0, sizeof(_PreBuffer));

    _FMWait = 0;
    _SSGWait = 0;
    _RhythmWait = 0;
    _ADPCMWait = 0;

    _FMWaitCount = 0;
    _SSGWaitCount = 0;
    _RhythmWaitCount = 0;
    _ADPCMWaitCount = 0;

    _ReadIndex = 0;
    _WriteIndex = 0;
    count2 = 0;

    ::memset(_InterpolationBuffer, 0, sizeof(_InterpolationBuffer));

    _OutputRate = 0;
    interpolation2 = false;
    delta = 0;
    delta_double = 0.0;

    // Sampling theorem and provisional setting of LPF
    ffirst = true;
    
    rest = 0.;
    _InterpolationIndex = NUMOFINTERPOLATION;
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

    size_t SampleCount;

    do
    {
        if (_WriteIndex + value > WAIT_PCM_BUFFER_SIZE)
            SampleCount = WAIT_PCM_BUFFER_SIZE - _WriteIndex;
        else
            SampleCount = (size_t) value;

        ::memset(&_PreBuffer[_WriteIndex * 2], 0, SampleCount * 2 * sizeof(Sample));

        OPNA::Mix(&_PreBuffer[_WriteIndex * 2], SampleCount);

        _WriteIndex += SampleCount;

        if (_WriteIndex == WAIT_PCM_BUFFER_SIZE)
            _WriteIndex = 0;

        value -= (int) SampleCount;
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
void OPNAW::MixInternal(Sample * sampleData, size_t sampleCount) noexcept
{
    if (_ReadIndex != _WriteIndex)
    {
        size_t bufsamples;
        size_t outsamples;

        // Output from buffer
        if (_ReadIndex < _WriteIndex)
            bufsamples = _WriteIndex - _ReadIndex;
        else
            bufsamples = _WriteIndex - _ReadIndex + WAIT_PCM_BUFFER_SIZE;

        if (bufsamples > sampleCount)
            bufsamples = sampleCount;

        do
        {
            if (_ReadIndex + bufsamples > WAIT_PCM_BUFFER_SIZE)
                outsamples = WAIT_PCM_BUFFER_SIZE - _ReadIndex;
            else
                outsamples = bufsamples;

            for (size_t i = 0; i < outsamples * 2; i++)
                *sampleData++ += _PreBuffer[_ReadIndex * 2 + i];

        //  memcpy(buffer, &_PreBuffer[_ReadIndex * 2], outsamples * 2 * sizeof(Sample));

            _ReadIndex += outsamples;

            if (_ReadIndex == WAIT_PCM_BUFFER_SIZE)
                _ReadIndex = 0;

            sampleCount -= outsamples;
            bufsamples -= outsamples;
        }
        while (bufsamples > 0);
    }

    OPNA::Mix(sampleData, sampleCount);
}
#pragma endregion
