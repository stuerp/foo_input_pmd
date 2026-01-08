
/** $VER: WAVEReader.h (2026.01.07) P. Stuer **/

#pragma once

#include "RIFFReader.h"

/// <summary>
/// Implements a reader for a RIFF WAVE file.
/// </summary>
class WAVEReader : public RIFFReader
{
public:
    WAVEReader() : RIFFReader(), _Format(), _Data(), _Size()
    {
    }

    virtual ~WAVEReader()
    {
        Reset();
    }

    virtual void Reset()
    {
        delete[] _Data;
        _Data = nullptr;
        _Size = 0;
    }

    uint16_t Format() const noexcept { return _Format.Format; }
    uint16_t ChannelCount() const noexcept { return _Format.ChannelCount; }
    uint32_t SampleRate() const noexcept { return _Format.SampleRate; }
    uint32_t AvgBytesPerSec() const noexcept { return _Format.AvgBytesPerSec; }
    uint16_t BlockAlign() const noexcept { return _Format.BlockAlign; }
    uint16_t BitsPerSample() const noexcept { return _Format.BitsPerSample; }

    const uint8_t * Data() const noexcept { return _Data; }
    uint32_t Size() const noexcept { return _Size; }

    virtual bool HandleChunk(uint32_t chunkId, uint32_t chunkSize)
    {
        switch (chunkId)
        {
            case FOURCC_FMT:
                return HandleFMT(chunkSize);

            case FOURCC_DATA:
                return HandleDATA(chunkSize);

            default:
                return RIFFReader::HandleChunk(chunkId, chunkSize);
        }
    }

    bool HandleFMT(uint32_t chunkSize)
    {
        if (chunkSize < sizeof(ChunkFMT))
            return false;

        DWORD Size = chunkSize + (chunkSize & 1);

        DWORD BytesRead;

        BOOL Success = ::ReadFile(_hFile, &_Format, Size, &BytesRead, nullptr);

        if (!(Success && (BytesRead == Size)))
            return false;

        return true;
    }

    bool HandleDATA(uint32_t chunkSize)
    {
        _Data = new uint8_t[chunkSize];
        _Size = chunkSize;

        DWORD BytesRead;

        if (!(::ReadFile(_hFile, _Data, (DWORD) _Size, &BytesRead, nullptr) && (BytesRead == (DWORD) _Size)))
            return false;

        if ((chunkSize & 1) == 0)
            return true;

        return (::SetFilePointer(_hFile, 1, nullptr, FILE_CURRENT) == 1);
    }

private:
    ChunkFMT _Format;
    uint8_t * _Data;
    uint32_t _Size;
};
