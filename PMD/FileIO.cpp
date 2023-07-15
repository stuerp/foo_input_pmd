
// Based on PMDWin code by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#define _CRT_SECURE_NO_WARNINGS

#include "FileIO.h"

#include <WCHAR.h>
#include <string.h>

size_t FilePath::Strlen(const WCHAR * str)
{
    return wcslen(str);
}

int FilePath::Strcmp(const WCHAR * str1, const WCHAR * str2)
{
    return ::wcscmp(str1, str2);
}

int FilePath::Strncmp(const WCHAR * str1, const WCHAR * str2, size_t size)
{
    return ::wcsncmp(str1, str2, size);
}

int FilePath::Stricmp(const WCHAR * str1, const WCHAR * str2)
{
    return ::_wcsicmp(str1, str2);
}

int FilePath::Strnicmp(const WCHAR * str1, const WCHAR * str2, size_t size)
{
    return ::_wcsnicmp(str1, str2, size);
}

WCHAR * FilePath::Strcpy(WCHAR * dest, const WCHAR * src)
{
    return ::wcscpy(dest, src);
}

WCHAR * FilePath::Strncpy(WCHAR * dest, const WCHAR * src, size_t size)
{
    return ::wcsncpy(dest, src, size);
}

WCHAR * FilePath::Strcat(WCHAR * dest, const WCHAR * src)
{
    return ::wcscat(dest, src);
}

WCHAR * FilePath::Strncat(WCHAR * dest, const WCHAR * src, size_t count)
{
    return ::wcsncat(dest, src, count);
}

WCHAR * FilePath::Strchr(const WCHAR * str, WCHAR c)
{
    return (WCHAR *) ::wcschr((WCHAR *) str, c);
}

WCHAR * FilePath::Strrchr(const WCHAR * str, WCHAR c)
{
    return (WCHAR *) ::wcsrchr((WCHAR *) str, c);
}

WCHAR * FilePath::AddDelimiter(WCHAR * str)
{
    if (Strrchr(str, '\\') != &str[Strlen(str) - 1])
        Strcat(str, L"\\");

    return str;
}

void FilePath::Splitpath(const WCHAR * path, WCHAR * drive, WCHAR * dir, WCHAR * fname, WCHAR * ext)
{
    ::_wsplitpath(path, drive, dir, fname, ext);
}

void FilePath::Makepath(WCHAR * path, const WCHAR * drive, const WCHAR * dir, const WCHAR * fname, const WCHAR * ext)
{
    ::_wmakepath(path, drive, dir, fname, ext);
}

void FilePath::Makepath_dir_filename(WCHAR * path, const WCHAR * dir, const WCHAR * filename)
{
    Strcpy(path, dir);

    if (Strrchr(dir, '\\') != &dir[Strlen(dir) - 1] && Strlen(path) > 0)
        Strcat(path, L"\\");

    Strcat(path, filename);
}

void FilePath::Extractpath(WCHAR * dest, const WCHAR * src, uint32_t flg)
{
    WCHAR  drive[_MAX_PATH];
    WCHAR  dir[_MAX_PATH];
    WCHAR  filename[_MAX_PATH];
    WCHAR  ext[_MAX_PATH];
    WCHAR * pdrive;
    WCHAR * pdir;
    WCHAR * pfilename;
    WCHAR * pext;

    *dest = '\0';

    if (flg & extractpath_drive)
        pdrive = drive;
    else
        pdrive = NULL;

    if (flg & extractpath_dir)
        pdir = dir;
    else
        pdir = NULL;

    if (flg & extractpath_filename)
        pfilename = filename;
    else
        pfilename = NULL;

    if (flg & extractpath_ext)
        pext = ext;
    else
        pext = NULL;

    Splitpath(src, pdrive, pdir, pfilename, pext);
    Makepath(dest, pdrive, pdir, pfilename, pext);
}

