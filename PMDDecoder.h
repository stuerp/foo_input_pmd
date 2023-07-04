
/** $VER: PMDDecoder.h (2023.04.07) **/

#pragma once

#include <sdk/foobar2000-lite.h>

#include <ymfm/portability_opna.h>
#include <pmdwin.h>
#include <ipmdwin.h>

class PMDDecoder
{
public:
    PMDDecoder();

    bool Read(std::vector<uint8_t> data, const WCHAR * filePath, const WCHAR * pdxSamplesPath);
    size_t Run(audio_sample * samples, size_t sampleCount);

    bool IsPMD(const std::vector<uint8_t> data) const;

    uint32_t GetChannelCount() const { return 2; }

    uint32_t GetLength() const { return _LengthInMS; }
    uint32_t GetLoop() const { return _LoopInMS; }

private:
    WCHAR _FilePath[MAX_PATH];
    uint32_t _LengthInMS;
    uint32_t _LoopInMS;

    char _Title[1024];
    char _Composer[1024];
    char _Arranger[1024];
    char _Memo[1024];

    OPEN_WORK * _OpenWork = nullptr;
};
