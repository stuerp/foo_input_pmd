
/** $VER: RIFF.h (2026.01.07) P. Stuer **/

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
#pragma pack(pop)

#define FOURCC_RIFF mmioFOURCC('R','I','F','F')
#define FOURCC_WAVE mmioFOURCC('W','A','V','E')
#define FOURCC_fmt  mmioFOURCC('f','m','t',' ')
#define FOURCC_data mmioFOURCC('d','a','t','a')
