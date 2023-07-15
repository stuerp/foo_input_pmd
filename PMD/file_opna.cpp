#define _CRT_SECURE_NO_WARNINGS

#include "file_opna.h"

#include <tchar.h>
#include <string.h>

//	文字列の長さを取得
size_t FilePath::Strlen(const TCHAR * str)
{
#ifdef _MBCS
    return strlen(str);
#endif

#ifdef _UNICODE
    return wcslen(str);
#endif
}

//	文字列を比較
int FilePath::Strcmp(const TCHAR * str1, const TCHAR * str2)
{
    return _tcscmp(str1, str2);
}

//	サイズを指定して文字列を比較
int FilePath::Strncmp(const TCHAR * str1, const TCHAR * str2, size_t size)
{
#ifdef _MBCS
    return strncmp(str1, str2, size);
#endif

#ifdef _UNICODE
    return wcsncmp(str1, str2, size);
#endif
}

//	大文字、小文字を同一視して文字列を比較
int FilePath::Stricmp(const TCHAR * str1, const TCHAR * str2)
{
    return _tcsicmp(str1, str2);
}

//	大文字、小文字を同一視、サイズを指定して文字列を比較
int FilePath::Strnicmp(const TCHAR * str1, const TCHAR * str2, size_t size)
{
#ifdef _MBCS
    return _strnicmp(str1, str2, size);
#endif

#ifdef _UNICODE
    return _wcsnicmp(str1, str2, size);
#endif
}

//	文字列をコピー
TCHAR * FilePath::Strcpy(TCHAR * dest, const TCHAR * src)
{
    return _tcscpy(dest, src);
}

//	長さを指定して文字列をコピー
TCHAR * FilePath::Strncpy(TCHAR * dest, const TCHAR * src, size_t size)
{
#ifdef _MBCS
    return strncpy(dest, src, size);
#endif

#ifdef _UNICODE
    return wcsncpy(dest, src, size);
#endif
}

//	文字列を追加
TCHAR * FilePath::Strcat(TCHAR * dest, const TCHAR * src)
{
    return _tcscat(dest, src);
}

//	文字数を指定して文字列を追加
TCHAR * FilePath::Strncat(TCHAR * dest, const TCHAR * src, size_t count)
{
#ifdef _MBCS
    return strncat(dest, src, count);
#endif

#ifdef _UNICODE
    return wcsncat(dest, src, count);
#endif
}

//	指定された文字の最初の出現箇所を検索
TCHAR * FilePath::Strchr(const TCHAR * str, TCHAR c)
{
#ifdef _MBCS
    return (TCHAR *) _mbschr((const unsigned char *) str, c);
#endif

#ifdef _UNICODE
    return (TCHAR *) wcschr((TCHAR *) str, c);
#endif
}

//	指定された文字の最後の出現箇所を検索
TCHAR * FilePath::Strrchr(const TCHAR * str, TCHAR c)
{
#ifdef _MBCS
    return (TCHAR *) _mbsrchr((const unsigned char *) str, c);
#endif

#ifdef _UNICODE
    return (TCHAR *) wcsrchr((TCHAR *) str, c);
#endif
}

//	文字列の末尾に「\」を付与
TCHAR * FilePath::AddDelimiter(TCHAR * str)
{
    if (Strrchr(str, _T('\\')) != &str[Strlen(str) - 1])
    {
        Strcat(str, _T("\\"));
    }
    return str;
}

//	ファイル名を分割
void FilePath::Splitpath(const TCHAR * path, TCHAR * drive, TCHAR * dir, TCHAR * fname, TCHAR * ext)
{
    _tsplitpath(path, drive, dir, fname, ext);
}

//	ファイル名を合成
void FilePath::Makepath(TCHAR * path, const TCHAR * drive, const TCHAR * dir, const TCHAR * fname, const TCHAR * ext)
{
    _tmakepath(path, drive, dir, fname, ext);
}

//	ファイル名を合成(ディレクトリ＋ファイル名)
void FilePath::Makepath_dir_filename(TCHAR * path, const TCHAR * dir, const TCHAR * filename)
{
    Strcpy(path, dir);

    if (Strrchr(dir, _T('\\')) != &dir[Strlen(dir) - 1] && Strlen(path) > 0)
    {
        Strcat(path, _T("\\"));
    }

    Strcat(path, filename);
}

//	ファイルパスから指定された要素を抽出
void FilePath::Extractpath(TCHAR * dest, const TCHAR * src, uint flg)
{
    TCHAR	drive[_MAX_PATH];
    TCHAR	dir[_MAX_PATH];
    TCHAR	filename[_MAX_PATH];
    TCHAR	ext[_MAX_PATH];
    TCHAR * pdrive;
    TCHAR * pdir;
    TCHAR * pfilename;
    TCHAR * pext;

    *dest = '\0';

    if (flg & extractpath_drive)
        pdrive = drive;
    else
        pdrive = NULL;

    if (flg & extractpath_dir)
    {
        pdir = dir;
    }
    else
    {
        pdir = NULL;
    }

    if (flg & extractpath_filename)
    {
        pfilename = filename;
    }
    else
    {
        pfilename = NULL;
    }

    if (flg & extractpath_ext)
    {
        pext = ext;
    }
    else
    {
        pext = NULL;
    }

    Splitpath(src, pdrive, pdir, pfilename, pext);
    Makepath(dest, pdrive, pdir, pfilename, pext);
}

