
// 8 Channel PCM Driver「PPZ8」Unit (Light Version) / Programmed by UKKY / Windows Converted by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>

#include <stdlib.h>
#include <math.h>

#include "PPZ8.h"

//  Constant table (ADPCM Volume to PPZ8 Volume)
const int ADPCM_EM_VOL[256] =
{
     0, 0, 0, 0, 0, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4,
     4, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6,
     6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
     8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,

};

// Constant table (ADPCM Pan to PPZ8 Pan)
const int ADPCM_EM_PAN[4] =
{
    0, 9, 1, 5
};

PPZ8::PPZ8(File * file) : _File(file)
{
    XMS_FRAME_ADR[0] = nullptr;
    XMS_FRAME_ADR[1] = nullptr;

    InitializeInternal();
}

PPZ8::~PPZ8()
{
    if (XMS_FRAME_ADR[0])
        ::free(XMS_FRAME_ADR[0]);

    if (XMS_FRAME_ADR[1])
        ::free(XMS_FRAME_ADR[1]);
}

bool PPZ8::Init(uint32_t rate, bool ip)
{
    InitializeInternal();

    return SetRate(rate, ip);
}

void PPZ8::InitializeInternal(void)
{
    ::memset(PCME_WORK, 0, sizeof(PCME_WORK));

    _HasPVI[0] = false;
    _HasPVI[1] = false;

    ::memset(_FilePath, 0, sizeof(_FilePath));

    _EmulateADPCM = false;
    _UseInterpolation = false;

    Reset();

    // 一旦開放する
    if (XMS_FRAME_ADR[0] != NULL)
    {
        free(XMS_FRAME_ADR[0]);
        XMS_FRAME_ADR[0] = NULL;
    }

    XMS_FRAME_SIZE[0] = 0;

    if (XMS_FRAME_ADR[1] != NULL)
    {
        free(XMS_FRAME_ADR[1]);
        XMS_FRAME_ADR[1] = NULL;
    }

    XMS_FRAME_SIZE[1] = 0;

    PCM_VOLUME = 0;
    _Volume = 0;
    SetAllVolume(VNUM_DEF);    // 全体ボリューム(DEF=12)

    DIST_F = RATE_DEF;       // 再生周波数
}

//  01H PCM 発音
bool PPZ8::Play(int ch, int bufnum, int num, uint16_t start, uint16_t stop)
{
    if ((ch >= _countof(_Channel)) || (XMS_FRAME_ADR[bufnum] == NULL) || (XMS_FRAME_SIZE[bufnum] == 0))
        return false;

    _Channel[ch]._HasPVI = _HasPVI[bufnum];
    _Channel[ch].PCM_FLG = 1;    // 再生開始
    _Channel[ch].PCM_NOW_XOR = 0;  // 小数点部
    _Channel[ch].PCM_NUM = num;

    if ((ch == 7) && _EmulateADPCM && (ch & 0x80) == 0)
    {
        _Channel[ch].PCM_NOW   = &XMS_FRAME_ADR[bufnum][Limit(((int) start) * 64, XMS_FRAME_SIZE[bufnum] - 1, 0)];
        _Channel[ch].PCM_END_S = &XMS_FRAME_ADR[bufnum][Limit(((int) stop - 1) * 64, XMS_FRAME_SIZE[bufnum] - 1, 0)];
        _Channel[ch].PCM_END   = _Channel[ch].PCM_END_S;
    }
    else
    {
        _Channel[ch].PCM_NOW   = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].PZIItem[num].Start];
        _Channel[ch].PCM_END_S = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].PZIItem[num].Start + PCME_WORK[bufnum].PZIItem[num].Size];

        if (_Channel[ch].PCM_LOOP_FLG == 0)
        {
            // ループなし
            _Channel[ch].PCM_END = _Channel[ch].PCM_END_S;
        }
        else
        {
            // ループあり
            if (_Channel[ch].PCM_LOOP_START >= PCME_WORK[bufnum].PZIItem[num].Size)
                _Channel[ch].PCM_LOOP = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].PZIItem[num].Start + PCME_WORK[bufnum].PZIItem[num].Size - 1];
            else
                _Channel[ch].PCM_LOOP = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].PZIItem[num].Start + _Channel[ch].PCM_LOOP_START];

            if (_Channel[ch].PCM_LOOP_END >= PCME_WORK[bufnum].PZIItem[num].Size)
                _Channel[ch].PCM_END = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].PZIItem[num].Start + PCME_WORK[bufnum].PZIItem[num].Size];
            else
                _Channel[ch].PCM_END = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].PZIItem[num].Start + _Channel[ch].PCM_LOOP_END];
        }
    }

    return true;
}

