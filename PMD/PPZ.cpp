
/** $VER: PPZ.cpp (2023.10.22) PC-98's 86 soundboard's 8 PCM driver (Programmed by UKKY / Based on Windows conversion by C60) **/

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
    Initialize();
}

PPZDriver::~PPZDriver()
{
}

//  00H Initialize
void PPZDriver::Initialize(uint32_t outputFrequency, bool useInterpolation)
{
    Initialize();
    SetOutputFrequency(outputFrequency, useInterpolation);
}

// 01H Start PCM
void PPZDriver::Play(size_t ch, int bankNumber, int sampleNumber, uint16_t start, uint16_t stop)
{
    PPZBank& pb = _PPZBank[bankNumber];

    if ((ch >= _countof(_Channel)) || pb.IsEmpty())
        return;

    _Channel[ch].IsPVI = pb._IsPVI;
    _Channel[ch].IsPlaying = true;
    _Channel[ch].SampleNumber = sampleNumber;
    _Channel[ch].PCMStartL = 0;

    if ((ch == 7) && _EmulateADPCM)
    {
        _Channel[ch].PCMStartH      = &pb._Data[Clamp(((int) start)    * 64, 0, pb._Size - 1)];
        _Channel[ch].PCMEndRounded = &pb._Data[Clamp(((int) stop - 1) * 64, 0, pb._Size - 1)];
        _Channel[ch].PCMEnd        = _Channel[ch].PCMEndRounded;
    }
    else
    {
        PZIITEM& pi = pb._PZIHeader.PZIItem[sampleNumber];

        _Channel[ch].PCMStartH      = &pb._Data[pi.Start];
        _Channel[ch].PCMEndRounded = &pb._Data[pi.Start + pi.Size];

        if (_Channel[ch].HasLoop)
        {
            if (_Channel[ch].LoopStartOffset < pi.Size)
                _Channel[ch].PCMLoopStart = &pb._Data[pi.Start + _Channel[ch].LoopStartOffset];
            else
                _Channel[ch].PCMLoopStart = &pb._Data[pi.Start + pi.Size - 1];

            if (_Channel[ch].LoopEndOffset < pi.Size)
                _Channel[ch].PCMEnd = &pb._Data[pi.Start + _Channel[ch].LoopEndOffset];
            else
                _Channel[ch].PCMEnd = &pb._Data[pi.Start + pi.Size];
        }
        else
            _Channel[ch].PCMEnd = _Channel[ch].PCMEndRounded;
    }
}

