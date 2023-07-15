﻿#pragma once

#include "ifileio.h"

class FilePath
{
public:
    FilePath() { }

    size_t	Strlen(const TCHAR * str);

    int		Strcmp(const TCHAR * str1, const TCHAR * str2);
    int		Strncmp(const TCHAR * str1, const TCHAR * str2, size_t size);
    int		Stricmp(const TCHAR * str1, const TCHAR * str2);
    int		Strnicmp(const TCHAR * str1, const TCHAR * str2, size_t size);

    TCHAR * Strcpy(TCHAR * dest, const TCHAR * src);
    TCHAR * Strncpy(TCHAR * dest, const TCHAR * src, size_t size);
    TCHAR * Strcat(TCHAR * dest, const TCHAR * src);
    TCHAR * Strncat(TCHAR * dest, const TCHAR * src, size_t count);

    TCHAR * Strchr(const TCHAR * str, TCHAR c);
    TCHAR * Strrchr(const TCHAR * str, TCHAR c);
    TCHAR * AddDelimiter(TCHAR * str);

    void Extractpath(TCHAR * dest, const TCHAR * src, uint flg);
    int Comparepath(TCHAR * filename1, const TCHAR * filename2, uint flg);
    void Makepath(TCHAR * path, const TCHAR * drive, const TCHAR * dir, const TCHAR * fname, const TCHAR * ext);
    void Makepath_dir_filename(TCHAR * path, const TCHAR * dir, const TCHAR * filename);
    TCHAR * ExchangeExt(TCHAR * dest, TCHAR * src, const TCHAR * ext);

    TCHAR * CharToTCHAR(TCHAR * dest, const char * src);
    TCHAR * CharToTCHARn(TCHAR * dest, const char * src, size_t count);

    enum Extractpath_Flags
    {
        extractpath_null = 0x000000,
        extractpath_drive = 0x000001,
        extractpath_dir = 0x000002,
        extractpath_filename = 0x000004,
        extractpath_ext = 0x000008,
    };

protected:
    void Splitpath(const TCHAR * path, TCHAR * drive, TCHAR * dir, TCHAR * fname, TCHAR * ext);

private:
    FilePath(const FilePath &);
};

class FileIO : public IFILEIO
{
public:
    FileIO();
    virtual ~FileIO();

    int64_t WINAPI GetFileSize(const TCHAR * filename);
    bool WINAPI Open(const TCHAR * filename, uint flg);
    bool WINAPI CreateNew(const TCHAR * filename);
    bool WINAPI Reopen(uint flg = 0);
    void WINAPI Close();

    Error WINAPI GetError() { return error; }

    int32_t WINAPI Read(void * dest, int32_t len);
    int32_t WINAPI Write(const void * src, int32_t len);
    bool WINAPI Seek(int32_t fpos, SeekMethod method);
    int32_t WINAPI Tellp();
    bool WINAPI SetEndOfFile();

    uint WINAPI GetFlags() { return flags; }
    void WINAPI SetLogicalOrigin(int32_t origin) { lorigin = origin; }

private:
    uint flags;
    uint32_t lorigin;
    Error error;
    TCHAR * path;

    HANDLE hfile;

    FileIO(const FileIO &);
};
