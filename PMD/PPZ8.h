
/** $VER: PPZ8.h (2026.01.07) PC-98's 86 soundboard's 8 PCM driver (Programmed by UKKY / Based on Windows conversion by C60) **/

#pragma once

#include "OPNA.h"

#define PPZ8_VERSION        "1.07"

#define PPZ_SUCCESS          0
#define PPZ_OPEN_FAILED      1
#define PPZ_UNKNOWN_FORMAT   2
#define PPZ_ALREADY_LOADED   3

#define PPZ_OUT_OF_MEMORY   99

#define FREQUENCY_44_1K     44100

#define DefaultSampleRate   FREQUENCY_44_1K
#define DefaultVolume       12
#define MaxPPZ8Channels      8

#define X_N0                0x80
#define DELTA_N0            0x7F

typedef int32_t sample_t;

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
    uint8_t Count;                  // Number of PZI entries used by the bank
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
    char Dummy1[0x0B - 4];
    uint8_t Count;                  // Number of PVI entries used by the bank
    char Dummy2[0x10 - 0x0B - 1];

    PVIITEM PVIItem[128];
};
#pragma pack(pop)

/// <summary>
/// Represents a bank of PPZ samples.
/// </summary>
class ppz_bank_t
{
public:
    ppz_bank_t() noexcept : _PZIHeader(), _Data(), _Size(), _IsPVI()
    {
    }

    virtual ~ppz_bank_t() noexcept
    {
        Reset();
    }

    void Reset() noexcept
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

    bool IsEmpty() const noexcept
    {
        return _Data == nullptr;
    }

public:
    std::wstring _FilePath;

    PZIHEADER _PZIHeader;
    uint8_t * _Data;
    int32_t _Size;
    bool _IsPVI;
};

#pragma warning(disable: 4820) // 'x' bytes padding added after data member 'y'

/// <summary>
/// Represents a PPZ channel.
/// </summary>
struct ppz_channel_t
{
    bool _IsPVI;
    bool _HasLoop;
    bool _IsPlaying;
    int32_t _Volume;
    int32_t _PanValue;

    int32_t _SourceFrequency;
    int32_t _SampleNumber;

    int32_t _PCMStartL;
    int32_t _PCMAddL;
    int32_t _PCMAddH;

    uint8_t * _PCMStartH;
    uint8_t * _PCMEnd;
    uint8_t * _PCMEndRounded;
    uint8_t * _PCMLoopStart;

    uint32_t _LoopStartOffset;
    uint32_t _LoopEndOffset;
};

#pragma warning(disable: 4820) // 'x' bytes padding added after data member 'y'

/// <summary>
/// Implements a driver that synthesizes up to 8 PCM channels using the 86PCM, with soft panning possibilities and no memory limit aside from the user's PC98 setup.
/// It supports 2 kinds of PCM banks: .PVI and .PZI
/// </summary>
class ppz8_t
{
public:
    ppz8_t(File * file);
    virtual ~ppz8_t();

    void Initialize(uint32_t sampleRate, bool useInterpolation) noexcept;

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
    void SetSampleRate(uint32_t outputFrequency, bool useInterpolation);
    void SetSourceFrequency(size_t channelNumber, int sourceFrequency);
    void SetAllVolume(int volume);
    void SetVolume(int volume);
    void EmulateADPCM(bool flag);
//  REMOVE_FSET; // 19H (PPZ8)常駐解除ﾌﾗｸﾞ設定
//  FIFOBUFF_SET; // 1AH (PPZ8)FIFOﾊﾞｯﾌｧの変更
//  RATE_SET; // 1BH (PPZ8)WSS詳細ﾚｰﾄ設定

    void Mix(frame32_t * frames, size_t frameCount) noexcept;

public:
    ppz_bank_t _PPZBanks[2];

private:
    void MoveSamplePointer(ppz_channel_t & channel) const noexcept;

    void InitializeInternal() noexcept;

    void CreateVolumeTable(int volume);
    void ReadHeader(File * file, PZIHEADER & pziheader);
    void ReadHeader(File * file, PVIHEADER & pviheader);

private:
    File * _File;

    bool _EmulateADPCM; // Should channel 8 emulate ADPCM?
    bool _UseInterpolation;

    ppz_channel_t _Channels[MaxPPZ8Channels];

    int32_t _MasterVolume; // Overall 86B Mixer volume
    int32_t _Volume;
    int32_t _SampleRate;

    sample_t _VolumeTable[16][256];
};