// 02H Stop PCM
void PPZDriver::Stop(size_t ch)
{
    if (ch >= _countof(_Channel))
        return;

    _Channel[ch].IsPlaying = false;
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

        uint8_t * DstData = (uint8_t *) ::malloc(DstSize);

        if (DstData == nullptr)
        {
            _File->Close();

            return PPZ_OUT_OF_MEMORY;
        }

        ::memset(DstData, 0, DstSize);

        _PPZBank[bankNumber]._Data = DstData;
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

            if (psrc == nullptr)
            {
                _File->Close();

                return PPZ_OUT_OF_MEMORY;
            }

            ::memset(psrc, 0, SrcSize);

            _File->Read(psrc, (uint32_t) Size);

            uint8_t * psrc2 = psrc;

            // Convert ADPCM to PCM.
            for (size_t i = 0; i < PVIHeader.Count; ++i)
            {
                int X_N     = X_N0;
                int DELTA_N = DELTA_N0;

                for (size_t j = 0; j < PZIHeader.PZIItem[i].Size / 2; ++j)
                {
                    X_N     = Clamp(X_N     + table1[(*psrc >> 4) & 0x0F] * DELTA_N /  8, -32768, 32767);
                    DELTA_N = Clamp(DELTA_N * table2[(*psrc >> 4) & 0x0F]           / 64,    127, 24576);

                    *DstData++ = (uint8_t) (X_N / (32768 / 128) + 128);

                    X_N     = Clamp(X_N     + table1[*psrc   & 0x0F] * DELTA_N /  8, -32768, 32767);
                    DELTA_N = Clamp(DELTA_N * table2[*psrc++ & 0x0F]           / 64,    127, 24576);

                    *DstData++ = (uint8_t) (X_N / (32768 / 128) + 128);
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
void PPZDriver::SetVolume(size_t ch, int volume)
{
    if (ch >= _countof(_Channel))
        return;

    if ((ch == 7) && _EmulateADPCM)
        _Channel[ch].Volume = ADPCM_EM_VOL[volume & 0xFF];
    else
        _Channel[ch].Volume = volume;
}

// 0BH Pitch Frequency
void PPZDriver::SetPitch(size_t ch, uint32_t pitch)
{
    if (ch >= _countof(_Channel))
        return;

    if ((ch == 7) && _EmulateADPCM)
        pitch = (pitch & 0xFFFF) * 0x8000 / 0x49BA;

    int AddsL = (int) (pitch & 0xFFFF);
    int AddsH = (int) (pitch >> 16);

    _Channel[ch].PCMAddH = (int) ((((int64_t) (AddsH) << 16) + AddsL) * 2 * _Channel[ch].SourceFrequency / _OutputFrequency);

    _Channel[ch].PCMAddL = _Channel[ch].PCMAddH & 0xFFFF;
    _Channel[ch].PCMAddH = _Channel[ch].PCMAddH >> 16;
}

// 0EH Set loop pointer
void PPZDriver::SetLoop(size_t ch, size_t bankNumber, size_t sampleNumber)
{
    if ((ch >= _countof(_Channel)) || (bankNumber > 1) || (sampleNumber > 127))
        return;

    const PZIITEM& pi = _PPZBank[bankNumber]._PZIHeader.PZIItem[sampleNumber];

    if ((pi.LoopStart != ~0U) && (pi.LoopStart < pi.LoopEnd))
    {
        _Channel[ch].HasLoop = true;
        _Channel[ch].LoopStartOffset = pi.LoopStart;
        _Channel[ch].LoopEndOffset = pi.LoopEnd;
    }
    else
    {
        _Channel[ch].HasLoop = false;
        _Channel[ch].PCMEnd = _Channel[ch].PCMEndRounded;
    }
}

void PPZDriver::SetLoop(size_t ch, size_t bankNumber, size_t sampleNumber, int loopStart, int loopEnd)
{
    if ((ch >= _countof(_Channel)) || (bankNumber > 1) || (sampleNumber > 127))
        return;

    const PZIITEM& pi = _PPZBank[bankNumber]._PZIHeader.PZIItem[sampleNumber];

    if (loopStart < 0)
        loopStart = (int) pi.Size + loopStart;

    if (loopEnd < 0)
        loopEnd = (int) pi.Size + loopEnd;

    if (loopStart < loopEnd)
    {
        _Channel[ch].HasLoop = true;
        _Channel[ch].LoopStartOffset = (uint32_t) loopStart;
        _Channel[ch].LoopEndOffset = (uint32_t) loopEnd;
    }
    else
    {
        _Channel[ch].HasLoop = false;
        _Channel[ch].PCMEnd = _Channel[ch].PCMEndRounded;
    }
}

// 12H Stop all channels.
void PPZDriver::AllStop()
{
    for (size_t i = 0; i < _countof(_Channel); ++i)
        Stop(i);
}

// 13H Set the pan value.
void PPZDriver::SetPan(size_t ch, int value)
{
    static const int PanValues[4] = { 0, 9, 1, 5 }; // { Left, Right, Leftwards, Rightwards }

    if (ch >= _countof(_Channel))
        return;

    if ((ch == 7) && _EmulateADPCM)
        _Channel[ch].PanValue = PanValues[value & 3];
    else
        _Channel[ch].PanValue = value;
}

// 14H Sets the output sample rate.
void PPZDriver::SetOutputFrequency(uint32_t outputFrequency, bool useInterpolation)
{
    _OutputFrequency = (int) outputFrequency;
    _UseInterpolation = useInterpolation;
}

// 15H Sets the original sample rate.
void PPZDriver::SetSourceFrequency(size_t ch, int sourceFrequency)
{
    if (ch >= _countof(_Channel))
        return;

    _Channel[ch].SourceFrequency = sourceFrequency;
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

// 18H Enables or disables ADPCM emulation.
void PPZDriver::EmulateADPCM(bool flag)
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

    for (int i = 0; i < _countof(_Channel); ++i)
    {
        if (_PCMVolume == 0)
            break;

        if (!_Channel[i].IsPlaying)
            continue;

        if (_Channel[i].Volume == 0)
        {
            // Update the position in the sample buffer.
            _Channel[i].PCMStartL += (int) (_Channel[i].PCMAddL * sampleCount);
            _Channel[i].PCMStartH +=        _Channel[i].PCMAddH * sampleCount + _Channel[i].PCMStartL / 0x10000;
            _Channel[i].PCMStartL %= 0x10000;

            while (_Channel[i].PCMStartH >= _Channel[i].PCMEnd - 1)
            {
                if (_Channel[i].HasLoop)
                {
                    _Channel[i].PCMStartH -= (_Channel[i].PCMEnd - 1 - _Channel[i].PCMLoopStart);
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
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH] * (0x10000 - _Channel[i].PCMStartL) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStartH + 1)] * _Channel[i].PCMStartL) >> 16;

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
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH] * (0x10000 - _Channel[i].PCMStartL) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStartH + 1)] * _Channel[i].PCMStartL) >> 16;

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
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH] * (0x10000 - _Channel[i].PCMStartL) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStartH + 1)] * _Channel[i].PCMStartL) >> 16;

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
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH] * (0x10000 - _Channel[i].PCMStartL) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStartH + 1)] * _Channel[i].PCMStartL) >> 16;

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
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH] * (0x10000 - _Channel[i].PCMStartL) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStartH + 1)] * _Channel[i].PCMStartL) >> 16;

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
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH] * (0x10000 - _Channel[i].PCMStartL) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStartH + 1)] * _Channel[i].PCMStartL) >> 16;

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
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH] * (0x10000 - _Channel[i].PCMStartL) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStartH + 1)] * _Channel[i].PCMStartL) >> 16;

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
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH] * (0x10000 - _Channel[i].PCMStartL) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStartH + 1)] * _Channel[i].PCMStartL) >> 16;

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
                        bx = (_VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH] * (0x10000 - _Channel[i].PCMStartL) + _VolumeTable[_Channel[i].Volume][*(_Channel[i].PCMStartH + 1)] * _Channel[i].PCMStartL) >> 16;

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
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH];

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
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH];

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
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH];

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
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH];

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
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH];

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
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH];

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
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH];

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
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH];

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
                        bx = _VolumeTable[_Channel[i].Volume][*_Channel[i].PCMStartH];

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
    _Channel[i].PCMStartH += _Channel[i].PCMAddH;
    _Channel[i].PCMStartL += _Channel[i].PCMAddL;

    if (_Channel[i].PCMStartL > 0xFFFF)
    {
        _Channel[i].PCMStartL -= 0x10000;
        _Channel[i].PCMStartH++;
    }

    if (_Channel[i].PCMStartH >= _Channel[i].PCMEnd - 1)
    {
        if (_Channel[i].HasLoop)
            _Channel[i].PCMStartH = _Channel[i].PCMLoopStart;
        else
            _Channel[i].IsPlaying = false;
    }
}

