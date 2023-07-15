#pragma once

#include <Windows.h>

#include <stdint.h>

typedef unsigned int uint;

class IFILEIO
{
public:
    enum Flags
    {
        flags_open = 0x000001,
        flags_readonly = 0x000002,
        flags_create = 0x000004,
    };

    enum SeekMethod
    {
        seekmethod_begin = 0, seekmethod_current = 1, seekmethod_end = 2,
    };

    enum Error
    {
        error_success = 0,
        error_file_not_found,
        error_sharing_violation,
        error_unknown = -1
    };

    virtual int64_t GetFileSize(const TCHAR * filename) = 0;
    virtual bool Open(const TCHAR * filename, uint flg = 0) = 0;
    virtual void Close() = 0;
    virtual int32_t Read(void * dest, int32_t len) = 0;
    virtual bool Seek(int32_t fpos, SeekMethod method) = 0;
    virtual int32_t Tellp() = 0;
};