//  02H PCM 停止
bool PPZ8::Stop(int ch)
{
    if (ch >= PCM_CNL_MAX) return false;

    _Channel[ch].PCM_FLG = 0;  // 再生停止
    return true;
}

// 03H Read PVI/PZI file
int PPZ8::Load(const WCHAR * filePath, int bufnum)
{
    if (filePath == nullptr || (filePath && (*filePath == '\0')))
        return PPZ_OPEN_FAILED;

    static const int table1[16] =
    {
          1,   3,   5,   7,   9,  11,  13,  15,
         -1,  -3,  -5,  -7,  -9, -11, -13, -15,
    };

    static const int table2[16] =
    {
         57,  57,  57,  57,  77, 102, 128, 153,
         57,  57,  57,  57,  77, 102, 128, 153,
    };

    bool NOW_PCM_CATE = HasExtension(filePath, MAX_PATH, L".PZI"); // True if PCM format is PZI

    Reset();

    _FilePath[bufnum][0] = '\0';

    if (!_File->Open(filePath))
    {
        if (XMS_FRAME_ADR[bufnum] != NULL)
        {
            free(XMS_FRAME_ADR[bufnum]);    // 開放
            XMS_FRAME_ADR[bufnum] = NULL;
            XMS_FRAME_SIZE[bufnum] = 0;
            memset(&PCME_WORK[bufnum], 0, sizeof(PZIHEADER));
        }
        return PPZ_OPEN_FAILED;        //  ファイルが開けない
    }

    int Size = (int) _File->GetFileSize(filePath);  // ファイルサイズ

    PZIHEADER PZIHeader;

    if (NOW_PCM_CATE)
    {
        ReadHeader(_File, PZIHeader);

        if (::strncmp(PZIHeader.ID, "PZI", 3) != 0)
        {
            _File->Close();

            return PPZ_UNKNOWN_FORMAT;
        }

        if (::memcmp(&PCME_WORK[bufnum], &PZIHeader, sizeof(PZIHEADER)) == 0)
        {
            ::wcscpy(_FilePath[bufnum], filePath);

            _File->Close();

            return PPZ_ALREADY_LOADED;
        }

        if (XMS_FRAME_ADR[bufnum])
        {
            ::free(XMS_FRAME_ADR[bufnum]);

            XMS_FRAME_ADR[bufnum] = NULL;
            XMS_FRAME_SIZE[bufnum] = 0;

            ::memset(&PCME_WORK[bufnum], 0, sizeof(PZIHEADER));
        }

        ::memcpy(&PCME_WORK[bufnum], &PZIHeader, sizeof(PZIHEADER));

        Size -= sizeof(PZIHEADER);

        uint8_t * Data;

        if ((Data = (uint8_t *) ::malloc((size_t) Size)) == NULL)
        {
            _File->Close();

            return PPZ_OUT_OF_MEMORY;
        }

        ::memset(Data, 0, (size_t) Size);

        _File->Read(Data, (uint32_t) Size);

        XMS_FRAME_ADR[bufnum] = Data;
        XMS_FRAME_SIZE[bufnum] = Size;

        ::wcscpy(_FilePath[bufnum], filePath);
        _HasPVI[bufnum] = false;
    }
    else
    {
        PVIHEADER PVIHeader;

        ReadHeader(_File, PVIHeader);

        if (::strncmp(PVIHeader.ID, "PVI", 3))
        {
            _File->Close();

            return PPZ_UNKNOWN_FORMAT;
        }

        ::strncpy(PZIHeader.ID, "PZI1", 4);

        int PVISize = 0;

        for (int i = 0; i < PVIHeader.Count; ++i)
        {
            PZIHeader.PZIItem[i].Start      = (uint32_t) ((                                 PVIHeader.PVIItem[i].Start      << (5 + 1)));
            PZIHeader.PZIItem[i].Size       = (uint32_t) ((PVIHeader.PVIItem[i].End - PVIHeader.PVIItem[i].Start + 1) << (5 + 1));
            PZIHeader.PZIItem[i].LoopStart  = 0xFFFF;
            PZIHeader.PZIItem[i].LoopEnd    = 0xFFFF;
            PZIHeader.PZIItem[i].SampleRate = 16000; // 16kHz

            PVISize += PZIHeader.PZIItem[i].Size;
        }

        for (int i = PVIHeader.Count; i < 128; ++i)
        {
            PZIHeader.PZIItem[i].Start      = 0;
            PZIHeader.PZIItem[i].Size       = 0;
            PZIHeader.PZIItem[i].LoopStart  = 0xFFFF;
            PZIHeader.PZIItem[i].LoopEnd    = 0xFFFF;
            PZIHeader.PZIItem[i].SampleRate = 0;
        }

        if (::memcmp(&PCME_WORK[bufnum].PZIItem, &PZIHeader.PZIItem, sizeof(PZIHEADER) - 0x20) == 0)
        {
            ::wcscpy(_FilePath[bufnum], filePath);
            _File->Close();

            return PPZ_ALREADY_LOADED;
        }

        if (XMS_FRAME_ADR[bufnum] != NULL)
        {
            ::free(XMS_FRAME_ADR[bufnum]);

            XMS_FRAME_ADR[bufnum] = NULL;
            XMS_FRAME_SIZE[bufnum] = 0;

            ::memset(&PCME_WORK[bufnum], 0, sizeof(PZIHEADER));
        }

        ::memcpy(&PCME_WORK[bufnum], &PZIHeader, sizeof(PZIHEADER));

        Size -= sizeof(PVIHEADER);
        PVISize /= 2;

        size_t DstSize = (size_t) (std::max)(Size, PVISize) * 2;

        uint8_t * pdst = (uint8_t *) ::malloc(DstSize);

        if (pdst == nullptr)
        {
            _File->Close();
            return PPZ_OUT_OF_MEMORY;
        }

        ::memset(pdst, 0, DstSize);


        XMS_FRAME_ADR[bufnum]  = pdst;
        XMS_FRAME_SIZE[bufnum] = PVISize * 2;

        {
            size_t SrcSize = (size_t) (std::max)(Size, PVISize);

            uint8_t * psrc = (uint8_t *) ::malloc(SrcSize);

            if (psrc == NULL)
            {
                _File->Close();
                return PPZ_OUT_OF_MEMORY;
            }

            ::memset(psrc, 0, SrcSize);

            _File->Read(psrc, (uint32_t) Size);

            uint8_t * psrc2 = psrc;

            // ADPCM > PCM に変換
            for (int i = 0; i < PVIHeader.Count; ++i)
            {
                int X_N     = X_N0;     // Xn (ADPCM>PCM 変換用)
                int DELTA_N = DELTA_N0; // DELTA_N(ADPCM>PCM 変換用)

                for (int j = 0; j < (int) PZIHeader.PZIItem[i].Size / 2; ++j)
                {

                    X_N     = Limit(X_N + table1[(*psrc >> 4) & 0x0f] * DELTA_N / 8, 32767, -32768);
                    DELTA_N = Limit(DELTA_N * table2[(*psrc >> 4) & 0x0f] / 64, 24576, 127);

                    *pdst++ = (uint8_t) (X_N / (32768 / 128) + 128);

                    X_N     = Limit(X_N + table1[*psrc & 0x0f] * DELTA_N / 8, 32767, -32768);
                    DELTA_N = Limit(DELTA_N * table2[*psrc++ & 0x0f] / 64, 24576, 127);

                    *pdst++ = (uint8_t) (X_N / (32768 / 128) + 128);
                }
            }

            ::free(psrc2);
        }

        ::wcscpy(_FilePath[bufnum], filePath);

        _HasPVI[bufnum] = true;
    }

    _File->Close();

    return PPZ_SUCCESS;
}

