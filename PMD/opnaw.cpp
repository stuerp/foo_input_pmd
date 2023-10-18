
/** $VER: OPNAW.h (2023.10.18) OPNA emulator with waiting (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <cmath>
#include <algorithm>

#include "OPNAW.h"

/// <summary>
/// Initializes the module.
/// </summary>
bool OPNAW::Initialize(uint32_t clock, uint32_t outputFrequency, bool useInterpolation, const WCHAR * directoryPath)
{
    Reset();

    _OutputFrequency = outputFrequency;

    return OPNA::Initialize(clock, useInterpolation ? FREQUENCY_55_4K : outputFrequency, false, directoryPath);
}

/// <summary>
/// Sets the output frequency.
/// </summary>
void OPNAW::SetOutputFrequency(uint32_t clock, uint32_t outputFrequency, bool useLinearInterpolation) noexcept
{
    _OutputFrequency = outputFrequency;
    _UseLinearInterpolation = useLinearInterpolation;

    OPNA::SetOutputFrequency(clock, useLinearInterpolation ? FREQUENCY_55_4K : outputFrequency, false);

    SetFMDelay(_FMDelay);
    SetSSGDelay(_SSGDelay);
    SetRhythmDelay(_RSSDelay);
    SetADPCMDelay(_ADPCMDelay);
}

/// <summary>
/// Sets the FM delay.
/// </summary>
void OPNAW::SetFMDelay(int ns)
{
    _FMDelay      = ns;
    _FMDelayCount = (int) (ns * _OutputFrequency / 1000000);
}

/// <summary>
/// Sets the SSG delay.
/// </summary>
void OPNAW::SetSSGDelay(int ns)
{
    _SSGDelay      = ns;
    _SSGDelayCount = (int) (ns * _OutputFrequency / 1000000);
}

/// <summary>
/// Sets the ADPCM delay.
/// </summary>
void OPNAW::SetADPCMDelay(int ns)
{
    _ADPCMDelay      = ns;
    _ADPCMDelayCount = (int) (ns * _OutputFrequency / 1000000);
}

/// <summary>
/// Sets the Rhythm delay.
/// </summary>
void OPNAW::SetRhythmDelay(int ns)
{
    _RSSDelay      = ns;
    _RSSDelayCount = (int) (ns * _OutputFrequency / 1000000);
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
    {   // Rhythm
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
    if ((_OutputFrequency != FREQUENCY_55_4K) && _UseLinearInterpolation)
    {
        for (size_t i = 0; i < sampleCount; ++i)
        {
            size_t Rest = (size_t) _Rest;

            if ((Rest + SINC_INTERPOLATION_SAMPLE_COUNT) > _DstIndex)
            {
                size_t nrefill = (size_t) (_Rest + (double) (sampleCount - i - 1) * ((double) FREQUENCY_55_4K / _OutputFrequency)) + SINC_INTERPOLATION_SAMPLE_COUNT - _DstIndex;

                if (_DstIndex + nrefill - DST_PCM_BUFFER_SIZE > Rest)
                    nrefill = (size_t) Rest + DST_PCM_BUFFER_SIZE - _DstIndex;

                // Replenishment
                size_t nrefill1 = (std::min)((size_t) DST_PCM_BUFFER_SIZE - (_DstIndex % DST_PCM_BUFFER_SIZE), nrefill);

                ::memset(&_DstBuffer[(_DstIndex % DST_PCM_BUFFER_SIZE) * 2], 0, sizeof(Sample) * 2 * nrefill1);
                MixInternal(&_DstBuffer[(_DstIndex % DST_PCM_BUFFER_SIZE) * 2], nrefill1);

                size_t nrefill2 = nrefill - nrefill1;

                ::memset(_DstBuffer, 0, sizeof(Sample) * 2 * nrefill2);
                MixInternal(_DstBuffer, nrefill2);

                _DstIndex += nrefill;
            }

            {
                double tempL = 0;
                double tempR = 0;

                for (size_t j = Rest; j < Rest + SINC_INTERPOLATION_SAMPLE_COUNT; ++j)
                {
                    double Factor = sinc((double) j - _Rest - SINC_INTERPOLATION_SAMPLE_COUNT / 2 + 1);

                    tempL += Factor * _DstBuffer[(j % DST_PCM_BUFFER_SIZE) * 2];
                    tempR += Factor * _DstBuffer[(j % DST_PCM_BUFFER_SIZE) * 2 + 1];
                }

                *sampleData++ += Limit((int) tempL, 32767, -32768);
                *sampleData++ += Limit((int) tempR, 32767, -32768);
            }

            _Rest += (double) FREQUENCY_55_4K / _OutputFrequency;
        }
    }
    else
        MixInternal(sampleData, sampleCount);
}

void OPNAW::ClearBuffer()
{
    ::memset(_SrcBuffer, 0, sizeof(_SrcBuffer));
    _SrcReadIndex = _SrcWriteIndex = 0;

    ::memset(_DstBuffer, 0, sizeof(_DstBuffer));
    _DstIndex = SINC_INTERPOLATION_SAMPLE_COUNT;
    _Rest = 0.;
}

#pragma region("Private")
/// <summary>
/// Resets the module.
/// </summary>
void OPNAW::Reset() noexcept
{
    _OutputFrequency = 0;
    _UseLinearInterpolation = false;

    _FMDelay = 0;
    _SSGDelay = 0;
    _RSSDelay = 0;
    _ADPCMDelay = 0;

    _FMDelayCount = 0;
    _SSGDelayCount = 0;
    _RSSDelayCount = 0;
    _ADPCMDelayCount = 0;

    _Counter = 0;

    ClearBuffer();
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
        ++value;
        _Counter -= 1000;
    }

    do
    {
        size_t SampleCount = (_SrcWriteIndex + value > SRC_PCM_BUFFER_SIZE) ? SRC_PCM_BUFFER_SIZE - _SrcWriteIndex : (size_t) value;

        ::memset(&_SrcBuffer[_SrcWriteIndex * 2], 0, SampleCount * 2 * sizeof(Sample));

        OPNA::Mix(&_SrcBuffer[_SrcWriteIndex * 2], SampleCount);

        _SrcWriteIndex += SampleCount;

        if (_SrcWriteIndex == SRC_PCM_BUFFER_SIZE)
            _SrcWriteIndex = 0;

        value -= (int) SampleCount;
    }
    while (value > 0);
}

/// <summary>
/// Fills the sample buffer.
/// </summary>
void OPNAW::MixInternal(Sample * sampleData, size_t sampleCount) noexcept
{
    if (_SrcReadIndex != _SrcWriteIndex)
    {
        size_t MaxSamplesToDo = (_SrcReadIndex < _SrcWriteIndex) ? _SrcWriteIndex - _SrcReadIndex : _SrcWriteIndex - _SrcReadIndex + SRC_PCM_BUFFER_SIZE;

        if (MaxSamplesToDo > sampleCount)
            MaxSamplesToDo = sampleCount;

        do
        {
            size_t SamplesToDo = (_SrcReadIndex + MaxSamplesToDo > SRC_PCM_BUFFER_SIZE) ? SRC_PCM_BUFFER_SIZE - _SrcReadIndex : MaxSamplesToDo;

            for (size_t i = 0; i < SamplesToDo * 2; ++i)
                *sampleData++ += _SrcBuffer[_SrcReadIndex * 2 + i];

            _SrcReadIndex += SamplesToDo;

            if (_SrcReadIndex == SRC_PCM_BUFFER_SIZE)
                _SrcReadIndex = 0;

            sampleCount -= SamplesToDo;
            MaxSamplesToDo -= SamplesToDo;
        }
        while (MaxSamplesToDo != 0);
    }

    OPNA::Mix(sampleData, sampleCount);
}
#pragma endregion
