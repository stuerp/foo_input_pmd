
/** $VER: PMDDecoder.h (2023.07.12) P. Stuer **/

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <sdk/foobar2000-lite.h>

/// <summary>
/// Implements a PMD decoder
/// </summary>
#pragma warning(disable: 4820) // x bytes padding added after last data member
class PMDDecoder
{
public:
    PMDDecoder();
    ~PMDDecoder();

    bool Open(const char * filePath, const char * pdxSamplesPath, const uint8_t * data, size_t size);

    void Initialize() const noexcept;
    size_t Render(audio_chunk & audioChunk, size_t sampleCount) noexcept;

    uint32_t GetPosition() const noexcept;
    void SetPosition(uint32_t seconds) const noexcept;

    bool IsPMD(const uint8_t * data, size_t size) const noexcept;

    uint32_t GetBlockSize() const noexcept { return BlockSize; }

    uint32_t GetSampleRate() const noexcept { return SampleRate; }
    uint32_t GetChannelCount() const noexcept { return ChannelCount; }
    uint32_t GetBitsPerSample () const noexcept { return BitsPerSample; }

    uint32_t GetLength() const noexcept { return _LengthInMS; }
    uint32_t GetLoop() const noexcept { return _LoopInMS; }

    const pfc::string8 & GetTitle() const noexcept { return _Title; }
    const pfc::string8 & GetComposer() const noexcept { return _Composer; }
    const pfc::string8 & GetArranger() const noexcept { return _Arranger; }
    const pfc::string8 & GetMemo() const noexcept { return _Memo; }

private:
    pfc::string8 _FilePath;
    uint8_t * _Data;
    size_t _Size;

    uint32_t _LengthInMS;
    uint32_t _LoopInMS;

    pfc::string8 _Title;
    pfc::string8 _Composer;
    pfc::string8 _Arranger;
    pfc::string8 _Memo;

    pfc::array_t<int16_t> _Samples;

    static const uint32_t BlockSize = 512; // Number of samples per block

    static const uint32_t SampleRate = 44100;
    static const uint32_t ChannelCount = 2;
    static const uint32_t BitsPerSample = 16;
};