// 07H Volume
bool PPZ8::SetVolume(int ch, int vol)
{
    if (ch >= PCM_CNL_MAX)
        return false;

    if (ch != 7 || !_EmulateADPCM)
        _Channel[ch].PCM_VOL = vol;
    else
        _Channel[ch].PCM_VOL = ADPCM_EM_VOL[vol & 0xff];

    return true;
}

// 0BH Pitch Frequency
bool PPZ8::SetPitch(int channelNumber, uint32_t pitch)
{
    if (channelNumber >= _countof(_Channel))
        return false;

    if (channelNumber == 7 && _EmulateADPCM) // Emulating ADPCM?
        pitch = (pitch & 0xffff) * 0x8000 / 0x49ba;

    _Channel[channelNumber].PCM_ADDS_L = (int) (pitch & 0xffff);
    _Channel[channelNumber].PCM_ADDS_H = (int) (pitch >> 16);

    _Channel[channelNumber].PCM_ADD_H = (int) ((((int64_t) (_Channel[channelNumber].PCM_ADDS_H) << 16) + _Channel[channelNumber].PCM_ADDS_L) * 2 * _Channel[channelNumber].PCM_SORC_F / DIST_F);
    _Channel[channelNumber].PCM_ADD_L = _Channel[channelNumber].PCM_ADD_H & 0xffff;
    _Channel[channelNumber].PCM_ADD_H = _Channel[channelNumber].PCM_ADD_H >> 16;

    return true;
}

