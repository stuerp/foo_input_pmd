
// SSG PCM Driver「PPSDRV Unit / Original Programmed by NaoNeko / Modified by Kaja / Windows Converted by C60

#pragma once

#include "OPNA.h"

#define PPS_VERSION     "0.37"

#define PPS_SUCCESS            0
#define PPS_OPEN_FAILED        1
#define PPS_ALREADY_LOADED     2
#define PPZ_OUT_OF_MEMORY     99

#define SOUND_44K     44100
#define SOUND_22K     22050
#define SOUND_11K     11025

typedef int32_t Sample;

#define MAX_PPS        14

#pragma pack(push)
#pragma pack(1)
struct PPSHEADER
{
    struct
    {
        uint16_t address;
        uint16_t leng;
        uint8_t toneofs;
        uint8_t volumeofs;
    } pcmnum[MAX_PPS];
};
#pragma pack(pop)

const size_t PPSHEADERSIZE = (sizeof(uint16_t) * 2 + sizeof(uint8_t) * 2) * MAX_PPS;

/// <summary>
/// Implemnts a SSG Sound Source module, a complete internal implementation of the Yamaha YM2149/SSG,
/// a variant of the popular AY-3-8910/PSG for producing three channels of square wave synthesis or noise.
/// </summary>
class PPSDRV
{
public:
    PPSDRV(File * file);
    virtual ~PPSDRV();

    bool Init(uint32_t r, bool ip);
    bool Stop(void);
    bool Play(int num, int shift, int volshift);

    bool SetParam(int index, bool value);
    bool SetRate(uint32_t r, bool ip);
    void SetVolume(int volume);

    void Mix(Sample * sampleData, int sampleCount);

    int Load(const WCHAR * filePath);

private:
    void _Init(void);
    void ReadHeader(File * file, PPSHEADER & ppsheader);

private:
    File * _File;              // ファイルアクセス関連のクラスライブラリ

    PPSHEADER ppsheader;              // PCMの音色ヘッダー
    WCHAR _FilePath[_MAX_PATH];

    int _SynthesisRate;
    bool _UseInterpolation;

    Sample _EmitTable[16];

    Sample * _Samples;
    Sample * data_offset1;
    Sample * data_offset2;

    bool  _IsPlaying;

    bool  single_flag;              // 単音モードか？
    bool  low_cpu_check_flag;            // 周波数半分で再生か？

    int    data_xor1;                // 現在の位置(小数部)
    int    data_xor2;                // 現在の位置(小数部)
    int    tick1;
    int    tick2;
    int    tick_xor1;
    int    tick_xor2;
    int    data_size1;
    int    data_size2;
    int    volume1;
    int    volume2;
    Sample _KeyOffVolume;
};
