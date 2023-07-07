
/** $VER: PMDDecoder.cpp (2023.07.07) **/

#pragma warning(disable: 5045)

#include "PMDDecoder.h"

#include <sdk/foobar2000-lite.h>
#include <shared/audio_math.h>

#include <string.h>

static size_t GetDirectoryPath(const WCHAR * filePath, WCHAR * directoryPath, size_t Size);
static bool ConvertShiftJITo2UTF8(const char * text, pfc::string8 & utf8);

PMDDecoder::PMDDecoder() :
    _FilePath(), _Data(), _Size(), _LengthInMS(), _LoopInMS()
{
    WCHAR CurrentDirectory[] = L".";

    ::pmdwininit(CurrentDirectory);
    ::setpcmrate(SOUND_55K);

    _Samples = new int16_t[SampleCount * ChannelCount];
}

PMDDecoder::~PMDDecoder()
{
    delete[] _Samples;
}

/// <summary>
/// Returns true if  the buffer points to PMD data.
/// </summary>
bool PMDDecoder::IsPMD(const uint8_t * data, size_t size) const noexcept
{
    if (size < 3)
        return false;

    if (data[0] > 0x0F)
        return false;

    if (data[1] != 0x18 && data[1] != 0x1A)
        return false;

    if (data[2] != 0x00 && data[2] != 0xE6)
        return false;

    return true;
}

/// <summary>
/// Reads the PMD data from memory.
/// </summary>
bool PMDDecoder::Read(const uint8_t * data, size_t size, const WCHAR * filePath, const WCHAR * pdxSamplesPath)
{
    if (!IsPMD(data, size))
        return false;

    _Data = new uint8_t[size];

    if (_Data == nullptr)
        throw exception_io();

    ::memcpy(_Data, data, size);

    _Size = size;

    ::wcsncpy_s(_FilePath, _countof(_FilePath), filePath, ::wcslen(filePath));

    {
        WCHAR DirectoryPath[MAX_PATH];

        WCHAR * Parts[4] = { 0 };

        const WCHAR * CurrentDirectory = L"./";

        if (::GetDirectoryPath(_FilePath, DirectoryPath, _countof(DirectoryPath)) > 0)
        {
            Parts[0] = DirectoryPath;
            Parts[1] = (WCHAR *) pdxSamplesPath;
            Parts[2] = (WCHAR *) CurrentDirectory;
            Parts[3] = nullptr;
        }
        else
        {
            Parts[0] = (WCHAR *) CurrentDirectory;
            Parts[1] = (WCHAR *) pdxSamplesPath;
            Parts[2] = nullptr;
        }

        if (!::setpcmdir(Parts))
            return false;
    }

    if (!::getlength(_FilePath, (int *) &_LengthInMS, (int *) &_LoopInMS))
        return false;

    {
        char Note[1024] = { 0 };

        (void) ::getmemo3(Note, _Data, (int) _Size, 1);
        ConvertShiftJITo2UTF8(Note, _Title);

        Note[0] = '\0';

        (void) ::getmemo3(Note, _Data, (int) _Size, 2);
        ConvertShiftJITo2UTF8(Note, _Composer);

        Note[0] = '\0';

        (void) ::getmemo3(Note, _Data, (int) _Size, 3);
        ConvertShiftJITo2UTF8(Note, _Arranger);

        Note[0] = '\0';

        (void) ::getmemo3(Note, _Data, (int) _Size, 4);
        ConvertShiftJITo2UTF8(Note, _Memo);
    }

    return true;
}

void PMDDecoder::Initialize() const noexcept
{
//  ::getppsfilename("")
//  ::ppc_load("")
//  ::setrhythmwithssgeffect(true); // true == SSG+RHY, false == SSG
//  ::setppsuse(true); // PSSDRV FLAG set false at init. true == use PPS, false == do not use PPS

    if (::music_load2(_Data, (int) _Size) != PMDWIN_OK)
        return;

    ::music_start();
}

/// <summary>
/// Render a chunk of audio samples.
/// </summary>
size_t PMDDecoder::Render(audio_sample * samples, size_t sampleCount) const noexcept
{
    ::getpcmdata(_Samples, (int) SampleCount);

    audio_math::convert_from_int16(_Samples, SampleCount, samples, (audio_sample) 2.0);

    return sampleCount;
}

/// <summary>
/// Gets the directory path from the file path.
/// </summary>
static size_t GetDirectoryPath(const WCHAR * filePath, WCHAR * directoryPath, size_t Size)
{
    size_t Index = 0;

    const WCHAR * p = ::wcsrchr(filePath, '/');

    if (p)
    {
        Index = (size_t)(p - filePath);
        ::wcsncpy_s(directoryPath, Size, filePath, Index);
    }

    directoryPath[Index] = 0;

    return Index;
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
