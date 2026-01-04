
/** $VER: PMDDecoder.cpp (2025.12.21) P. Stuer **/

#include "pch.h"

#include "PMDDecoder.h"
#include "Configuration.h"

#include <pfc\string-conv-lite.h>
#include <shared\audio_math.h>

#include <pathcch.h>

#pragma comment(lib, "pathcch")

#pragma hdrstop

static bool ConvertShiftJISToUTF8(const char * text, pfc::string8 & utf8);

/// <summary>
/// Initializes a new instance.
/// </summary>
pmd_decoder_t::pmd_decoder_t() :
    _FilePath(), _Data(), _Size(), _PMD(), _Length(), _LoopLength(), _TickCount(), _LoopTickCount(),
    _MaxLoopNumber(DefaultLoopCount), _FadeOutDuration(DefaultFadeOutDuration), _SampleRate(DefaultSynthesisRate)
{
    _Frames.set_count((t_size) BlockSize * ChannelCount);
}

/// <summary>
/// Deletes an instance.
/// </summary>
pmd_decoder_t::~pmd_decoder_t()
{
    delete[] _Data;
    delete _PMD;
}

/// <summary>
/// Reads the PMD data from memory.
/// </summary>
bool pmd_decoder_t::Open(const uint8_t * data, size_t size, uint32_t sampleRate, const char * filePath, const char * directoryPathDrums)
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

    _SampleRate = sampleRate;

    delete _PMD;
    _PMD = new pmd_driver_t();

    {
        WCHAR FilePath[MAX_PATH];
        WCHAR DirectoryPathDrums[MAX_PATH];

        {
            if (::MultiByteToWideChar(CP_UTF8, 0, filePath, -1, FilePath, _countof(FilePath)) == 0)
                return false;

            if (::MultiByteToWideChar(CP_UTF8, 0, directoryPathDrums, -1, DirectoryPathDrums, _countof(DirectoryPathDrums)) == 0)
                return false;
        }

        _PMD->Initialize(DirectoryPathDrums);
        _PMD->SetSampleRate(_SampleRate);

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
                    Paths.push_back(DirectoryPathDrums);

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

        {
            std::wstring FileName = _PMD->GetPCMFileName();

            if (!FileName.empty())
                _PCMFileName = pfc::utf8FromWide(FileName.c_str());

            FileName = _PMD->GetPPSFileName();

            if (!FileName.empty())
                _PPSFileName = pfc::utf8FromWide(FileName.c_str());

            FileName = _PMD->GetPPZFileName(0);

            if (!FileName.empty())
                _PPZFileName1 = pfc::utf8FromWide(FileName.c_str());

            FileName = _PMD->GetPPZFileName(1);

            if (!FileName.empty())
                _PPZFileName2 = pfc::utf8FromWide(FileName.c_str());
        }
    }

    return true;
}

/// <summary>
/// Returns true if  the buffer points to PMD data.
/// </summary>
bool pmd_decoder_t::IsPMD(const uint8_t * data, size_t size) const noexcept
{
    return pmd_driver_t::IsPMD(data, size);
}

/// <summary>
/// Initializes the decoder.
/// </summary>
void pmd_decoder_t::Initialize() noexcept
{
    _PMD->UsePPSForDrums(CfgUsePPS);
    _PMD->UseSSGForDrums(CfgUseSSG);

    _PMD->Start();
/*
    for (int i = 0; i < MaxChannels; ++i)
        _PMD->DisableChannel(i);

    _PMD->EnableChannel(16); // PPZ 1
*/
    console::printf("PMDDecoder: ADPCM ROM %sloaded.", (_PMD->HasADPCMROM() ? "" : "not "));
    console::printf("PMDDecoder: Percussion samples %sloaded.", (_PMD->HasPercussionSamples() ? "" : "not "));

    if (!_PCMFileName.empty())
        console::printf("PMDDecoder: Requires PCM samples from \"%s\": %sfound.", _PCMFileName.c_str(), (_PMD->GetPCMFilePath().empty() ? "not " : ""));

    if (!_PPSFileName.empty())
        console::printf("PMDDecoder: Requires PPS samples from \"%s\": %sfound.", _PPSFileName.c_str(), (_PMD->GetPPSFilePath().empty() ? "not " : ""));

    if (!_PPZFileName1.empty())
        console::printf("PMDDecoder: Requires PPZ samples from \"%s\": %sfound.", _PPZFileName1.c_str(), (_PMD->GetPPZFilePath(0).empty() ? "not " : ""));

    if (!_PPZFileName2.empty())
        console::printf("PMDDecoder: Requires PPZ samples from \"%s\": %sfound.", _PPZFileName2.c_str(), (_PMD->GetPPZFilePath(1).empty() ? "not " : ""));
}

/// <summary>
/// Renders a chunk of audio samples.
/// </summary>
size_t pmd_decoder_t::Render(audio_chunk & audioChunk, size_t sampleCount) noexcept
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

    _PMD->Render(&_Frames[0], BlockSize);

    audioChunk.set_data_fixedpoint(&_Frames[0], (t_size) BlockSize * ((t_size) BitsPerSample / 8 * ChannelCount), SampleRate, ChannelCount, BitsPerSample, audio_chunk::g_guess_channel_config(ChannelCount));

    return sampleCount;
}

/// <summary>
/// Gets the current decoding position (in ms).
/// </summary>
uint32_t pmd_decoder_t::GetPosition() const noexcept
{
    return _PMD->GetPosition();
}

/// <summary>
/// Sets the current decoding position (in ms).
/// </summary>
void pmd_decoder_t::SetPosition(uint32_t value) const noexcept
{
    _PMD->SetPosition(value);
};

/// <summary>
/// Gets the number of the current loop. 0 if not looped yet.
/// </summary>
uint32_t pmd_decoder_t::GetLoopNumber() const noexcept
{
    return (uint32_t) _PMD->GetLoopNumber();
}

/// <summary>
/// Returns true if a track is being decoded.
/// </summary>
/// <returns></returns>
bool pmd_decoder_t::IsBusy() const noexcept
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
