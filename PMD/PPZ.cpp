
// 8 Channel PCM Driver「PPZ8」Unit (Light Version) / Programmed by UKKY / Windows Converted by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>

#include <stdlib.h>
#include <math.h>

#include "PPZ.h"

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

    pviflag[0] = false;
    pviflag[1] = false;

    ::memset(PVI_FILE, 0, sizeof(PVI_FILE));

    ADPCM_EM_FLG = false;
    interpolation = false;

    WORK_INIT();

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
    volume = 0;
    SetAllVolume(VNUM_DEF);    // 全体ボリューム(DEF=12)

    DIST_F = RATE_DEF;       // 再生周波数
}

//  01H PCM 発音
bool PPZ8::Play(int ch, int bufnum, int num, uint16_t start, uint16_t stop)
{
    if (ch >= PCM_CNL_MAX) return false;
    if (XMS_FRAME_ADR[bufnum] == NULL || XMS_FRAME_SIZE[bufnum] == 0) return false;

    // PVIの定義数より大きいとスキップ
    //if(num >= PCME_WORK[bufnum].pzinum) return false;

    channelwork[ch].pviflag = pviflag[bufnum];
    channelwork[ch].PCM_FLG = 1;    // 再生開始
    channelwork[ch].PCM_NOW_XOR = 0;  // 小数点部
    channelwork[ch].PCM_NUM = num;

    // ADPCM エミュレート処理
    if (ch == 7 && ADPCM_EM_FLG && (ch & 0x80) == 0)
    {
        channelwork[ch].PCM_NOW   = &XMS_FRAME_ADR[bufnum][Limit(((int) start) * 64, XMS_FRAME_SIZE[bufnum] - 1, 0)];
        channelwork[ch].PCM_END_S = &XMS_FRAME_ADR[bufnum][Limit(((int) stop - 1) * 64, XMS_FRAME_SIZE[bufnum] - 1, 0)];
        channelwork[ch].PCM_END   = channelwork[ch].PCM_END_S;
    }
    else
    {
        channelwork[ch].PCM_NOW   = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].pcmnum[num].Start];
        channelwork[ch].PCM_END_S = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].pcmnum[num].Start + PCME_WORK[bufnum].pcmnum[num].Size];

        if (channelwork[ch].PCM_LOOP_FLG == 0)
        {
            // ループなし
            channelwork[ch].PCM_END = channelwork[ch].PCM_END_S;
        }
        else
        {
            // ループあり
            if (channelwork[ch].PCM_LOOP_START >= PCME_WORK[bufnum].pcmnum[num].Size)
                channelwork[ch].PCM_LOOP = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].pcmnum[num].Start + PCME_WORK[bufnum].pcmnum[num].Size - 1];
            else
                channelwork[ch].PCM_LOOP = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].pcmnum[num].Start + channelwork[ch].PCM_LOOP_START];

            if (channelwork[ch].PCM_LOOP_END >= PCME_WORK[bufnum].pcmnum[num].Size)
                channelwork[ch].PCM_END = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].pcmnum[num].Start + PCME_WORK[bufnum].pcmnum[num].Size];
            else
                channelwork[ch].PCM_END = &XMS_FRAME_ADR[bufnum][PCME_WORK[bufnum].pcmnum[num].Start + channelwork[ch].PCM_LOOP_END];
        }
    }

    return true;
}

//  02H PCM 停止
bool PPZ8::Stop(int ch)
{
    if (ch >= PCM_CNL_MAX) return false;

    channelwork[ch].PCM_FLG = 0;  // 再生停止
    return true;
}

/// <summary>
/// Reads the PZI header.
/// </summary>
void PPZ8::ReadHeader(File * file, PZIHEADER & ph)
{
    uint8_t Data[2336];

    if (file->Read(Data, sizeof(Data)) != sizeof(Data))
        return;

    ::memcpy(ph.ID,     &Data[0x00], sizeof(ph.ID));
    ::memcpy(ph.Dummy1, &Data[0x04], sizeof(ph.Dummy1));
    ph.Count = Data[0x0B];
    ::memcpy(ph.Dummy2, &Data[0x0C], sizeof(ph.Dummy2));

    for (int i = 0; i < 128; i++)
    {
        ph.pcmnum[i].Start      = (uint32_t) ((Data[0x20 + i * 18]) | (Data[0x21 + i * 18] << 8) | (Data[0x22 + i * 18] << 16) | (Data[0x23 + i * 18] << 24));
        ph.pcmnum[i].Size       = (uint32_t) ((Data[0x24 + i * 18]) | (Data[0x25 + i * 18] << 8) | (Data[0x26 + i * 18] << 16) | (Data[0x27 + i * 18] << 24));
        ph.pcmnum[i].LoopStart  = (uint32_t) ((Data[0x28 + i * 18]) | (Data[0x29 + i * 18] << 8) | (Data[0x2a + i * 18] << 16) | (Data[0x2b + i * 18] << 24));
        ph.pcmnum[i].LoopEnd    = (uint32_t) ((Data[0x2c + i * 18]) | (Data[0x2d + i * 18] << 8) | (Data[0x2e + i * 18] << 16) | (Data[0x2f + i * 18] << 24));
        ph.pcmnum[i].SampleRate = (uint16_t) ((Data[0x30 + i * 18]) | (Data[0x31 + i * 18] << 8));
    }
}

