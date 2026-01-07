
/** $VER: PPZ8.cpp (2026.01.07) PC-98's 86 soundboard's 8 PCM driver (Programmed by UKKY / Based on Windows conversion by C60) **/

#include <pch.h>

#include "PPZ8.h"

// ADPCM to PPZ Volume mapping
static const int32_t ADPCMEmulationVolume[256] =
{
     0, 0, 0, 0, 0, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4,
     4, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6,
     6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
     8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
};

ppz8_t::ppz8_t(File * file) : _File(file)
{
    InitializeInternal();
}

ppz8_t::~ppz8_t()
{
}

//  00H Initialize
void ppz8_t::Initialize(uint32_t sampleRate, bool useInterpolation) noexcept
{
    InitializeInternal();

    SetSampleRate(sampleRate, useInterpolation);
}

// 01H Start PCM
void ppz8_t::Play(size_t ch, int bankNumber, int sampleNumber, uint16_t start, uint16_t stop)
{
    ppz_bank_t & Bank = _PPZBanks[bankNumber];

    if ((ch >= _countof(_Channels)) || Bank.IsEmpty())
        return;

    _Channels[ch]._IsPVI        = Bank._IsPVI;
    _Channels[ch]._IsPlaying    = true;
    _Channels[ch]._SampleNumber = sampleNumber;
    _Channels[ch]._PCMStartL    = 0;

    if ((ch == 7) && _EmulateADPCM)
    {
        _Channels[ch]._PCMStartH     = &Bank._Data[std::clamp(((int) start)    * 64, 0, Bank._Size - 1)];
        _Channels[ch]._PCMEndRounded = &Bank._Data[std::clamp(((int) stop - 1) * 64, 0, Bank._Size - 1)];
        _Channels[ch]._PCMEnd        = _Channels[ch]._PCMEndRounded;
    }
    else
    {
        PZIITEM& pi = Bank._PZIHeader.PZIItem[sampleNumber];

        _Channels[ch]._PCMStartH     = &Bank._Data[pi.Start];
        _Channels[ch]._PCMEndRounded = &Bank._Data[pi.Start + pi.Size];

        if (_Channels[ch]._HasLoop)
        {
            if (_Channels[ch]._LoopStartOffset < pi.Size)
                _Channels[ch]._PCMLoopStart = &Bank._Data[pi.Start + _Channels[ch]._LoopStartOffset];
            else
                _Channels[ch]._PCMLoopStart = &Bank._Data[pi.Start + pi.Size - 1];

            if (_Channels[ch]._LoopEndOffset < pi.Size)
                _Channels[ch]._PCMEnd = &Bank._Data[pi.Start + _Channels[ch]._LoopEndOffset];
            else
                _Channels[ch]._PCMEnd = &Bank._Data[pi.Start + pi.Size];
        }
        else
            _Channels[ch]._PCMEnd = _Channels[ch]._PCMEndRounded;
    }
}

// 02H Stop PCM
void ppz8_t::Stop(size_t ch)
{
    if (ch >= _countof(_Channels))
        return;

    _Channels[ch]._IsPlaying = false;
}

