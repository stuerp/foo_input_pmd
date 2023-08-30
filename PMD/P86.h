
// 86B PCM Driver「P86DRV Unit / Programmed by M.Kajihara 96/01/16 / Windows Converted by C60

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

#define SOUND_44K   44100
#define SOUND_22K   22050
#define SOUND_11K   11025

#define MAX_P86     256

typedef int32_t Sample;

#pragma pack(push)
#pragma pack(1)
struct P86HEADER // header(original)
{
    char header[12];      // "PCM86 DATA",0,0
    uint8_t Version;
    char All_Size[3];
    struct
    {
        uint8_t  start[3];
        uint8_t  size[3];
    } pcmnum[MAX_P86];
};
#pragma pack(pop)

const size_t P86HEADERSIZE = sizeof(char) * 12 + sizeof(uint8_t) + sizeof(char) * 3 + sizeof(uint8_t) * (3 + 3) * MAX_P86;

struct P86HEADER2  // header(for PMDWin, int alignment)
{
    char header[12];      // "PCM86 DATA",0,0
    int Version;
    int All_Size;
    struct
    {
        int    start;
        int    size;
    } pcmnum[MAX_P86];
};

const int ratetable[] =
{
    4135, 5513, 8270, 11025, 16540, 22050, 33080, 44100
};

/// <summary>
/// Implements a ADPCM Sound Source module, a single channel for samples in 4-bit ADPCM format at a sampling rate between 2–55 kHz.
/// </summary>
class P86DRV
{
public:
    P86DRV(File * file);
    virtual ~P86DRV();

    bool Init(uint32_t r, bool useInterpolation);            // 初期化
    bool Stop(void);                // P86 停止
    bool Play(void);                // P86 再生
    bool Keyoff(void);                // P86 keyoff
    int Load(const WCHAR * filePath);
    bool SetRate(uint32_t r, bool ip);          // レート設定
    void SetVolume(int volume);            // 全体音量調節用
    bool SetVol(int _vol);              // 音量設定
    bool SetOntei(int rate, uint32_t ontei);      // 音程周波数の設定
    bool SetPan(int flag, int data);        // PAN 設定
    bool SetNeiro(int num);              // PCM 番号設定
    bool SetLoop(int loop_start, int loop_end, int release_start, bool adpcm); // ループ設定
    void Mix(Sample * sampleData, size_t sampleCount) noexcept;

    WCHAR _FilePath[_MAX_PATH];
    P86HEADER2 p86header;              // P86 の音色ヘッダー

private:
    File * _File;

    bool  interpolation;              // 補完するか？
    int    rate;                  // 再生周波数
    int    srcrate;                // 元データの周波数
    uint32_t  ontei;                  // 音程(fnum)
    int    vol;                  // 音量
    uint8_t * p86_addr;                // P86 保存用メモリポインタ
    uint8_t * start_ofs;                // 発音中PCMデータ番地
    int    start_ofs_x;              // 発音中PCMデータ番地（小数部）
    int    size;                  // 残りサイズ
    uint8_t * _start_ofs;              // 発音開始PCMデータ番地
    int    _size;                  // PCMデータサイズ
    int    addsize1;                // PCMアドレス加算値 (整数部)
    int    addsize2;                // PCMアドレス加算値 (小数部)
    uint8_t * repeat_ofs;              // リピート開始位置
    int    repeat_size;              // リピート後のサイズ
    uint8_t * release_ofs;              // リリース開始位置
    int    release_size;              // リリース後のサイズ
    bool  repeat_flag;              // リピートするかどうかのflag
    bool  release_flag1;              // リリースするかどうかのflag
    bool  release_flag2;              // リリースしたかどうかのflag

    int    pcm86_pan_flag;    // パンデータ１(bit0=左/bit1=右/bit2=逆)
    int    pcm86_pan_dat;    // パンデータ２(音量を下げるサイドの音量値)
    bool  play86_flag;              // 発音中?flag

    int _AVolume;
    Sample _VolumeTable[16][256];

    void _Init();

    void CreateVolumeTable(int volume);
    void ReadHeader(File * file, P86HEADER & p86header);
    void double_trans(Sample * dest, int nsamples);
    void double_trans_g(Sample * dest, int nsamples);
    void left_trans(Sample * dest, int nsamples);
    void left_trans_g(Sample * dest, int nsamples);
    void right_trans(Sample * dest, int nsamples);
    void right_trans_g(Sample * dest, int nsamples);
    void double_trans_i(Sample * dest, int nsamples);
    void double_trans_g_i(Sample * dest, int nsamples);
    void left_trans_i(Sample * dest, int nsamples);
    void left_trans_g_i(Sample * dest, int nsamples);
    void right_trans_i(Sample * dest, int nsamples);
    void right_trans_g_i(Sample * dest, int nsamples);

    bool add_address(void);
};
