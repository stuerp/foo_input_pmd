
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

    int SourceFrequency;
    int SampleNumber;

    int PCMStartL;
    int PCMAddL;
    int PCMAddH;

    uint8_t * PCMStartH;
    uint8_t * PCMEnd;
    uint8_t * PCMEndRounded;
    uint8_t * PCMLoopStart;

    uint32_t LoopStartOffset;
    uint32_t LoopEndOffset;
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
    uint8_t Count;                  // Number of PZI entries available
    char Dummy2[22];
    PZIITEM PZIItem[128];
};

struct PVIITEM
{
    uint16_t Start;
    uint16_t End;
};

struct PVIHEADER
{
    char ID[4];                     // 'PVI2'
    char Dummy1[0x0b - 4];
    uint8_t Count;                  // Number of PVI entries available
    char Dummy2[0x10 - 0x0b - 1];
    PVIITEM PVIItem[128];
};
#pragma pack(pop)

/// <summary>
/// Represents a bank of PPZ samples.
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

public:
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

    void Initialize(uint32_t outputFrequency, bool useInterpolation);
    void Play(size_t channelNumber, int bankNumber, int sampleNumber, uint16_t start, uint16_t stop);
    void Stop(size_t channelNumber);
    int Load(const WCHAR * filePath, size_t bankNumber);
    void SetInstrument(size_t ch, size_t bankNumber, size_t instrumentNumber);
    void SetVolume(size_t channelNumber, int volume);
    void SetPitch(size_t channelNumber, uint32_t pitch);
    void SetLoop(size_t channelNumber, size_t bankNumber, size_t instrumentNumber);
    void SetLoop(size_t channelNumber, size_t bankNumber, size_t instrumentNumber, int loopStart, int loopEnd);
    void AllStop();
    void SetPan(size_t channelNumber, int value);
    void SetOutputFrequency(uint32_t outputFrequency, bool useInterpolation);
    void SetSourceFrequency(size_t channelNumber, int sourceFrequency);
    void SetAllVolume(int volume);
    void SetVolume(int volume);
    void EmulateADPCM(bool flag);
//  REMOVE_FSET; // 19H (PPZ8)常駐解除ﾌﾗｸﾞ設定
//  FIFOBUFF_SET; // 1AH (PPZ8)FIFOﾊﾞｯﾌｧの変更
//  RATE_SET; // 1BH (PPZ8)WSS詳細ﾚｰﾄ設定

    void Mix(Sample * sampleData, size_t sampleCount) noexcept;

public:
    PPZBank _PPZBank[2];

private:
    void MoveSamplePointer(int i) noexcept;

    void Initialize();

    void CreateVolumeTable(int volume);
    void ReadHeader(File * file, PZIHEADER & pziheader);
    void ReadHeader(File * file, PVIHEADER & pviheader);

    inline int Clamp(int value, int min, int max) const noexcept
    {
        return value > max ? max : (value < min ? min : value);
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
