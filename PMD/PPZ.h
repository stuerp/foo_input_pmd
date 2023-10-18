
/** $VER: PPZ.h (2023.10.18) PC-98's 86 soundboard's 8 PCM driver (Programmed by UKKY / Based on Windows conversion by C60) **/

#pragma once

#include "OPNA.h"

#define PPZ8_VERSION    "1.07"

#define PPZ_SUCCESS          0
#define PPZ_OPEN_FAILED      1
#define PPZ_UNKNOWN_FORMAT   2
#define PPZ_ALREADY_LOADED   3

#define PPZ_OUT_OF_MEMORY   99

#define FREQUENCY_44_1K     44100

#define DefaultSampleRate   FREQUENCY_44_1K
#define DefaultVolume       12
#define MaxPPZChannels      8

#define X_N0                0x80
#define DELTA_N0            0x7F

typedef int32_t Sample;

struct PPZChannel
{
    bool IsPVI;
    bool HasLoop;
    bool IsPlaying;
    int Volume;
    int PanValue;

    int PCM_ADD_L;                // アドレス増加量 LOW
    int PCM_ADD_H;                // アドレス増加量 HIGH
    int PCM_ADDS_L;                // アドレス増加量 LOW（元の値）
    int PCM_ADDS_H;                // アドレス増加量 HIGH（元の値）
    int SourceFrequency;
    int SampleNumber;

    uint8_t * PCMStart;                // Current value
    int PCM_NOW_XOR;            // Current value (decimal part)

    uint8_t * PCMEnd;                // 現在の終了アドレス
    uint8_t * PCMEndRounded;                // 本当の終了アドレス

    uint8_t * PCM_LOOP;                // ループ開始アドレス

    uint32_t PCMLoopStart;            // リニアなループ開始アドレス
    uint32_t PCMLoopEnd;            // リニアなループ終了アドレス
};

#pragma pack(push)
#pragma pack(1)
struct PZIITEM
{
    uint32_t Start;
    uint32_t Size;
    uint32_t LoopStart;
    uint32_t LoopEnd;
    uint16_t SampleRate;
};

struct PZIHEADER
{
    char ID[4];                     // 'PZI1'
    char Dummy1[7];
    uint8_t Count;                  // Number of PCM entries available
    char Dummy2[22];
    PZIITEM PZIItem[128];
};

struct PVIHEADER
{
    char ID[4];                     // 'PVI2'
    char Dummy1[0x0b - 4];
    uint8_t Count;                  // Number of PVI entries available
    char Dummy2[0x10 - 0x0b - 1];
    struct
    {
        uint16_t Start;
        uint16_t End;
    } PVIItem[128];
};
#pragma pack(pop)

/// <summary>
/// Represents a PPZ sample.
/// </summary>
class PPZBank
{
public:
    PPZBank() : _PZIHeader(), _Data(), _Size(), _IsPVI()
    {
    }

    virtual ~PPZBank()
    {
        Reset();
    }

    void Reset()
    {
        _FilePath.clear();

        ::memset(&_PZIHeader, 0, sizeof(_PZIHeader));

        if (_Data)
        {
            ::free(_Data);
            _Data = nullptr;
        }

        _Size = 0;
        _IsPVI = false;
    }

    bool IsEmpty() const { return _Data == nullptr; }

    std::wstring _FilePath;

    PZIHEADER _PZIHeader;
    uint8_t * _Data;
    int _Size;
    bool _IsPVI;
};

/// <summary>
/// Implements a driver that synthesizes up to 8 PCM channels using the 86PCM, with soft panning possibilities and no memory limit aside from the user's PC98 setup.
/// It supports 2 kinds of PCM banks: .PVI and .PZI
/// </summary>
class PPZDriver
{
public:
    PPZDriver(File * file);
    virtual ~PPZDriver();

    bool Initialize(uint32_t outputFrequency, bool useInterpolation);
    bool Play(int channelNumber, int bankNumber, int sampleNumber, uint16_t start, uint16_t stop);
    bool Stop(int channelNumber);
    int  Load(const WCHAR * filePath, size_t bankNumber);
    void SetInstrument(size_t ch, size_t bankNumber, size_t instrumentNumber);
    bool SetVolume(int channelNumber, int volume);
    bool SetPitch(int channelNumber, uint32_t pitch);
    bool SetLoop(size_t channelNumber, size_t bankNumber, size_t instrumentNumber);
    bool SetLoop(size_t channelNumber, size_t bankNumber, size_t instrumentNumber, int loopStart, int loopEnd);
    void AllStop();
    bool SetPan(size_t channelNumber, int value);
    bool SetOutputFrequency(uint32_t outputFrequency, bool useInterpolation);
    bool SetSourceFrequency(size_t channelNumber, int sourceFrequency);
    void SetAllVolume(int volume);
    void SetVolume(int volume);
    void ADPCM_EM_SET(bool flag);
//  REMOVE_FSET; // 19H (PPZ8)常駐解除ﾌﾗｸﾞ設定
//  FIFOBUFF_SET; // 1AH (PPZ8)FIFOﾊﾞｯﾌｧの変更
//  RATE_SET; // 1BH (PPZ8)WSS詳細ﾚｰﾄ設定

    void Mix(Sample * sampleData, size_t sampleCount) noexcept;

public:
    PPZBank _PPZBank[2];

private:
    void MoveSamplePointer(int i) noexcept;

    void InitializeInternal();
    void Reset();

    void CreateVolumeTable(int volume);
    void ReadHeader(File * file, PZIHEADER & pziheader);
    void ReadHeader(File * file, PVIHEADER & pviheader);

    inline int Limit(int v, int max, int min) const noexcept
    {
        return v > max ? max : (v < min ? min : v);
    }

private:
    File * _File;

    bool _EmulateADPCM; // Should channel 8 emulate ADPCM?
    bool _UseInterpolation;

    PPZChannel _Channel[MaxPPZChannels];

    int _PCMVolume; // Overall 86B Mixer volume
    int _Volume;
    int _OutputFrequency;

    Sample _VolumeTable[16][256];
};
