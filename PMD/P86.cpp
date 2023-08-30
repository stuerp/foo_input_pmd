
// 86B PCM Driver「P86DRV Unit / Programmed by M.Kajihara 96/01/16 / Windows Converted by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <windows.h>
#include <tchar.h>

#include <stdlib.h>
#include <math.h>

#include "P86.h"

P86DRV::P86DRV(File * file) : _File(file), _Data()
{
    _Init();
}

P86DRV::~P86DRV()
{
    if (_Data)
        ::free(_Data);
}

//  初期化
bool P86DRV::Init(uint32_t r, bool useInterpolation)
{
    _Init();

    SetSampleRate(r, useInterpolation);

    return true;
}

//  初期化(内部処理)
void P86DRV::_Init(void)
{
    ::memset(_FilePath, 0, sizeof(_FilePath));
    ::memset(&_Header, 0, sizeof(_Header));

    if (_Data)
    {
        ::free(_Data);
        _Data = NULL;
    }

    _UseInterpolation = false;
    _SampleRate = SOUND_44K;
    _OrigSampleRate = SampleRates[3]; // 16.54kHz
    _Pitch = 0;
    _Volume = 0;

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

    _PanFlag = 0;
    _PanData = 0;

    _AVolume = 0;

    SetVolume(0);

    _Enabled = false;
}