/// <summary>
/// Reads the PVI header.
/// </summary>
void PPZ8::ReadHeader(File * file, PVIHEADER & pviheader)
{
    uint8_t Data[528];

    file->Read(Data, sizeof(Data));

    ::memcpy(pviheader.ID, &Data[0x00], 4);
    ::memcpy(pviheader.dummy1, &Data[0x04], 0x0b - 4);

    pviheader.pvinum = Data[0x0b];

    ::memcpy(pviheader.dummy2, &Data[0x0c], 0x10 - 0x0b - 1);

    for (int i = 0; i < 128; i++)
    {
        pviheader.pcmnum[i].startaddress = (uint16_t) ((Data[0x20 + i * 18]) | (Data[0x21 + i * 18] << 8) | (Data[0x22 + i * 18] << 16) | (Data[0x23 + i * 18] << 24));
//FIXME: Why is startaddress overwritten here?
        pviheader.pcmnum[i].startaddress = (uint16_t) ((Data[0x10 + i * 4]) | (Data[0x11 + i * 4] << 8));
        pviheader.pcmnum[i].endaddress   = (uint16_t) ((Data[0x12 + i * 4]) | (Data[0x13 + i * 4] << 8));
    }
}

// 03H Read PVI/PZI file
int PPZ8::Load(TCHAR * filePath, int bufnum)
{
    if (filePath == nullptr || (filePath && (*filePath == '\0')))
        return _ERR_OPEN_PPZ_FILE;

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

    WORK_INIT();

    PVI_FILE[bufnum][0] = '\0';

    if (!_File->Open(filePath))
    {
        if (XMS_FRAME_ADR[bufnum] != NULL)
        {
            free(XMS_FRAME_ADR[bufnum]);    // 開放
            XMS_FRAME_ADR[bufnum] = NULL;
            XMS_FRAME_SIZE[bufnum] = 0;
            memset(&PCME_WORK[bufnum], 0, sizeof(PZIHEADER));
        }
        return _ERR_OPEN_PPZ_FILE;        //  ファイルが開けない
    }

    int Size = (int) _File->GetFileSize(filePath);  // ファイルサイズ

    PZIHEADER PZIHeader;

    if (NOW_PCM_CATE)
    {
        ReadHeader(_File, PZIHeader);

        if (::strncmp(PZIHeader.ID, "PZI", 3) != 0)
        {
            _File->Close();

            return ERR_PPZ_UNKNOWN_FORMAT;
        }

        if (::memcmp(&PCME_WORK[bufnum], &PZIHeader, sizeof(PZIHEADER)) == 0)
        {
            ::wcscpy(PVI_FILE[bufnum], filePath);

            _File->Close();

            return ERR_PPZ_ALREADY_LOADED;
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

            return PPS_OUT_OF_MEMORY;
        }

        ::memset(Data, 0, (size_t) Size);

        _File->Read(Data, (uint32_t) Size);

        XMS_FRAME_ADR[bufnum] = Data;
        XMS_FRAME_SIZE[bufnum] = Size;

        ::wcscpy(PVI_FILE[bufnum], filePath);
        pviflag[bufnum] = false;
    }
    else
    {
        PVIHEADER PVIHeader;

        ReadHeader(_File, PVIHeader);

        if (::strncmp(PVIHeader.ID, "PVI", 3))
        {
            _File->Close();

            return ERR_PPZ_UNKNOWN_FORMAT;
        }

        ::strncpy(PZIHeader.ID, "PZI1", 4);

        int PVISize = 0;

        for (int i = 0; i < PVIHeader.pvinum; ++i)
        {
            PZIHeader.pcmnum[i].Start      = (uint32_t) ((                                 PVIHeader.pcmnum[i].startaddress      << (5 + 1)));
            PZIHeader.pcmnum[i].Size       = (uint32_t) ((PVIHeader.pcmnum[i].endaddress - PVIHeader.pcmnum[i].startaddress + 1) << (5 + 1));
            PZIHeader.pcmnum[i].LoopStart  = 0xFFFF;
            PZIHeader.pcmnum[i].LoopEnd    = 0xFFFF;
            PZIHeader.pcmnum[i].SampleRate = 16000; // 16kHz

            PVISize += PZIHeader.pcmnum[i].Size;
        }

        for (int i = PVIHeader.pvinum; i < 128; ++i)
        {
            PZIHeader.pcmnum[i].Start      = 0;
            PZIHeader.pcmnum[i].Size       = 0;
            PZIHeader.pcmnum[i].LoopStart  = 0xFFFF;
            PZIHeader.pcmnum[i].LoopEnd    = 0xFFFF;
            PZIHeader.pcmnum[i].SampleRate = 0;
        }

        if (::memcmp(&PCME_WORK[bufnum].pcmnum, &PZIHeader.pcmnum, sizeof(PZIHEADER) - 0x20) == 0)
        {
            ::wcscpy(PVI_FILE[bufnum], filePath);
            _File->Close();

            return ERR_PPZ_ALREADY_LOADED;
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
            return PPS_OUT_OF_MEMORY;
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
                return PPS_OUT_OF_MEMORY;
            }

            ::memset(psrc, 0, SrcSize);

            _File->Read(psrc, (uint32_t) Size);

            uint8_t * psrc2 = psrc;

            // ADPCM > PCM に変換
            for (int i = 0; i < PVIHeader.pvinum; ++i)
            {
                int X_N     = X_N0;     // Xn (ADPCM>PCM 変換用)
                int DELTA_N = DELTA_N0; // DELTA_N(ADPCM>PCM 変換用)

                for (int j = 0; j < (int) PZIHeader.pcmnum[i].Size / 2; ++j)
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

        ::wcscpy(PVI_FILE[bufnum], filePath);

        pviflag[bufnum] = true;
    }

    _File->Close();

    return _PPZ8_OK;
}

// 07H Volume
bool PPZ8::SetVolume(int ch, int vol)
{
    if (ch >= PCM_CNL_MAX)
        return false;

    if (ch != 7 || !ADPCM_EM_FLG)
        channelwork[ch].PCM_VOL = vol;
    else
        channelwork[ch].PCM_VOL = ADPCM_EM_VOL[vol & 0xff];

    return true;
}

// 0BH Pitch Frequency
bool PPZ8::SetPitchFrequency(int ch, uint32_t frequency)
{
    if (ch >= PCM_CNL_MAX)
        return false;

    if (ch == 7 && ADPCM_EM_FLG)
    {            // ADPCM エミュレート中
        frequency = (frequency & 0xffff) * 0x8000 / 0x49ba;
    }

    channelwork[ch].PCM_ADDS_L = (int) (frequency & 0xffff);
    channelwork[ch].PCM_ADDS_H = (int) (frequency >> 16);

    channelwork[ch].PCM_ADD_H = (int) ((((int64_t) (channelwork[ch].PCM_ADDS_H) << 16) + channelwork[ch].PCM_ADDS_L) * 2 * channelwork[ch].PCM_SORC_F / DIST_F);
    channelwork[ch].PCM_ADD_L = channelwork[ch].PCM_ADD_H & 0xffff;
    channelwork[ch].PCM_ADD_H = channelwork[ch].PCM_ADD_H >> 16;

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
        channelwork[ch].PCM_LOOP_FLG = 1;
        channelwork[ch].PCM_LOOP_START = loop_start;
        channelwork[ch].PCM_LOOP_END = loop_end;
    }
    else
    {
        // ループ解除
        // PCM_LPS_01:

        channelwork[ch].PCM_LOOP_FLG = 0;
        channelwork[ch].PCM_END = channelwork[ch].PCM_END_S;
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

    if (ch != 7 || !ADPCM_EM_FLG)
    {
        channelwork[ch].PCM_PAN = pan;
    }
    else
    {
        channelwork[ch].PCM_PAN = ADPCM_EM_PAN[pan & 3];
    }
    return true;
}

//  14H (PPZ8)ﾚｰﾄ設定
bool PPZ8::SetRate(uint32_t rate, bool ip)
{
    DIST_F = (int) rate;
    interpolation = ip;

    return true;
}

//  15H (PPZ8)元ﾃﾞｰﾀ周波数設定
bool PPZ8::SetSourceRate(int ch, int rate)
{
    if (ch >= PCM_CNL_MAX) return false;

    channelwork[ch].PCM_SORC_F = rate;
    return true;
}

//  16H (PPZ8)全体ﾎﾞﾘﾕｰﾑの設定（86B Mixer)
void PPZ8::SetAllVolume(int vol)
{
    if (vol < 16 && vol != PCM_VOLUME)
    {
        PCM_VOLUME = vol;
        MakeVolumeTable(volume);
    }
}

//  音量調整用
void PPZ8::SetVolume(int vol)
{
    if (vol != volume)
    {
        MakeVolumeTable(vol);
    }
}

//  ADPCM エミュレート設定
void PPZ8::ADPCM_EM_SET(bool flag)
{
    ADPCM_EM_FLG = flag;
}

//  音量テーブル作成
void PPZ8::MakeVolumeTable(int vol)
{
    int    i, j;
    double  temp;

    volume = vol;
    AVolume = (int) (0x1000 * pow(10.0, vol / 40.0));

    for (i = 0; i < 16; i++)
    {
        temp = pow(2.0, ((double) (i) +PCM_VOLUME) / 2.0) * AVolume / 0x18000;
        for (j = 0; j < 256; j++)
        {
            VolumeTable[i][j] = (Sample) ((j - 128) * temp);
        }
    }
}

//  ﾜｰｸ初期化
void PPZ8::WORK_INIT(void)
{
    int    i;

    memset(channelwork, 0, sizeof(channelwork));

    for (i = 0; i < PCM_CNL_MAX; i++)
    {
        channelwork[i].PCM_ADD_H = 1;
        channelwork[i].PCM_ADD_L = 0;
        channelwork[i].PCM_ADDS_H = 1;
        channelwork[i].PCM_ADDS_L = 0;
        channelwork[i].PCM_SORC_F = 16000;    // 元データの再生レート
        channelwork[i].PCM_PAN = 5;      // PAN中心
        channelwork[i].PCM_VOL = 8;      // ボリュームデフォルト
    }

    // MOV  PCME_WORK0+PVI_NUM_MAX,0  ;@ PVIのMAXを０にする
}

//  合成、出力
void PPZ8::Mix(Sample * sampleData, int sampleCount)
{
    Sample * di;
    Sample  bx;

    for (int i = 0; i < PCM_CNL_MAX; i++)
    {
        if (PCM_VOLUME == 0)
            break;

        if (channelwork[i].PCM_FLG == 0)
            continue;

        if (channelwork[i].PCM_VOL == 0)
        {
            // PCM_NOW ポインタの移動(ループ、end 等も考慮して)
            channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L * sampleCount;
            channelwork[i].PCM_NOW     += channelwork[i].PCM_ADD_H * sampleCount + channelwork[i].PCM_NOW_XOR / 0x10000;
            channelwork[i].PCM_NOW_XOR %= 0x10000;

            while (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
            {
                // @一次補間のときもちゃんと動くように実装        ^^
                if (channelwork[i].PCM_LOOP_FLG)
                {
                    // ループする場合
                    channelwork[i].PCM_NOW -= (channelwork[i].PCM_END - 1 - channelwork[i].PCM_LOOP);
                }
                else
                {
                    channelwork[i].PCM_FLG = 0;
                    break;
                }
            }
            continue;
        }

        if (channelwork[i].PCM_PAN == 0)
        {
            channelwork[i].PCM_FLG = 0;
            continue;
        }

        if (interpolation)
        {
            di = sampleData;

            switch (channelwork[i].PCM_PAN)
            {
                case 1:  //  1 , 0
                    while (di < &sampleData[sampleCount * 2])
                    {
                        *di++ += (VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW] * (0x10000 - channelwork[i].PCM_NOW_XOR)
                            + VolumeTable[channelwork[i].PCM_VOL][*(channelwork[i].PCM_NOW + 1)] * channelwork[i].PCM_NOW_XOR) >> 16;
                        di++;    // 左のみ

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;
                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^

                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW
                                    = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 2:  //  1 ,1/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW] * (0x10000 - channelwork[i].PCM_NOW_XOR)
                            + VolumeTable[channelwork[i].PCM_VOL][*(channelwork[i].PCM_NOW + 1)] * channelwork[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx / 4;

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;
                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^

                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW
                                    = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 3:  //  1 ,2/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW] * (0x10000 - channelwork[i].PCM_NOW_XOR)
                            + VolumeTable[channelwork[i].PCM_VOL][*(channelwork[i].PCM_NOW + 1)] * channelwork[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx / 2;

                        channelwork[i].PCM_NOW     += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^

                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 4:  //  1 ,3/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW] * (0x10000 - channelwork[i].PCM_NOW_XOR)
                            + VolumeTable[channelwork[i].PCM_VOL][*(channelwork[i].PCM_NOW + 1)] * channelwork[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx * 3 / 4;

                        channelwork[i].PCM_NOW     += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 5:  //  1 , 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW] * (0x10000 - channelwork[i].PCM_NOW_XOR)
                            + VolumeTable[channelwork[i].PCM_VOL][*(channelwork[i].PCM_NOW + 1)] * channelwork[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx;

                        channelwork[i].PCM_NOW     += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 6:  // 3/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW] * (0x10000 - channelwork[i].PCM_NOW_XOR)
                            + VolumeTable[channelwork[i].PCM_VOL][*(channelwork[i].PCM_NOW + 1)] * channelwork[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx * 3 / 4;
                        *di++ += bx;

                        channelwork[i].PCM_NOW     += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 7:  // 2/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW] * (0x10000 - channelwork[i].PCM_NOW_XOR)
                            + VolumeTable[channelwork[i].PCM_VOL][*(channelwork[i].PCM_NOW + 1)] * channelwork[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx / 2;
                        *di++ += bx;

                        channelwork[i].PCM_NOW     += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 8:  // 1/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = (VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW] * (0x10000 - channelwork[i].PCM_NOW_XOR)
                            + VolumeTable[channelwork[i].PCM_VOL][*(channelwork[i].PCM_NOW + 1)] * channelwork[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx / 4;
                        *di++ += bx;

                        channelwork[i].PCM_NOW     += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 9:  //  0 , 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        di++;    // 右のみ
                        *di++ += (VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW] * (0x10000 - channelwork[i].PCM_NOW_XOR)
                            + VolumeTable[channelwork[i].PCM_VOL][*(channelwork[i].PCM_NOW + 1)] * channelwork[i].PCM_NOW_XOR) >> 16;

                        channelwork[i].PCM_NOW     += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END - 1)
                        {
                            // @一次補間のときもちゃんと動くように実装   ^^

                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
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

            switch (channelwork[i].PCM_PAN)
            {
                case 1:  //  1 , 0
                    while (di < &sampleData[sampleCount * 2])
                    {
                        *di++ += VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW];
                        di++;    // 左のみ

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END)
                        {
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 2:  //  1 ,1/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW];
                        *di++ += bx;
                        *di++ += bx / 4;

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END)
                        {
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 3:  //  1 ,2/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW];
                        *di++ += bx;
                        *di++ += bx / 2;

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END)
                        {
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 4:  //  1 ,3/4
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW];
                        *di++ += bx;
                        *di++ += bx * 3 / 4;

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END)
                        {
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 5:  //  1 , 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW];
                        *di++ += bx;
                        *di++ += bx;

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END)
                        {
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 6:  // 3/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW];
                        *di++ += bx * 3 / 4;
                        *di++ += bx;

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END)
                        {
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 7:  // 2/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW];
                        *di++ += bx / 2;
                        *di++ += bx;

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;

                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END)
                        {
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 8:  // 1/4, 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        bx = VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW];
                        *di++ += bx / 4;
                        *di++ += bx;

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;
                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END)
                        {
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;

                case 9:  //  0 , 1
                    while (di < &sampleData[sampleCount * 2])
                    {
                        di++;      // 右のみ
                        *di++ += VolumeTable[channelwork[i].PCM_VOL][*channelwork[i].PCM_NOW];

                        channelwork[i].PCM_NOW += channelwork[i].PCM_ADD_H;
                        channelwork[i].PCM_NOW_XOR += channelwork[i].PCM_ADD_L;
                        if (channelwork[i].PCM_NOW_XOR > 0xffff)
                        {
                            channelwork[i].PCM_NOW_XOR -= 0x10000;
                            channelwork[i].PCM_NOW++;
                        }

                        if (channelwork[i].PCM_NOW >= channelwork[i].PCM_END)
                        {
                            if (channelwork[i].PCM_LOOP_FLG)
                            {
                                // ループする場合
                                channelwork[i].PCM_NOW = channelwork[i].PCM_LOOP;
                            }
                            else
                            {
                                channelwork[i].PCM_FLG = 0;
                                break;
                            }
                        }
                    }
                    break;
            }
        }
    }
}
