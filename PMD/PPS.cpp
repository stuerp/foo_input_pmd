﻿
// SSG PCM Driver「PPSDRV」 Unit / Original Programmed  by NaoNeko / Modified by Kaja / Windows Converted by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <Windows.h>
#include <tchar.h>

#include <stdlib.h>
#include <math.h>

#include "PPS.h"

PPSDRV::PPSDRV(File * file) : _File(file), _Samples()
{
    _Init();
}

PPSDRV::~PPSDRV()
{
    if (_Samples)
        ::free(_Samples);
}

bool PPSDRV::Init(uint32_t r, bool ip)
{
    _Init();

    SetRate(r, ip);

    return true;
}

void PPSDRV::_Init(void)
{
    _FilePath[0] = '\0';

    ::memset(&ppsheader, 0, sizeof(PPSHEADER));

    _SynthesisRate = SOUND_44K;
    _UseInterpolation = false;

    if (_Samples)
    {
        ::free(_Samples);
        _Samples = nullptr;
    }

    _IsPlaying = false;
    single_flag = false;
    low_cpu_check_flag = false;

    data_offset1 = nullptr;
    data_offset2 = nullptr;
    data_size1 = 0;
    data_size2 = 0;

    data_xor1 = 0;
    data_xor2 = 0;
    tick1 = 0;
    tick2 = 0;
    tick_xor1 = 0;
    tick_xor2 = 0;
    volume1 = 0;
    volume2 = 0;
    _KeyOffVolume = 0;

    SetVolume(-10);
}

//  00H PDR 停止
bool PPSDRV::Stop(void)
{
    _IsPlaying = false;

    data_offset1 = data_offset2 = nullptr;
    data_size1 = data_size2 = 0;

    return true;
}

//  01H PDR 再生
bool PPSDRV::Play(int num, int shift, int volshift)
{
    if (ppsheader.pcmnum[num].address == 0)
        return false;

    int al = 225 + ppsheader.pcmnum[num].toneofs;

    al = al % 256;

    if (shift < 0)
    {
        if ((al += shift) <= 0)
            al = 1;
    }
    else
    {
        if ((al += shift) > 255)
            al = 255;
    }

    if (ppsheader.pcmnum[num].volumeofs + volshift >= 15)
        return false;

    // Don't play when the volume is below 0
    if (_IsPlaying && !single_flag)
    {
        //  ２重発音処理
        volume2 = volume1;          // １音目を２音目に移動
        data_offset2 = data_offset1;
        data_size2 = data_size1;
        data_xor2 = data_xor1;
        tick2 = tick1;
        tick_xor2 = tick_xor1;
    }
    else
    {
        //  １音目で再生
        data_size2 = 0;            // ２音目は停止中
    }

    volume1      = ppsheader.pcmnum[num].volumeofs + volshift;
    data_offset1 = &_Samples[(ppsheader.pcmnum[num].address - PPSHEADERSIZE) * 2];
    data_size1   = ppsheader.pcmnum[num].leng * 2;  // １音目を消して再生
    data_xor1    = 0;

    if (low_cpu_check_flag)
    {
        tick1 = ((8000 * al / 225) << 16) / _SynthesisRate;
        tick_xor1 = tick1 & 0xffff;
        tick1 >>= 16;
    }
    else
    {
        tick1 = ((16000 * al / 225) << 16) / _SynthesisRate;
        tick_xor1 = tick1 & 0xffff;
        tick1 >>= 16;
    }

    _IsPlaying = true;

    return true;
}