/// <summary>
/// Loads a P86 file (Professional Music Driver P86 Samples Pack file)
/// </summary>
int P86DRV::Load(const WCHAR * filePath)
{
    Stop();

    _FilePath[0] = '\0';

    if (*filePath == '\0')
        return P86_OPEN_FAILED;

    if (!_File->Open(filePath))
    {
        if (_Data)
        {
            ::free(_Data);
            _Data = NULL;

            ::memset(&_Header, 0, sizeof(_Header));
            ::memset(_FilePath, 0, sizeof(_FilePath));
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

    if (::memcmp(&_Header, &p86header2, sizeof(_Header)) == 0)
    {
        ::wcscpy_s(_FilePath, filePath);

        _File->Close();

        return P86_ALREADY_LOADED;    // 同じファイル
    }

    if (_Data)
    {
        ::free(_Data);
        _Data = NULL;
    }

    ::memcpy(&_Header, &p86header2, sizeof(_Header));

    FileSize -= P86HEADERSIZE;

    if ((_Data = (uint8_t *) ::malloc(FileSize)) == NULL)
    {
        _File->Close();
        return PPZ_OUT_OF_MEMORY;
    }

    _File->Read(_Data, (uint32_t) FileSize);

    ::wcscpy_s(_FilePath, filePath);

    _File->Close();

    return P86_SUCCESS;
}

// Playback frequency, primary complement setting
void P86DRV::SetSampleRate(uint32_t synthesisRate, bool useInterpolation)
{
    _SampleRate = (int) synthesisRate;
    _UseInterpolation = useInterpolation;

    uint32_t Pitch = (uint32_t) ((uint64_t) _Pitch * _OrigSampleRate / _SampleRate);

    addsize2 = (int) ((Pitch & 0xffff) >>  4);
    addsize1 = (int) ( Pitch           >> 16);
}

void P86DRV::SetVolume(int volume)
{
    CreateVolumeTable(volume);
}

bool P86DRV::SetVol(int volume)
{
    _Volume = volume;

    return true;
}

//  PCM 番号設定
bool P86DRV::SetNeiro(int num)
{
    if (_Data == NULL)
    {
        _start_ofs = NULL;
    }
    else
    {
        _start_ofs = _Data + _Header.pcmnum[num].start;
    }
    _size = _Header.pcmnum[num].size;
    repeat_flag = false;
    release_flag1 = false;
    return true;
}

//  PAN 設定
bool P86DRV::SetPan(int flag, int data)
{
    _PanFlag = flag;
    _PanData = data;

    return true;
}

bool P86DRV::SetPitch(int sampleRateIndex, uint32_t pitch)
{
    if (sampleRateIndex < 0 || sampleRateIndex >= _countof(SampleRates))
        return false;

    if (pitch > 0x1fffff)
        return false;

    _OrigSampleRate = SampleRates[sampleRateIndex];
    _Pitch = pitch;

    pitch = (uint32_t) ((uint64_t) pitch * _OrigSampleRate / _SampleRate);

    addsize2 = (int) ((pitch & 0xffff) >> 4);
    addsize1 = (int) (pitch >> 16);

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

void P86DRV::Play()
{
    start_ofs = _start_ofs;
    start_ofs_x = 0;
    size = _size;

    _Enabled = true;
    release_flag2 = false;
}

//  P86 停止
bool P86DRV::Stop(void)
{
    _Enabled = false;
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
        _Enabled = false;
    }
    return true;
}

//  合成
void P86DRV::Mix(Sample * sampleData, size_t sampleCount) noexcept
{
    if (!_Enabled)
        return;

    if (size <= 1)
    {
        // Primary interpolation measures
        _Enabled = false;

        return;
    }

    if (_UseInterpolation)
    {
        switch (_PanFlag)
        {
            case 0: double_trans_i(sampleData, sampleCount); break;
            case 1: left_trans_i(sampleData, sampleCount); break;
            case 2: right_trans_i(sampleData, sampleCount); break;
            case 3: double_trans_i(sampleData, sampleCount); break;
            case 4: double_trans_g_i(sampleData, sampleCount); break;
            case 5: left_trans_g_i(sampleData, sampleCount); break;
            case 6: right_trans_g_i(sampleData, sampleCount); break;
            case 7: double_trans_g_i(sampleData, sampleCount); break;
        }
    }
    else
    {
        switch (_PanFlag)
        {
            case 0: double_trans(sampleData, sampleCount); break;
            case 1: left_trans(sampleData, sampleCount); break;
            case 2: right_trans(sampleData, sampleCount); break;
            case 3: double_trans(sampleData, sampleCount); break;
            case 4: double_trans_g(sampleData, sampleCount); break;
            case 5: left_trans_g(sampleData, sampleCount); break;
            case 6: right_trans_g(sampleData, sampleCount); break;
            case 7: double_trans_g(sampleData, sampleCount); break;
        }
    }
}

//  真ん中（一次補間あり）
void P86DRV::double_trans_i(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample data = (_VolumeTable[_Volume][*start_ofs] * (0x1000 - start_ofs_x) + _VolumeTable[_Volume][*(start_ofs + 1)] * start_ofs_x) >> 12;

        *sampleData++ += data;
        *sampleData++ += data;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  真ん中（逆相、一次補間あり）
void P86DRV::double_trans_g_i(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample data = (_VolumeTable[_Volume][*start_ofs] * (0x1000 - start_ofs_x) + _VolumeTable[_Volume][*(start_ofs + 1)] * start_ofs_x) >> 12;

        *sampleData++ += data;
        *sampleData++ -= data;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  左寄り（一次補間あり）
void P86DRV::left_trans_i(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample data = (_VolumeTable[_Volume][*start_ofs] * (0x1000 - start_ofs_x) + _VolumeTable[_Volume][*(start_ofs + 1)] * start_ofs_x) >> 12;

        *sampleData++ += data;

        data = data * _PanData / (256 / 2);

        *sampleData++ += data;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  左寄り（逆相、一次補間あり）
void P86DRV::left_trans_g_i(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample data = (_VolumeTable[_Volume][*start_ofs] * (0x1000 - start_ofs_x) + _VolumeTable[_Volume][*(start_ofs + 1)] * start_ofs_x) >> 12;

        *sampleData++ += data;

        data = data * _PanData / (256 / 2);

        *sampleData++ -= data;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  右寄り（一次補間あり）
void P86DRV::right_trans_i(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample Right = (_VolumeTable[_Volume][*start_ofs] * (0x1000 - start_ofs_x) + _VolumeTable[_Volume][*(start_ofs + 1)] * start_ofs_x) >> 12;
        Sample Left  = Right * _PanData / (256 / 2);

        *sampleData++ += Left;
        *sampleData++ += Right;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  右寄り（逆相、一次補間あり）
void P86DRV::right_trans_g_i(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample Right = (_VolumeTable[_Volume][*start_ofs] * (0x1000 - start_ofs_x) + _VolumeTable[_Volume][*(start_ofs + 1)] * start_ofs_x) >> 12;
        Sample Left  = Right * _PanData / (256 / 2);

        *sampleData++ += Left;
        *sampleData++ -= Right;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  真ん中（一次補間なし）
void P86DRV::double_trans(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample data = _VolumeTable[_Volume][*start_ofs];

        *sampleData++ += data;
        *sampleData++ += data;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  真ん中（逆相、一次補間なし）
void P86DRV::double_trans_g(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample data = _VolumeTable[_Volume][*start_ofs];

        *sampleData++ += data;
        *sampleData++ -= data;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  左寄り（一次補間なし）
void P86DRV::left_trans(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample data = _VolumeTable[_Volume][*start_ofs];

        *sampleData++ += data;

        data = data * _PanData / (256 / 2);

        *sampleData++ += data;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  左寄り（逆相、一次補間なし）
void P86DRV::left_trans_g(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample data = _VolumeTable[_Volume][*start_ofs];

        *sampleData++ += data;

        data = data * _PanData / (256 / 2);

        *sampleData++ -= data;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  右寄り（一次補間なし）
void P86DRV::right_trans(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample Right = _VolumeTable[_Volume][*start_ofs];
        Sample Left  = Right * _PanData / (256 / 2);

        *sampleData++ += Left;
        *sampleData++ += Right;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

//  右寄り（逆相、一次補間なし）
void P86DRV::right_trans_g(Sample * sampleData, size_t sampleCount)
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        Sample Right = _VolumeTable[_Volume][*start_ofs];
        Sample Left  = Right * _PanData / (256 / 2);

        *sampleData++ += Left;
        *sampleData++ -= Right;

        if (AddAddress())
        {
            _Enabled = false;
            return;
        }
    }
}

/// <summary>
/// Adds an address.
/// </summary>
bool P86DRV::AddAddress()
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

void P86DRV::CreateVolumeTable(int volume)
{
    int NewAVolume = (int) (0x1000 * ::pow(10.0, volume / 40.0));

    if (NewAVolume == _AVolume)
        return;

    _AVolume = NewAVolume;

    for (int i = 0; i < 16; ++i)
    {
        double Volume = (double) _AVolume * i / 256; // ::pow(2.0, (i + 15) / 2.0) * AVolume / 0x18000;

        for (int j = 0; j < 256; ++j)
            _VolumeTable[i][j] = (Sample) (Volume * (int8_t) j);
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

