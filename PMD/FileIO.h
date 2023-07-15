
// Based on PMDWin code by C60

#pragma once

#include "ifileio.h"

class FilePath
{
public:
    FilePath() { }

    size_t  Strlen(const WCHAR * str);

    int    Strcmp(const WCHAR * str1, const WCHAR * str2);
    int    Strncmp(const WCHAR * str1, const WCHAR * str2, size_t size);
    int    Stricmp(const WCHAR * str1, const WCHAR * str2);
    int    Strnicmp(const WCHAR * str1, const WCHAR * str2, size_t size);

    WCHAR * Strcpy(WCHAR * dest, const WCHAR * src);
    WCHAR * Strncpy(WCHAR * dest, const WCHAR * src, size_t size);
    WCHAR * Strcat(WCHAR * dest, const WCHAR * src);
    WCHAR * Strncat(WCHAR * dest, const WCHAR * src, size_t count);

    WCHAR * Strchr(const WCHAR * str, WCHAR c);
    WCHAR * Strrchr(const WCHAR * str, WCHAR c);
    WCHAR * AddDelimiter(WCHAR * str);

    void Extractpath(WCHAR * dest, const WCHAR * src, unsigned int flg);
    int Comparepath(WCHAR * filename1, const WCHAR * filename2, unsigned int flg);
    void Makepath(WCHAR * path, const WCHAR * drive, const WCHAR * dir, const WCHAR * fname, const WCHAR * ext);
    void Makepath_dir_filename(WCHAR * path, const WCHAR * dir, const WCHAR * filename);
    WCHAR * ExchangeExt(WCHAR * dest, WCHAR * src, const WCHAR * ext);

    WCHAR * CharToWCHAR(WCHAR * dest, const char * src);
    WCHAR * CharToWCHARn(WCHAR * dest, const char * src, size_t count);

    enum Extractpath_Flags
    {
        extractpath_null = 0x000000,
        extractpath_drive = 0x000001,
        extractpath_dir = 0x000002,
        extractpath_filename = 0x000004,
        extractpath_ext = 0x000008,
    };

protected:
    void Splitpath(const WCHAR * path, WCHAR * drive, WCHAR * dir, WCHAR * fname, WCHAR * ext);

private:
    FilePath(const FilePath &);
};

#pragma warning(disable: 4820)
class FileIO : public IFileIO
{
public:
    FileIO();
    virtual ~FileIO();

    int64_t WINAPI GetFileSize(const WCHAR * filename);

    bool WINAPI Open(const WCHAR * filename, unsigned int flg);
    void WINAPI Close();

    Error WINAPI GetError() { return error; }

    int32_t WINAPI Read(void * dest, int32_t len);
    bool WINAPI Seek(int32_t fpos, SeekMethod method);

    uint32_t WINAPI GetFlags() { return flags; }
    void WINAPI SetLogicalOrigin(int32_t origin) { lorigin = origin; }

private:
    uint32_t flags;
    int32_t lorigin;
    Error error;
    WCHAR * path;

    HANDLE hfile;

    FileIO(const FileIO &);
};
