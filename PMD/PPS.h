﻿
// SSG PCM Driver「PPSDRV Unit / Original Programmed by NaoNeko / Modified by Kaja / Windows Converted by C60

#pragma once

#include "OPNA.h"

//  DLL の 戻り値
#define  _PPSDRV_OK            0    // 正常終了
#define  _ERR_OPEN_PPS_FILE        1    // PPS を開けなかった
#define  _ERR_WRONG_PPS_FILE         2    // PPS の形式が異なっている
#define  _WARNING_PPS_ALREADY_LOAD    3    // PPS はすでに読み込まれている
#define  _ERR_OUT_OF_MEMORY       99    // メモリを確保できなかった

#define  SOUND_44K     44100
#define  SOUND_22K     22050
#define  SOUND_11K     11025

#define  PPSDRV_VER    "0.37"
#define  MAX_PPS        14

typedef int Sample;

#pragma pack(push)
#pragma pack(1)
struct PPSHEADER
{
    struct
    {
        uint16_t  address;        // 先頭アドレス
        uint16_t  leng;          // データ量
        uint8_t    toneofs;        // 音階
        uint8_t    volumeofs;        // 音量
    } pcmnum[MAX_PPS];
};

const size_t PPSHEADERSIZE = (sizeof(uint16_t) * 2 + sizeof(uint8_t) * 2) * MAX_PPS;
#pragma pack(pop)

class PPSDRV
{
public:
    PPSDRV(File * file);
    virtual ~PPSDRV();

    bool  Init(uint32_t r, bool ip);          //     初期化
    bool  Stop(void);                // 00H PDR 停止
    bool  Play(int num, int shift, int volshift);  // 01H PDR 再生
    bool  Check(void);              // 02H 再生中かどうかcheck
    bool  SetParam(int paramno, bool data);    // 05H PDRパラメータの設定
    bool  GetParam(int paramno);          // 06H PDRパラメータの取得

    int    Load(WCHAR * filename);          // PPS 読み込み
    bool  SetRate(uint32_t r, bool ip);        // レート設定
    void  SetVolume(int vol);            // 音量設定
    void  Mix(Sample * dest, int nsamples);    // 合成

    PPSHEADER  ppsheader;              // PCMの音色ヘッダー
    WCHAR  pps_file[_MAX_PATH];          // ファイル名

private:
    File * _File;              // ファイルアクセス関連のクラスライブラリ

    bool  interpolation;              // 補完するか？
    int    rate;
    Sample * dataarea1;                // PPS 保存用メモリポインタ
    bool  single_flag;              // 単音モードか？
    bool  low_cpu_check_flag;            // 周波数半分で再生か？
    bool  keyon_flag;                // Keyon 中か？
    Sample  EmitTable[16];
    Sample * data_offset1;
    Sample * data_offset2;
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
    Sample  keyoff_vol;
    //  PSG    psg;                  // @暫定

    void  _Init(void);
    void   ReadHeader(File * file, PPSHEADER & ppsheader);
};