// 03H Read PVI/PZI file
int ppz8_t::Load(const WCHAR * filePath, size_t bankNumber)
{
    if (filePath == nullptr || (filePath && (*filePath == '\0')))
        return PPZ_OPEN_FAILED;

    auto & Bank = _PPZBanks[bankNumber];

    if (!_File->Open(filePath))
    {
        Bank.Reset();

        return PPZ_OPEN_FAILED;
    }

    int32_t Size = (int) _File->GetFileSize(filePath);

    PZIHEADER PZIHeader = { };

    if (HasExtension(filePath, MAX_PATH, L".PZI"))
    {
        ReadHeader(_File, PZIHeader);

        if (::memcmp(PZIHeader.ID, "PZI", 3) != 0)
        {
            _File->Close();

            return PPZ_UNKNOWN_FORMAT;
        }

        if (::memcmp(&Bank._PZIHeader, &PZIHeader, sizeof(PZIHEADER)) == 0)
        {
            Bank._FilePath = filePath;

            _File->Close();

            return PPZ_ALREADY_LOADED;
        }

        Bank.Reset();

        ::memcpy(&Bank._PZIHeader, &PZIHeader, sizeof(PZIHEADER));

        Size -= sizeof(PZIHEADER);

        uint8_t * Data = (uint8_t *) ::malloc((size_t) Size);

        if (Data == nullptr)
        {
            _File->Close();

            return PPZ_OUT_OF_MEMORY;
        }

        _File->Read(Data, (uint32_t) Size);

        Bank._FilePath = filePath;

        Bank._Data = Data;
        Bank._Size = Size;

        Bank._IsPVI = false;
    }
    else
    {
        PVIHEADER PVIHeader = { };

        ReadHeader(_File, PVIHeader);

        if (::memcmp(PVIHeader.ID, "PVI", 3))
        {
            _File->Close();

            return PPZ_UNKNOWN_FORMAT;
        }

        // Convert PVI to PZI.
        int32_t PVISize = 0;

        {
            ::memcpy(PZIHeader.ID, "PZI1", 4);

            for (size_t i = 0; i < PVIHeader.Count; ++i)
            {
                PZIHeader.PZIItem[i].Start      = (uint32_t) ((                           PVIHeader.PVIItem[i].Start      << (5 + 1)));
                PZIHeader.PZIItem[i].Size       = (uint32_t) ((PVIHeader.PVIItem[i].End - PVIHeader.PVIItem[i].Start + 1) << (5 + 1));
                PZIHeader.PZIItem[i].LoopStart  = ~0U;
                PZIHeader.PZIItem[i].LoopEnd    = ~0U;
                PZIHeader.PZIItem[i].SampleRate = 16000; // 16kHz

                PVISize += PZIHeader.PZIItem[i].Size;
            }

            PZIHeader.Count = PVIHeader.Count;

            for (size_t i = PVIHeader.Count; i < _countof(PZIHeader.PZIItem); ++i)
            {
                PZIHeader.PZIItem[i].Start      = 0;
                PZIHeader.PZIItem[i].Size       = 0;
                PZIHeader.PZIItem[i].LoopStart  = ~0U;
                PZIHeader.PZIItem[i].LoopEnd    = ~0U;
                PZIHeader.PZIItem[i].SampleRate = 0;
            }
        }

        if (::memcmp(&Bank._PZIHeader.PZIItem, &PZIHeader.PZIItem, sizeof(Bank._PZIHeader.PZIItem)) == 0)
        {
            Bank._FilePath = filePath;

            _File->Close();

            return PPZ_ALREADY_LOADED;
        }

        Bank.Reset();

        // Copy the sample descriptors.
        ::memcpy(&Bank._PZIHeader, &PZIHeader, sizeof(PZIHEADER));

        // Convert the ADPCM samples to PCM.
        {
            Size -= sizeof(PVIHEADER);
            PVISize /= 2;

            size_t DstSize = (size_t) (std::max)(Size, PVISize) * 2;

            uint8_t * DstData = (uint8_t *) ::malloc(DstSize);

            if (DstData == nullptr)
            {
                _File->Close();

                return PPZ_OUT_OF_MEMORY;
            }

            ::memset(DstData, 0, DstSize);

            Bank._Data = DstData;
            Bank._Size = PVISize * 2;

            {
                static const int32_t table1[16] =
                {
                      1,   3,   5,   7,   9,  11,  13,  15,
                     -1,  -3,  -5,  -7,  -9, -11, -13, -15,
                };

                static const int32_t table2[16] =
                {
                     57,  57,  57,  57,  77, 102, 128, 153,
                     57,  57,  57,  57,  77, 102, 128, 153,
                };

                size_t SrcSize = (size_t) (std::max)(Size, PVISize);

                uint8_t * psrc = (uint8_t *) ::malloc(SrcSize);

                if (psrc == nullptr)
                {
                    _File->Close();

                    return PPZ_OUT_OF_MEMORY;
                }

                ::memset(psrc, 0, SrcSize);

                _File->Read(psrc, (uint32_t) Size);

                uint8_t * psrc2 = psrc;

                // Convert ADPCM to PCM.
                for (size_t i = 0; i < PVIHeader.Count; ++i)
                {
                    int32_t X_N     = X_N0;
                    int32_t DELTA_N = DELTA_N0;

                    for (size_t j = 0; j < PZIHeader.PZIItem[i].Size / 2; ++j)
                    {
                        X_N     = std::clamp(X_N     + table1[(*psrc >> 4) & 0x0F] * DELTA_N /  8, -32768, 32767);
                        DELTA_N = std::clamp(DELTA_N * table2[(*psrc >> 4) & 0x0F]           / 64,    127, 24576);

                        *DstData++ = (uint8_t) (X_N / (32768 / 128) + 128);

                        X_N     = std::clamp(X_N     + table1[*psrc   & 0x0F] * DELTA_N /  8, -32768, 32767);
                        DELTA_N = std::clamp(DELTA_N * table2[*psrc++ & 0x0F]           / 64,    127, 24576);

                        *DstData++ = (uint8_t) (X_N / (32768 / 128) + 128);
                    }
                }

                ::free(psrc2);
            }
        }

        Bank._FilePath = filePath;
        Bank._IsPVI = true;
    }

    _File->Close();

    return PPZ_SUCCESS;
}