//  PPS 読み込み
int PPSDRV::Load(WCHAR * filePath)
{
    Stop();

    _FilePath[0] = '\0';

    if (*filePath == '\0')
        return PPS_OPEN_FAILED;

    if (!_File->Open(filePath))
    {
        if (_Samples)
        {
            ::free(_Samples);
            _Samples = nullptr;

            ::memset(&ppsheader, 0, sizeof(ppsheader));
        }

        return PPS_OPEN_FAILED;            //  ファイルが開けない
    }

    PPSHEADER  ppsheader2;

    size_t size = (size_t) _File->GetFileSize(filePath);    // ファイルサイズ

    ReadHeader(_File, ppsheader2);

    if (::memcmp(&ppsheader, &ppsheader2, sizeof(ppsheader)) == 0)
    {
        ::wcscpy_s(_FilePath, filePath);
        _File->Close();

        return PPS_ALREADY_LOADED;    // 同じファイル
    }

    if (_Samples)
    {
        ::free(_Samples);    // いったん開放
        _Samples = nullptr;
    }

    ::memcpy(&ppsheader, &ppsheader2, sizeof(ppsheader));

    size -= PPSHEADERSIZE;

    if ((_Samples = (Sample *) malloc(size * sizeof(Sample) * 2 / sizeof(uint8_t))) == NULL)
    {
        _File->Close();
        return PPS_OUT_OF_MEMORY;      // メモリが確保できない
    }

    {
        uint8_t * Data = (uint8_t *) ::malloc(size);

        if (Data ==nullptr)
        {
            _File->Close();
            return PPS_OUT_OF_MEMORY;
        }

        if (_File->Read(Data, (uint32_t) size) == (int32_t) size)
        {
            // Convert the sample format.
            uint8_t * Src = Data;
            Sample * Dst = _Samples;

            for (size_t i = 0; i < size / (int) sizeof(uint8_t); i++)
            {
                *Dst++ = ((*Src) >> 4) & 0x0F;
                *Dst++ =  (*Src)       & 0x0F;

                Src++;
            }
        }

        ::free(Data);

        //  PPS correction (miniature noise countermeasure) / Attenuate by 160 samples
        uint32_t  j, start_pps, end_pps;

        for (size_t i = 0; i < MAX_PPS; i++)
        {
            end_pps   = (uint32_t) (ppsheader.pcmnum[i].address - PPSHEADERSIZE * 2) + (uint32_t) (ppsheader.pcmnum[i].leng * 2);
            start_pps = end_pps - 160;

            if (start_pps < ppsheader.pcmnum[i].address - PPSHEADERSIZE * 2)
                start_pps = ppsheader.pcmnum[i].address - PPSHEADERSIZE * 2;

            for (j = start_pps; j < end_pps; j++)
            {
                _Samples[j] = (Sample) (_Samples[j] - (j - start_pps) * 16 / (end_pps - start_pps));

                if (_Samples[j] < 0)
                    _Samples[j] = 0;
            }
        }
    }

    ::wcscpy_s(_FilePath, filePath);

    _File->Close();

    return PPS_SUCCESS;
}

/// <summary>
/// Reads  the header.
/// </summary>
void PPSDRV::ReadHeader(File * file, PPSHEADER & header)
{
    uint8_t Data[84];

    file->Read(Data, sizeof(Data));

    for (int i = 0; i < MAX_PPS; i++)
    {
        header.pcmnum[i].address   = (uint16_t) (Data[i * 6]     | (Data[i * 6 + 1] << 8));
        header.pcmnum[i].leng      = (uint16_t) (Data[i * 6 + 2] | (Data[i * 6 + 3] << 8));
        header.pcmnum[i].toneofs   =             Data[i * 6 + 4];
        header.pcmnum[i].volumeofs =             Data[i * 6 + 5];
    }
}

/// <summary>
/// Sets a parameter.
/// </summary>
bool PPSDRV::SetParam(int index, bool value)
{
    switch (index)
    {
        case 0:
            single_flag = value;
            return true;

        case 1:
            low_cpu_check_flag = value;
            return true;

        default:
            return false;
    }
}

/// <summary>
/// Sets the synthesis rate.
/// </summary>
bool PPSDRV::SetRate(uint32_t rate, bool useInterpolation)
{
    _SynthesisRate = (int) rate;
    _UseInterpolation = useInterpolation;

    return true;
}

/// <summary>
/// Sets the volume.
/// </summary>
void PPSDRV::SetVolume(int vol)
{
    double Base = 0x4000 * 2 / 3.0 * ::pow(10.0, vol / 40.0);

    for (int i = 15; i >= 1; i--)
    {
        _EmitTable[i] = (int) Base;
        Base /= 1.189207115;
    }

    _EmitTable[0] = 0;
}

