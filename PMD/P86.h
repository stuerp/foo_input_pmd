
// PMD's internal 86PCM driver for the PC-98's 86 soundboard / Programmed by M.Kajihara 96/01/16 / Windows Converted by C60

#pragma once

#include "OPNA.h"

#define P86_VERSION     "1.1c"
#define vers            0x11
#define date            "Sep.11th 1996"

#define P86_SUCCESS          0
#define P86_OPEN_FAILED     81
#define P86_UNKNOWN_FORMAT  82
#define P86_ALREADY_LOADED  83
#define PPZ_OUT_OF_MEMORY   99

#define FREQUENCY_44_1K   44100
#define FREQUENCY_22_0K   22050
#define FREQUENCY_11_0K   11025

#define MAX_P86     256

typedef int32_t Sample;

#pragma pack(push)
#pragma pack(1)
struct P86FILEHEADER
{
    char Id[12]; // "PCM86 DATA",0,0
    uint8_t Version;
    char Size[3];
    struct
    {
        uint8_t Offset[3];
        uint8_t Size[3];
    } P86Item[MAX_P86];
};
#pragma pack(pop)

const size_t P86FILEHEADERSIZE = (sizeof(char) * 12) + sizeof(uint8_t) + (sizeof(char) * 3) + (sizeof(uint8_t) * (3 + 3) * MAX_P86);

struct P86HEADER
{
    char Id[12]; // "PCM86 DATA",0,0
    int Version;
    int Size;
    struct
    {
        int Offset;
        int Size;
    } P86Item[MAX_P86];
};

const int SampleRates[] =
{
    4135, 5513, 8270, 11025, 16540, 22050, 33080, 44100
};

/// <summary>
/// Implements PMD's internal 86PCM driver.
/// It replaces the ADPCM channel with 8-bit sample playback. Its stereo capabilities also adds a new functionality to panning.
/// </summary>
class P86Driver
{
public:
    P86Driver(File * file);
    virtual ~P86Driver();

    bool Initialize(uint32_t r, bool useInterpolation);
    bool Stop(void);
    void Play();

    bool Keyoff(void);
    int Load(const WCHAR * filePath);

    void SetOutputFrequency(uint32_t sampleRate, bool useInterpolation);
    void SetVolume(int volume);
    bool SelectVolume(int value);
    bool SetPitch(int sampleRateIndex, uint32_t pitch);
    bool SetPan(int flag, int value);
    bool SelectSample(int num); // PCM number setting
    bool SetLoop(int loopStart, int loopEnd, int releaseStart, bool isADPCM);

    void Mix(Sample * sampleData, size_t sampleCount) noexcept;

public:
    std::wstring _FilePath;
    P86HEADER _Header;

private:
    void InitializeInternal();

    void CreateVolumeTable(int volume);
    void ReadHeader(File * file, P86FILEHEADER & p86header);

    void double_trans(Sample * sampleData, size_t sampleCount);
    void double_trans_g(Sample * sampleData, size_t sampleCount);
    void left_trans(Sample * sampleData, size_t sampleCount);
    void left_trans_g(Sample * sampleData, size_t sampleCount);
    void right_trans(Sample * sampleData, size_t sampleCount);
    void right_trans_g(Sample * sampleData, size_t sampleCount);
    void double_trans_i(Sample * sampleData, size_t sampleCount);
    void double_trans_g_i(Sample * sampleData, size_t sampleCount);
    void left_trans_i(Sample * sampleData, size_t sampleCount);
    void left_trans_g_i(Sample * sampleData, size_t sampleCount);
    void right_trans_i(Sample * sampleData, size_t sampleCount);
    void right_trans_g_i(Sample * sampleData, size_t sampleCount);

    bool AddAddress();

private:
    File * _File;
    uint8_t * _Data;

    bool _Enabled;

    int _OutputFrequency;
    bool _UseInterpolation;
    uint32_t _Pitch;
    int _Volume;


    uint8_t * start_ofs;                // 発音中PCMデータ番地

    int    start_ofs_x;              // 発音中PCMデータ番地（小数部）
    int    size;                  // 残りサイズ

    uint8_t * _start_ofs;              // 発音開始PCMデータ番地
    int    _size;                  // PCMデータサイズ

    int    addsize1;                // PCMアドレス加算値 (整数部)
    int    addsize2;                // PCMアドレス加算値 (小数部)

    uint8_t * repeat_ofs;              // リピート開始位置
    int repeat_size;              // リピート後のサイズ

    uint8_t * release_ofs;              // リリース開始位置
    int release_size;              // リリース後のサイズ

    bool  repeat_flag;              // リピートするかどうかのflag
    bool  release_flag1;              // リリースするかどうかのflag
    bool  release_flag2;              // リリースしたかどうかのflag

    int _PanFlag;   // Pan data 1 (bit0=left/bit1=right/bit2=reverse)
    int _PanValue;   // Pan data 2 (volume value on the side where the volume is lowered)

    int _AVolume;
    Sample _VolumeTable[16][256];

    int _OrigSampleRate;
};
