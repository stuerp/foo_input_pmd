
/** $VER: OPNAW.cpp (2026.01.07) OPNA emulator with waiting (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#include <pch.h>

#include "OPNAW.h"

/// <summary>
/// Initializes the module.
/// </summary>
bool opnaw_t::Initialize(uint32_t clockSpeed, uint32_t sampleRate, bool useInterpolation, const WCHAR * directoryPathDrums) noexcept
{
    Reset();

    _SampleRate = sampleRate;
    _UseInterpolation = useInterpolation;

    return opna_t::Initialize(clockSpeed, useInterpolation ? FREQUENCY_55_4K : sampleRate, directoryPathDrums);
}

/// <summary>
/// Initializes the module.
/// </summary>
void opnaw_t::Initialize(uint32_t clockSpeed, uint32_t sampleRate, bool useInterpolation) noexcept
{
    _SampleRate = sampleRate;
    _UseInterpolation = useInterpolation;

    opna_t::Initialize(clockSpeed, useInterpolation ? FREQUENCY_55_4K : sampleRate);

    SetFMDelay(_FMDelay);
    SetSSGDelay(_SSGDelay);
    SetRhythmDelay(_RhythmDelay);
    SetADPCMDelay(_ADPCMDelay);
}

/// <summary>
/// Sets the FM delay.
/// </summary>
void opnaw_t::SetFMDelay(int ns) noexcept
{
    _FMDelay      = ns;
    _FMDelayCount = (int) (ns * _SampleRate / 1'000'000);
}

/// <summary>
/// Sets the SSG delay.
/// </summary>
void opnaw_t::SetSSGDelay(int ns) noexcept
{
    _SSGDelay      = ns;
    _SSGDelayCount = (int) (ns * _SampleRate / 1'000'000);
}

/// <summary>
/// Sets the ADPCM delay.
/// </summary>
void opnaw_t::SetADPCMDelay(int ns) noexcept
{
    _ADPCMDelay      = ns;
    _ADPCMDelayCount = (int) (ns * _SampleRate / 1'000'000);
}

/// <summary>
/// Sets the Rhythm delay.
/// </summary>
void opnaw_t::SetRhythmDelay(int ns) noexcept
{
    _RhythmDelay      = ns;
    _RhythmDelayCount = (int) (ns * _SampleRate / 1'000'000);
}

/// <summary>
/// Sets the value of a register.
/// </summary>
void opnaw_t::SetReg(uint32_t reg, uint32_t value) noexcept
{
    if (reg < 0x10)
    {   // SSG
        if (_SSGDelayCount != 0)
            CalcWaitPCM(_SSGDelayCount);
    }
    else
    if ((reg % 0x100) <= 0x10)
    {   // ADPCM
        if (_ADPCMDelayCount !=0)
            CalcWaitPCM(_ADPCMDelayCount);
    }
    else
    if (reg < 0x20)
    {   // Rhythm
        if (_RhythmDelayCount != 0)
            CalcWaitPCM(_RhythmDelayCount);
    }
    else
    {   // FM
        if (_FMDelayCount != 0)
            CalcWaitPCM(_FMDelayCount);
    }

    opna_t::SetReg(reg, value);
}

/// <summary>
/// Synthesizes a buffer with samples.
/// </summary>
void opnaw_t::Mix(sample_t * sampleData, size_t sampleCount) noexcept
{
    if ((_SampleRate != FREQUENCY_55_4K) && _UseInterpolation)
    {
        for (size_t i = 0; i < sampleCount; ++i)
        {
            size_t Rest = (size_t) _Rest;

            if ((Rest + SINC_INTERPOLATION_SAMPLE_COUNT) > _DstIndex)
            {
                size_t Refill = (size_t) (_Rest + (double) (sampleCount - i - 1) * ((double) FREQUENCY_55_4K / _SampleRate)) + SINC_INTERPOLATION_SAMPLE_COUNT - _DstIndex;

                if (_DstIndex + Refill - DST_PCM_BUFFER_SIZE > Rest)
                    Refill = (size_t) Rest + DST_PCM_BUFFER_SIZE - _DstIndex;

                // Replenishment
                size_t Refill1 = (std::min)((size_t) DST_PCM_BUFFER_SIZE - (_DstIndex % DST_PCM_BUFFER_SIZE), Refill);

                auto Samples = &_DstBuffer[(_DstIndex % DST_PCM_BUFFER_SIZE) * 2];

                ::memset(Samples, 0, sizeof(sample_t) * 2 * Refill1);
                MixInternal(Samples, Refill1);

                size_t Refill2 = Refill - Refill1;

                ::memset(_DstBuffer, 0, sizeof(sample_t) * 2 * Refill2);
                MixInternal(_DstBuffer, Refill2);

                _DstIndex += Refill;
            }

            {
                double SampleL = 0.;
                double SampleR = 0.;

                for (size_t j = Rest; j < Rest + SINC_INTERPOLATION_SAMPLE_COUNT; ++j)
                {
                    const double Factor = sinc((double) j - _Rest - SINC_INTERPOLATION_SAMPLE_COUNT / 2 + 1);

                    SampleL += Factor * _DstBuffer[(j % DST_PCM_BUFFER_SIZE) * 2];
                    SampleR += Factor * _DstBuffer[(j % DST_PCM_BUFFER_SIZE) * 2 + 1];
                }

                *sampleData++ += std::clamp((int) SampleL, -32768, 32767);
                *sampleData++ += std::clamp((int) SampleR, -32768, 32767);
            }

            _Rest += (double) FREQUENCY_55_4K / _SampleRate;
        }
    }
    else
        MixInternal(sampleData, sampleCount);
}

void opnaw_t::ClearBuffer() noexcept
{
    ::memset(_SrcBuffer, 0, sizeof(_SrcBuffer));
    _SrcReadIndex = _SrcWriteIndex = 0;

    ::memset(_DstBuffer, 0, sizeof(_DstBuffer));
    _DstIndex = SINC_INTERPOLATION_SAMPLE_COUNT;
    _Rest = 0.;
}

#pragma region Private

/// <summary>
/// Resets the module.
/// </summary>
void opnaw_t::Reset() noexcept
{
    _SampleRate = 0;
    _UseInterpolation = false;

    _FMDelay = 0;
    _SSGDelay = 0;
    _RhythmDelay = 0;
    _ADPCMDelay = 0;

    _FMDelayCount = 0;
    _SSGDelayCount = 0;
    _RhythmDelayCount = 0;
    _ADPCMDelayCount = 0;

    _Counter = 0;

    ClearBuffer();
}

/// <summary>
/// Calculates the PCM delay whenever a register gets set.
/// </summary>
void opnaw_t::CalcWaitPCM(int value)
{
    _Counter += value % 1'000;
    value /= 1'000;

    if (_Counter > 1'000)
    {
        ++value;
        _Counter -= 1'000;
    }

    do
    {
        const size_t SampleCount = (_SrcWriteIndex + value > SRC_PCM_BUFFER_SIZE) ? SRC_PCM_BUFFER_SIZE - _SrcWriteIndex : (size_t) value;

        ::memset(&_SrcBuffer[_SrcWriteIndex * 2], 0, SampleCount * 2 * sizeof(sample_t));

        opna_t::Mix(&_SrcBuffer[_SrcWriteIndex * 2], SampleCount);

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
void opnaw_t::MixInternal(sample_t * sampleData, size_t sampleCount) noexcept
{
    if (_SrcReadIndex != _SrcWriteIndex)
    {
        size_t MaxSamplesToDo = (_SrcReadIndex < _SrcWriteIndex) ? _SrcWriteIndex - _SrcReadIndex : (_SrcWriteIndex + SRC_PCM_BUFFER_SIZE) - _SrcReadIndex;

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

    opna_t::Mix(sampleData, sampleCount);
}

#pragma endregion
