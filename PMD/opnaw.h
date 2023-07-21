
// Based on PMDWin code by C60

#pragma once

#include "OPNA.h"

// Composite frequency when primary interpolation is enabled
#define  SOUND_55K        55555
#define  SOUND_55K_2        55466

// wait で計算した分を代入する buffer size(samples)
#define  WAIT_PCM_BUFFER_SIZE  65536

// 線形補間時に計算した分を代入する buffer size(samples)
#define    IP_PCM_BUFFER_SIZE   2048

// sinc 補間のサンプル数
// #define    NUMOFINTERPOLATION     32
// #define    NUMOFINTERPOLATION     64
#define    NUMOFINTERPOLATION    128
// #define    NUMOFINTERPOLATION    256
// #define    NUMOFINTERPOLATION    512

class OPNAW : public OPNA
{
public:
    OPNAW(File * file) : OPNA(file)
    {
        Reset();
    }

    virtual ~OPNAW() { }

    bool Init(uint32_t c, uint32_t r, bool ipflag, const WCHAR * path);
    bool SetRate(uint32_t c, uint32_t r, bool ipflag = false);

    void SetFMWait(int nsec);
    void SetSSGWait(int nsec);
    void SetRhythmWait(int nsec);
    void SetADPCMWait(int nsec);

    int GetFMWait() const { return _FMWait; }          // FM wait in ns
    int GetSSGWait() const { return _SSGWait; }        // SSG wait in ns
    int GetRhythmWait() const { return _RhythmWait; }  // Rythm wait in ns
    int GetADPCMWait() { return _ADPCMWait; }    // ADPCM wait in ns

    void  SetReg(uint32_t addr, uint32_t data);    // レジスタ設定
    void  Mix(Sample * buffer, int nsamples);  // 合成
    void  ClearBuffer();          // 内部バッファクリア

private:
    void Reset() noexcept;
    void CalcWaitPCM(int waitcount);
    double Sinc(double x);
    double Fmod2(double x, double y);
    void MixInternal(Sample * buffer, int nsamples);

    inline int Limit(int v, int max, int min)
    {
        return v > max ? max : (v < min ? min : v);
    }

private:
    Sample  pre_buffer[WAIT_PCM_BUFFER_SIZE * 2];

    // Synthetic sound saving during wait
    int    _FMWait; // in ns
    int    _SSGWait; // in ns
    int    _ADPCMWait; // in ns
    int    _RhythmWait; // in ns

    int    _FMWaitCount;
    int    _SSGWaitCount;
    int    _ADPCMWaitCount;
    int    _RhythmWaitCount;

    int    read_pos;              // Write position
    int    write_pos;              // Read position
    int    count2;                // Count decimal part (*1000)

    Sample ip_buffer[IP_PCM_BUFFER_SIZE * 2];  // Work area for linear interpolation

    uint32_t _OutputRate; // in Hz
    bool  interpolation2;            // First order interpolation flag
    int    delta;                // Difference fraction (divided by 16384 samples)

    double  delta_double;            // Difference fraction

    bool  ffirst;                // Data first time flag
    double  rest;                // Position of the previous remaining sample data when resampling the sampling theorem
    int    write_pos_ip;            // Write position (ip)
};
