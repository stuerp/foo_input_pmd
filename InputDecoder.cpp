 
/** $VER: InputDecoder.cpp (2023.07.04) **/

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
    _FilePath = filePath;

    delete _Decoder;
    _Decoder = nullptr;

    if (file.is_empty())
        filesystem::g_open(file, _FilePath, filesystem::open_mode_read, abortHandler);

    {
        _FileStats = file->get_stats(abortHandler);

        if ((_FileStats.m_size == 0) || (_FileStats.m_size > (t_size)(1 << 30)))
            throw exception_io_unsupported_format();

        _FileStats2 = file->get_stats2_((uint32_t)stats2_all, abortHandler);

        if ((_FileStats2.m_size == 0) || (_FileStats2.m_size > (t_size)(1 << 30)))
            throw exception_io_unsupported_format();
    }

    std::vector<uint8_t> Data;

    Data.resize(_FileStats.m_size);

    file->read_object(&Data[0], _FileStats.m_size, abortHandler);

    _Decoder = new PMDDecoder();

    if (!_Decoder->Read(Data))
        throw exception_io_data("Invalid PMD file");
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

    fileInfo.set_length(30); // Length in seconds
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
}

/// <summary>
/// Reads/decodes one chunk of audio data.
/// </summary>
bool InputDecoder::decode_run(audio_chunk & audioChunk, abort_callback & abortHandler)
{
    abortHandler.check();

    const uint32_t SampleCount = DefaultSampleCount;
    const uint32_t ChannelCount = _Decoder->GetChannelCount();

    audioChunk.set_data_size((size_t)SampleCount * ChannelCount);

    audio_sample * Samples = audioChunk.get_data();

    size_t SamplesDone =_Decoder->Run(Samples, SampleCount);

    if (SamplesDone == 0)
        return false;

    audioChunk.set_channels(ChannelCount);
    audioChunk.set_sample_count(SamplesDone);
    audioChunk.set_sample_rate(_SampleRate);

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
