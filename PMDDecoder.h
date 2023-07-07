
/** $VER: PMDDecoder.h (2023.07.07) **/

#pragma once

#include <sdk/foobar2000-lite.h>

#include <ymfm/portability_opna.h>
#include <pmdwin.h>
#include <ipmdwin.h>

#pragma warning(disable: 4820) // Padding added
class PMDDecoder
{
public:
    PMDDecoder();
    ~PMDDecoder();

    bool Read(const uint8_t * data, size_t size, const WCHAR * filePath, const WCHAR * pdxSamplesPath);

    void Initialize() const noexcept;
    size_t Render(audio_sample * samples, size_t sampleCount) const noexcept;

    bool IsPMD(const uint8_t * data, size_t size) const noexcept;

    uint32_t GetSampleCount() const { return SampleCount; }
    uint32_t GetChannelCount() const { return ChannelCount; }

    uint32_t GetLength() const { return _LengthInMS; }
    uint32_t GetLoop() const { return _LoopInMS; }

    const pfc::string8 & GetTitle() const { return _Title; }
    const pfc::string8 & GetComposer() const { return _Composer; }
    const pfc::string8 & GetArranger() const { return _Arranger; }
    const pfc::string8 & GetMemo() const { return _Memo; }

private:
    WCHAR _FilePath[MAX_PATH];
    uint8_t * _Data;
    size_t _Size;

    uint32_t _LengthInMS;
    uint32_t _LoopInMS;

    pfc::string8 _Title;
    pfc::string8 _Composer;
    pfc::string8 _Arranger;
    pfc::string8 _Memo;

    OPEN_WORK * _OpenWork = nullptr;

    int16_t * _Samples;

    static const uint32_t SampleCount = 512;
    static const uint32_t ChannelCount = 2;
};