void PPSDRV::Mix(Sample * sampleData, int sampleCount)  // 合成
{
/*
    static const int table[16*16] = {
            0, 0, 0, 5, 9,10,11,12,13,13,14,14,14,15,15,15,
            0, 0, 3, 5, 9,10,11,12,13,13,14,14,14,15,15,15,
            0, 3, 5, 7, 9,10,11,12,13,13,14,14,14,15,15,15,
            5, 5, 7, 9,10,11,12,13,13,13,14,14,14,15,15,15,
            9, 9, 9,10,11,12,12,13,13,14,14,14,15,15,15,15,
        10,10,10,11,12,12,13,13,13,14,14,14,15,15,15,15,
        11,11,11,12,12,13,13,13,14,14,14,14,15,15,15,15,
        12,12,12,12,13,13,13,14,14,14,14,15,15,15,15,15,
        13,13,13,13,13,13,14,14,14,14,14,15,15,15,15,15,
        13,13,13,13,14,14,14,14,14,14,15,15,15,15,15,15,
        14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,
        14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,
        14,14,14,14,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15
    };
*/
    if (!_IsPlaying && (_KeyOffVolume == 0))
        return;

    for (int i = 0; i < sampleCount; i++)
    {
        int al1, al2, ah1, ah2;

        if (data_size1 > 1)
        {
            al1 = * data_offset1      - volume1;
            al2 = *(data_offset1 + 1) - volume1;

            if (al1 < 0)
                al1 = 0;

            if (al2 < 0)
                al2 = 0;
        }
        else
        {
            al1 = al2 = 0;
        }

        if (data_size2 > 1)
        {
            ah1 = * data_offset2      - volume2;
            ah2 = *(data_offset2 + 1) - volume2;

            if (ah1 < 0)
                ah1 = 0;

            if (ah2 < 0)
                ah2 = 0;
        }
        else
            ah1 = ah2 = 0;

        Sample data;

        //    al1 = table[(al1 << 4) + ah1];
        //    psg.SetReg(0x0a, al1);
        if (_UseInterpolation)
        {
            data = (_EmitTable[al1] * (0x10000 - data_xor1) + _EmitTable[al2] * data_xor1 +
                    _EmitTable[ah1] * (0x10000 - data_xor2) + _EmitTable[ah2] * data_xor2) / 0x10000;
        }
        else
            data = _EmitTable[al1] + _EmitTable[ah1];

        _KeyOffVolume = (_KeyOffVolume * 255) / 256;

        data += _KeyOffVolume;

        *sampleData++ += data;
        *sampleData++ += data;

        //    psg.Mix(dest, 1);
        //    dest += 2;

        if (data_size2 > 1)
        {  // ２音合成再生
            data_xor2 += tick_xor2;

            if (data_xor2 >= 0x10000)
            {
                data_size2--;
                data_offset2++;
                data_xor2 -= 0x10000;
            }

            data_size2 -= tick2;
            data_offset2 += tick2;

            if (low_cpu_check_flag)
            {
                data_xor2 += tick_xor2;

                if (data_xor2 >= 0x10000)
                {
                    data_size2--;
                    data_offset2++;
                    data_xor2 -= 0x10000;
                }

                data_size2 -= tick2;
                data_offset2 += tick2;
            }
        }

        data_xor1 += tick_xor1;

        if (data_xor1 >= 0x10000)
        {
            data_size1--;
            data_offset1++;
            data_xor1 -= 0x10000;
        }

        data_size1 -= tick1;
        data_offset1 += tick1;

        if (low_cpu_check_flag)
        {
            data_xor1 += tick_xor1;
            if (data_xor1 >= 0x10000)
            {
                data_size1--;
                data_offset1++;
                data_xor1 -= 0x10000;
            }
            data_size1 -= tick1;
            data_offset1 += tick1;
        }

        if (data_size1 <= 1 && data_size2 <= 1)
        {    // 両方停止
            if (_IsPlaying)
                _KeyOffVolume += _EmitTable[data_offset1[data_size1 - 1]];

            _IsPlaying = false;    // 発音停止
        }
        else
        if (data_size1 <= 1 && data_size2 > 1)
        {  // １音目のみが停止
            volume1 = volume2;
            data_size1 = data_size2;
            data_offset1 = data_offset2;
            data_xor1 = data_xor2;
            tick1 = tick2;
            tick_xor1 = tick_xor2;
            data_size2 = 0;
        }
        else
        if (data_size1 > 1 && data_size2 < 1)
        {  // ２音目のみが停止
            if (data_offset2 != NULL)
            {
                _KeyOffVolume += _EmitTable[data_offset2[data_size2 - 1]];
                data_offset2 = NULL;
            }
        }
    }
}
