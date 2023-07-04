
/** $VER: PMDDecoder.cpp (2023.04.07) **/

#pragma warning(disable: 5045)

#include "PMDDecoder.h"

#define _CRT_SECURE_NO_WARNINGS

#include <string.h>

PMDDecoder::PMDDecoder()
{
    WCHAR CurrentDirectory[] = L".";

    ::pmdwininit(CurrentDirectory);
    ::setpcmrate(SOUND_55K);
}

bool PMDDecoder::IsPMD(const std::vector<uint8_t> data) const
{
    if (data.size() < 3)
        return false;

    if (data[0] > 0x0F)
        return false;

    if (data[1] != 0x18 && data[1] != 0x1A)
        return false;

    if (data[2] != 0x00 && data[2] != 0xE6)
        return false;

    return true;
}

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

bool PMDDecoder::Read(std::vector<uint8_t> data, const WCHAR * filePath, const WCHAR * pdxSamplesPath)
{
    if (!IsPMD(data))
        return false;

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
    {
        _LengthInMS = 0;
        _LoopInMS = 0;
    }

    {
        _Title[0] = 0;

        int rc = ::fgetmemo3(_Title, _FilePath, 1);

        _Composer[0] = 0;

        rc = ::fgetmemo3(_Composer, _FilePath, 2);

        _Arranger[0] = 0;

        rc =::fgetmemo3(_Arranger, _FilePath, 3);

        _Memo[0] = 0;

        rc = ::fgetmemo3(_Memo, _FilePath, 4);
    }

//  ::getppsfilename("")
//  ::ppc_load("")
//  ::setrhythmwithssgeffect(true); // true == SSG+RHY, false == SSG
//  ::setppsuse(true); // PSSDRV FLAG set false at init. true == use PPS, false == do not use PPS

    if (::music_load(_FilePath) != PMDWIN_OK)
        return false;

    ::music_start();

    _OpenWork = ::getopenwork();

    return true;
}

size_t PMDDecoder::Run(audio_sample * samples, size_t sampleCount)
{
    ::memset(samples, 0, sampleCount * GetChannelCount() * sizeof(audio_sample));

    return sampleCount;
}
