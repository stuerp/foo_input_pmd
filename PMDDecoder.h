
/** $VER: PMDPlayer.h (2025.10.01) P. Stuer **/

#pragma once

#include "pch.h"

#include <PMD.h>

#pragma warning(disable: 4820) // x bytes padding added after last data member

/// <summary>
/// Implements a PMD player.
/// </summary>
class pmd_decoder_t
{
public:
    pmd_decoder_t();
    ~pmd_decoder_t();

    bool Open(const uint8_t * data, size_t size, uint32_t sampleRate, const char * filePath, const char * pdxSamplesPath);

    void Initialize() noexcept;
    size_t Render(audio_chunk & audioChunk, size_t sampleCount) noexcept;

    bool IsPMD(const uint8_t * data, size_t size) const noexcept;

    #pragma region State

    uint32_t GetPosition() const noexcept;
    void SetPosition(uint32_t seconds) const noexcept;

    uint32_t GetLoopNumber() const noexcept;

    uint32_t GetBlockSize() const noexcept { return BlockSize; }

    uint32_t GetSampleRate() const noexcept { return SampleRate; }
    uint32_t GetChannelCount() const noexcept { return ChannelCount; }
    uint32_t GetBitsPerSample () const noexcept { return BitsPerSample; }

    uint32_t GetLength() const noexcept { return _Length; }
    uint32_t GetLoopLength() const noexcept { return _LoopLength; }

    uint32_t GetTickCount() const noexcept { return _TickCount; }
    uint32_t GetLoopTickCount() const noexcept { return _LoopTickCount; }

    const pfc::string GetTitle() const noexcept { return _Title; }
    const pfc::string GetArranger() const noexcept { return _Arranger; } // Artist
    const pfc::string GetComposer() const noexcept { return _Composer; }
    const pfc::string GetMemo() const noexcept { return _Memo; }

    const pfc::string GetPCMFileName() const noexcept { return _PCMFileName; }
    const pfc::string GetPPSFileName() const noexcept { return _PPSFileName; }
    const pfc::string GetPPZFileName(size_t index) const noexcept { return (index == 1) ? _PPZFileName1 : ((index == 2) ? _PPZFileName2 : pfc::string()); }

    #pragma endregion

    #pragma region Configuration

    uint32_t GetMaxLoopNumber() const noexcept { return _MaxLoopNumber; }
    void SetMaxLoopNumber(uint32_t value) noexcept { _MaxLoopNumber = value; }

    uint32_t GetFadeOutDuration() const noexcept { return _FadeOutDuration; }
    void SetFadeOutDuration(uint32_t value) noexcept { _FadeOutDuration = value; }

    #pragma endregion

private:
    bool IsBusy() const noexcept;

private:
    pfc::string _FilePath;
    uint8_t * _Data;
    size_t _Size;

    pmd_driver_t * _PMD;

    uint32_t _Length;           // Length of the song in ms.
    uint32_t _LoopLength;       // Length of the loop part of the song in ms. 0 if no loop defined.

    uint32_t _TickCount;        // Number of ticks in the song.
    uint32_t _LoopTickCount;    // Number of ticks in the loop part of the song.

    pfc::string _Title;
    pfc::string _Composer;
    pfc::string _Arranger;
    pfc::string _Memo;

    pfc::string _PCMFileName;
    pfc::string _PPSFileName;
    pfc::string _PPZFileName1;
    pfc::string _PPZFileName2;

    pfc::array_t<int16_t> _Frames;

    // Configuration
    uint32_t _MaxLoopNumber;    // Maximum number of times to loop. 0 if looping is disabled.
    uint32_t _FadeOutDuration;  // Fade out duration (in ms).
    uint32_t _SampleRate;    // Fade out duration (in Hz).

    static const size_t BlockSize = 512; // Number of samples per block

    static const uint32_t SampleRate = 44100;
    static const uint32_t ChannelCount = 2;
    static const uint32_t BitsPerSample = 16;
};