// 0EH Set loop pointer
bool PPZ8::SetLoop(int ch, uint32_t loop_start, uint32_t loop_end)
{
    if (ch >= PCM_CNL_MAX) return false;

    if (loop_start != 0xffff && loop_end > loop_start)
    {
        // ループ設定
        // PCM_LPS_02:
        _Channel[ch].PCM_LOOP_FLG = 1;
        _Channel[ch].PCM_LOOP_START = loop_start;
        _Channel[ch].PCM_LOOP_END = loop_end;
    }
    else
    {
        // ループ解除
        // PCM_LPS_01:

        _Channel[ch].PCM_LOOP_FLG = 0;
        _Channel[ch].PCM_END = _Channel[ch].PCM_END_S;
    }
    return true;
}

//  12H (PPZ8)全停止
void PPZ8::AllStop(void)
{
    int    i;

    // とりあえず各パート停止で対応
    for (i = 0; i < PCM_CNL_MAX; i++)
    {
        Stop(i);
    }
}

//  13H (PPZ8)PAN指定
bool PPZ8::SetPan(int ch, int pan)
{
    if (ch >= PCM_CNL_MAX) return false;

    if (ch != 7 || !_EmulateADPCM)
    {
        _Channel[ch].PCM_PAN = pan;
    }
    else
    {
        _Channel[ch].PCM_PAN = ADPCM_EM_PAN[pan & 3];
    }
    return true;
}