void PPZDriver::Initialize()
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
        _Channel[i].PCMAddH = 1;
        _Channel[i].PCMAddL = 0;
        _Channel[i].SourceFrequency = 16000; // in Hz
        _Channel[i].PanValue = 5; // Pan Center
        _Channel[i].Volume = 8; // Default volume
    }
}

void PPZDriver::CreateVolumeTable(int volume)
{
    _Volume = volume;

    int AVolume = (int) (0x1000 * ::pow(10.0, volume / 40.0));

    for (int i = 0; i < 16; ++i)
    {
        double Value = ::pow(2.0, ((double) i + _PCMVolume) / 2.0) * AVolume / 0x18000;

        for (int j = 0; j < 256; ++j)
            _VolumeTable[i][j] = (Sample) ((j - 128) * Value);
    }
}

/// <summary>
/// Reads the PZI header.
/// </summary>
void PPZDriver::ReadHeader(File * file, PZIHEADER& ph)
{
    uint8_t Data[2336];

    if (file->Read(Data, sizeof(Data)) != sizeof(Data))
        return;

    ::memcpy(ph.ID, Data, sizeof(ph.ID));
    ::memcpy(ph.Dummy1, &Data[0x04], sizeof(ph.Dummy1));

    ph.Count = Data[0x0B];

    ::memcpy(ph.Dummy2, &Data[0x0C], sizeof(ph.Dummy2));

    for (int i = 0; i < 128; ++i)
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

    for (int i = 0; i < 128; ++i)
    {
        ph.PVIItem[i].Start = (uint16_t) ((Data[0x20 + i * 18]) | (Data[0x21 + i * 18] << 8) | (Data[0x22 + i * 18] << 16) | (Data[0x23 + i * 18] << 24));
//FIXME: Why is startaddress overwritten here?
        ph.PVIItem[i].Start = (uint16_t) ((Data[0x10 + i * 4]) | (Data[0x11 + i * 4] << 8));
        ph.PVIItem[i].End   = (uint16_t) ((Data[0x12 + i * 4]) | (Data[0x13 + i * 4] << 8));
    }
}
