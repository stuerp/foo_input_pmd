﻿
// 86B PCM Driver「P86DRV Unit / Programmed by M.Kajihara 96/01/16 / Windows Converted by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <windows.h>
#include <tchar.h>

#include <stdlib.h>
#include <math.h>

#include "P86.h"

P86DRV::P86DRV(File * file) : _File(file), p86_addr()
{
    _Init();
}

P86DRV::~P86DRV()
{
    if (p86_addr)
        free(p86_addr);
}

//  初期化
bool P86DRV::Init(uint32_t r, bool ip)
{
    _Init();

    SetRate(r, ip);

    return true;
}

//  初期化(内部処理)
void P86DRV::_Init(void)
{
    ::memset(_FileName, 0, sizeof(_FileName));
    ::memset(&p86header, 0, sizeof(p86header));

    interpolation = false;
    rate = SOUND_44K;
    srcrate = ratetable[3];    // 16.54kHz
    ontei = 0;
    vol = 0;

    if (p86_addr != NULL)
    {
        free(p86_addr);      // メモリ開放
        p86_addr = NULL;
    }

    start_ofs = NULL;
    start_ofs_x = 0;
    size = 0;
    _start_ofs = NULL;
    _size = 0;
    addsize1 = 0;
    addsize2 = 0;
    repeat_ofs = NULL;
    repeat_size = 0;
    release_ofs = NULL;
    release_size = 0;
    repeat_flag = false;
    release_flag1 = false;
    release_flag2 = false;

    pcm86_pan_flag = 0;
    pcm86_pan_dat = 0;
    play86_flag = false;

    AVolume = 0;

    SetVolume(0);
}

// Playback frequency, primary complement setting
bool P86DRV::SetRate(uint32_t r, bool ip)
{
    uint32_t  _ontei;

    rate = (int) r;
    interpolation = ip;

    _ontei = (uint32_t) ((uint64_t) ontei * srcrate / rate);
    addsize2 = (int) ((_ontei & 0xffff) >> 4);
    addsize1 = (int) (_ontei >> 16);

    return true;
}

//  音量調整用
void P86DRV::SetVolume(int volume)
{
    MakeVolumeTable(volume);
}

//  音量テーブル作成
void P86DRV::MakeVolumeTable(int volume)
{
    int    i, j;
    int    AVolume_temp;
    double  temp;

    AVolume_temp = (int) (0x1000 * pow(10.0, volume / 40.0));
    if (AVolume != AVolume_temp)
    {
        AVolume = AVolume_temp;
        for (i = 0; i < 16; i++)
        {
            //@      temp = pow(2.0, (i + 15) / 2.0) * AVolume / 0x18000;
            temp = i * AVolume / 256;
            for (j = 0; j < 256; j++)
            {
                VolumeTable[i][j] = (Sample) ((int8_t) j * temp);
            }
        }
    }
}

//  ヘッダ読み込み
void P86DRV::ReadHeader(File * file, P86HEADER & header)
{
    uint8_t buf[1552];

    file->Read(buf, sizeof(buf));

    ::memcpy(header.header, &buf[0x00], 12);

    header.Version = buf[0x0c];

    ::memcpy(header.All_Size, &buf[0x0d], 3);

    for (int i = 0; i < MAX_P86; i++)
    {
        ::memcpy(&header.pcmnum[i].start[0], &buf[0x10 + i * 6], 3);
        ::memcpy(&header.pcmnum[i].size[0], &buf[0x13 + i * 6], 3);
    }
}

