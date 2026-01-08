 
/** $VER: InputDecoder.cpp (2026.01.08) P. Stuer **/

#include "pch.h"

#include <sdk\input_impl.h>
#include <sdk\input_file_type.h>
#include <sdk\file_info_impl.h>
#include <sdk\tag_processor.h>

#include "Configuration.h"
#include "Resources.h"
#include "PMDDecoder.h"
#include "Preferences.h"

#pragma hdrstop

#pragma warning(disable: 4820) // x bytes padding added after last data member

/// <summary>
/// Implements an input decoder.
/// </summary>
class InputDecoder : public input_stubs
{
public:
    InputDecoder() noexcept :
        _File(), _FilePath(), _FileStats(),
        _Decoder(),
        _SynthesisRate(DefaultSynthesisRate),
        _LoopNumber(),
        _IsDynamicInfoSet(false)
    {
    }

    InputDecoder(const InputDecoder &) = delete;
    InputDecoder(InputDecoder &&) = delete;
    InputDecoder& operator=(const InputDecoder &) = delete;
    InputDecoder& operator=(InputDecoder &&) = delete;

    ~InputDecoder() noexcept
    {
        delete _Decoder;
    }

public:
    #pragma region input_impl

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

            _File->read_object(&Data[0], (t_size) _FileStats.m_size, abortHandler);

            {
                const auto NativeFilePath = filesystem::g_get_native_path(filePath, abortHandler);

                _Decoder = new pmd_decoder_t();

                if (!_Decoder->Open(Data.get_ptr(), (size_t) _FileStats.m_size, (uint32_t) CfgSynthesisRate, NativeFilePath, CfgSamplesPath.get()))
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
        for (const auto & Extention : { "m", "m2", "mz" })
        {
            if (::stricmp_utf8(extension, Extention) == 0)
                return true;
        }

        return false;
    }

    static GUID g_get_guid()
    {
        return GUID_COMPONENT;
    }

    static const char * g_get_name()
    {
        return STR_COMPONENT_NAME;
    }

    static GUID g_get_preferences_guid()
    {
        return GUID_PREFERENCES;
    }

    static bool g_is_low_merit()
    {
        return false;
    }

    #pragma endregion

    #pragma region input_info_reader

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
        double Length = _Decoder->GetLength() / 1'000.;

        fileInfo.set_length(Length);

        // General info tags
        fileInfo.info_set("encoding", "Synthesized");

        fileInfo.info_set("pcm_filename", _Decoder->GetPCMFileName());
        fileInfo.info_set("pps_filename", _Decoder->GetPPSFileName());
        fileInfo.info_set("ppz_filename_1", _Decoder->GetPPZFileName(1));
        fileInfo.info_set("ppz_filename_2", _Decoder->GetPPZFileName(2));

        fileInfo.info_set("pmd_file_version", pfc::format_int(_Decoder->GetFileVersion()));

        double Loop = _Decoder->GetLoopLength() / 1'000.;

        if (Loop > 0.)
            fileInfo.info_set("loop_length", pfc::format_time_ex(Loop, 0));

        // Meta data tags
        fileInfo.meta_add("title", _Decoder->GetTitle());
        fileInfo.meta_add("artist", _Decoder->GetArranger());
        fileInfo.meta_add("composer", _Decoder->GetComposer());
        fileInfo.meta_add("memo", _Decoder->GetMemo());
    }

    #pragma endregion

    #pragma region input_info_reader_v2

    t_filestats2 get_stats2(uint32_t stats, abort_callback & abortHandler)
    {
        return _File->get_stats2_(stats, abortHandler);
    }

    t_filestats get_file_stats(abort_callback &)
    {
        return _FileStats;
    }

    #pragma endregion

    #pragma region input_info_writer

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

    #pragma region input_decoder

    /// <summary>
    /// Initializes the decoder before playing the specified subsong. Resets playback position to the beginning of specified subsong.
    /// </summary>
    void decode_initialize(unsigned, unsigned, abort_callback & abortHandler)
    {
        _File->reopen(abortHandler); // Equivalent to seek to zero, except it also works on nonseekable streams

        _Decoder->Initialize();

        _Decoder->SetMaxLoopNumber((uint32_t) CfgLoopCount);
        _Decoder->SetFadeOutDuration((uint32_t) CfgFadeOutDuration);

        _LoopNumber = 0;

        _IsDynamicInfoSet = false;
    }

    /// <summary>
    /// Reads/decodes one chunk of audio data.
    /// </summary>
    bool decode_run(audio_chunk & audioChunk, abort_callback & abortHandler)
    {
        abortHandler.check();

        // Fill the audio chunk.
        {
            const size_t FramesToRender = _Decoder->GetBlockSize();

            size_t FramesRendered = _Decoder->Render(audioChunk, FramesToRender);

            if (FramesRendered == 0)
                return false;

            audioChunk.set_sample_count(FramesRendered);
        }

        return true;
    }

    /// <summary>
    /// Seeks to the specified time offset.
    /// </summary>
    void decode_seek(double timeInSeconds, abort_callback & abortHandler)
    {
        abortHandler.check();

        _Decoder->SetPosition((uint32_t)(timeInSeconds * 1'000.));
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
        bool IsDynamicInfoUpdated = false;

        if (!_IsDynamicInfoSet)
        {
            fileInfo.info_set_int("synthesis_rate", _SynthesisRate);
            fileInfo.info_set_bitrate(((t_int64) _Decoder->GetBitsPerSample() * _Decoder->GetChannelCount() * _SynthesisRate + 500 /* rounding for bps to kbps*/) / 1'000 /* bps to kbps */);

            _IsDynamicInfoSet = true;

            IsDynamicInfoUpdated = true;
        }

        if (_LoopNumber != _Decoder->GetLoopNumber())
        {
            _LoopNumber = _Decoder->GetLoopNumber();

            fileInfo.info_set_int("loop_number", _LoopNumber);

            IsDynamicInfoUpdated = true;
        }

        return IsDynamicInfoUpdated;
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
    pfc::string _FilePath;
    t_filestats _FileStats;

    pmd_decoder_t * _Decoder;

    uint32_t _SynthesisRate;

    // Dynamic track info
    uint32_t _LoopNumber;

    bool _IsDynamicInfoSet;
};

#pragma warning(default: 4820) // x bytes padding added after last data member

// Declare the supported file types to make it show in "open file" dialog etc.
DECLARE_FILE_TYPE("PMD (Professional Music Driver) files", "*.m;*.m2;*.mz");

static input_factory_t<InputDecoder> _InputDecoderFactory;
