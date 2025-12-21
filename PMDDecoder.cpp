
/** $VER: PMDDecoder.cpp (2025.12.21) P. Stuer **/

#include "pch.h"

#include "PMDDecoder.h"
#include "Configuration.h"

#include <pfc/string-conv-lite.h>
#include <shared/audio_math.h>
#include <pathcch.h>

#pragma comment(lib, "pathcch")

#pragma hdrstop

static bool ConvertShiftJISToUTF8(const char * text, pfc::string8 & utf8);

/// <summary>
/// Initializes a new instance.
/// </summary>
PMDDecoder::PMDDecoder() :
    _FilePath(), _Data(), _Size(), _PMD(), _Length(), _LoopLength(), _TickCount(), _LoopTickCount(),
    _MaxLoopNumber(DefaultLoopCount), _FadeOutDuration(DefaultFadeOutDuration), _SynthesisRate(DefaultSynthesisRate)
{
    _Samples.set_count((t_size) BlockSize * ChannelCount);
}

/// <summary>
/// Deletes an instance.
/// </summary>
PMDDecoder::~PMDDecoder()
{
    delete[] _Data;
    delete _PMD;
}

/// <summary>
/// Reads the PMD data from memory.
/// </summary>
bool PMDDecoder::Open(const uint8_t * data, size_t size, uint32_t outputFrequency, const char * filePath, const char * pdxSamplesPath)
{
    _FilePath = filePath;

    if (!IsPMD(data, size))
        return false;

    {
        _Data = new uint8_t[size];

        if (_Data == nullptr)
            throw exception_io();

        ::memcpy(_Data, data, size);

        _Size = size;
    }

    _SynthesisRate = outputFrequency;

    delete _PMD;
    _PMD = new PMD();

    {
        WCHAR FilePath[MAX_PATH];
        WCHAR PDXSamplesPath[MAX_PATH];

        {
            if (::MultiByteToWideChar(CP_UTF8, 0, filePath, -1, FilePath, _countof(FilePath)) == 0)
                return false;

            if (::MultiByteToWideChar(CP_UTF8, 0, pdxSamplesPath, -1, PDXSamplesPath, _countof(PDXSamplesPath)) == 0)
                return false;
        }

        _PMD->Initialize(PDXSamplesPath);
        _PMD->SetSampleRate(_SynthesisRate);

        {
            WCHAR DirectoryPath[MAX_PATH];

            ::wcsncpy_s(DirectoryPath, _countof(DirectoryPath), FilePath, ::wcslen(FilePath));
        
            if (!SUCCEEDED(::PathCchRemoveFileSpec(DirectoryPath, _countof(DirectoryPath))))
                return false;

            {
                std::vector<const WCHAR *> Paths;

                if (::wcslen(DirectoryPath) > 0)
                    Paths.push_back(DirectoryPath);

                Paths.push_back(L".\\");

                if (::wcslen(DirectoryPath) > 0)
                    Paths.push_back(PDXSamplesPath);

                if (!_PMD->SetSearchPaths(Paths))
                    return false;
            }
        }

        if (_PMD->Load(_Data, _Size) != ERR_SUCCESS)
            return false;

        if (!_PMD->GetLength((int *) &_Length, (int *) &_LoopLength, (int *) &_TickCount, (int *) &_LoopTickCount))
            return false;

        {
            char Memo[1024] = { 0 };

            _PMD->GetMemo(data, size, 1, Memo, _countof(Memo));
            ConvertShiftJISToUTF8(Memo, _Title);

            Memo[0] = '\0';
            _PMD->GetMemo(data, size, 2, Memo, _countof(Memo));
            ConvertShiftJISToUTF8(Memo, _Composer);

            Memo[0] = '\0';
            _PMD->GetMemo(data, size, 3, Memo, _countof(Memo));
            ConvertShiftJISToUTF8(Memo, _Arranger);

            Memo[0] = '\0';
            _PMD->GetMemo(data, size, 4, Memo, _countof(Memo));
            ConvertShiftJISToUTF8(Memo, _Memo);
        }
    }

    return true;
}

/// <summary>
/// Returns true if  the buffer points to PMD data.
/// </summary>
bool PMDDecoder::IsPMD(const uint8_t * data, size_t size) const noexcept
{
    return PMD::IsPMD(data, size);
}

