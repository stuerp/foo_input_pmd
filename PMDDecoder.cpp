
/** $VER: PMDDecoder.cpp (2023.07.19) P. Stuer **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4710 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "PMDDecoder.h"
#include "Configuration.h"

#include "framework.h"

#include <shared/audio_math.h>
#include <pathcch.h>

#pragma comment(lib, "pathcch")

#pragma hdrstop

static bool ConvertShiftJITo2UTF8(const char * text, pfc::string8 & utf8);

/// <summary>
/// Initializes a new instance.
/// </summary>
PMDDecoder::PMDDecoder() :
    _FilePath(), _Data(), _Size(), _PMD(), _Length(), _LoopLength(), _EventCount(), _LoopEventCount(),
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
bool PMDDecoder::Open(const char * filePath, const char * pdxSamplesPath, const uint8_t * data, size_t size, uint32_t synthesisRate)
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

    _SynthesisRate = synthesisRate;

    delete _PMD;
    _PMD = new PMD();

    WCHAR FilePath[MAX_PATH];
    WCHAR PDXSamplesPath[MAX_PATH];

    {
        if (::MultiByteToWideChar(CP_UTF8, 0, filePath, -1, FilePath, _countof(FilePath)) == 0)
            return false;

        if (::MultiByteToWideChar(CP_UTF8, 0, pdxSamplesPath, -1, PDXSamplesPath, _countof(PDXSamplesPath)) == 0)
            return false;
    }

    {
        WCHAR DirectoryPath[MAX_PATH];

        ::wcsncpy_s(DirectoryPath, _countof(DirectoryPath), FilePath, ::wcslen(FilePath));
        
        if (!SUCCEEDED(::PathCchRemoveFileSpec(DirectoryPath, _countof(DirectoryPath))))
            return false;

        _PMD->Initialize(PDXSamplesPath);
        _PMD->SetSynthesisRate(_SynthesisRate);

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

    {
        PMD * pmd = new PMD();

        pmd->Initialize(PDXSamplesPath);
        pmd->SetSynthesisRate(_SynthesisRate);

        pmd->Load(data, size);

        if (!pmd->GetLength((int *) &_Length, (int *) &_LoopLength))
            return false;

        if (!pmd->GetLengthInEvents((int *) &_EventCount, (int *) &_LoopEventCount))
            return false;

        {
            char Note[1024] = { 0 };

            pmd->GetNote(data, size, 1, Note, _countof(Note));
            ConvertShiftJITo2UTF8(Note, _Title);

            Note[0] = '\0';
            pmd->GetNote(data, size, 2, Note, _countof(Note));
            ConvertShiftJITo2UTF8(Note, _Composer);

            Note[0] = '\0';
            pmd->GetNote(data, size, 3, Note, _countof(Note));
            ConvertShiftJITo2UTF8(Note, _Arranger);

            Note[0] = '\0';
            pmd->GetNote(data, size, 4, Note, _countof(Note));
            ConvertShiftJITo2UTF8(Note, _Memo);
        }

        delete pmd;
    }

    return true;
}

/// <summary>
/// Returns true if the buffer points to PMD data.
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
    if (_PMD->Load(_Data, _Size) != ERR_SUCCESS)
        return;

    _PMD->UsePPS(CfgUsePPS);
    _PMD->UseSSG(CfgUseSSG);
    _PMD->Start();

    console::printf("PMDDecoder: ADPCM ROM %s.", (_PMD->HasADPCMROM() ? "available" : "missing"));
    console::printf("PMDDecoder: Percussion samples %s.", (_PMD->HasPercussionSamples() ? "available" : "missing"));
}

/// <summary>
/// Renders a chunk of audio samples.
/// </summary>
size_t PMDDecoder::Render(audio_chunk & audioChunk, size_t sampleCount) noexcept
{
    uint32_t TotalEventCount = _EventCount;

    if ((CfgPlaybackMode == PlaybackModes::Loop) || (CfgPlaybackMode == PlaybackModes::LoopWithFadeOut))
        TotalEventCount += (_LoopEventCount * _MaxLoopNumber);

    if (!IsBusy() || ((CfgPlaybackMode != LoopForever) && (GetEventNumber() > TotalEventCount)))
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
/// Gets the number of the current event.
/// </summary>
uint32_t PMDDecoder::GetEventNumber() const noexcept
{
    return (uint32_t) _PMD->GetEventNumber();
}

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
    State * State = _PMD->GetState();

    return State->_IsPlaying;
}

/// <summary>
/// Converts a Shift-JIS character array to an UTF-8 string.
/// </summary>
static bool ConvertShiftJITo2UTF8(const char * text, pfc::string8 & utf8)
{
    utf8.clear();

    int Size = ::MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, text, -1, 0, 0);

    if (Size == 0)
        return false;

    WCHAR * Wide = new WCHAR[(size_t) Size];

    if (::MultiByteToWideChar(932, 0, text, -1, Wide, Size) != 0)
    {
        Size = ::WideCharToMultiByte(CP_UTF8, 0, Wide, -1, 0, 0, 0, 0);

        if (Size != 0)
        {
            char * UTF8 = new char[(size_t) Size + 16];

            if (::WideCharToMultiByte(CP_UTF8, 0, Wide, -1, UTF8, Size, 0, 0) != 0)
                utf8 = UTF8;

            delete[] UTF8;
        }

        delete[] Wide;
    }

    return true;
}
