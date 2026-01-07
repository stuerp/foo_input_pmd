
/** $VER: PPS.h (2026.01.07) PCM driver for the SSG (Software-controlled Sound Generator) / Original Programmed by NaoNeko / Modified by Kaja / Windows Converted by C60 **/

#pragma once

#include "OPNA.h"

#define PPS_VERSION     "0.37"

#define PPS_SUCCESS            0
#define PPS_OPEN_FAILED        1
#define PPS_ALREADY_LOADED     2
#define PPZ_OUT_OF_MEMORY     99

#define FREQUENCY_44_1K     44100
#define FREQUENCY_22_0K     22050
#define FREQUENCY_11_0K     11025

typedef int32_t sample_t;

#define MAX_PPS 14

#pragma pack(push)
#pragma pack(1)
struct PPSHEADER
{
    struct
    {
        uint16_t _Offset;
        uint16_t _Size;
        uint8_t _Tone;
        uint8_t _Volume;
    } PPSSamples[MAX_PPS];
};
#pragma pack(pop)

const size_t PPSHEADERSIZE = (sizeof(uint16_t) * 2 + sizeof(uint8_t) * 2) * MAX_PPS;

#pragma warning(disable: 4820) // 'x' bytes padding added after data member 'y'

/// <summary>
/// PCM driver for the SSG (Software-controlled Sound Generator)
/// 4-bit 16000Hz PCM playback on the SSG channel 3. It can also play 2 samples simultanelously, but at a lower quality.
/// </summary>
class pps_t
{
public:
    pps_t(File * file);
    virtual ~pps_t();

    bool Initialize(uint32_t sampleRate, bool useInterpolation) noexcept;
    bool Stop();
    bool Start(int num, int shift, int volshift);

    bool SetParameter(int index, bool value);
    bool SetSampleRate(uint32_t r, bool ip);
    void SetVolume(int volume);

    void Mix(frame32_t * frames, size_t frameCount) noexcept;

    int Load(const WCHAR * filePath);

private:
    void Reset(void);
    void ReadHeader(File * file, PPSHEADER & ph);

private:
    File * _File;

    PPSHEADER _Header;
    WCHAR _FilePath[_MAX_PATH];

    int _SampleRate;
    bool _UseInterpolation;

    sample_t _EmitTable[16];

    sample_t * _Samples;

    bool _IsPlaying;
    bool _IsMonophonic;
    bool _IsSlowCPU;      // Play at half frequency?

    int _Volume1;
    sample_t * _Data1;
    int _Size1;
    int _DataXOr1;
    int _Tick1;
    int _TickXOr1;

    int _Volume2;
    sample_t * _Data2;
    int _Size2;
    int _DataXOr2;
    int _Tick2;
    int _TickXOr2;

    sample_t _KeyOffVolume;
};