// 07H Volume
void ppz8_t::SetVolume(size_t ch, int volume)
{
    if (ch >= _countof(_Channels))
        return;

    if ((ch == 7) && _EmulateADPCM)
        _Channels[ch]._Volume = ADPCMEmulationVolume[volume & 0xFF];
    else
        _Channels[ch]._Volume = volume;
}

// 0BH Pitch Frequency
void ppz8_t::SetPitch(size_t ch, uint32_t pitch)
{
    if (ch >= _countof(_Channels))
        return;

    if ((ch == 7) && _EmulateADPCM)
        pitch = (pitch & 0xFFFF) * 0x8000 / 0x49BA;
/*
    int32_t AddsL = (int) (pitch & 0xFFFF);
    int32_t AddsH = (int) (pitch >> 16);

    _Channel[ch].PCMAddH = (int) ((((int64_t) AddsH << 16) + AddsL) * 2 * _Channel[ch].SourceFrequency / _OutputFrequency);
*/
    _Channels[ch]._PCMAddH = (int) (sizeof(int16_t) * (int64_t) pitch * _Channels[ch]._SourceFrequency / _SampleRate);

    _Channels[ch]._PCMAddL = _Channels[ch]._PCMAddH & 0xFFFF;
    _Channels[ch]._PCMAddH = _Channels[ch]._PCMAddH >> 16;
}

// 0EH Set loop pointer
void ppz8_t::SetLoop(size_t ch, size_t bankNumber, size_t sampleNumber)
{
    if ((ch >= _countof(_Channels)) || (bankNumber > 1) || (sampleNumber > 127))
        return;

    const PZIITEM & pi = _PPZBanks[bankNumber]._PZIHeader.PZIItem[sampleNumber];

    if ((pi.LoopStart != ~0U) && (pi.LoopStart < pi.LoopEnd))
    {
        _Channels[ch]._HasLoop = true;
        _Channels[ch]._LoopStartOffset = pi.LoopStart;
        _Channels[ch]._LoopEndOffset = pi.LoopEnd;
    }
    else
    {
        _Channels[ch]._HasLoop = false;
        _Channels[ch]._PCMEnd = _Channels[ch]._PCMEndRounded;
    }
}

void ppz8_t::SetLoop(size_t ch, size_t bankNumber, size_t sampleNumber, int loopStart, int loopEnd)
{
    if ((ch >= _countof(_Channels)) || (bankNumber > 1) || (sampleNumber > 127))
        return;

    const PZIITEM & pi = _PPZBanks[bankNumber]._PZIHeader.PZIItem[sampleNumber];

    if (loopStart < 0)
        loopStart = (int) pi.Size + loopStart;

    if (loopEnd < 0)
        loopEnd = (int) pi.Size + loopEnd;

    if (loopStart < loopEnd)
    {
        _Channels[ch]._HasLoop = true;
        _Channels[ch]._LoopStartOffset = (uint32_t) loopStart;
        _Channels[ch]._LoopEndOffset = (uint32_t) loopEnd;
    }
    else
    {
        _Channels[ch]._HasLoop = false;
        _Channels[ch]._PCMEnd = _Channels[ch]._PCMEndRounded;
    }
}

// 12H Stop all channels.
void ppz8_t::AllStop()
{
    for (size_t i = 0; i < _countof(_Channels); ++i)
        Stop(i);
}

