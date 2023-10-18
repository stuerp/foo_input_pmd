
/** $VER: PPZ.cpp (2023.10.18) PC-98's 86 soundboard's 8 PCM driver (Programmed by UKKY / Based on Windows conversion by C60) **/

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

PPZDriver::PPZDriver(File * file) : _File(file)
{
    InitializeInternal();
}

PPZDriver::~PPZDriver()
{
}

//  00H Initialize
bool PPZDriver::Initialize(uint32_t outputFrequency, bool useInterpolation)
{
    InitializeInternal();

    return SetOutputFrequency(outputFrequency, useInterpolation);
}

// 01H Start PCM
bool PPZDriver::Play(int ch, int bankNumber, int sampleNumber, uint16_t start, uint16_t stop)
{
    PPZBank& pb = _PPZBank[bankNumber];

    if ((ch >= _countof(_Channel)) || pb.IsEmpty())
        return false;

    _Channel[ch].IsPVI = pb._IsPVI;
    _Channel[ch].IsPlaying = true;
    _Channel[ch].SampleNumber = sampleNumber;
    _Channel[ch].PCM_NOW_XOR = 0; // Decimal part

    if ((ch == 7) && _EmulateADPCM && (ch & 0x80) == 0)
    {
        _Channel[ch].PCMStart      = &pb._Data[Limit(((int) start)    * 64, pb._Size - 1, 0)];
        _Channel[ch].PCMEndRounded = &pb._Data[Limit(((int) stop - 1) * 64, pb._Size - 1, 0)];
        _Channel[ch].PCMEnd        = _Channel[ch].PCMEndRounded;
    }
    else
    {
        PZIITEM& pi = pb._PZIHeader.PZIItem[sampleNumber];

        _Channel[ch].PCMStart      = &pb._Data[pi.Start];
        _Channel[ch].PCMEndRounded = &pb._Data[pi.Start + pi.Size];

        if (_Channel[ch].HasLoop)
        {
            if (_Channel[ch].PCMLoopStart < pi.Size)
                _Channel[ch].PCM_LOOP = &pb._Data[pi.Start + _Channel[ch].PCMLoopStart];
            else
                _Channel[ch].PCM_LOOP = &pb._Data[pi.Start + pi.Size - 1];

            if (_Channel[ch].PCMLoopEnd < pi.Size)
                _Channel[ch].PCMEnd = &pb._Data[pi.Start + _Channel[ch].PCMLoopEnd];
            else
                _Channel[ch].PCMEnd = &pb._Data[pi.Start + pi.Size];
        }
        else
            _Channel[ch].PCMEnd = _Channel[ch].PCMEndRounded;
    }

    return true;
}

// 02H Stop PCM
bool PPZDriver::Stop(int ch)
{
    if (ch >= MaxPPZChannels)
        return false;

    _Channel[ch].IsPlaying = false;

    return true;
}

