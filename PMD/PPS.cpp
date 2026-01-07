
/** $VER: PPS.cpp (2026.01.07) PCM driver for the SSG (Software-controlled Sound Generator) / Original Programmed by NaoNeko / Modified by Kaja / Windows Converted by C60 **/

#include <pch.h>

#include "PPS.h"

pps_t::pps_t(File * file) : _File(file), _Samples()
{
    Reset();
}

pps_t::~pps_t()
{
    if (_Samples)
    {
        ::free(_Samples);
        _Samples = nullptr;
    }
}

/// <summary>
/// Initializes this instance.
/// </summary>
bool pps_t::Initialize(uint32_t sampleRate, bool useInterpolation) noexcept
{
    Reset();

    SetSampleRate(sampleRate, useInterpolation);

    return true;
}

/// <summary>
/// Resets this instance.
/// </summary>
void pps_t::Reset()
{
    _FilePath[0] = '\0';

    ::memset(&_Header, 0, sizeof(_Header));

    _SampleRate = FREQUENCY_44_1K;
    _UseInterpolation = false;

    if (_Samples)
    {
        ::free(_Samples);
        _Samples = nullptr;
    }

    _IsPlaying = false;
    _IsMonophonic = false;
    _IsSlowCPU = false;

    _Volume1 = 0;
    _Data1 = nullptr;
    _Size1   = 0;
    _DataXOr1 = 0;
    _Tick1 = 0;
    _TickXOr1 = 0;

    _Volume2 = 0;
    _Data2 = nullptr;
    _Size2   = 0;
    _DataXOr2 = 0;
    _Tick2 = 0;
    _TickXOr2 = 0;


    _KeyOffVolume = 0;

    SetVolume(-10);
}

/// <summary>
/// Stops the PDR. (00H PDR 停止)
/// </summary>
bool pps_t::Stop(void)
{
    _IsPlaying = false;

    _Data1 = nullptr;
    _Data2 = nullptr;

    _Size1   = 0;
    _Size2   = 0;

    return true;
}

/// <summary>
/// Starts the PDR. (01H PDR 再生)
/// </summary>
bool pps_t::Start(int sampleNumber, int toneShift, int volumeShift)
{
    const auto & PPSSample = _Header.PPSSamples[sampleNumber];

    if (PPSSample._Offset == 0)
        return false;

    int32_t ToneOffset = (225 + PPSSample._Tone) % 256;

    if (toneShift < 0)
    {
        if ((ToneOffset += toneShift) <= 0)
            ToneOffset = 1;
    }
    else
    {
        if ((ToneOffset += toneShift) > 255)
            ToneOffset = 255;
    }

    if (PPSSample._Volume + volumeShift >= 15)
        return false;

    if (_IsPlaying && !_IsMonophonic)
    {
        _Volume2  = _Volume1;
        _Data2    = _Data1;
        _Size2    = _Size1;
        _DataXOr2 = _DataXOr1;
        _Tick2    = _Tick1;
        _TickXOr2 = _TickXOr1;
    }
    else
        _Size2 = 0;

    _Volume1  = PPSSample._Volume + volumeShift;
    _Data1    = &_Samples[(PPSSample._Offset - PPSHEADERSIZE) * 2];
    _Size1    = PPSSample._Size * 2;
    _DataXOr1 = 0;
    _Tick1    = (((_IsSlowCPU ? 8000 : 16000) * ToneOffset / 225) << 16) / _SampleRate;
    _TickXOr1 = _Tick1 & 0xFFFF;
    _Tick1 >>= 16;

    _IsPlaying = true;

    return true;
}