// 13H Set the pan value.
void ppz8_t::SetPan(size_t ch, int value)
{
    static const int32_t PanValues[4] = { 0, 9, 1, 5 }; // { Left, Right, Leftwards, Rightwards }

    if (ch >= _countof(_Channels))
        return;

    if ((ch == 7) && _EmulateADPCM)
        _Channels[ch]._PanValue = PanValues[value & 3];
    else
        _Channels[ch]._PanValue = value;
}

// 14H Sets the output sample rate.
void ppz8_t::SetSampleRate(uint32_t sampleRate, bool useInterpolation)
{
    _SampleRate = (int) sampleRate;
    _UseInterpolation = useInterpolation;
}

// 15H Sets the original sample rate.
void ppz8_t::SetSourceFrequency(size_t ch, int sourceFrequency)
{
    if (ch >= _countof(_Channels))
        return;

    _Channels[ch]._SourceFrequency = sourceFrequency;
}

// 16H Set the overal volume（86B Mixer)
void ppz8_t::SetAllVolume(int volume)
{
    if ((volume < 16) && (volume != _MasterVolume))
    {
        _MasterVolume = volume;

        CreateVolumeTable(_Volume);
    }
}

// 17H PCM Volume (PCMTMP_SET)
void ppz8_t::SetVolume(int volume)
{
    if (volume != _Volume)
        CreateVolumeTable(volume);
}

// 18H Enables or disables ADPCM emulation.
void ppz8_t::EmulateADPCM(bool flag)
{
    _EmulateADPCM = flag;
}

/// <summary>
/// Sets the instrument to play.
/// </summary>
void ppz8_t::SetInstrument(size_t ch, size_t bankNumber, size_t instrumentNumber)
{
    SetLoop(ch, bankNumber, instrumentNumber);
    SetSourceFrequency(ch, _PPZBanks[bankNumber]._PZIHeader.PZIItem[instrumentNumber].SampleRate);
}

