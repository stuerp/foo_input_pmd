
/** $VER: PMDDecoder.cpp (2023.04.07) **/

#pragma warning(disable: 5045)

#include "PMDDecoder.h"

bool PMDDecoder::Read(std::vector<uint8_t> data)
{
    if (data.size() < 3)
        return false;

    if (data[0] > 0x0F)
        return false;

    if (data[1] != 0x18 && data[1] != 0x1A)
        return false;

    if (data[2] != 0x00 && data[2] != 0xE6)
        return false;

    return true;
}

size_t PMDDecoder::Run(audio_sample * samples, size_t sampleCount)
{
    ::memset(samples, 0, sampleCount * 2 * sizeof(audio_sample));

    return sampleCount;
}