//  14H (PPZ8)ﾚｰﾄ設定
bool PPZ8::SetRate(uint32_t rate, bool ip)
{
    DIST_F = (int) rate;
    _UseInterpolation = ip;

    return true;
}

//  15H (PPZ8)元ﾃﾞｰﾀ周波数設定
bool PPZ8::SetSourceRate(int ch, int rate)
{
    if (ch >= PCM_CNL_MAX) return false;

    _Channel[ch].PCM_SORC_F = rate;
    return true;
}

//  16H (PPZ8)全体ﾎﾞﾘﾕｰﾑの設定（86B Mixer)
void PPZ8::SetAllVolume(int vol)
{
    if (vol < 16 && vol != PCM_VOLUME)
    {
        PCM_VOLUME = vol;
        CreateVolumeTable(_Volume);
    }
}

void PPZ8::SetVolume(int volume)
{
    if (volume != _Volume)
        CreateVolumeTable(volume);
}

//  ADPCM Emulation settings
void PPZ8::ADPCM_EM_SET(bool flag)
{
    _EmulateADPCM = flag;
}

/// <summary>
/// Reads the PZI header.
/// </summary>
void PPZ8::ReadHeader(File * file, PZIHEADER & ph)
{
    uint8_t Data[2336];

    if (file->Read(Data, sizeof(Data)) != sizeof(Data))
        return;

    ::memcpy(ph.ID, Data, sizeof(ph.ID));
    ::memcpy(ph.Dummy1, &Data[0x04], sizeof(ph.Dummy1));

    ph.Count = Data[0x0B];

    ::memcpy(ph.Dummy2, &Data[0x0C], sizeof(ph.Dummy2));

    for (int i = 0; i < 128; i++)
    {
        ph.PZIItem[i].Start      = (uint32_t) ((Data[0x20 + i * 18]) | (Data[0x21 + i * 18] << 8) | (Data[0x22 + i * 18] << 16) | (Data[0x23 + i * 18] << 24));
        ph.PZIItem[i].Size       = (uint32_t) ((Data[0x24 + i * 18]) | (Data[0x25 + i * 18] << 8) | (Data[0x26 + i * 18] << 16) | (Data[0x27 + i * 18] << 24));
        ph.PZIItem[i].LoopStart  = (uint32_t) ((Data[0x28 + i * 18]) | (Data[0x29 + i * 18] << 8) | (Data[0x2a + i * 18] << 16) | (Data[0x2b + i * 18] << 24));
        ph.PZIItem[i].LoopEnd    = (uint32_t) ((Data[0x2c + i * 18]) | (Data[0x2d + i * 18] << 8) | (Data[0x2e + i * 18] << 16) | (Data[0x2f + i * 18] << 24));
        ph.PZIItem[i].SampleRate = (uint16_t) ((Data[0x30 + i * 18]) | (Data[0x31 + i * 18] << 8));
    }
}

/// <summary>
/// Reads the PVI header.
/// </summary>
void PPZ8::ReadHeader(File * file, PVIHEADER & ph)
{
    uint8_t Data[528];

    file->Read(Data, sizeof(Data));

    ::memcpy(ph.ID, Data, 4);
    ::memcpy(ph.Dummy1, &Data[0x04], (size_t) 0x0b - 4);

    ph.Count = Data[0x0b];

    ::memcpy(ph.Dummy2, &Data[0x0c], (size_t) 0x10 - 0x0b - 1);

    for (int i = 0; i < 128; i++)
    {
        ph.PVIItem[i].Start = (uint16_t) ((Data[0x20 + i * 18]) | (Data[0x21 + i * 18] << 8) | (Data[0x22 + i * 18] << 16) | (Data[0x23 + i * 18] << 24));
//FIXME: Why is startaddress overwritten here?
        ph.PVIItem[i].Start = (uint16_t) ((Data[0x10 + i * 4]) | (Data[0x11 + i * 4] << 8));
        ph.PVIItem[i].End   = (uint16_t) ((Data[0x12 + i * 4]) | (Data[0x13 + i * 4] << 8));
    }
}

