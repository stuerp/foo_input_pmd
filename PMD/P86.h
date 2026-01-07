
/** $VER: P86.h (2026.01.07) PMD's internal 86PCM driver for the PC-98's 86 soundboard / Programmed by M.Kajihara 96/01/16 / Windows Converted by C60 **/

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

typedef int32_t sample_t;

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
    int32_t Version;
    int32_t Size;
    struct
    {
        int32_t Offset;
        int32_t Size;
    } P86Item[MAX_P86];
};

#pragma warning(disable: 4820) // 'x' bytes padding added after data member 'y'

/// <summary>
/// Implements PMD's internal 86PCM driver.
/// It replaces the ADPCM channel with 8-bit sample playback. Its stereo capabilities also adds a new functionality to panning.
/// </summary>
class p86_t
{
public:
    p86_t(File * file);
    virtual ~p86_t();

    bool Initialize(uint32_t sampleRate, bool useInterpolation) noexcept;
    bool Stop(void);
    void Start();

    bool Keyoff(void);
    int Load(const WCHAR * filePath);

    void SetSampleRate(uint32_t sampleRate, bool useInterpolation);
    void InitializeVolume(int volume);
    bool SetVolume(int value);
    bool SetPitch(int sampleRateIndex, uint32_t pitch);
    bool SetPan(int flags, int value);
    bool SelectSample(int number);
    bool SetLoop(int loopStart, int loopEnd, int releaseStart, bool isADPCM);

    void Mix(frame32_t * frames, size_t frameCount) noexcept;

public:
    std::wstring _FilePath;
    P86HEADER _Header;

private:
    void InitializeInternal();

    void CreateVolumeTable(int volume);
    void ReadHeader(File * file, P86FILEHEADER & p86header);

    void MixCenter(sample_t * sampleData, size_t sampleCount);
    void MixCenterPhaseReversed(sample_t * sampleData, size_t sampleCount);
    void MixLeft(sample_t * sampleData, size_t sampleCount);
    void MixLeftPhaseReversed(sample_t * sampleData, size_t sampleCount);
    void MixRight(sample_t * sampleData, size_t sampleCount);
    void MixRightPhaseReversed(sample_t * sampleData, size_t sampleCount);
    void MixCenterInterpolated(sample_t * sampleData, size_t sampleCount);
    void MixCenterInterpolatedPhaseReversed(sample_t * sampleData, size_t sampleCount);
    void MixLeftInterpolated(sample_t * sampleData, size_t sampleCount);
    void MixLeftInterpolatedPhaseReversed(sample_t * sampleData, size_t sampleCount);
    void MixRightInterpolated(sample_t * sampleData, size_t sampleCount);
    void MixRightInterpolatedPhaseReversed(sample_t * sampleData, size_t sampleCount);

    bool MoveSamplePointer() noexcept;

private:
    File * _File;
    uint8_t * _Data;

    int _SampleRate;
    bool _UseInterpolation;
    uint32_t _Pitch;
    int _Volume;

    const uint8_t * _SampleAddr;    // PCM sample address
    int _SampleSize;                // PCM sample size

    const uint8_t * _CurrAddr;      // Current address
    int _CurrOffs;                  // Current offset
    int _SizeToDo;                  // Remaining number of bytes to process

    int _IncrementHi;
    int _IncrementLo;

    const uint8_t * _LoopAddr;
    int _LoopSize;

    const uint8_t * _ReleaseAddr;
    int _ReleaseSize;

    bool _IsPlaying;
    bool _IsLooping;
    bool _IsReleaseRequested;
    bool _IsReleasing;

    int _PanFlags;                   // 0: left / 1: right / 2: reverse
    int _PanValue;                  // Volume value on the side where the volume is lowered.

    int _VolumeBase;
    sample_t _VolumeTable[16][256];

    int _SampleRateOriginal;        // Original sample rate
};
