
/** $VER: OPNAW.h (2023.10.18) OPNA emulator with waiting (Based on PMDWin code by C60 / Masahiro Kajihara) **/

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
class OPNAW : public OPNA
{
public:
    OPNAW(File * file) noexcept : OPNA(file)
    {
        Reset();
    }

    virtual ~OPNAW() noexcept { }

    bool Initialize(uint32_t clock, uint32_t outputFrequency, bool useInterpolation, const WCHAR * directoryPath) noexcept;
    void Initialize(uint32_t clock, uint32_t outputFrequency, bool useInterpolation) noexcept;

    void SetFMDelay(int nsec);
    void SetSSGDelay(int nsec);
    void SetADPCMDelay(int nsec);
    void SetRhythmDelay(int nsec);

    int GetFMDelay() const { return _FMDelay; }
    int GetSSGDelay() const { return _SSGDelay; }
    int GetADPCMDelay() { return _ADPCMDelay; }
    int GetRSSDelay() const { return _RSSDelay; }

    void SetReg(uint32_t addr, uint32_t data);
    void Mix(Sample * sampleData, size_t sampleCount) noexcept;
    void ClearBuffer();

private:
    void Reset() noexcept;

    void CalcWaitPCM(int waitcount);

    void MixInternal(Sample * sampleData, size_t sampleCount) noexcept;

    inline int Limit(int value, int max, int min)
    {
        return (value > max) ? max : ((value < min) ? min : value);
    }

    /// <summary>
    /// Normalized sinc function
    /// </summary>
    inline double sinc(double x) const noexcept
    {
    #define M_PI 3.14159265358979323846

        return (x != 0.0) ? sin(M_PI * x) / (M_PI * x) : 1.0;
    }

private:
    uint32_t _OutputFrequency;      // in Hz
    bool _UseLinearInterpolation;

    int _FMDelay;           // in ns
    int _SSGDelay;          // in ns
    int _ADPCMDelay;        // in ns
    int _RSSDelay;          // in ns

    int _FMDelayCount;      // No. of samples
    int _SSGDelayCount;     // No. of samples
    int _ADPCMDelayCount;   // No. of samples
    int _RSSDelayCount;     // No. of samples

    int _Counter;

    Sample _SrcBuffer[SRC_PCM_BUFFER_SIZE * 2];
    size_t _SrcReadIndex;
    size_t _SrcWriteIndex;

    Sample _DstBuffer[DST_PCM_BUFFER_SIZE * 2];
    size_t _DstIndex;

    double _Rest; // Position of the previous remaining sample data when resampling the sampling theorem.
};