/// <summary>
/// Loads a P86 file (Professional Music Driver P86 Samples Pack file)
/// </summary>
int P86DRV::Load(const WCHAR * filePath)
{
    Stop();

    _FileName[0] = '\0';

    if (*filePath == '\0')
        return P86_OPEN_FAILED;

    if (!_File->Open(filePath))
    {
        if (p86_addr)
        {
            ::free(p86_addr);
            p86_addr = NULL;

            ::memset(&p86header, 0, sizeof(p86header));
            ::memset(_FileName, 0, sizeof(_FileName));
        }

        return P86_OPEN_FAILED;
    }

    // Header Hexdump:  50 43 4D 38 36 20 44 41 54 41 0A
    int i;
    P86HEADER  _p86header;
    P86HEADER2  p86header2;

    size_t FileSize = (size_t) _File->GetFileSize(filePath);    // ファイルサイズ

    ReadHeader(_File, _p86header);

    // P86HEADER → P86HEADER2 へ変換
    memset(&p86header2, 0, sizeof(p86header2));

    for (i = 0; i < MAX_P86; i++)
    {
        p86header2.pcmnum[i].start = _p86header.pcmnum[i].start[0] + _p86header.pcmnum[i].start[1] * 0x100 + _p86header.pcmnum[i].start[2] * 0x10000 - 0x610;
        p86header2.pcmnum[i].size  = _p86header.pcmnum[i].size[0]  + _p86header.pcmnum[i].size[1]  * 0x100 + _p86header.pcmnum[i].size[2]  * 0x10000;
    }

    if (::memcmp(&p86header, &p86header2, sizeof(p86header)) == 0)
    {
        ::wcscpy_s(_FileName, filePath);

        _File->Close();

        return P86_ALREADY_LOADED;    // 同じファイル
    }

    if (p86_addr != NULL)
    {
        free(p86_addr);    // いったん開放
        p86_addr = NULL;
    }

    ::memcpy(&p86header, &p86header2, sizeof(p86header));

    FileSize -= P86HEADERSIZE;

    if ((p86_addr = (uint8_t *) malloc(FileSize)) == NULL)
    {
        _File->Close();
        return PPZ_OUT_OF_MEMORY;      // メモリが確保できない
    }

    _File->Read(p86_addr, (uint32_t) FileSize);

    ::wcscpy_s(_FileName, filePath);

    _File->Close();

    return P86_SUCCESS;
}

//  PCM 番号設定
bool P86DRV::SetNeiro(int num)
{
    if (p86_addr == NULL)
    {
        _start_ofs = NULL;
    }
    else
    {
        _start_ofs = p86_addr + p86header.pcmnum[num].start;
    }
    _size = p86header.pcmnum[num].size;
    repeat_flag = false;
    release_flag1 = false;
    return true;
}

//  PAN 設定
bool P86DRV::SetPan(int flag, int data)
{
    pcm86_pan_flag = flag;
    pcm86_pan_dat = data;
    return true;
}

//  音量設定
bool P86DRV::SetVol(int _vol)
{
    vol = _vol;
    return true;
}

// Setting the pitch frequency
//    _srcrate : 入力データの周波数
//      0 : 4.13kHz
//      1 : 5.52kHz
//      2 : 8.27kHz
//      3 : 11.03kHz
//      4 : 16.54kHz
//      5 : 22.05kHz
//      6 : 33.08kHz
//      7 : 44.1kHz
//    _ontei : 設定音程
bool P86DRV::SetOntei(int _srcrate, uint32_t _ontei)
{
    if (_srcrate < 0 || _srcrate > 7)
        return false;

    if (_ontei > 0x1fffff)
        return false;

    ontei = _ontei;
    srcrate = ratetable[_srcrate];

    _ontei = (uint32_t) ((uint64_t) _ontei * srcrate / rate);

    addsize2 = (int) ((_ontei & 0xffff) >> 4);
    addsize1 = (int) (_ontei >> 16);

    return true;
}

//  リピート設定
bool P86DRV::SetLoop(int loop_start, int loop_end, int release_start, bool adpcm)
{
    int    ax, dx, _dx;

    repeat_flag = true;
    release_flag1 = false;
    dx = _dx = _size;

    // 一個目 = リピート開始位置
    ax = loop_start;
    if (ax >= 0)
    {
        // 正の場合
        if (adpcm) ax *= 32;
        if (ax >= _size - 1) ax = _size - 2;    // アクセス違反対策
        if (ax < 0) ax = 0;

        repeat_size = _size - ax;    // リピートサイズ＝全体のサイズ-指定値
        repeat_ofs = _start_ofs + ax;  // リピート開始位置から指定値を加算
    }
    else
    {
        // 負の場合
        ax = -ax;
        if (adpcm) ax *= 32;
        dx -= ax;
        if (dx < 0)
        {              // アクセス違反対策
            ax = _dx;
            dx = 0;
        }

        repeat_size = ax;  // リピートサイズ＝neg(指定値)
        repeat_ofs = _start_ofs + dx;  //リピート開始位置に(全体サイズ-指定値)を加算
    }

    // ２個目 = リピート終了位置
    ax = loop_end;

    if (ax > 0)
    {
        // 正の場合
        if (adpcm) ax *= 32;
        if (ax >= _size - 1) ax = _size - 2;    // アクセス違反対策
        if (ax < 0) ax = 0;

        _size = ax;
        dx -= ax;
        // リピートサイズから(旧サイズ-新サイズ)を引く
        repeat_size -= dx;
    }
    else
    if (ax < 0)
    {
        // 負の場合
        ax = -ax;
        if (adpcm) ax *= 32;
        if (ax > repeat_size) ax = repeat_size;
        repeat_size -= ax;  // リピートサイズからneg(指定値)を引く
        _size -= ax;      // 本来のサイズから指定値を引く
    }

    // ３個目 = リリース開始位置
    ax = release_start;
    if ((uint16_t) ax != 0x8000)
    {        // 8000Hなら設定しない
// release開始位置 = start位置に設定
        release_ofs = _start_ofs;

        // release_size = 今のsizeに設定
        release_size = _dx;

        // リリースするに設定
        release_flag1 = true;
        if (ax > 0)
        {
            // 正の場合
            if (adpcm) ax *= 32;
            if (ax >= _size - 1) ax = _size - 2;    // アクセス違反対策
            if (ax < 0) ax = 0;

            // リースサイズ＝全体のサイズ-指定値
            release_size -= ax;

            // リリース開始位置から指定値を加算
            release_ofs += ax;
        }
        else
        {
            // 負の場合
            ax = -ax;
            if (adpcm) ax *= 32;
            if (ax > _size) ax = _size;

            // リリースサイズ＝neg(指定値)
            release_size = ax;

            _dx -= ax;

            // リリース開始位置に(全体サイズ-指定値)を加算
            release_ofs += _dx;
        }
    }
    return true;
}

