
/** $VER: OPNAW.h (2026.01.07) OPNA emulator with waiting (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#pragma once

#include "OPNA.h"

// Composite frequency when primary interpolation is enabled
#define FREQUENCY_55_4K         55466

// Buffer size (in samples) to assign the amount calculated by wait
#define SRC_PCM_BUFFER_SIZE     65536UL

// Buffer size (in samples) to substitute the amount calculated during linear interpolation
#define DST_PCM_BUFFER_SIZE     2048

// Number of samples for sinc interpolation (32, 64, 128, 256, 512)
#define SINC_INTERPOLATION_SAMPLE_COUNT 128

/// <summary>
/// Implements a YM2608, aka OPNA, is a sixteen-channel sound chip developed by Yamaha.
/// It's a member of Yamaha's OPN family of FM synthesis chips, and the successor to the YM2203. It was notably used in NEC's PC-8801/PC-9801 series computers.
/// </summary>
class opnaw_t : public opna_t
{
public:
    opnaw_t(File * file) noexcept : opna_t(file)
    {
        Reset();
    }

    virtual ~opnaw_t() noexcept { }

    bool Initialize(uint32_t clock, uint32_t sampleRate, bool useInterpolation, const WCHAR * directoryPath) noexcept;
    void Initialize(uint32_t clock, uint32_t sameplRate, bool useInterpolation) noexcept;

    void SetFMDelay(int nsec) noexcept;
    void SetSSGDelay(int nsec) noexcept;
    void SetADPCMDelay(int nsec) noexcept;
    void SetRhythmDelay(int nsec) noexcept;

    int GetFMDelay() const noexcept { return _FMDelay; }
    int GetSSGDelay() const noexcept { return _SSGDelay; }
    int GetADPCMDelay() const noexcept { return _ADPCMDelay; }
    int GetRSSDelay() const noexcept { return _RhythmDelay; }

    void SetReg(uint32_t reg, uint32_t value) noexcept;
    void Mix(sample_t * sampleData, size_t sampleCount) noexcept;
    void ClearBuffer() noexcept;

private:
    void Reset() noexcept;

    void CalcWaitPCM(int waitcount);

    void MixInternal(sample_t * sampleData, size_t sampleCount) noexcept;

    /// <summary>
    /// Normalized sinc function
    /// </summary>
    inline double sinc(double x) const noexcept
    {
        const double M_PI = 3.14159265358979323846;

        return (x != 0.0) ? std::sin(M_PI * x) / (M_PI * x) : 1.0;
    }

private:
    uint32_t _SampleRate;   // in Hz
    bool _UseInterpolation;

    int _FMDelay;           // in ns
    int _SSGDelay;          // in ns
    int _ADPCMDelay;        // in ns
    int _RhythmDelay;          // in ns

    int _FMDelayCount;      // No. of samples
    int _SSGDelayCount;     // No. of samples
    int _ADPCMDelayCount;   // No. of samples
    int _RhythmDelayCount;     // No. of samples

    int _Counter;

    sample_t _SrcBuffer[SRC_PCM_BUFFER_SIZE * 2];
    size_t _SrcReadIndex;
    size_t _SrcWriteIndex;

    sample_t _DstBuffer[DST_PCM_BUFFER_SIZE * 2];
    size_t _DstIndex;

    double _Rest; // Position of the previous remaining sample data when resampling the sampling theorem.
};
