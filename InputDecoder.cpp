 
/** $VER: InputDecoder.cpp (2023.07.07) **/

#pragma warning(disable: 5045 26481 26485)

#include "InputDecoder.h"

#include <sdk/hasher_md5.h>
#include <sdk/metadb_index.h>
#include <sdk/system_time_keeper.h>

#pragma region("input_impl")
/// <summary>
/// Opens the specified file and parses it.
/// </summary>
void InputDecoder::open(service_ptr_t<file> file, const char * filePath, t_input_open_reason, abort_callback & abortHandler)
{
    delete _Decoder;
    _Decoder = nullptr;

    _FilePath = filePath;

    {
        if (file.is_empty())
            filesystem::g_open(file, _FilePath, filesystem::open_mode_read | filesystem::open_shareable, abortHandler);

        {
            _FileStats = file->get_stats(abortHandler);

            if ((_FileStats.m_size == 0) || (_FileStats.m_size > (t_size)(1 << 30)))
                throw exception_io_unsupported_format();

            _FileStats2 = file->get_stats2_((uint32_t)stats2_all, abortHandler);

            if ((_FileStats2.m_size == 0) || (_FileStats2.m_size > (t_size)(1 << 30)))
                throw exception_io_unsupported_format();
        }
    }

    uint8_t * Data = new uint8_t[_FileStats.m_size];

    if (Data == nullptr)
        throw exception_io();

    file->read_object(Data, _FileStats.m_size, abortHandler);

    {
        _Decoder = new PMDDecoder();

        WCHAR FilePath[MAX_PATH];

        {
            // Skip past the "file://" prefix.
            if (::_strnicmp(filePath, "file://", 7) != 0)
                return;

            ::MultiByteToWideChar(CP_UTF8, 0, filePath + 7, -1, FilePath, _countof(FilePath));
        }

        WCHAR PDXSamplesPathName[MAX_PATH] = L".";

        if (!_Decoder->Read(Data, _FileStats.m_size, FilePath, PDXSamplesPathName))
            throw exception_io_data("Invalid PMD file");
    }

    delete[] Data;
}
#pragma endregion

#pragma region("input_info_reader")
/// <summary>
/// Retrieves information about specified subsong.
/// </summary>
void InputDecoder::get_info(t_uint32 subsongIndex, file_info & fileInfo, abort_callback & abortHandler)
{
    UNREFERENCED_PARAMETER(subsongIndex), UNREFERENCED_PARAMETER(abortHandler);

    // General tags
    fileInfo.info_set_int("channels", 2);
    fileInfo.info_set("encoding", "Synthesized");

    fileInfo.set_length(_Decoder->GetLength() * 0.001);

    fileInfo.meta_add("title", _Decoder->GetTitle());
    fileInfo.meta_add("artist", _Decoder->GetArranger());
    fileInfo.meta_add("pmd_composer", _Decoder->GetComposer());
    fileInfo.meta_add("pmd_memo", _Decoder->GetMemo());
}
#pragma endregion

#pragma region("input_info_writer")
/// <summary>
/// Set the tags for the specified file.
/// </summary>
void InputDecoder::retag_set_info(t_uint32 subsongIndex, const file_info & fileInfo, abort_callback & abortHandler)
{
    UNREFERENCED_PARAMETER(subsongIndex), UNREFERENCED_PARAMETER(fileInfo), UNREFERENCED_PARAMETER(abortHandler);

    throw exception_io_data("You cannot tag PMD files");
}
#pragma endregion

#pragma region("input_decoder")
/// <summary>
/// Initializes the decoder before playing the specified subsong. Resets playback position to the beginning of specified subsong.
/// </summary>
void InputDecoder::decode_initialize(unsigned subsongIndex, unsigned flags, abort_callback & abortHandler)
{
    UNREFERENCED_PARAMETER(subsongIndex), UNREFERENCED_PARAMETER(flags), UNREFERENCED_PARAMETER(abortHandler);

    _Decoder->Initialize();

    _SamplesRendered = 0;
    _TimeInMS = 0;
}

/// <summary>
/// Reads/decodes one chunk of audio data.
/// </summary>
bool InputDecoder::decode_run(audio_chunk & audioChunk, abort_callback & abortHandler)
{
    abortHandler.check();

    uint32_t LengthInMS = _Decoder->GetLength();

    if ((LengthInMS != 0) && (_TimeInMS >= LengthInMS))
        return false;

    // Fill the audio chunk.
    {
        const uint32_t SamplesToRender = _Decoder->GetSampleCount();
        const uint32_t ChannelCount = _Decoder->GetChannelCount();

        audioChunk.set_data_size((t_size) SamplesToRender * ChannelCount);

        audio_sample * Samples = audioChunk.get_data();

        size_t SamplesRendered =_Decoder->Render(Samples, SamplesToRender);

        if (SamplesRendered == 0)
            return false;

        audioChunk.set_channels(ChannelCount);
        audioChunk.set_sample_rate(_SampleRate);
        audioChunk.set_sample_count(SamplesRendered);

        _SamplesRendered += SamplesRendered;
    }

    while (_SamplesRendered >= _SampleRate)
    {
        _SamplesRendered -= _SampleRate;
        _TimeInMS += 1000;
    }

    return true;
}

/// <summary>
/// Seeks to the specified time offset.
/// </summary>
void InputDecoder::decode_seek(double timeInSeconds, abort_callback & abortHandler)
{
    UNREFERENCED_PARAMETER(timeInSeconds);

    abortHandler.check();
}

/// <summary>
/// Returns dynamic VBR bitrate etc...
/// </summary>
bool InputDecoder::decode_get_dynamic_info(file_info & fileInfo, double & timestampDelta)
{
    fileInfo.info_set_int("samplerate", _SampleRate);
    timestampDelta = 0.;

    return true;
}
#pragma endregion

static input_factory_t<InputDecoder> _InputDecoderFactory;