/// <summary>
/// Initializes the decoder.
/// </summary>
void PMDDecoder::Initialize() const noexcept
{
    _PMD->UsePPS(CfgUsePPS);
    _PMD->UseSSG(CfgUseSSG);
    _PMD->Start();
/*
    for (int i = 0; i < MaxChannels; ++i)
        _PMD->DisableChannel(i);

    _PMD->EnableChannel(16); // PPZ 1
*/
    console::printf("PMDDecoder: ADPCM ROM %sloaded.", (_PMD->HasADPCMROM() ? "" : "not "));
    console::printf("PMDDecoder: Percussion samples %sloaded.", (_PMD->HasPercussionSamples() ? "" : "not "));

    std::wstring FileName = _PMD->GetPCMFileName();

    if (!FileName.empty())
        console::printf("PMDDecoder: Requires PCM samples from \"%s\": %sfound.", pfc::utf8FromWide(FileName.c_str()).c_str(), (_PMD->GetPCMFilePath().empty() ? "not " : ""));

    FileName = _PMD->GetPPSFileName();

    if (!FileName.empty())
        console::printf("PMDDecoder: Requires PPS samples from \"%s\": %sfound.", pfc::utf8FromWide(FileName.c_str()).c_str(), (_PMD->GetPPSFilePath().empty() ? "not " : ""));

    for (size_t i = 0; i < 2; ++i)
    {
        FileName = _PMD->GetPPZFileName(i);

        if (!FileName.empty())
            console::printf("PMDDecoder: Requires PPZ samples from \"%s\": %sfound.", pfc::utf8FromWide(FileName.c_str()).c_str(), (_PMD->GetPPZFilePath(i).empty() ? "not " : ""));
    }
}

/// <summary>
/// Renders a chunk of audio samples.
/// </summary>
size_t PMDDecoder::Render(audio_chunk & audioChunk, size_t sampleCount) noexcept
{
    uint32_t TotalTickCount = _TickCount;

    if ((CfgPlaybackMode == PlaybackModes::Loop) || (CfgPlaybackMode == PlaybackModes::LoopWithFadeOut))
        TotalTickCount += (_LoopTickCount * _MaxLoopNumber);

    if (!IsBusy() || ((CfgPlaybackMode != LoopForever) && ((uint32_t) _PMD->GetPositionInTicks() > TotalTickCount)))
    {
        _PMD->Stop();

        return false;
    }

    if ((CfgPlaybackMode == PlaybackModes::LoopWithFadeOut) && (_MaxLoopNumber > 0) && (GetLoopNumber() > _MaxLoopNumber - 1))
        _PMD->SetFadeOutDurationHQ((int) _FadeOutDuration);

    _PMD->Render(&_Samples[0], BlockSize);

    audioChunk.set_data_fixedpoint(&_Samples[0], (t_size) BlockSize * ((t_size) BitsPerSample / 8 * ChannelCount), SampleRate, ChannelCount, BitsPerSample, audio_chunk::g_guess_channel_config(ChannelCount));

    return sampleCount;
}

/// <summary>
/// Gets the current decoding position (in ms).
/// </summary>
uint32_t PMDDecoder::GetPosition() const noexcept
{
    return _PMD->GetPosition();
}

/// <summary>
/// Sets the current decoding position (in ms).
/// </summary>
void PMDDecoder::SetPosition(uint32_t milliseconds) const noexcept
{
    _PMD->SetPosition(milliseconds);
};

/// <summary>
/// Gets the number of the current loop. 0 if not looped yet.
/// </summary>
uint32_t PMDDecoder::GetLoopNumber() const noexcept
{
    return (uint32_t) _PMD->GetLoopNumber();
}

/// <summary>
/// Returns true if a track is being decoded.
/// </summary>
/// <returns></returns>
bool PMDDecoder::IsBusy() const noexcept
{
    return _PMD->IsPlaying();
}

/// <summary>
/// Converts a Shift-JIS character array to an UTF-8 string.
/// </summary>
static bool ConvertShiftJISToUTF8(const char * text, pfc::string8 & textDst)
{
    textDst.clear();

    int Size = ::MultiByteToWideChar(932, MB_PRECOMPOSED, text, -1, 0, 0);

    if (Size == 0)
        return false;

    WCHAR * Wide = new WCHAR[(size_t) Size];

    if (::MultiByteToWideChar(932, MB_PRECOMPOSED, text, -1, Wide, Size) != 0)
    {
        Size = ::WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, Wide, -1, nullptr, 0, nullptr, nullptr);

        if (Size != 0)
        {
            char * UTF8 = new char[(size_t) Size + 16];

            if (::WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, Wide, -1, UTF8, Size, nullptr, nullptr) != 0)
                textDst = UTF8;

            delete[] UTF8;
        }

        delete[] Wide;
    }

    return true;
}