//	ファイルパスから指定された要素を比較
int FilePath::Comparepath(TCHAR * filename1, const TCHAR * filename2, uint flg)
{
    TCHAR	extfilename1[_MAX_PATH];
    TCHAR	extfilename2[_MAX_PATH];

    Extractpath(extfilename1, filename1, flg);
    Extractpath(extfilename2, filename2, flg);

    return Stricmp(extfilename1, extfilename2);
}


//	拡張子を変更
TCHAR * FilePath::ExchangeExt(TCHAR * dest, TCHAR * src, const TCHAR * ext)
{
    TCHAR	drive2[_MAX_PATH];
    TCHAR	dir2[_MAX_PATH];
    TCHAR	filename2[_MAX_PATH];
    TCHAR	ext2[_MAX_PATH];

    Splitpath(src, drive2, dir2, filename2, ext2);
    Makepath(dest, drive2, dir2, filename2, ext);

    return dest;
}

//	char配列→TCHAR配列に変換
TCHAR * FilePath::CharToTCHAR(TCHAR * dest, const char * src)
{
#ifdef _MBCS
    return strcpy(dest, src);
#endif

#ifdef _UNICODE
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, src, -1, dest, strlen(src));
    return dest;
#endif
}

//	char配列→TCHAR配列に変換(文字数指定)
TCHAR * FilePath::CharToTCHARn(TCHAR * dest, const char * src, size_t count)
{
    dest[0] = '\0';
#ifdef _MBCS
    return Strncpy(dest, src, count);
#endif

#ifdef _UNICODE
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, src, -1, dest, count);
    return dest;
#endif
}

//	構築/消滅
FileIO::FileIO()
{
    flags = 0;
    lorigin = 0;
    error = error_success;
    path = new TCHAR[_MAX_PATH];

    hfile = 0;
}

FileIO::~FileIO()
{
    Close();
    delete[] path;
}

// Get size of file indicated by filename
int64_t FileIO::GetFileSize(const TCHAR * filename)
{
    HANDLE	handle;
    WIN32_FIND_DATA	FindFileData;

    if ((handle = FindFirstFile(filename, &FindFileData)) == INVALID_HANDLE_VALUE)
    {
        return -1;		// 取得不可
    }
    else
    {
        FindClose(handle);
        return (((int64_t) FindFileData.nFileSizeHigh) << 32) + FindFileData.nFileSizeLow;
    }
}

bool FileIO::Open(const TCHAR * filename, uint flg)
{
    FilePath	filepath;

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
            case ERROR_FILE_NOT_FOUND:		error = error_file_not_found; break;
            case ERROR_SHARING_VIOLATION:	error = error_sharing_violation; break;
            default: error = error_unknown; break;
        }
    }

    SetLogicalOrigin(0);

    return !!(flags & flags_open);
}

//	ファイルがない場合は作成
bool FileIO::CreateNew(const TCHAR * filename)
{
    FilePath	filepath;
    Close();

    filepath.Strncpy(path, filename, _MAX_PATH);

    uint32_t access = GENERIC_WRITE | GENERIC_READ;
    uint32_t share = 0;
    uint32_t creation = CREATE_NEW;

    hfile = CreateFile(filename, access, share, 0, creation, 0, 0);

    flags = (hfile == INVALID_HANDLE_VALUE ? 0 : flags_open);

    SetLogicalOrigin(0);

    return !!(flags & flags_open);
}

//	ファイルを作り直す
bool FileIO::Reopen(uint flg)
{
    if (!(flags & flags_open)) return false;

    if ((flags & flags_readonly) && (flg & flags_create)) return false;

    if (flags & flags_readonly) flg |= flags_readonly;

    Close();

    uint32_t access = (flg & flags_readonly ? 0 : GENERIC_WRITE) | GENERIC_READ;
    uint32_t share = flg & flags_readonly ? FILE_SHARE_READ : 0;
    uint32_t creation = flg & flags_create ? CREATE_ALWAYS : OPEN_EXISTING;

    hfile = CreateFile(path, access, share, 0, creation, 0, 0);

    flags = (flg & flags_readonly) | (hfile == INVALID_HANDLE_VALUE ? 0 : flags_open);

    SetLogicalOrigin(0);

    return !!(flags & flags_open);
}

//	ファイルを閉じる
void FileIO::Close()
{
    if (GetFlags() & flags_open)
    {
        CloseHandle(hfile);
        hfile = 0;
        flags = 0;
    }
}

//	ファイルからの読み出し
int32_t FileIO::Read(void * dest, int32_t size)
{
    if (!(GetFlags() & flags_open))
        return -1;

    DWORD readsize;

    if (!ReadFile(hfile, dest, size, &readsize, 0))
        return -1;

    return readsize;
}

//	ファイルへの書き出し
int32_t FileIO::Write(const void * dest, int32_t size)
{
    if (!(GetFlags() & flags_open) || (GetFlags() & flags_readonly))
        return -1;

    DWORD writtensize;

    if (!WriteFile(hfile, dest, size, &writtensize, 0))
        return -1;

    return writtensize;
}

//	ファイルをシーク
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

//	ファイルの位置を得る
int32_t FileIO::Tellp()
{
    if (!(GetFlags() & flags_open))
        return 0;

    return SetFilePointer(hfile, 0, 0, FILE_CURRENT) - lorigin;
}

//	現在の位置をファイルの終端とする
bool FileIO::SetEndOfFile()
{
    if (!(GetFlags() & flags_open))
        return false;

    return ::SetEndOfFile(hfile) != 0;
}
