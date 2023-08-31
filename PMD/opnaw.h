
// Based on PMDWin code by C60

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
/// 
/// Implements an FM Sound Source module with six concurrent FM channels (voices), four operators per channel, with dual interrupt timers and an LFO.
/// It also includes eight possible operator interconnections, or algorithms, for producing different types of instrument sounds.
/// </summary>
class OPNAW : public OPNA
{
public:
    OPNAW(File * file) : OPNA(file)
    {
        Reset();
    }

    virtual ~OPNAW() { }

    bool Init(uint32_t clock, uint32_t synthesisRate, bool useInterpolation, const WCHAR * directoryPath);
    bool SetRate(uint32_t clock, uint32_t synthesisRate, bool useInterpolation = false);

    void SetFMWait(int nsec);
    void SetSSGWait(int nsec);
    void SetADPCMWait(int nsec);
    void SetRhythmWait(int nsec);

    int GetFMWait() const { return _FMWait; }          // FM wait in ns
    int GetSSGWait() const { return _SSGWait; }        // SSG wait in ns
    int GetADPCMWait() { return _ADPCMWait; }    // ADPCM wait in ns
    int GetRhythmWait() const { return _RhythmWait; }  // Rythm wait in ns

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

    // Synthetic sound saving during wait
    int _FMWait; // in ns
    int _SSGWait; // in ns
    int _ADPCMWait; // in ns
    int _RhythmWait; // in ns

    int _FMWaitCount;
    int _SSGWaitCount;
    int _ADPCMWaitCount;
    int _RhythmWaitCount;

    size_t _ReadIndex;
    size_t _WriteIndex;
    int    count2;                // Count decimal part (* 1000)

    Sample _InterpolationBuffer[IP_PCM_BUFFER_SIZE * 2];
    size_t _InterpolationIndex;

    uint32_t _OutputRate; // in Hz
    bool  interpolation2;            // First order interpolation flag
    int    delta;                // Difference fraction (divided by 16384 samples)

    double  delta_double;            // Difference fraction

    bool  ffirst;                // Data first time flag
    double  rest; // Position of the previous remaining sample data when resampling the sampling theorem
};