/// <summary>
/// Mixes the output of all the channels.
/// </summary>
void ppz8_t::Mix(frame32_t * frames, size_t frameCount) noexcept
{
    if (_MasterVolume == 0)
        return;

    for (auto & Channel : _Channels)
    {
        if (!Channel._IsPlaying)
            continue;

        if (Channel._Volume == 0)
        {
            // Update the position in the sample buffer.
            Channel._PCMStartL += (int) (Channel._PCMAddL * frameCount);
            Channel._PCMStartH +=        Channel._PCMAddH * frameCount + Channel._PCMStartL / 0x10000;
            Channel._PCMStartL %= 0x10000;

            while (Channel._PCMStartH >= Channel._PCMEnd - 1)
            {
                if (Channel._HasLoop)
                {
                    Channel._PCMStartH -= (Channel._PCMEnd - 1 - Channel._PCMLoopStart);
                }
                else
                {
                    Channel._IsPlaying = false;
                    break;
                }
            }
            continue;
        }

        if (Channel._PanValue == 0)
        {
            Channel._IsPlaying = false;
            continue;
        }

        if (_UseInterpolation)
        {
            sample_t * Samples = (sample_t *) frames;

            switch (Channel._PanValue)
            {
                case 1: // 1, 0
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = (_VolumeTable[Channel._Volume][*Channel._PCMStartH] * (0x10000 - Channel._PCMStartL) + _VolumeTable[Channel._Volume][*(Channel._PCMStartH + 1)] * Channel._PCMStartL) >> 16;

                        *Samples++ += Sample;
                         Samples++;             // Don't set the right sample.

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 2: // 1, 1/4
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = (_VolumeTable[Channel._Volume][*Channel._PCMStartH] * (0x10000 - Channel._PCMStartL) + _VolumeTable[Channel._Volume][*(Channel._PCMStartH + 1)] * Channel._PCMStartL) >> 16;

                        *Samples++ += Sample;
                        *Samples++ += Sample / 4;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 3: // 1, 2/4
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = (_VolumeTable[Channel._Volume][*Channel._PCMStartH] * (0x10000 - Channel._PCMStartL) + _VolumeTable[Channel._Volume][*(Channel._PCMStartH + 1)] * Channel._PCMStartL) >> 16;

                        *Samples++ += Sample;
                        *Samples++ += Sample / 2;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 4: // 1 ,3/4
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = (_VolumeTable[Channel._Volume][*Channel._PCMStartH] * (0x10000 - Channel._PCMStartL) + _VolumeTable[Channel._Volume][*(Channel._PCMStartH + 1)] * Channel._PCMStartL) >> 16;

                        *Samples++ += Sample;
                        *Samples++ += Sample * 3 / 4;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 5: // 1 , 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = (_VolumeTable[Channel._Volume][*Channel._PCMStartH] * (0x10000 - Channel._PCMStartL) + _VolumeTable[Channel._Volume][*(Channel._PCMStartH + 1)] * Channel._PCMStartL) >> 16;

                        *Samples++ += Sample;
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 6: // 3/4, 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = (_VolumeTable[Channel._Volume][*Channel._PCMStartH] * (0x10000 - Channel._PCMStartL) + _VolumeTable[Channel._Volume][*(Channel._PCMStartH + 1)] * Channel._PCMStartL) >> 16;

                        *Samples++ += Sample * 3 / 4;
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 7:  // 2/4, 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = (_VolumeTable[Channel._Volume][*Channel._PCMStartH] * (0x10000 - Channel._PCMStartL) + _VolumeTable[Channel._Volume][*(Channel._PCMStartH + 1)] * Channel._PCMStartL) >> 16;

                        *Samples++ += Sample / 2;
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 8: // 1/4, 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = (_VolumeTable[Channel._Volume][*Channel._PCMStartH] * (0x10000 - Channel._PCMStartL) + _VolumeTable[Channel._Volume][*(Channel._PCMStartH + 1)] * Channel._PCMStartL) >> 16;

                        *Samples++ += Sample / 4;
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 9: // 0, 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = (_VolumeTable[Channel._Volume][*Channel._PCMStartH] * (0x10000 - Channel._PCMStartL) + _VolumeTable[Channel._Volume][*(Channel._PCMStartH + 1)] * Channel._PCMStartL) >> 16;

                         Samples++; // Only the right channel
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }
            }
        }
        else
        {
            sample_t * Samples = (sample_t *) frames;

            switch (Channel._PanValue)
            {
                case 1: // 1, 0
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = _VolumeTable[Channel._Volume][*Channel._PCMStartH];

                        *Samples++ += Sample;
                         Samples++; // Only the left channel

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 2: // 1, 1/4
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = _VolumeTable[Channel._Volume][*Channel._PCMStartH];

                        *Samples++ += Sample;
                        *Samples++ += Sample / 4;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 3: // 1, 2/4
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = _VolumeTable[Channel._Volume][*Channel._PCMStartH];

                        *Samples++ += Sample;
                        *Samples++ += Sample / 2;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 4: // 1, 3/4
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = _VolumeTable[Channel._Volume][*Channel._PCMStartH];

                        *Samples++ += Sample;
                        *Samples++ += Sample * 3 / 4;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 5: // 1, 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = _VolumeTable[Channel._Volume][*Channel._PCMStartH];

                        *Samples++ += Sample;
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 6: // 3/4, 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = _VolumeTable[Channel._Volume][*Channel._PCMStartH];

                        *Samples++ += Sample * 3 / 4;
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 7: // 2/4, 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = _VolumeTable[Channel._Volume][*Channel._PCMStartH];

                        *Samples++ += Sample / 2;
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 8: // 1/4, 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = _VolumeTable[Channel._Volume][*Channel._PCMStartH];

                        *Samples++ += Sample / 4;
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }

                case 9: // 0, 1
                {
                    while (Channel._IsPlaying && (Samples < (sample_t *) (frames + frameCount)))
                    {
                        const sample_t Sample = _VolumeTable[Channel._Volume][*Channel._PCMStartH];

                         Samples++;             // Don't set the left sample.
                        *Samples++ += Sample;

                        MoveSamplePointer(Channel);
                    }
                    break;
                }
            }
        }
    }
}

/// <summary>
/// Moves the sample pointer/
/// </summary>
void ppz8_t::MoveSamplePointer(ppz_channel_t & channel) const noexcept
{
    channel._PCMStartH += channel._PCMAddH;
    channel._PCMStartL += channel._PCMAddL;

    if (channel._PCMStartL > 0xFFFF)
    {
        channel._PCMStartL -= 0x10000;
        channel._PCMStartH++;
    }

    if (channel._PCMStartH >= channel._PCMEnd - 1)
    {
        if (channel._HasLoop)
            channel._PCMStartH = channel._PCMLoopStart;
        else
            channel._IsPlaying = false;
    }
}

