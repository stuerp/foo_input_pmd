
/** $VER: PMDDecoder.h (2023.07.18) P. Stuer **/

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "framework.h"

#include <PMD.h>

/// <summary>
/// Implements a PMD decoder
/// </summary>
#pragma warning(disable: 4820) // x bytes padding added after last data member
class PMDDecoder
{
public:
    PMDDecoder();
    ~PMDDecoder();

    bool Open(const char * filePath, const char * pdxSamplesPath, const uint8_t * data, size_t size, uint32_t synthesisRate);

    void Initialize() const noexcept;
    size_t Render(audio_chunk & audioChunk, size_t sampleCount) noexcept;

    bool IsPMD(const uint8_t * data, size_t size) const noexcept;

    #pragma region(State)
    uint32_t GetPosition() const noexcept;
    void SetPosition(uint32_t seconds) const noexcept;

    uint32_t GetEventNumber() const noexcept;
    uint32_t GetLoopNumber() const noexcept;

    uint32_t GetBlockSize() const noexcept { return BlockSize; }

    uint32_t GetSampleRate() const noexcept { return SampleRate; }
    uint32_t GetChannelCount() const noexcept { return ChannelCount; }
    uint32_t GetBitsPerSample () const noexcept { return BitsPerSample; }

    uint32_t GetLength() const noexcept { return _Length; }
    uint32_t GetLoopLength() const noexcept { return _LoopLength; }

    uint32_t GetEventCount() const noexcept { return _EventCount; }
    uint32_t GetLoopEventCount() const noexcept { return _LoopEventCount; }

    const pfc::string8 & GetTitle() const noexcept { return _Title; }
    const pfc::string8 & GetComposer() const noexcept { return _Composer; }
    const pfc::string8 & GetArranger() const noexcept { return _Arranger; }
    const pfc::string8 & GetMemo() const noexcept { return _Memo; }
    #pragma endregion

    #pragma region(Configuration)
    uint32_t GetMaxLoopNumber() const noexcept { return _MaxLoopNumber; }
    void SetMaxLoopNumber(uint32_t value) noexcept { _MaxLoopNumber = value; }

    uint32_t GetFadeOutDuration() const noexcept { return _FadeOutDuration; }
    void SetFadeOutDuration(uint32_t value) noexcept { _FadeOutDuration = value; }
    #pragma endregion

private:
    bool IsBusy() const noexcept;

private:
    pfc::string8 _FilePath;
    uint8_t * _Data;
    size_t _Size;

    // State
    PMD * _PMD;

    uint32_t _Length;           // Length of the song in ms.
    uint32_t _LoopLength;       // Length of the loop part of the song in ms. 0 if no loop defined.

    uint32_t _EventCount;       // Number of events in the song
    uint32_t _LoopEventCount;   // Number of events in the loop part of the song

    pfc::string8 _Title;
    pfc::string8 _Composer;
    pfc::string8 _Arranger;
    pfc::string8 _Memo;

    pfc::array_t<int16_t> _Samples;

    // Configuration
    uint32_t _MaxLoopNumber;    // Maximum number of times to loop. 0 if looping is disabled.
    uint32_t _FadeOutDuration;  // Fade out duration (in ms).
    uint32_t _SynthesisRate;    // Fade out duration (in Hz).

    static const uint32_t BlockSize = 512; // Number of samples per block

    static const uint32_t SampleRate = 44100;
    static const uint32_t ChannelCount = 2;
    static const uint32_t BitsPerSample = 16;
};