//  P86 再生
bool P86DRV::Play(void)
{
    start_ofs = _start_ofs;
    start_ofs_x = 0;
    size = _size;

    play86_flag = true;
    release_flag2 = false;
    return true;
}

//  P86 停止
bool P86DRV::Stop(void)
{
    play86_flag = false;
    return true;
}

//  P86 keyoff
bool P86DRV::Keyoff(void)
{
    if (release_flag1 == true)
    {    // リリースが設定されているか?
        start_ofs = release_ofs;
        size = release_size;
        release_flag2 = true;    // リリースした
    }
    else
    {
        play86_flag = false;
    }
    return true;
}

//  合成
void P86DRV::Mix(Sample * dest, int nsamples)
{
    if (play86_flag == false) return;
    if (size <= 1)
    {    // 一次補間対策
        play86_flag = false;
        return;
    }

    //  double_trans(dest, nsamples); return;    // @test

    if (interpolation)
    {
        switch (pcm86_pan_flag)
        {
            case 0: double_trans_i(dest, nsamples); break;
            case 1: left_trans_i(dest, nsamples); break;
            case 2: right_trans_i(dest, nsamples); break;
            case 3: double_trans_i(dest, nsamples); break;
            case 4: double_trans_g_i(dest, nsamples); break;
            case 5: left_trans_g_i(dest, nsamples); break;
            case 6: right_trans_g_i(dest, nsamples); break;
            case 7: double_trans_g_i(dest, nsamples); break;
        }
    }
    else
    {
        switch (pcm86_pan_flag)
        {
            case 0: double_trans(dest, nsamples); break;
            case 1: left_trans(dest, nsamples); break;
            case 2: right_trans(dest, nsamples); break;
            case 3: double_trans(dest, nsamples); break;
            case 4: double_trans_g(dest, nsamples); break;
            case 5: left_trans_g(dest, nsamples); break;
            case 6: right_trans_g(dest, nsamples); break;
            case 7: double_trans_g(dest, nsamples); break;
        }
    }
}

