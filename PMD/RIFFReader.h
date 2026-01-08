
/** $VER: RIFFReader.h (2026.01.07) P. Stuer **/

#pragma once

#include <windows.h>

#include <stdio.h>
#include <stdint.h>

#define FOURCC_WAVE mmioFOURCC('W','A','V','E')

#define FOURCC_FMT  mmioFOURCC('f','m','t',' ')
#define FOURCC_FACT mmioFOURCC('f','a','c','t')
#define FOURCC_DATA mmioFOURCC('d','a','t','a')

#define TRACE_CHUNK(id, size) { char s[5]; ::memcpy(&s, &id, 4); s[4] = 0; printf("%s %8d\n", s, size); }

struct ChunkHeader
{
    uint32_t ID;
    uint32_t Size;
};

struct ChunkFMT
{
    uint16_t Format;
    uint16_t ChannelCount;
    uint32_t SampleRate;
    uint32_t AvgBytesPerSec;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;
};

struct ChunkFMTEx
{
    ChunkFMT Fmt;

    uint16_t ext_size;
    uint16_t valid_bits_per_sample;
    uint32_t channel_mask;

    uint8_t sub_format[16];
};

/// <summary>
/// Implements a reader for a RIFF file.
/// </summary>
class RIFFReader
{
public:
    RIFFReader() : _hFile(INVALID_HANDLE_VALUE)
    {
    }

    virtual ~RIFFReader()
    {
    }

    virtual bool Open(const WCHAR * filePath)
    {
        if (filePath == nullptr)
            return false;

        _hFile = ::CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);

        if (_hFile == INVALID_HANDLE_VALUE)
            return false;

        return ReadChunks();
    }

    virtual void Close() noexcept
    {
        if (_hFile != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(_hFile);
            _hFile = INVALID_HANDLE_VALUE;
        }
    }

    virtual bool HandleChunk(uint32_t, uint32_t chunkSize)
    {
        DWORD Size = chunkSize + (chunkSize & 1);

        return (::SetFilePointer(_hFile, (LONG) Size, nullptr, FILE_CURRENT) != INVALID_SET_FILE_POINTER);
    }

private:
    bool ReadChunks()
    {
        ChunkHeader ck;
        DWORD BytesRead;

        BOOL Success = ::ReadFile(_hFile, &ck, sizeof(ck), &BytesRead, nullptr);

        while (Success && (BytesRead == sizeof(ck)) && (ck.Size != 0))
        {
            TRACE_CHUNK(ck.ID, ck.Size);

            switch (ck.ID)
            {
                case FOURCC_RIFF:
                {
                    if (!HandleRIFF())
                        return false;
                    break;
                }

                case FOURCC_LIST:
                {
                    if (!HandleLIST())
                        return false;
                    break;
                }

                default:
                {
                    if (!HandleChunk(ck.ID, ck.Size))
                        return false;
                }
            }

            Success = ::ReadFile(_hFile, &ck, sizeof(ck), &BytesRead, nullptr);
        }

        return true;
    }

    bool HandleRIFF()
    {
        uint32_t RIFFType;
        DWORD BytesRead;

        BOOL Success = ::ReadFile(_hFile, &RIFFType, sizeof(RIFFType), &BytesRead, nullptr);

        if (!(Success && (BytesRead == sizeof(RIFFType))))
            return false;

        { char s[5]; ::memcpy(&s, &RIFFType, 4); s[4] = 0; printf("%s\n", s); }

        return (RIFFType == FOURCC_WAVE);
    }

    bool HandleLIST()
    {
        uint32_t ListType;
        DWORD BytesRead;

        BOOL Success = ::ReadFile(_hFile, &ListType, sizeof(ListType), &BytesRead, nullptr);

        if (!(Success && (BytesRead == sizeof(ListType))))
            return false;

        { char s[5]; ::memcpy(&s, &ListType, 4); s[4] = 0; printf("%s\n", s); }

        return true;
    }

protected:
    HANDLE _hFile;
};