int FilePath::Comparepath(WCHAR * filename1, const WCHAR * filename2, uint32_t flg)
{
    WCHAR  extfilename1[_MAX_PATH];
    WCHAR  extfilename2[_MAX_PATH];

    Extractpath(extfilename1, filename1, flg);
    Extractpath(extfilename2, filename2, flg);

    return Stricmp(extfilename1, extfilename2);
}

WCHAR * FilePath::ExchangeExt(WCHAR * dest, WCHAR * src, const WCHAR * ext)
{
    WCHAR  drive2[_MAX_PATH];
    WCHAR  dir2[_MAX_PATH];
    WCHAR  filename2[_MAX_PATH];
    WCHAR  ext2[_MAX_PATH];

    Splitpath(src, drive2, dir2, filename2, ext2);
    Makepath(dest, drive2, dir2, filename2, ext);

    return dest;
}

WCHAR * FilePath::CharToWCHAR(WCHAR * dest, const char * src)
{
    ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, src, -1, dest, strlen(src));

    return dest;
}

WCHAR * FilePath::CharToWCHARn(WCHAR * dest, const char * src, size_t count)
{
    dest[0] = '\0';

    ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, src, -1, dest, count);
    return dest;
}

FileIO::FileIO()
{
    flags = 0;
    lorigin = 0;
    error = error_success;
    path = new WCHAR[_MAX_PATH];

    hfile = 0;
}

FileIO::~FileIO()
{
    Close();
    delete[] path;
}

// Get size of file indicated by filename
int64_t FileIO::GetFileSize(const WCHAR * filename)
{
    WIN32_FIND_DATA fd;

    HANDLE handle = FindFirstFile(filename, &fd);

    if (handle == INVALID_HANDLE_VALUE)
        return -1;

    ::FindClose(handle);

    return (((int64_t) fd.nFileSizeHigh) << 32) + fd.nFileSizeLow;
}

bool FileIO::Open(const WCHAR * filename, uint32_t flg)
{
    FilePath filepath;

    Close();

    filepath.Strncpy(path, filename, _MAX_PATH);

    uint32_t access = (flg & flags_readonly ? 0 : GENERIC_WRITE) | GENERIC_READ;
    uint32_t share = (flg & flags_readonly) ? FILE_SHARE_READ : 0;
    uint32_t creation = flg & flags_create ? CREATE_ALWAYS : OPEN_EXISTING;

    hfile = CreateFile(filename, access, share, 0, creation, 0, 0);

    flags = (flg & flags_readonly) | (hfile == INVALID_HANDLE_VALUE ? 0 : flags_open);

    if (!(flags & flags_open))
    {
        switch (GetLastError())
        {
            case ERROR_FILE_NOT_FOUND:    error = error_file_not_found; break;
            case ERROR_SHARING_VIOLATION:  error = error_sharing_violation; break;
            default: error = error_unknown; break;
        }
    }

    SetLogicalOrigin(0);

    return !!(flags & flags_open);
}

void FileIO::Close()
{
    if (GetFlags() & flags_open)
    {
        CloseHandle(hfile);
        hfile = 0;
        flags = 0;
    }
}

int32_t FileIO::Read(void * dest, int32_t size)
{
    if (!(GetFlags() & flags_open))
        return -1;

    DWORD readsize;

    if (!::ReadFile(hfile, dest, size, &readsize, 0))
        return -1;

    return readsize;
}

bool FileIO::Seek(int32_t pos, SeekMethod method)
{
    if (!(GetFlags() & flags_open))
        return false;

    uint32_t wmethod;

    switch (method)
    {
        case seekmethod_begin:
            wmethod = FILE_BEGIN; pos += lorigin;
            break;

        case seekmethod_current:
            wmethod = FILE_CURRENT;
            break;

        case seekmethod_end:
            wmethod = FILE_END;
            break;

        default:
            return false;
    }

    return 0xffffffff != SetFilePointer(hfile, pos, 0, wmethod);
}