void PPZ8::CreateVolumeTable(int volume)
{
    _Volume = volume;

    int AVolume = (int) (0x1000 * ::pow(10.0, volume / 40.0));

    for (int i = 0; i < 16; i++)
    {
        double Value = ::pow(2.0, ((double) (i) + PCM_VOLUME) / 2.0) * AVolume / 0x18000;

        for (int j = 0; j < 256; j++)
            _VolumeTable[i][j] = (Sample) ((j - 128) * Value);
    }
}

void PPZ8::Reset()
{
    ::memset(_Channel, 0, sizeof(_Channel));

    for (size_t i = 0; i < _countof(_Channel); ++i)
    {
        _Channel[i].PCM_ADD_H = 1;
        _Channel[i].PCM_ADD_L = 0;
        _Channel[i].PCM_ADDS_H = 1;
        _Channel[i].PCM_ADDS_L = 0;
        _Channel[i].PCM_SORC_F = 16000; // Original playback rate
        _Channel[i].PCM_PAN = 5;        // Pan Center
        _Channel[i].PCM_VOL = 8;        // Default volume
    }

    // MOV  PCME_WORK0 + PVI_NUM_MAX, 0  ; @ PVIのMAXを０にする
}

//  合成、出力
void PPZ8::Mix(Sample * sampleData, size_t sampleCount) noexcept
{
    Sample * di;
    Sample  bx;

    for (int i = 0; i < PCM_CNL_MAX; i++)
    {
        if (PCM_VOLUME == 0)
            break;

        if (_Channel[i].PCM_FLG == 0)
            continue;

        if (_Channel[i].PCM_VOL == 0)
        {
            // PCM_NOW ポインタの移動(ループ、end 等も考慮して)
            _Channel[i].PCM_NOW_XOR += (int) (_Channel[i].PCM_ADD_L * sampleCount);
            _Channel[i].PCM_NOW     += _Channel[i].PCM_ADD_H * sampleCount + _Channel[i].PCM_NOW_XOR / 0x10000;
            _Channel[i].PCM_NOW_XOR %= 0x10000;

            while (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
            {
                // @一次補間のときもちゃんと動くように実装        ^^
                if (_Channel[i].PCM_LOOP_FLG)
                {
                    // ループする場合
                    _Channel[i].PCM_NOW -= (_Channel[i].PCM_END - 1 - _Channel[i].PCM_LOOP);
                }
                else
                {
                    _Channel[i].PCM_FLG = 0;
                    break;
                }
            }
            continue;
        }

        if (_Channel[i].PCM_PAN == 0)
        {
            _Channel[i].PCM_FLG = 0;
            continue;
        }

        if (_UseInterpolation)
        {
            di = sampleData;

            switch (_Channel[i].PCM_PAN)
            {
                case 1:  //  1 , 0
                    while (di < &sampleData[sampleCount * 2])
                    {
                        *di++ += (_VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW] * (0x10000 - _Channel[i].PCM_NOW_XOR)
                            + _VolumeTable[_Channel[i].PCM_VOL][*(_Channel[i].PCM_NOW + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;
                        di++;    // 左のみ

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^

                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW
                                    = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 2:  //  1 ,1/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (_VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW] * (0x10000 - _Channel[i].PCM_NOW_XOR)
                            + _VolumeTable[_Channel[i].PCM_VOL][*(_Channel[i].PCM_NOW + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx / 4;

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^

                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW
                                    = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 3:  //  1 ,2/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (_VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW] * (0x10000 - _Channel[i].PCM_NOW_XOR)
                            + _VolumeTable[_Channel[i].PCM_VOL][*(_Channel[i].PCM_NOW + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx / 2;

                        _Channel[i].PCM_NOW     += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^

                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 4:  //  1 ,3/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (_VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW] * (0x10000 - _Channel[i].PCM_NOW_XOR)
                            + _VolumeTable[_Channel[i].PCM_VOL][*(_Channel[i].PCM_NOW + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx * 3 / 4;

                        _Channel[i].PCM_NOW     += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 5:  //  1 , 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (_VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW] * (0x10000 - _Channel[i].PCM_NOW_XOR)
                            + _VolumeTable[_Channel[i].PCM_VOL][*(_Channel[i].PCM_NOW + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx;

                        _Channel[i].PCM_NOW     += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 6:  // 3/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (_VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW] * (0x10000 - _Channel[i].PCM_NOW_XOR)
                            + _VolumeTable[_Channel[i].PCM_VOL][*(_Channel[i].PCM_NOW + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx * 3 / 4;
                        *di++ += bx;

                        _Channel[i].PCM_NOW     += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 7:  // 2/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (_VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW] * (0x10000 - _Channel[i].PCM_NOW_XOR)
                            + _VolumeTable[_Channel[i].PCM_VOL][*(_Channel[i].PCM_NOW + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx / 2;
                        *di++ += bx;

                        _Channel[i].PCM_NOW     += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 8:  // 1/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (_VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW] * (0x10000 - _Channel[i].PCM_NOW_XOR)
                            + _VolumeTable[_Channel[i].PCM_VOL][*(_Channel[i].PCM_NOW + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx / 4;
                        *di++ += bx;

                        _Channel[i].PCM_NOW     += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 9:  //  0 , 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        di++;    // 右のみ
                        *di++ += (_VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW] * (0x10000 - _Channel[i].PCM_NOW_XOR)
                            + _VolumeTable[_Channel[i].PCM_VOL][*(_Channel[i].PCM_NOW + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        _Channel[i].PCM_NOW     += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^

                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;
            }
        }
        else
        {
            di = sampleData;

            switch (_Channel[i].PCM_PAN)
            {
                case 1:  //  1 , 0
                    while (di < &sampleData[sampleCount * 2])
                    {
                        *di++ += _VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW];
                        di++;    // 左のみ

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END)
                        {
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 2:  //  1 ,1/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = _VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW];
                        *di++ += bx;
                        *di++ += bx / 4;

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END)
                        {
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 3:  //  1 ,2/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = _VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW];
                        *di++ += bx;
                        *di++ += bx / 2;

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END)
                        {
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 4:  //  1 ,3/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = _VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW];
                        *di++ += bx;
                        *di++ += bx * 3 / 4;

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END)
                        {
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 5:  //  1 , 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = _VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW];
                        *di++ += bx;
                        *di++ += bx;

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END)
                        {
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 6:  // 3/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = _VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW];
                        *di++ += bx * 3 / 4;
                        *di++ += bx;

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END)
                        {
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 7:  // 2/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = _VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW];
                        *di++ += bx / 2;
                        *di++ += bx;

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END)
                        {
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 8:  // 1/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = _VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW];
                        *di++ += bx / 4;
                        *di++ += bx;

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;
                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END)
                        {
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 9:  //  0 , 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        di++;      // 右のみ
                        *di++ += _VolumeTable[_Channel[i].PCM_VOL][*_Channel[i].PCM_NOW];

                        _Channel[i].PCM_NOW += _Channel[i].PCM_ADD_H;
                        _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;
                        if (_Channel[i].PCM_NOW_XOR > 0xffff)
                        {
                            _Channel[i].PCM_NOW_XOR -= 0x10000;
                            _Channel[i].PCM_NOW++;
                        }

                        if (_Channel[i].PCM_NOW >= _Channel[i].PCM_END)
                        {
                            if (_Channel[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                _Channel[i].PCM_NOW = _Channel[i].PCM_LOOP;
                            }
                            else
                            {
                                _Channel[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;
            }
        }
    }
}