// 03H Read PVI/PZI file
int PPZDriver::Load(const WCHAR * filePath, size_t bankNumber)
{
    if (filePath == nullptr || (filePath && (*filePath == '\0')))
        return PPZ_OPEN_FAILED;

    Reset();

    if (!_File->Open(filePath))
    {
        _PPZBank[bankNumber].Reset();

        return PPZ_OPEN_FAILED;
    }

    int Size = (int) _File->GetFileSize(filePath);

    PZIHEADER PZIHeader;

    if (HasExtension(filePath, MAX_PATH, L".PZI"))
    {
        ReadHeader(_File, PZIHeader);

        if (::strncmp(PZIHeader.ID, "PZI", 3) != 0)
        {
            _File->Close();

            return PPZ_UNKNOWN_FORMAT;
        }

        if (::memcmp(&_PPZBank[bankNumber]._PZIHeader, &PZIHeader, sizeof(PZIHEADER)) == 0)
        {
            _PPZBank[bankNumber]._FilePath = filePath;

            _File->Close();

            return PPZ_ALREADY_LOADED;
        }

        _PPZBank[bankNumber].Reset();

        ::memcpy(&_PPZBank[bankNumber]._PZIHeader, &PZIHeader, sizeof(PZIHEADER));

        Size -= sizeof(PZIHEADER);

        uint8_t * Data = (uint8_t *) ::malloc((size_t) Size);

        if (Data == nullptr)
        {
            _File->Close();

            return PPZ_OUT_OF_MEMORY;
        }

        ::memset(Data, 0, (size_t) Size);

        _File->Read(Data, (uint32_t) Size);

        _PPZBank[bankNumber]._FilePath = filePath;

        _PPZBank[bankNumber]._Data = Data;
        _PPZBank[bankNumber]._Size = Size;

        _PPZBank[bankNumber]._IsPVI = false;
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
            PZIHeader.PZIItem[i].Start      = (uint32_t) ((                           PVIHeader.PVIItem[i].Start      << (5 + 1)));
            PZIHeader.PZIItem[i].Size       = (uint32_t) ((PVIHeader.PVIItem[i].End - PVIHeader.PVIItem[i].Start + 1) << (5 + 1));
            PZIHeader.PZIItem[i].LoopStart  = ~0U;
            PZIHeader.PZIItem[i].LoopEnd    = ~0U;
            PZIHeader.PZIItem[i].SampleRate = 16000; // 16kHz

            PVISize += PZIHeader.PZIItem[i].Size;
        }

        for (int i = PVIHeader.Count; i < 128; ++i)
        {
            PZIHeader.PZIItem[i].Start      = 0;
            PZIHeader.PZIItem[i].Size       = 0;
            PZIHeader.PZIItem[i].LoopStart  = ~0U;
            PZIHeader.PZIItem[i].LoopEnd    = ~0U;
            PZIHeader.PZIItem[i].SampleRate = 0;
        }

        if (::memcmp(&_PPZBank[bankNumber]._PZIHeader.PZIItem, &PZIHeader.PZIItem, sizeof(PZIHEADER) - 0x20) == 0)
        {
            _PPZBank[bankNumber]._FilePath = filePath;

            _File->Close();

            return PPZ_ALREADY_LOADED;
        }

        _PPZBank[bankNumber].Reset();

        ::memcpy(&_PPZBank[bankNumber]._PZIHeader, &PZIHeader, sizeof(PZIHEADER));

        Size -= sizeof(PVIHEADER);
        PVISize /= 2;

        size_t DstSize = (size_t) (std::max)(Size, PVISize) * 2;

        uint8_t * Data = (uint8_t *) ::malloc(DstSize);

        if (Data == nullptr)
        {
            _File->Close();

            return PPZ_OUT_OF_MEMORY;
        }

        ::memset(Data, 0, DstSize);

        _PPZBank[bankNumber]._Data = Data;
        _PPZBank[bankNumber]._Size = PVISize * 2;

        {
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

                    *Data++ = (uint8_t) (X_N / (32768 / 128) + 128);

                    X_N     = Limit(X_N + table1[*psrc & 0x0f] * DELTA_N / 8, 32767, -32768);
                    DELTA_N = Limit(DELTA_N * table2[*psrc++ & 0x0f] / 64, 24576, 127);

                    *Data++ = (uint8_t) (X_N / (32768 / 128) + 128);
                }
            }

            ::free(psrc2);
        }

        _PPZBank[bankNumber]._FilePath = filePath;
        _PPZBank[bankNumber]._IsPVI = true;
    }

    _File->Close();

    return PPZ_SUCCESS;
}

// 07H Volume
bool PPZDriver::SetVolume(int channelNumber, int volume)
{
    if (channelNumber >= MaxPPZChannels)
        return false;

    if (channelNumber != 7 || !_EmulateADPCM)
        _Channel[channelNumber].Volume = volume;
    else
        _Channel[channelNumber].Volume = ADPCM_EM_VOL[volume & 0xff];

    return true;
}

// 0BH Pitch Frequency
bool PPZDriver::SetPitch(int channelNumber, uint32_t pitch)
{
    if (channelNumber >= _countof(_Channel))
        return false;

    if (channelNumber == 7 && _EmulateADPCM) // Emulating ADPCM?
        pitch = (pitch & 0xFFFF) * 0x8000 / 0x49ba;

    _Channel[channelNumber].PCM_ADDS_L = (int) (pitch & 0xFFFF);
    _Channel[channelNumber].PCM_ADDS_H = (int) (pitch >> 16);

    _Channel[channelNumber].PCM_ADD_H = (int) ((((int64_t) (_Channel[channelNumber].PCM_ADDS_H) << 16) + _Channel[channelNumber].PCM_ADDS_L) * 2 * _Channel[channelNumber].SourceFrequency / _OutputFrequency);

    _Channel[channelNumber].PCM_ADD_L = _Channel[channelNumber].PCM_ADD_H & 0xFFFF;
    _Channel[channelNumber].PCM_ADD_H = _Channel[channelNumber].PCM_ADD_H >> 16;

    return true;
}

// 0EH Set loop pointer
bool PPZDriver::SetLoop(size_t ch, size_t bankNumber, size_t sampleNumber)
{
    if ((ch >= MaxPPZChannels) || (bankNumber > 1) || (sampleNumber > 127))
        return false;

    const PZIITEM& pi = _PPZBank[bankNumber]._PZIHeader.PZIItem[sampleNumber];

    if ((pi.LoopStart != ~0U) && (pi.LoopStart < pi.LoopEnd))
    {
        _Channel[ch].HasLoop = true;
        _Channel[ch].PCMLoopStart = pi.LoopStart;
        _Channel[ch].PCMLoopEnd = pi.LoopEnd;
    }
    else
    {
        _Channel[ch].HasLoop = false;
        _Channel[ch].PCMEnd = _Channel[ch].PCMEndRounded;
    }

    return true;
}