//  真ん中（一次補間あり）
void P86DRV::double_trans_i(Sample * dest, int nsamples)
{
    int    i;
    Sample  data;

    for (i = 0; i < nsamples; i++)
    {
        data = (VolumeTable[vol][*start_ofs] * (0x1000 - start_ofs_x) + VolumeTable[vol][*(start_ofs + 1)] * start_ofs_x) >> 12;
        *dest++ += data;
        *dest++ += data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  真ん中（逆相、一次補間あり）
void P86DRV::double_trans_g_i(Sample * dest, int nsamples)
{
    int    i;
    Sample  data;

    for (i = 0; i < nsamples; i++)
    {
        data = (VolumeTable[vol][*start_ofs] * (0x1000 - start_ofs_x) + VolumeTable[vol][*(start_ofs + 1)] * start_ofs_x) >> 12;
        *dest++ += data;
        *dest++ -= data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  左寄り（一次補間あり）
void P86DRV::left_trans_i(Sample * dest, int nsamples)
{
    int    i;
    Sample  data;

    for (i = 0; i < nsamples; i++)
    {
        data = (VolumeTable[vol][*start_ofs] * (0x1000 - start_ofs_x) + VolumeTable[vol][*(start_ofs + 1)] * start_ofs_x) >> 12;
        *dest++ += data;
        data = data * pcm86_pan_dat / (256 / 2);
        *dest++ += data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  左寄り（逆相、一次補間あり）
void P86DRV::left_trans_g_i(Sample * dest, int nsamples)
{
    int    i;
    Sample  data;

    for (i = 0; i < nsamples; i++)
    {
        data = (VolumeTable[vol][*start_ofs] * (0x1000 - start_ofs_x) + VolumeTable[vol][*(start_ofs + 1)] * start_ofs_x) >> 12;
        *dest++ += data;
        data = data * pcm86_pan_dat / (256 / 2);
        *dest++ -= data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  右寄り（一次補間あり）
void P86DRV::right_trans_i(Sample * dest, int nsamples)
{
    int    i;
    Sample  data, data2;

    for (i = 0; i < nsamples; i++)
    {
        data = (VolumeTable[vol][*start_ofs] * (0x1000 - start_ofs_x) + VolumeTable[vol][*(start_ofs + 1)] * start_ofs_x) >> 12;
        data2 = data * pcm86_pan_dat / (256 / 2);
        *dest++ += data2;
        *dest++ += data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  右寄り（逆相、一次補間あり）
void P86DRV::right_trans_g_i(Sample * dest, int nsamples)
{
    int    i;
    Sample  data, data2;

    for (i = 0; i < nsamples; i++)
    {
        data = (VolumeTable[vol][*start_ofs] * (0x1000 - start_ofs_x) + VolumeTable[vol][*(start_ofs + 1)] * start_ofs_x) >> 12;
        data2 = data * pcm86_pan_dat / (256 / 2);
        *dest++ += data2;
        *dest++ -= data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  真ん中（一次補間なし）
void P86DRV::double_trans(Sample * dest, int nsamples)
{
    int    i;
    Sample  data;

    for (i = 0; i < nsamples; i++)
    {
        data = VolumeTable[vol][*start_ofs];
        *dest++ += data;
        *dest++ += data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  真ん中（逆相、一次補間なし）
void P86DRV::double_trans_g(Sample * dest, int nsamples)
{
    int    i;
    Sample  data;

    for (i = 0; i < nsamples; i++)
    {
        data = VolumeTable[vol][*start_ofs];
        *dest++ += data;
        *dest++ -= data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  左寄り（一次補間なし）
void P86DRV::left_trans(Sample * dest, int nsamples)
{
    int    i;
    Sample  data;

    for (i = 0; i < nsamples; i++)
    {
        data = VolumeTable[vol][*start_ofs];
        *dest++ += data;

        data = data * pcm86_pan_dat / (256 / 2);
        *dest++ += data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  左寄り（逆相、一次補間なし）
void P86DRV::left_trans_g(Sample * dest, int nsamples)
{
    int    i;
    Sample  data;

    for (i = 0; i < nsamples; i++)
    {
        data = VolumeTable[vol][*start_ofs];
        *dest++ += data;

        data = data * pcm86_pan_dat / (256 / 2);
        *dest++ -= data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  右寄り（一次補間なし）
void P86DRV::right_trans(Sample * dest, int nsamples)
{
    int    i;
    Sample  data, data2;

    for (i = 0; i < nsamples; i++)
    {
        data = VolumeTable[vol][*start_ofs];
        data2 = data * pcm86_pan_dat / (256 / 2);
        *dest++ += data2;
        *dest++ += data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

//  右寄り（逆相、一次補間なし）
void P86DRV::right_trans_g(Sample * dest, int nsamples)
{
    int    i;
    Sample  data, data2;

    for (i = 0; i < nsamples; i++)
    {
        data = VolumeTable[vol][*start_ofs];
        data2 = data * pcm86_pan_dat / (256 / 2);
        *dest++ += data2;
        *dest++ -= data;

        if (add_address())
        {
            play86_flag = false;
            return;
        }
    }
}

/// <summary>
/// Adds an address.
/// </summary>
bool P86DRV::add_address()
{
    start_ofs_x += addsize2;

    if (start_ofs_x >= 0x1000)
    {
        start_ofs_x -= 0x1000;
        start_ofs++;
        size--;
    }

    start_ofs += addsize1;
    size -= addsize1;

    if (size > 1)
        return false; // Primary interpolation measures

    if (repeat_flag == false || release_flag2)
        return true;

    size = repeat_size;
    start_ofs = repeat_ofs;

    return false;
}