/// <summary>
/// Loads a PPS sample library.
/// </summary>
int pps_t::Load(const WCHAR * filePath)
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

            ::memset(&_Header, 0, sizeof(_Header));
        }

        return PPS_OPEN_FAILED;
    }

    size_t Size = (size_t) _File->GetFileSize(filePath);

    {
        PPSHEADER ph;

        ReadHeader(_File, ph);

        if (::memcmp(&_Header, &ph, sizeof(_Header)) == 0)
        {
            ::wcscpy_s(_FilePath, filePath);
            _File->Close();

            return PPS_ALREADY_LOADED;
        }

        if (_Samples)
        {
            ::free(_Samples);
            _Samples = nullptr;
        }

        ::memcpy(&_Header, &ph, sizeof(_Header));
    }

    Size -= PPSHEADERSIZE;

    if ((_Samples = (sample_t *) malloc(Size * sizeof(sample_t) * 2 / sizeof(uint8_t))) == NULL)
    {
        _File->Close();

        return PPZ_OUT_OF_MEMORY;
    }

    {
        uint8_t * Data = (uint8_t *) ::malloc(Size);

        if (Data ==nullptr)
        {
            _File->Close();
            return PPZ_OUT_OF_MEMORY;
        }

        if (_File->Read(Data, (uint32_t) Size) == (int32_t) Size)
        {
            // Convert the sample format.
            uint8_t * Src = Data;
            sample_t * Dst = _Samples;

            for (size_t i = 0; i < Size / (int32_t) sizeof(uint8_t); ++i)
            {
                *Dst++ = ((*Src) >> 4) & 0x0F;
                *Dst++ =  (*Src)       & 0x0F;

                Src++;
            }
        }

        ::free(Data);

        //  PPS correction (Small noise countermeasure) / Attenuate by 160 samples
        for (size_t i = 0; i < _countof(_Header.PPSSamples); ++i)
        {
            const uint32_t End = (uint32_t) (_Header.PPSSamples[i]._Offset - PPSHEADERSIZE * 2) + (uint32_t) (_Header.PPSSamples[i]._Size * 2);

            uint32_t Begin = End - 160;

            if (Begin < _Header.PPSSamples[i]._Offset - PPSHEADERSIZE * 2)
                Begin = _Header.PPSSamples[i]._Offset - PPSHEADERSIZE * 2;

            for (uint32_t j = Begin; j < End; ++j)
            {
                _Samples[j] = (sample_t) (_Samples[j] - (j - Begin) * 16 / (End - Begin));

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
/// Reads the header.
/// </summary>
void pps_t::ReadHeader(File * file, PPSHEADER & header)
{
    uint8_t Data[84];

    file->Read(Data, sizeof(Data));

    for (size_t i = 0; i < _countof(header.PPSSamples); ++i)
    {
        header.PPSSamples[i]._Offset = (uint16_t) (Data[i * 6]     | (Data[i * 6 + 1] << 8));
        header.PPSSamples[i]._Size   = (uint16_t) (Data[i * 6 + 2] | (Data[i * 6 + 3] << 8));
        header.PPSSamples[i]._Tone   =             Data[i * 6 + 4];
        header.PPSSamples[i]._Volume =             Data[i * 6 + 5];
    }
}

/// <summary>
/// Sets a parameter.
/// </summary>
bool pps_t::SetParameter(int index, bool value)
{
    switch (index)
    {
        case 0:
            _IsMonophonic = value;
            return true;

        case 1:
            _IsSlowCPU = value;
            return true;

        default:
            return false;
    }
}

/// <summary>
/// Sets the sample rate.
/// </summary>
bool pps_t::SetSampleRate(uint32_t sampleRate, bool useInterpolation)
{
    _SampleRate = (int32_t) sampleRate;
    _UseInterpolation = useInterpolation;

    return true;
}

/// <summary>
/// Sets the volume.
/// </summary>
void pps_t::SetVolume(int volume)
{
    double Base = 0x4000 * 2 / 3.0 * ::pow(10.0, volume / 40.0);

    for (int i = 15; i >= 1; i--)
    {
        _EmitTable[i] = (int32_t) Base;
        Base /= 1.189207115;
    }

    _EmitTable[0] = 0;
}

/// <summary>
/// Mixes the samples of the PPS.
/// </summary>
void pps_t::Mix(frame32_t * frames, size_t frameCount) noexcept
{
/*
    static const int table[16*16] =
    {
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

    auto Samples = (sample_t *) frames;

    for (size_t i = 0; i < frameCount; ++i)
    {
        int l1, l2, h1, h2;

        if (_Size1 > 1)
        {
            l1 = * _Data1      - _Volume1;
            l2 = *(_Data1 + 1) - _Volume1;

            if (l1 < 0)
                l1 = 0;

            if (l2 < 0)
                l2 = 0;
        }
        else
            l1 = l2 = 0;

        if (_Size2 > 1)
        {
            h1 = * _Data2      - _Volume2;
            h2 = *(_Data2 + 1) - _Volume2;

            if (h1 < 0)
                h1 = 0;

            if (h2 < 0)
                h2 = 0;
        }
        else
            h1 = h2 = 0;

        sample_t Sample = _UseInterpolation ? (_EmitTable[l1] * (0x10000 - _DataXOr1) + _EmitTable[l2] * _DataXOr1 + _EmitTable[h1] * (0x10000 - _DataXOr2) + _EmitTable[h2] * _DataXOr2) / 0x10000 : (_EmitTable[l1] + _EmitTable[h1]);

        _KeyOffVolume = (_KeyOffVolume * 255) / 256;

        Sample += _KeyOffVolume;

        *Samples++ += Sample;
        *Samples++ += Sample;

        if (_Size2 > 1)
        {
            _DataXOr2 += _TickXOr2;

            if (_DataXOr2 >= 0x10000)
            {
                _Size2--;
                _Data2++;
                _DataXOr2 -= 0x10000;
            }

            _Size2 -= _Tick2;
            _Data2 += _Tick2;

            if (_IsSlowCPU)
            {
                _DataXOr2 += _TickXOr2;

                if (_DataXOr2 >= 0x10000)
                {
                    _Size2--;
                    _Data2++;
                    _DataXOr2 -= 0x10000;
                }

                _Size2 -= _Tick2;
                _Data2 += _Tick2;
            }
        }

        _DataXOr1 += _TickXOr1;

        if (_DataXOr1 >= 0x10000)
        {
            _Size1--;
            _Data1++;
            _DataXOr1 -= 0x10000;
        }

        _Size1 -= _Tick1;
        _Data1 += _Tick1;

        if (_IsSlowCPU)
        {
            _DataXOr1 += _TickXOr1;

            if (_DataXOr1 >= 0x10000)
            {
                _Size1--;
                _Data1++;
                _DataXOr1 -= 0x10000;
            }

            _Size1   -= _Tick1;
            _Data1 += _Tick1;
        }

        if (_Size1 <= 1 && _Size2 <= 1) // Both are stopped
        {
            if (_IsPlaying)
                _KeyOffVolume += _EmitTable[_Data1[_Size1 - 1]];

            _IsPlaying = false; // Stop playing
        }
        else
        if (_Size1 <= 1 && _Size2 > 1)
        {
            _Volume1  = _Volume2;
            _Size1    = _Size2;
            _Data1    = _Data2;
            _DataXOr1 = _DataXOr2;
            _Tick1    = _Tick2;
            _TickXOr1 = _TickXOr2;

            _Size2 = 0;
        }
        else
        if (_Size1 > 1 && _Size2 < 1)
        {
            if (_Data2 != nullptr)
            {
                _KeyOffVolume += _EmitTable[_Data2[_Size2 - 1]];
                _Data2 = nullptr;
            }
        }
    }
}
