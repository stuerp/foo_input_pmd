
/** $VER: PMDDecoder.h (2023.04.07) **/

#pragma once

#include <sdk/foobar2000-lite.h>

class PMDDecoder
{
public:
    bool Read(std::vector<uint8_t> data);
    size_t Run(audio_sample * samples, size_t sampleCount);

    uint32_t GetChannelCount() const { return 2; }
};