/// <summary>
/// Initializes this instance.
/// </summary>
void ppz8_t::InitializeInternal() noexcept
{
    for (auto & Channel : _Channels)
    {
        Channel =
        {
            ._Volume          = 8,       // Default volume
            ._PanValue        = 5,       // Pan Center
            ._SourceFrequency = 16000,   // in Hz
            ._PCMAddL         = 0,
            ._PCMAddH         = 1,
        };
    }

    _PPZBanks[0].Reset();
    _PPZBanks[1].Reset();

    _EmulateADPCM = false;
    _UseInterpolation = false;

    _MasterVolume = 0;
    _Volume = 0;

    SetAllVolume(DefaultVolume);

    _SampleRate = DefaultSampleRate;
}

/// <summary>
/// Creates the volume lookup table.
/// </summary>
void ppz8_t::CreateVolumeTable(int volume)
{
    _Volume = volume;

    const int32_t VolumeBase = (int) (0x1000 * ::pow(10.0, volume / 40.0));

    for (int32_t i = 0; i < 16; ++i)
    {
        double Value = ::pow(2.0, ((double) i + _MasterVolume) / 2.0) * VolumeBase / 0x18000;

        for (int32_t j = 0; j < 256; ++j)
            _VolumeTable[i][j] = (sample_t) ((j - 128) * Value);
    }
}

/// <summary>
/// Reads the PZI header.
/// </summary>
void ppz8_t::ReadHeader(File * file, PZIHEADER& ph)
{
    uint8_t Data[2336];

    if (file->Read(Data, sizeof(Data)) != sizeof(Data))
        return;

    ::memcpy(ph.ID, Data, sizeof(ph.ID));
    ::memcpy(ph.Dummy1, &Data[0x04], sizeof(ph.Dummy1));

    ph.Count = Data[0x0B];

    ::memcpy(ph.Dummy2, &Data[0x0C], sizeof(ph.Dummy2));

    for (int32_t i = 0; i < 128; ++i)
    {
        ph.PZIItem[i].Start      = (uint32_t) ((Data[0x20 + i * 18]) | (Data[0x21 + i * 18] << 8) | (Data[0x22 + i * 18] << 16) | (Data[0x23 + i * 18] << 24));
        ph.PZIItem[i].Size       = (uint32_t) ((Data[0x24 + i * 18]) | (Data[0x25 + i * 18] << 8) | (Data[0x26 + i * 18] << 16) | (Data[0x27 + i * 18] << 24));
        ph.PZIItem[i].LoopStart  = (uint32_t) ((Data[0x28 + i * 18]) | (Data[0x29 + i * 18] << 8) | (Data[0x2a + i * 18] << 16) | (Data[0x2b + i * 18] << 24));
        ph.PZIItem[i].LoopEnd    = (uint32_t) ((Data[0x2c + i * 18]) | (Data[0x2d + i * 18] << 8) | (Data[0x2e + i * 18] << 16) | (Data[0x2f + i * 18] << 24));
        ph.PZIItem[i].SampleRate = (uint16_t) ((Data[0x30 + i * 18]) | (Data[0x31 + i * 18] << 8));
    }
}

/// <summary>
/// Reads the PVI header.
/// </summary>
void ppz8_t::ReadHeader(File * file, PVIHEADER & ph)
{
    uint8_t Data[528];

    file->Read(Data, sizeof(Data));

    ::memcpy(ph.ID, Data, 4);
    ::memcpy(ph.Dummy1, &Data[0x04], (size_t) 0x0b - 4);

    ph.Count = Data[0x0b];

    ::memcpy(ph.Dummy2, &Data[0x0c], (size_t) 0x10 - 0x0b - 1);

    for (int32_t i = 0; i < 128; ++i)
    {
        ph.PVIItem[i].Start = (uint16_t) ((Data[0x10 + i * 4]) | (Data[0x11 + i * 4] << 8));
        ph.PVIItem[i].End   = (uint16_t) ((Data[0x12 + i * 4]) | (Data[0x13 + i * 4] << 8));
    }
}