bool PPZDriver::SetLoop(size_t ch, size_t bankNumber, size_t sampleNumber, int loopStart, int loopEnd)
{
    if ((ch >= MaxPPZChannels) || (bankNumber > 1) || (sampleNumber > 127))
        return false;

    const PZIITEM& pi = _PPZBank[bankNumber]._PZIHeader.PZIItem[sampleNumber];

    if (loopStart < 0)
        loopStart = (int) pi.Size + loopStart;

    if (loopEnd < 0)
        loopEnd = (int) pi.Size + loopEnd;

    if (loopStart < loopEnd)
    {
        _Channel[ch].HasLoop = true;
        _Channel[ch].PCMLoopStart = (uint32_t) loopStart;
        _Channel[ch].PCMLoopEnd = (uint32_t) loopEnd;
    }
    else
    {
        _Channel[ch].HasLoop = false;
        _Channel[ch].PCMEnd = _Channel[ch].PCMEndRounded;
    }

    return true;
}

// 12H Stop all channels.
void PPZDriver::AllStop()
{
    for (int i = 0; i < MaxPPZChannels; i++)
        Stop(i);
}

// 13H Set the pan value.
bool PPZDriver::SetPan(size_t ch, int value)
{
    static const int PanValues[4] = { 0, 9, 1, 5 }; // { Left, Right, Leftwards, Rightwards }

    if (ch >= MaxPPZChannels)
        return false;

    if (ch != 7 || !_EmulateADPCM)
        _Channel[ch].PanValue = value;
    else
        _Channel[ch].PanValue = PanValues[value & 3];

    return true;
}

// 14H Sets the output sample rate.
bool PPZDriver::SetOutputFrequency(uint32_t outputFrequency, bool useInterpolation)
{
    _OutputFrequency = (int) outputFrequency;
    _UseInterpolation = useInterpolation;

    return true;
}

// 15H Sets the original sample rate.
bool PPZDriver::SetSourceFrequency(size_t channelNumber, int sourceFrequency)
{
    if (channelNumber >= MaxPPZChannels)
        return false;

    _Channel[channelNumber].SourceFrequency = sourceFrequency;

    return true;
}

// 16H Set the overal volume（86B Mixer)
void PPZDriver::SetAllVolume(int volume)
{
    if (volume < 16 && volume != _PCMVolume)
    {
        _PCMVolume = volume;
        CreateVolumeTable(_Volume);
    }
}

// 17H PCM Volume (PCMTMP_SET)
void PPZDriver::SetVolume(int volume)
{
    if (volume != _Volume)
        CreateVolumeTable(volume);
}

// 18H ADPCM Emulation
void PPZDriver::ADPCM_EM_SET(bool flag)
{
    _EmulateADPCM = flag;
}

void PPZDriver::SetInstrument(size_t ch, size_t bankNumber, size_t instrumentNumber)
{
    SetLoop(ch, bankNumber, instrumentNumber);
    SetSourceFrequency(ch, _PPZBank[bankNumber]._PZIHeader.PZIItem[instrumentNumber].SampleRate);
}

