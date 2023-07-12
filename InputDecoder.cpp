 
/** $VER: InputDecoder.cpp (2023.07.12) P. Stuer **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4710 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "Configuration.h"

#include <sdk/foobar2000-lite.h>
#include <sdk/input_impl.h>
#include <sdk/input_file_type.h>
#include <sdk/file_info_impl.h>
#include <sdk/tag_processor.h>

#include "framework.h"

#include "Resources.h"

#include "PMDDecoder.h"

#include "Preferences.h"

#pragma hdrstop

/// <summary>
/// Implements an input decoder.
/// </summary>
#pragma warning(disable: 4820) // x bytes padding added after last data member
class InputDecoder : public input_stubs
{
public:
    InputDecoder() noexcept :
        _File(), _FilePath(), _FileStats(),
        _SampleRate(DefaultSampleRate),
        _Decoder(),
        _SamplesRendered(),
        _TimeInMS(),
        _IsDynamicInfoSet(false)
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
    /// <summary>
    /// Opens the specified file and parses it.
    /// </summary>
    void open(service_ptr_t<file> file, const char * filePath, t_input_open_reason reason, abort_callback & abortHandler)
    {
        if (reason == input_open_info_write)
            throw exception_tagging_unsupported(); // Decoder does not support retagging.

        delete _Decoder;
        _Decoder = nullptr;

        _File = file;
        _FilePath = filePath;

        input_open_file_helper(_File, filePath, reason, abortHandler);

        {
            _FileStats = _File->get_stats(abortHandler);

            if ((_FileStats.m_size == 0) || (_FileStats.m_size > (t_size)(1 << 16)))
                throw exception_io_unsupported_format("File too big");
        }

        {
            pfc::array_t<t_uint8> Data;

            Data.resize((size_t)_FileStats.m_size);

            _File->read_object(&Data[0], (t_size)_FileStats.m_size, abortHandler);

            {
                _Decoder = new PMDDecoder();

                // Skip past the "file://" prefix.
                if (::_strnicmp(filePath, "file://", 7) == 0)
                    filePath += 7;

                if (!_Decoder->Open(filePath, CfgSamplesPath, &Data[0], (size_t) _FileStats.m_size))
                    throw exception_io_data("Invalid PMD file");
            }
        }
    }

    static bool g_is_our_content_type(const char * contentType)
    {
        return ::stricmp_utf8(contentType, "audio/pmd") == 0;
    }

    static bool g_is_our_path(const char *, const char * extension)
    {
        return (::stricmp_utf8(extension, "m") == 0) || (::stricmp_utf8(extension, "m2") == 0);
    }

    static GUID g_get_guid()
    {
        static const GUID Guid = {0xfcd5756a,0x1db5,0x4783,{0xa0,0x74,0xe5,0xc1,0xc1,0x06,0x4e,0xe6}};

        return Guid;
    }

    static const char * g_get_name()
    {
        return STR_COMPONENT_NAME;
    }

    static GUID g_get_preferences_guid()
    {
        return PreferencesPageGUID;
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

    /// <summary>
    /// Retrieves information about specified subsong.
    /// </summary>
    void get_info(t_uint32, file_info & fileInfo, abort_callback &)
    {
        double Length = _Decoder->GetLength() / 1000.;

        fileInfo.set_length(Length);

        // General info tags
        fileInfo.info_set("encoding", "Synthesized");

        double Loop = _Decoder->GetLoop() / 1000.;

        if ((Loop > 0.) && (Length > 0.) && ((Length - Loop) > 5.))
            fileInfo.info_set("pmd_loop", pfc::format_time_ex(Loop, 0));

        // Meta data tags
        fileInfo.meta_add("title", _Decoder->GetTitle());
        fileInfo.meta_add("artist", _Decoder->GetArranger());
        fileInfo.meta_add("composer", _Decoder->GetComposer());
        fileInfo.meta_add("memo", _Decoder->GetMemo());
    }
    #pragma endregion

    #pragma region("input_info_reader_v2")
    t_filestats2 get_stats2(uint32_t stats, abort_callback & abortHandler)
    {
        return _File->get_stats2_(stats, abortHandler);
    }

    t_filestats get_file_stats(abort_callback &)
    {
        return _FileStats;
    }
    #pragma endregion

    #pragma region("input_info_writer")
    /// <summary>
    /// Set the tags for the specified file.
    /// </summary>
    void retag_set_info(t_uint32, const file_info &, abort_callback &)
    {
        throw exception_tagging_unsupported();
    }

    void retag_commit(abort_callback &)
    {
        throw exception_tagging_unsupported();
    }

    void remove_tags(abort_callback &)
    {
        throw exception_tagging_unsupported();
    }
    #pragma endregion

    #pragma region("input_decoder")
    /// <summary>
    /// Initializes the decoder before playing the specified subsong. Resets playback position to the beginning of specified subsong.
    /// </summary>
    void decode_initialize(unsigned, unsigned, abort_callback & abortHandler)
    {
        _File->reopen(abortHandler); // Equivalent to seek to zero, except it also works on nonseekable streams

        _Decoder->Initialize();

        _SamplesRendered = 0;
        _TimeInMS = 0;

        _IsDynamicInfoSet = false;
    }

    /// <summary>
    /// Reads/decodes one chunk of audio data.
    /// </summary>
    bool decode_run(audio_chunk & audioChunk, abort_callback & abortHandler)
    {
        abortHandler.check();

        uint32_t LengthInMS = _Decoder->GetLength();

        if ((LengthInMS == 0) || ((LengthInMS != 0) && (_TimeInMS >= LengthInMS)))
            return false;

        // Fill the audio chunk.
        {
            const uint32_t SamplesToRender = _Decoder->GetBlockSize();

            size_t SamplesRendered =_Decoder->Render(audioChunk, SamplesToRender);

            if (SamplesRendered == 0)
                return false;

            audioChunk.set_sample_count(SamplesRendered);

            _SamplesRendered += SamplesRendered;
        }

        _TimeInMS = (uint32_t) ((_SamplesRendered * 1000) / _SampleRate);

        return true;
    }

    /// <summary>
    /// Seeks to the specified time offset.
    /// </summary>
    void decode_seek(double timeInSeconds, abort_callback & abortHandler)
    {
        abortHandler.check();

        _Decoder->SetPosition((uint32_t)(timeInSeconds * 1000.));

        _SamplesRendered = (uint64_t) _Decoder->GetPosition() * _SampleRate;
    }

    /// <summary>
    /// Returns true if the input decoder supports seeking.
    /// </summary>
    bool decode_can_seek() noexcept
    {
        return true;
    }

    /// <summary>
    /// Returns dynamic VBR bitrate etc...
    /// </summary>
    bool decode_get_dynamic_info(file_info & fileInfo, double &) noexcept
    {
        if (!_IsDynamicInfoSet)
        {
            fileInfo.info_set_int("samplerate", _SampleRate);
            fileInfo.info_set_bitrate(((t_int64) _Decoder->GetBitsPerSample() * _Decoder->GetChannelCount() * _SampleRate + 500 /* rounding for bps to kbps*/) / 1000 /* bps to kbps */);

            _IsDynamicInfoSet = true;
        }

        return false;
    }

    /// <summary>
    /// Deals with dynamic information such as track changes in live streams.
    /// </summary>
    bool decode_get_dynamic_info_track(file_info &, double &) noexcept
    {
        return false;
    }

    void decode_on_idle(abort_callback & abortHandler)
    {
        _File->on_idle(abortHandler);
    }
    #pragma endregion

private:
    service_ptr_t<file> _File;
    pfc::string8 _FilePath;
    t_filestats _FileStats;

    uint32_t _SampleRate;

    PMDDecoder * _Decoder;

    uint64_t _SamplesRendered;
    uint32_t _TimeInMS;

    bool _IsDynamicInfoSet;

    static const uint32_t DefaultSampleRate = 44100;
};
#pragma warning(default: 4820) // x bytes padding added after last data member

// Declare the supported file types to make it show in "open file" dialog etc.
DECLARE_FILE_TYPE("Professional Music Driver (PMD) files", "*.m;*.m2");

static input_factory_t<InputDecoder> _InputDecoderFactory;
