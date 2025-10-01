
// PCM driver for the SSG (Software-controlled Sound Generator) / Original Programmed by NaoNeko / Modified by Kaja / Windows Converted by C60

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

typedef int32_t Sample;

#define MAX_PPS 14

#pragma pack(push)
#pragma pack(1)
struct PPSHEADER
{
    struct
    {
        uint16_t Address;
        uint16_t Size;
        uint8_t ToneOffset;
        uint8_t VolumeOffset;
    } pcmnum[MAX_PPS];
};
#pragma pack(pop)

const size_t PPSHEADERSIZE = (sizeof(uint16_t) * 2 + sizeof(uint8_t) * 2) * MAX_PPS;

#pragma warning(disable: 4820) // 'x' bytes padding added after data member 'y'

/// <summary>
/// PCM driver for the SSG (Software-controlled Sound Generator)
/// 4-bit 16000Hz PCM playback on the SSG Channel 3. It can also play 2 samples simultanelously, but at a lower quality.
/// </summary>
class PPSDriver
{
public:
    PPSDriver(File * file);
    virtual ~PPSDriver();

    bool Initialize(uint32_t r, bool ip);
    bool Stop(void);
    bool Play(int num, int shift, int volshift);

    bool SetParameter(int index, bool value);
    bool SetSampleRate(uint32_t r, bool ip);
    void SetVolume(int volume);

    void Mix(Sample * sampleData, size_t sampleCount);

    int Load(const WCHAR * filePath);

private:
    void _Init(void);
    void ReadHeader(File * file, PPSHEADER & ph);

private:
    File * _File;

    PPSHEADER _Header;
    WCHAR _FilePath[_MAX_PATH];

    int _SampleRate;
    bool _UseInterpolation;

    Sample _EmitTable[16];

    Sample * _Samples;
    Sample * data_offset1;
    Sample * data_offset2;

    bool _IsPlaying;

    bool _SingleNodeMode;
    bool _LowCPUCheck;      // Play at half frequency?

    int data_xor1;          // Current position (decimal part)
    int data_xor2;          // Current position (decimal part)
    int tick1;
    int tick2;
    int tick_xor1;
    int tick_xor2;
    int data_size1;
    int data_size2;
    int volume1;
    int volume2;

    Sample _KeyOffVolume;
};