// Output
void PPZDriver::Mix(Sample * sampleData, size_t sampleCount) noexcept
{
    Sample * di;
    Sample  bx;

    for (int i = 0; i < MaxPPZChannels; i++)
    {
        if (_PCMVolume == 0)
            break;

        if (!_Channel[i].IsPlaying)
            continue;

        if (_Channel[i].Volume == 0)
        {
            // Update the position in the sample buffer.
            _Channel[i].PCM_NOW_XOR += (int) (_Channel[i].PCM_ADD_L * sampleCount);
            _Channel[i].PCMStart    += _Channel[i].PCM_ADD_H * sampleCount + _Channel[i].PCM_NOW_XOR / 0x10000;
            _Channel[i].PCM_NOW_XOR %= 0x10000;

            while (_Channel[i].PCMStart >= _Channel[i].PCMEnd - 1)
            {
                if (_Channel[i].HasLoop)
                {
                    _Channel[i].PCMStart -= (_Channel[i].PCMEnd - 1 - _Channel[i].PCM_LOOP);
                }
                else
                {
                    _Channel[i].IsPlaying = false;
                    break;
                }
            }
            continue;
        }

        if (_Channel[i].PanValue == 0)
        {
            _Channel[i].IsPlaying = false;
            continue;
        }

        if (_UseInterpolation)
        {
            di = sampleData;

            switch (_Channel[i].PanValue)
            {
                case 1: // 1, 0
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart] * (0x10000 - _Channel[i].PCM_NOW_XOR) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStart + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                         di++; // Only the left channel

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 2: // 1, 1/4
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart] * (0x10000 - _Channel[i].PCM_NOW_XOR) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStart + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx / 4;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 3: // 1, 2/4
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart] * (0x10000 - _Channel[i].PCM_NOW_XOR) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStart + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx / 2;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 4: // 1 ,3/4
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart] * (0x10000 - _Channel[i].PCM_NOW_XOR) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStart + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx * 3 / 4;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 5: // 1 , 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart] * (0x10000 - _Channel[i].PCM_NOW_XOR) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStart + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx;
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 6: // 3/4, 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart] * (0x10000 - _Channel[i].PCM_NOW_XOR) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStart + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx * 3 / 4;
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 7:  // 2/4, 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart] * (0x10000 - _Channel[i].PCM_NOW_XOR) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStart + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx / 2;
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 8: // 1/4, 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart] * (0x10000 - _Channel[i].PCM_NOW_XOR) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStart + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                        *di++ += bx / 4;
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 9: // 0, 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart] * (0x10000 - _Channel[i].PCM_NOW_XOR) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStart + 1)] * _Channel[i].PCM_NOW_XOR) >> 16;

                         di++; // Only the right channel
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }
            }
        }
        else
        {
            di = sampleData;

            switch (_Channel[i].PanValue)
            {
                case 1: // 1, 0
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart];

                        *di++ += bx;
                         di++; // Only the left channel

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 2: // 1, 1/4
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart];

                        *di++ += bx;
                        *di++ += bx / 4;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 3: // 1, 2/4
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart];

                        *di++ += bx;
                        *di++ += bx / 2;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 4: // 1, 3/4
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart];

                        *di++ += bx;
                        *di++ += bx * 3 / 4;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 5: // 1, 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart];

                        *di++ += bx;
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 6: // 3/4, 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart];

                        *di++ += bx * 3 / 4;
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 7: // 2/4, 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart];

                        *di++ += bx / 2;
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 8: // 1/4, 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart];

                        *di++ += bx / 4;
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }

                case 9: // 0, 1
                {
                    while (_Channel[i].IsPlaying && (di < &sampleData[sampleCount * 2]))
                    {
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStart];

                         di++; // Only the right channel
                        *di++ += bx;

                        MoveSamplePointer(i);
                    }
                    break;
                }
            }
        }
    }
}

void PPZDriver::MoveSamplePointer(int i) noexcept
{
    _Channel[i].PCMStart    += _Channel[i].PCM_ADD_H;
    _Channel[i].PCM_NOW_XOR += _Channel[i].PCM_ADD_L;

    if (_Channel[i].PCM_NOW_XOR > 0xFFFF)
    {
        _Channel[i].PCM_NOW_XOR -= 0x10000;
        _Channel[i].PCMStart++;
    }

    if (_Channel[i].PCMStart >= _Channel[i].PCMEnd - 1)
    {
        if (_Channel[i].HasLoop)
            _Channel[i].PCMStart = _Channel[i].PCM_LOOP;
        else
            _Channel[i].IsPlaying = false;
    }
}

void PPZDriver::InitializeInternal()
{
    _PPZBank[0].Reset();
    _PPZBank[1].Reset();

    _EmulateADPCM = false;
    _UseInterpolation = false;

    Reset();

    _PCMVolume = 0;
    _Volume = 0;

    SetAllVolume(DefaultVolume);

    _OutputFrequency = DefaultSampleRate;
}

void PPZDriver::Reset()
{
    ::memset(_Channel, 0, sizeof(_Channel));

    for (size_t i = 0; i < _countof(_Channel); ++i)
    {
        _Channel[i].PCM_ADD_H = 1;
        _Channel[i].PCM_ADD_L = 0;
        _Channel[i].PCM_ADDS_H = 1;
        _Channel[i].PCM_ADDS_L = 0;
        _Channel[i].SourceFrequency = 16000; // in Hz
        _Channel[i].PanValue = 5; // Pan Center
        _Channel[i].Volume = 8; // Default volume
    }
}

void PPZDriver::CreateVolumeTable(int volume)
{
    _Volume = volume;

    int AVolume = (int) (0x1000 * ::pow(10.0, volume / 40.0));

    for (int i = 0; i < 16; i++)
    {
        double Value = ::pow(2.0, ((double) (i) + _PCMVolume) / 2.0) * AVolume / 0x18000;

        for (int j = 0; j < 256; j++)
            _VolumeTable[i][j] = (Sample) ((j - 128) * Value);
    }
}

/// <summary>
/// Reads the PZI header.
/// </summary>
void PPZDriver::ReadHeader(File * file, PZIHEADER & ph)
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
void PPZDriver::ReadHeader(File * file, PVIHEADER & ph)
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

