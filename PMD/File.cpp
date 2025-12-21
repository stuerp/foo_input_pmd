
// $VER: File.cpp (2023.10.08) P. Stuer - File manipulation

#include <pch.h>

#include "File.h"

#include <pathcch.h>

#pragma comment(lib, "pathcch")

bool HasExtension(const WCHAR * filePath, size_t size, const WCHAR * fileExtension)
{
    const WCHAR * FileExtension = nullptr;

    HRESULT hResult = ::PathCchFindExtension(filePath, size, &FileExtension);

    if (!SUCCEEDED(hResult))
        return false;

    return (FileExtension && (::_wcsicmp(FileExtension, fileExtension) == 0));
}

bool AddBackslash(WCHAR * filePath, size_t size)
{
    return SUCCEEDED(::PathCchAddBackslash(filePath, size));
}

bool RenameExtension(WCHAR * filePath, size_t size, const WCHAR * fileExtension)
{
    return SUCCEEDED(::PathCchRenameExtension(filePath, size, fileExtension));
}

bool CombinePath(WCHAR * filePath, size_t size, const WCHAR * part1, const WCHAR * part2)
{
    return SUCCEEDED(::PathCchCombine(filePath, size, part1, part2));
}

int64_t File::GetFileSize(const WCHAR * filePath)
{
    WIN32_FIND_DATA fd;

    HANDLE Handle = ::FindFirstFileW(filePath, &fd);

    if (Handle == INVALID_HANDLE_VALUE)
        return -1;

    ::FindClose(Handle);

    return (int64_t) (((uint64_t) fd.nFileSizeHigh) << 32) + fd.nFileSizeLow;
}

bool File::Open(const WCHAR * filePath)
{
    Close();

    hFile = ::CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    return (hFile != INVALID_HANDLE_VALUE);
}

void File::Close()
{
    if (hFile != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }
}

int32_t File::Read(void * dest, uint32_t size)
{
    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    DWORD BytesRead;

    return (::ReadFile(hFile, dest, (DWORD) size, &BytesRead, 0) ? (int32_t) BytesRead : -1);
}

bool File::Seek(int32_t offset, SeekMethod method)
{
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    uint32_t Method;

    switch (method)
    {
        case SeekBegin:
            Method = FILE_BEGIN;
            break;

        case SeekCurrent:
            Method = FILE_CURRENT;
            break;

        case SeekEnd:
            Method = FILE_END;
            break;

        default:
            return false;
    }

    return (::SetFilePointer(hFile, offset, 0, Method) != INVALID_SET_FILE_POINTER);
}
