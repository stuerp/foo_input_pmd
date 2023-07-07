
/** $VER: InputDecoder.h (2023.07.07) **/

#pragma once

#define NOMINMAX

#pragma warning(disable: 5045)

#include <sdk/foobar2000-lite.h>
#include <sdk/input_impl.h>
#include <sdk/file_info_impl.h>
#include <sdk/tag_processor.h>

#include "Resources.h"

#include "PMDDecoder.h"

/// <summary>
/// Implements an input decoder.
/// </summary>
#pragma warning(disable: 4820) // x bytes padding added after last data member
class InputDecoder : public input_stubs
{
public:
    InputDecoder() noexcept :
        _FilePath(), _FileStats(), _FileStats2(),
        _SampleRate(DefaultSampleRate),
        _Decoder(),
        _SamplesRendered(),
        _TimeInMS()
    {
    }

    InputDecoder(const InputDecoder&) = delete;
    InputDecoder(const InputDecoder&&) = delete;
    InputDecoder& operator=(const InputDecoder&) = delete;
    InputDecoder& operator=(InputDecoder&&) = delete;

    ~InputDecoder() noexcept
    {
    }

public:
    #pragma region("input_impl")
    void open(service_ptr_t<file> file, const char * filePath, t_input_open_reason, abort_callback & abortHandler);

    static bool g_is_our_content_type(const char * contentType)
    {
        return ::_stricmp(contentType, "audio/pmd") == 0;
    }

    static bool g_is_our_path(const char *, const char * extension)
    {
        return (::_stricmp(extension, "m") == 0) || (::_stricmp(extension, "m2") == 0);
    }

    static GUID g_get_guid()
    {
        static const GUID Guid = COMPONENT_GUID;

        return Guid;
    }

    static const char * g_get_name()
    {
        return STR_COMPONENT_NAME;
    }

    static GUID g_get_preferences_guid()
    {
        static const GUID guid = COMPONENT_PREFERENCES_GUID;

        return guid;
    }

    static bool g_is_low_merit()
    {
        return false;
    }
    #pragma endregion

    #pragma region("input_info_reader")
    unsigned get_subsong_count()
    {
        return 1;
    }

    t_uint32 get_subsong(unsigned subSongIndex)
    {
        return subSongIndex;
    }

    void get_info(t_uint32 subsongIndex, file_info & fileInfo, abort_callback & abortHandler);
    #pragma endregion

    #pragma region("input_info_reader_v2")
    t_filestats2 get_stats2(uint32_t, abort_callback &)
    {
        return _FileStats2;
    }

    t_filestats get_file_stats(abort_callback &)
    {
        return _FileStats;
    }
    #pragma endregion

    #pragma region("input_info_writer")
    void retag_set_info(t_uint32, const file_info & fileInfo, abort_callback & abortHandler);

    void retag_commit(abort_callback &) { }

    void remove_tags(abort_callback &) { }
    #pragma endregion

    #pragma region("input_decoder")
    void decode_initialize(unsigned subsongIndex, unsigned flags, abort_callback & abortHandler);

    bool decode_run(audio_chunk & audioChunk, abort_callback & abortHandler);

    void decode_seek(double p_seconds, abort_callback &);

    bool decode_can_seek() { return true; }

    bool decode_get_dynamic_info(file_info & fileInfo, double & timestampDelta);

    bool decode_get_dynamic_info_track(file_info &, double &) noexcept { return false; }

    void decode_on_idle(abort_callback &) noexcept { }
    #pragma endregion

private:
    pfc::string8 _FilePath;
    t_filestats _FileStats;
    t_filestats2 _FileStats2;

    uint32_t _SampleRate;

    PMDDecoder * _Decoder;

    uint64_t _SamplesRendered;
    uint32_t _TimeInMS;

    static const uint32_t DefaultSampleRate = 44100;
};
#pragma warning(default: 4820) // x bytes padding added after last data member
