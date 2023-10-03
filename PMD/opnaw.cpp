
// OPNA emulator with waiting (Based on PMDWin code by C60)

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
bool OPNAW::Initialize(uint32_t clock, uint32_t synthesisRate, bool useInterpolation, const WCHAR * directoryPath)
{
    Reset();

    _OutputRate = synthesisRate;

#ifdef USE_INTERPOLATION
    return OPNA::Initialize(clock, useInterpolation ? SOUND_55K_2 : synthesisRate, false, directoryPath);
#else
    return OPNA::Init(clock, synthesisRate, useInterpolation, directoryPath);
#endif
}

/// <summary>
/// Sets the synthesis rate.
/// </summary>
bool OPNAW::SetRate(uint32_t clock, uint32_t synthesisRate, bool useFM55kHzSynthesis)
{
    SetFMDelay(_FMDelay);
    SetSSGDelay(_SSGDelay);
    SetRhythmDelay(_RSSDelay);
    SetADPCMDelay(_ADPCMDelay);

    _interpolation2 = useFM55kHzSynthesis;
    _OutputRate = synthesisRate;

    // Sampling theorem and provisional setting of LPF.
    _ffirst = true;

#ifdef USE_INTERPOLATION
    bool Result =  OPNA::SetRate(clock, useFM55kHzSynthesis ? SOUND_55K_2 : synthesisRate, false);
#else
    bool Result = OPNA::SetRate(clock, synthesisRate, useFM55kHzSynthesis);
#endif
/*
    _FMDelayCount = (int) (_FMWait * _SynthesisRate / 1000000);
    _SSGDelayCount = (int) (_SSGDelay * _SynthesisRate / 1000000);
    _RSSDelayCount = (int) (_RSSDelay * _SynthesisRate / 1000000);
    _ADPCMDelayCount = (int) (_ADPCMDelay * _SynthesisRate / 1000000);
*/
    return Result;
}

/// <summary>
/// Sets the FM delay.
/// </summary>
void OPNAW::SetFMDelay(int ns)
{
    _FMDelay      = ns;
    _FMDelayCount = (int) (ns * _SynthesisRate / 1000000);
}

/// <summary>
/// Sets the SSG delay.
/// </summary>
void OPNAW::SetSSGDelay(int ns)
{
    _SSGDelay      = ns;
    _SSGDelayCount = (int) (ns * _SynthesisRate / 1000000);
}

/// <summary>
/// Sets the ADPCM delay.
/// </summary>
void OPNAW::SetADPCMDelay(int ns)
{
    _ADPCMDelay      = ns;
    _ADPCMDelayCount = (int) (ns * _SynthesisRate / 1000000);
}

/// <summary>
/// Sets the Rhythm delay.
/// </summary>
void OPNAW::SetRhythmDelay(int ns)
{
    _RSSDelay      = ns;
    _RSSDelayCount = (int) (ns * _SynthesisRate / 1000000);
}

/// <summary>
/// Sets the value of a register.
/// </summary>
void OPNAW::SetReg(uint32_t addr, uint32_t value)
{
    if (addr < 0x10)
    {   // SSG
        if (_SSGDelayCount != 0)
            CalcWaitPCM(_SSGDelayCount);
    }
    else
    if ((addr % 0x100) <= 0x10)
    {   // ADPCM
        if (_ADPCMDelayCount !=0)
            CalcWaitPCM(_ADPCMDelayCount);
    }
    else
    if (addr < 0x20)
    {   // RHYTHM
        if (_RSSDelayCount != 0)
            CalcWaitPCM(_RSSDelayCount);
    }
    else
    {   // FM
        if (_FMDelayCount != 0)
            CalcWaitPCM(_FMDelayCount);
    }

    OPNA::SetReg(addr, value);
}

/// <summary>
/// Synthesizes a buffer with samples.
/// </summary>
void OPNAW::Mix(Sample * sampleData, size_t sampleCount) noexcept
{
#ifdef USE_INTERPOLATION
    if (_interpolation2 && (_OutputRate != SOUND_55K_2))
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
            size_t irest = (size_t) _Rest;

            if ((irest + NUMOFINTERPOLATION) > _InterpolationIndex)
            {
                size_t nrefill = (size_t) (_Rest + (double) (sampleCount - i - 1) * ((double) SOUND_55K_2 / _OutputRate)) + NUMOFINTERPOLATION - _InterpolationIndex;

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

                temps = Sinc((double) j - _Rest - NUMOFINTERPOLATION / 2 + 1);

                tempL += temps * _InterpolationBuffer[(j % IP_PCM_BUFFER_SIZE) * 2];
                tempR += temps * _InterpolationBuffer[(j % IP_PCM_BUFFER_SIZE) * 2 + 1];
            }

            *sampleData++ += Limit((int) tempL, 32767, -32768);
            *sampleData++ += Limit((int) tempR, 32767, -32768);

            _Rest += (double) SOUND_55K_2 / _OutputRate;
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

    _Rest = 0.;
    _InterpolationIndex = NUMOFINTERPOLATION;
}

#pragma region("Private")
/// <summary>
/// Resets the module.
/// </summary>
void OPNAW::Reset() noexcept
{
    ::memset(_PreBuffer, 0, sizeof(_PreBuffer));

    _FMDelay = 0;
    _SSGDelay = 0;
    _RSSDelay = 0;
    _ADPCMDelay = 0;

    _FMDelayCount = 0;
    _SSGDelayCount = 0;
    _RSSDelayCount = 0;
    _ADPCMDelayCount = 0;

    _ReadIndex = 0;
    _WriteIndex = 0;
    _Counter = 0;

    ::memset(_InterpolationBuffer, 0, sizeof(_InterpolationBuffer));

    _OutputRate = 0;
    _interpolation2 = false;
    _delta = 0;
    _delta_double = 0.0;

    // Sampling theorem and provisional setting of LPF
    _ffirst = true;
    
    _Rest = 0.;
    _InterpolationIndex = NUMOFINTERPOLATION;
}

/// <summary>
/// Calculates the PCM delay whenever a register gets set.
/// </summary>
void OPNAW::CalcWaitPCM(int value)
{
    _Counter += value % 1000;
    value /= 1000;

    if (_Counter > 1000)
    {
        value++;
        _Counter -= 1000;
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
/// Least non-negative remainder
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
