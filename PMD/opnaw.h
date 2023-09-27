
// OPNA emulator with waiting (Based on PMDWin code by C60)

#pragma once

#include "OPNA.h"

// Composite frequency when primary interpolation is enabled
#define  SOUND_55K      55555
#define  SOUND_55K_2    55466

// Buffer size(samples) to assign the amount calculated by wait
#define  WAIT_PCM_BUFFER_SIZE  65536UL

// Buffer size(samples) to substitute the amount calculated during linear interpolation
#define IP_PCM_BUFFER_SIZE  2048

// Number of samples for sinc interpolation (32, 64, 128, 256, 512)
#define NUMOFINTERPOLATION  128

/// <summary>
/// Implements a YM2608, aka OPNA, is a sixteen-channel sound chip developed by Yamaha.
/// It's a member of Yamaha's OPN family of FM synthesis chips, and the successor to the YM2203. It was notably used in NEC's PC-8801/PC-9801 series computers.
/// </summary>
class OPNAW : public OPNA
{
public:
    OPNAW(File * file) : OPNA(file)
    {
        Reset();
    }

    virtual ~OPNAW() { }

    bool Initialize(uint32_t clock, uint32_t synthesisRate, bool useInterpolation, const WCHAR * directoryPath);
    bool SetRate(uint32_t clock, uint32_t synthesisRate, bool useInterpolation = false);

    void SetFMDelay(int nsec);
    void SetSSGDelay(int nsec);
    void SetADPCMDelay(int nsec);
    void SetRSSDelay(int nsec);

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
    double Sinc(double x);
    double Fmod2(double x, double y);
    void MixInternal(Sample * sampleData, size_t sampleCount) noexcept;

    inline int Limit(int value, int max, int min)
    {
        return value > max ? max : (value < min ? min : value);
    }

private:
    Sample _PreBuffer[WAIT_PCM_BUFFER_SIZE * 2];

    int _FMDelay;           // in ns
    int _SSGDelay;          // in ns
    int _ADPCMDelay;        // in ns
    int _RSSDelay;          // in ns

    int _FMDelayCount;      // No. of samples
    int _SSGDelayCount;     // No. of samples
    int _ADPCMDelayCount;   // No. of samples
    int _RSSDelayCount;     // No. of samples

    size_t _ReadIndex;
    size_t _WriteIndex;

    int _Counter;

    Sample _InterpolationBuffer[IP_PCM_BUFFER_SIZE * 2];
    size_t _InterpolationIndex;

    uint32_t _OutputRate; // in Hz

    bool _interpolation2;            // First order interpolation flag
    int _delta;                // Difference fraction (divided by 16384 samples)

    double _delta_double;            // Difference fraction

    bool _ffirst;                // Data first time flag
    double _Rest; // Position of the previous remaining sample data when resampling the sampling theorem
};
