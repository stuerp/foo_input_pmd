
// Based on PMDWin code by C60

#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>

#include <stdint.h>
#include <string.h>

bool HasExtension(const WCHAR * filePath, size_t size, const WCHAR * fileExtension);
bool AddBackslash(WCHAR * filePath, size_t size);
bool RenameExtension(WCHAR * filePath, size_t size, const WCHAR * fileExtension);
bool CombinePath(WCHAR * filePath, size_t size, const WCHAR * part1, const WCHAR * part2);

#pragma warning(disable: 4820)
class File
{
public:
    enum SeekMethod
    {
        SeekBegin = 0, SeekCurrent = 1, SeekEnd = 2,
    };

public:
    File() : hFile(INVALID_HANDLE_VALUE) { }
    virtual ~File() { Close(); }

    int64_t GetFileSize(const WCHAR * filePath);

    bool Open(const WCHAR * filePath);
    void Close();

    int32_t Read(void * data, uint32_t size);
    bool Seek(int32_t offset, SeekMethod method);

private:
    HANDLE hFile;
};
