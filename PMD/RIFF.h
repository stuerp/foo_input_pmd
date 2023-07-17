
/** $VER: RIFF.h (2023.07.17) P. Stuer **/

#pragma once

#include <stdint.h>

struct ckheader
{
    uint32_t ID;
    uint32_t Size;
};

#pragma pack(push)
#pragma pack(2)

struct ckfmt
{
    uint16_t FormatTag;         // Format category
    uint16_t Channels;          // Number of channels
    uint32_t SampleRate;        // Sampling rate
    uint32_t AvgBytesPerSec;    // For buffer estimation
    uint16_t BlockAlign;        // Data block size
};

/*
            struct
            {
                ckheader Header1;
                    uint32_t Type;

                ckheader Header2;
                    ckfmt Format;
                    uint16_t SampleSize;
            } wh;

            if (_File->Read(&wh, sizeof(wh)) != sizeof(wh))
                break;

            if ((wh.Header1.ID != FOURCC_RIFF) || (wh.Type != FOURCC_WAVE) || (wh.Header2.ID != FOURCC_fmt) || (wh.Format.FormatTag != 0x0001) || (wh.Format.Channels != 1) || (wh.SampleSize != 16))
                break;

            ckheader Header3;

            do
            {
                if (_File->Read(&Header3, sizeof(Header3)) != sizeof(Header3))
                    break;

                if (!_File->Seek(Header3.Size, File::SeekCurrent))
                    break;
            }
            while (Header3.ID != FOURCC_data);

            size_t SampleCount = Header3.Size >> 1;

            {
                delete _Rhythm[i].sample;

                _Rhythm[i].sample = new int16_t[SampleCount];

                if (_Rhythm[i].sample == nullptr)
                    break;

                if (_File->Read(_Rhythm[i].sample, Header3.Size) != Header3.Size)
                    break;

                _Rhythm[i].rate = wh.Format.SampleRate;
                _Rhythm[i].step = _Rhythm[i].rate * 1024 / _SynthesisRate;
                _Rhythm[i].pos  =
                _Rhythm[i].size = Header3.Size;
            }
*/

#define FOURCC_RIFF mmioFOURCC('R','I','F','F')
#define FOURCC_WAVE mmioFOURCC('W','A','V','E')
#define FOURCC_fmt  mmioFOURCC('f','m','t',' ')
#define FOURCC_data mmioFOURCC('d','a','t','a')

#pragma pack(pop)
