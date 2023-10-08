
// PC-98's 86 soundboard's 8 PCM driver / Programmed by UKKY / Windows Converted by C60

#pragma once

#include "OPNA.h"

#define PPZ8_VERSION    "1.07"

#define PPZ_SUCCESS          0
#define PPZ_OPEN_FAILED      1
#define PPZ_UNKNOWN_FORMAT   2
#define PPZ_ALREADY_LOADED   3

#define PPZ_OUT_OF_MEMORY   99

#define SOUND_44K           44100

#define DefaultSampleRate   SOUND_44K
#define DefaultVolume       12
#define MaxPPZChannels      8

#define X_N0                0x80
#define DELTA_N0            0x7F

typedef int32_t Sample;

struct PPZChannel
{
    int PCM_ADD_L;                // アドレス増加量 LOW
    int PCM_ADD_H;                // アドレス増加量 HIGH
    int PCM_ADDS_L;                // アドレス増加量 LOW（元の値）
    int PCM_ADDS_H;                // アドレス増加量 HIGH（元の値）
    int PCM_SORC_F;                // 元データの再生レート
    int PCM_FLG;                // 再生フラグ
    int PCM_VOL;                // ボリューム
    int PanValue;
    int PCM_NUM;                // PCM番号
    int PCM_LOOP_FLG;            // ループ使用フラグ
    uint8_t * PCM_NOW;                // 現在の値
    int PCM_NOW_XOR;            // 現在の値（小数部）
    uint8_t * PCM_END;                // 現在の終了アドレス
    uint8_t * PCM_END_S;                // 本当の終了アドレス
    uint8_t * PCM_LOOP;                // ループ開始アドレス
    uint32_t PCM_LOOP_START;            // リニアなループ開始アドレス
    uint32_t PCM_LOOP_END;            // リニアなループ終了アドレス
    bool _HasPVI;                // PVI なら true
};

#pragma pack(push)
#pragma pack(1)
struct PZIHEADER
{
    char ID[4];                     // 'PZI1'
    char Dummy1[7];
    uint8_t Count;                  // Number of PCM entries available
    char Dummy2[22];
    struct
    {
        uint32_t Start;
        uint32_t Size;
        uint32_t LoopStart;
        uint32_t LoopEnd;
        uint16_t SampleRate;
    } PZIItem[128];
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
/// Implements a driver that synthesizes up to 8 PCM channels using the 86PCM, with soft panning possibilities and no memory limit aside from the user's PC98 setup.
/// It supports 2 kinds of PCM banks: .PVI and .PZI
/// </summary>
class PPZDriver
{
public:
    PPZDriver(File * file);
    virtual ~PPZDriver();

    bool Initialize(uint32_t sampleRate, bool useInterpolation);
    bool Play(int ch, int bufnum, int num, uint16_t start, uint16_t stop);
    bool Stop(int ch);
    int  Load(const WCHAR * filePath, int bufnum);
    bool SetVolume(int ch, int volume);
    bool SetPitch(int channelNumber, uint32_t pitch);
    bool SetLoop(int ch, uint32_t loop_start, uint32_t loop_end);
    void AllStop();
    bool SetPan(int ch, int value);
    bool SetSampleRate(uint32_t sampleRate, bool useInterpolation);
    bool SetSourceRate(int ch, int sampleRate);
    void SetAllVolume(int volume);
    void SetVolume(int volume);
    void ADPCM_EM_SET(bool flag);
//  REMOVE_FSET; // 19H (PPZ8)常駐解除ﾌﾗｸﾞ設定
//  FIFOBUFF_SET; // 1AH (PPZ8)FIFOﾊﾞｯﾌｧの変更
//  RATE_SET; // 1BH (PPZ8)WSS詳細ﾚｰﾄ設定

    void Mix(Sample * sampleData, size_t sampleCount) noexcept;

public:
    PZIHEADER PCME_WORK[2];
    bool _HasPVI[2];
    std::wstring _FilePath[2];

private:
    void Reset();

    void InitializeInternal();
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
    uint8_t * XMS_FRAME_ADR[2]; // Memory allocated by XMS
    int XMS_FRAME_SIZE[2]; // PZI or PVI internal state
    int _PCMVolume; // Overall 86B Mixer volume
    int _Volume;
    int _SampleRate; // Playback frequency

    Sample _VolumeTable[16][256];
};
