
/** $VER: P86.cpp (2026.01.07) PMD's internal 86PCM driver for the PC-98's 86 soundboard / Programmed by M.Kajihara 96/01/16 / Windows Converted by C60 **/

#include <pch.h>

#include "P86.h"

static const int32_t SampleRates[] =
{
    4135, 5513, 8270, 11025, 16540, 22050, 33080, 44100
};

p86_t::p86_t(File * file) : _File(file), _Data()
{
    InitializeInternal();
}

p86_t::~p86_t()
{
    if (_Data)
        ::free(_Data);
}

/// <summary>
/// Initializes this instance.
/// </summary>
bool p86_t::Initialize(uint32_t sampleRate, bool useInterpolation) noexcept
{
    InitializeInternal();

    SetSampleRate(sampleRate, useInterpolation);

    return true;
}

/// <summary>
/// Initializes this instance.
/// </summary>
void p86_t::InitializeInternal()
{
    ::memset(&_Header, 0, sizeof(_Header));

    if (_Data)
    {
        ::free(_Data);
        _Data = nullptr;
    }

    _SampleRate         = FREQUENCY_44_1K;
    _SampleRateOriginal = SampleRates[4]; // 16.54kHz

    _Pitch  = 0;
    _Volume = 0;

    _CurrAddr = nullptr;
    _CurrOffs = 0;
    _SizeToDo = 0;

    _SampleAddr = nullptr;
    _SampleSize = 0;

    _LoopAddr = nullptr;
    _LoopSize = 0;

    _ReleaseAddr = nullptr;
    _ReleaseSize = 0;

    _UseInterpolation   = false;
    _IsLooping          = false;
    _IsReleaseRequested = false;
    _IsReleasing        = false;

    _IncrementHi = 0;
    _IncrementLo = 0;

    _PanFlags = 0;
    _PanValue = 0;

    _VolumeBase = 0;

    InitializeVolume(0);

    _IsPlaying = false;
}

/// <summary>
/// Loads a P86 file (Professional Music Driver P86 Samples Pack file)
/// </summary>
int p86_t::Load(const WCHAR * filePath)
{
    Stop();

    _FilePath.clear();

    if (*filePath == '\0')
        return P86_OPEN_FAILED;

    if (!_File->Open(filePath))
    {
        if (_Data != nullptr)
        {
            ::free(_Data);
            _Data = nullptr;
        }

        ::memset(&_Header, 0, sizeof(_Header));

        return P86_OPEN_FAILED;
    }

    // "PCM86 DATA\n"
    P86HEADER ph = { };

    size_t FileSize = (size_t) _File->GetFileSize(filePath);

    {
        P86FILEHEADER fh = { };

        ReadHeader(_File, fh);

        for (size_t i = 0; i < _countof(P86HEADER::P86Item); ++i)
        {
            ph.P86Item[i].Offset = fh.P86Item[i].Offset[0] + (fh.P86Item[i].Offset[1] * 0x100) + fh.P86Item[i].Offset[2] * 0x10000 - 0x610;
            ph.P86Item[i].Size   = fh.P86Item[i].Size[0]   + (fh.P86Item[i].Size[1]   * 0x100) + fh.P86Item[i].Size[2]   * 0x10000;
        }

        if (::memcmp(&_Header, &ph, sizeof(_Header)) == 0)
        {
            _FilePath = filePath;

            _File->Close();

            return P86_ALREADY_LOADED;
        }
    }

    if (_Data != nullptr)
    {
        ::free(_Data);
        _Data = nullptr;
    }

    ::memcpy(&_Header, &ph, sizeof(_Header));

    FileSize -= P86FILEHEADERSIZE;

    _Data = (uint8_t *) ::malloc(FileSize);

    if (_Data == nullptr)
    {
        _File->Close();

        return PPZ_OUT_OF_MEMORY;
    }

    _File->Read(_Data, (uint32_t) FileSize);

    _FilePath = filePath;

    _File->Close();

    return P86_SUCCESS;
}

/// <summary>
/// Sets the sample rate.
/// </summary>
void p86_t::SetSampleRate(uint32_t sampleRate, bool useInterpolation)
{
    _SampleRate = (int32_t) sampleRate;
    _UseInterpolation = useInterpolation;

    const uint32_t Pitch = (uint32_t) ((uint64_t) _Pitch * _SampleRateOriginal / _SampleRate);

    _IncrementHi = (int32_t) ( Pitch           >> 16);
    _IncrementLo = (int32_t) ((Pitch & 0xFFFF) >>  4);
}

/// <summary>
/// Initializes the volume lookup table.
/// </summary>
void p86_t::InitializeVolume(int value)
{
    CreateVolumeTable(value);
}

/// <summary>
/// Sets the volume.
/// </summary>
bool p86_t::SetVolume(int value)
{
    _Volume = value;

    return true;
}

/// <summary>
/// 
/// </summary>
bool p86_t::SelectSample(int index)
{
    _SampleAddr = (_Data != nullptr) ? _Data + _Header.P86Item[index].Offset : nullptr;
    _SampleSize = _Header.P86Item[index].Size;

    _IsLooping = false;
    _IsReleaseRequested = false;

    return true;
}

/// <summary>
/// 
/// </summary>
bool p86_t::SetPan(int flags, int value)
{
    _PanFlags = flags;
    _PanValue = value;

    return true;
}

/// <summary>
/// 
/// </summary>
bool p86_t::SetPitch(int sampleRateIndex, uint32_t pitch)
{
    if ((sampleRateIndex < 0) || (sampleRateIndex >= (int32_t) _countof(SampleRates)) || (pitch > 0x1FFFFF))
        return false;

    _SampleRateOriginal = SampleRates[sampleRateIndex];
    _Pitch = pitch;

    pitch = (uint32_t) ((uint64_t) pitch * _SampleRateOriginal / _SampleRate);

    _IncrementHi = (int32_t)  (pitch           >> 16);
    _IncrementLo = (int32_t) ((pitch & 0xFFFF) >>  4);

    return true;
}

/// <summary>
/// 
/// </summary>
bool p86_t::SetLoop(int loopBegin, int loopEnd, int releaseBegin, bool isADPCM)
{
    _IsLooping = true;
    _IsReleaseRequested = false;

    const int32_t OldSampleSize = _SampleSize;

    int32_t SampleSize = _SampleSize;
    int32_t Offset = loopBegin;

    if (Offset >= 0)
    {
        if (isADPCM)
            Offset *= 32;

        if (Offset > _SampleSize - 2)
            Offset = _SampleSize - 2;

        if (Offset < 0)
            Offset = 0;

        _LoopAddr = _SampleAddr + Offset;
        _LoopSize = _SampleSize - Offset;
    }
    else
    {
        Offset = -Offset;

        if (isADPCM)
            Offset *= 32;

        SampleSize -= Offset;

        if (SampleSize < 0)
        {
            Offset = OldSampleSize;
            SampleSize = 0;
        }

        _LoopAddr = _SampleAddr + SampleSize;
        _LoopSize = Offset;
    }

    Offset = loopEnd;

    if (Offset > 0)
    {
        if (isADPCM)
            Offset *= 32;

        if (Offset > _SampleSize - 2)
            Offset = _SampleSize - 2;

        if (Offset < 0)
            Offset = 0;

        _SampleSize = Offset;

        SampleSize -= Offset;

        _LoopSize -= SampleSize;
    }
    else
    if (Offset < 0)
    {
        Offset = -Offset;

        if (isADPCM)
            Offset *= 32;

        if (Offset > _LoopSize)
            Offset = _LoopSize;

        _LoopSize   -= Offset;
        _SampleSize -= Offset;
    }

    Offset = releaseBegin;

    if ((uint16_t) Offset != 0x8000)
    {
        _ReleaseAddr = _SampleAddr;
        _ReleaseSize = OldSampleSize;

        _IsReleaseRequested = true;

        if (Offset > 0)
        {
            if (isADPCM)
                Offset *= 32;

            if (Offset > _SampleSize - 2)
                Offset = _SampleSize - 2;

            if (Offset < 0)
                Offset = 0;

            _ReleaseAddr += Offset;
            _ReleaseSize -= Offset;
        }
        else
        {
            Offset = -Offset;

            if (isADPCM)
                Offset *= 32;

            if (Offset > _SampleSize)
                Offset = _SampleSize;

            _ReleaseAddr += OldSampleSize - Offset;
            _ReleaseSize = Offset;
        }
    }

    return true;
}

/// <summary>
/// Starts playing a sample.
/// </summary>
void p86_t::Start()
{
    _CurrAddr = _SampleAddr;
    _CurrOffs = 0;
    _SizeToDo = _SampleSize;

    _IsPlaying   = true;
    _IsReleasing = false;
}

/// <summary>
/// Stops playing a sample.
/// </summary>
bool p86_t::Stop(void)
{
    _IsPlaying = false;

    return true;
}

/// <summary>
/// 
/// </summary>
bool p86_t::Keyoff(void)
{
    if (_IsReleaseRequested)
    {
        _CurrAddr = _ReleaseAddr;
        _SizeToDo = _ReleaseSize;

        _IsReleasing = true;
    }
    else
        _IsPlaying = false;

    return true;
}

/// <summary>
/// Mixes the samples of the PPS.
/// </summary>
void p86_t::Mix(frame32_t * frames, size_t frameCount) noexcept
{
    if (!_IsPlaying)
        return;

    if (_SizeToDo <= 1)
    {
        _IsPlaying = false;

        return;
    }

    if (_UseInterpolation)
    {
        switch (_PanFlags)
        {
            case 0: MixCenterInterpolated((sample_t *) frames, frameCount); break;
            case 1: MixLeftInterpolated  ((sample_t *) frames, frameCount); break;
            case 2: MixRightInterpolated ((sample_t *) frames, frameCount); break;
            case 3: MixCenterInterpolated((sample_t *) frames, frameCount); break;

            case 4: MixCenterInterpolatedPhaseReversed((sample_t *) frames, frameCount); break;
            case 5: MixLeftInterpolatedPhaseReversed  ((sample_t *) frames, frameCount); break;
            case 6: MixRightInterpolatedPhaseReversed ((sample_t *) frames, frameCount); break;
            case 7: MixCenterInterpolatedPhaseReversed((sample_t *) frames, frameCount); break;
        }
    }
    else
    {
        switch (_PanFlags)
        {
            case 0: MixCenter((sample_t *) frames, frameCount); break;
            case 1: MixLeft  ((sample_t *) frames, frameCount); break;
            case 2: MixRight ((sample_t *) frames, frameCount); break;
            case 3: MixCenter((sample_t *) frames, frameCount); break;

            case 4: MixCenterPhaseReversed((sample_t *) frames, frameCount); break;
            case 5: MixLeftPhaseReversed  ((sample_t *) frames, frameCount); break;
            case 6: MixRightPhaseReversed ((sample_t *) frames, frameCount); break;
            case 7: MixCenterPhaseReversed((sample_t *) frames, frameCount); break;
        }
    }
}

/// <summary>
/// Center with linear interpolation
/// </summary>
void p86_t::MixCenterInterpolated(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t Sample = (_VolumeTable[_Volume][_CurrAddr[0]] * (0x1000 - _CurrOffs) + _VolumeTable[_Volume][_CurrAddr[1]] * _CurrOffs) >> 12;

        *frames++ += Sample;
        *frames++ += Sample;

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Center with linear interpolation (Reverse phase)
/// </summary>
void p86_t::MixCenterInterpolatedPhaseReversed(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t Sample = (_VolumeTable[_Volume][_CurrAddr[0]] * (0x1000 - _CurrOffs) + _VolumeTable[_Volume][_CurrAddr[1]] * _CurrOffs) >> 12;

        *frames++ += Sample;
        *frames++ -= Sample;

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Left with linear interpolation
/// </summary>
void p86_t::MixLeftInterpolated(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t Sample = (_VolumeTable[_Volume][_CurrAddr[0]] * (0x1000 - _CurrOffs) + _VolumeTable[_Volume][_CurrAddr[1]] * _CurrOffs) >> 12;

        *frames++ += Sample;
        *frames++ += Sample * _PanValue / (256 / 2);

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Left with linear interpolation (Reverse phase)
/// </summary>
void p86_t::MixLeftInterpolatedPhaseReversed(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t Sample = (_VolumeTable[_Volume][_CurrAddr[0]] * (0x1000 - _CurrOffs) + _VolumeTable[_Volume][_CurrAddr[1]] * _CurrOffs) >> 12;

        *frames++ += Sample;
        *frames++ -= Sample * _PanValue / (256 / 2);

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Right with linear interpolation
/// </summary>
void p86_t::MixRightInterpolated(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t SampleR = (_VolumeTable[_Volume][_CurrAddr[0]] * (0x1000 - _CurrOffs) + _VolumeTable[_Volume][_CurrAddr[1]] * _CurrOffs) >> 12;
        const sample_t SampleL = SampleR * _PanValue / (256 / 2);

        *frames++ += SampleL;
        *frames++ += SampleR;

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Right with linear interpolation (Reverse phase)
/// </summary>
void p86_t::MixRightInterpolatedPhaseReversed(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t SampleR = (_VolumeTable[_Volume][_CurrAddr[0]] * (0x1000 - _CurrOffs) + _VolumeTable[_Volume][_CurrAddr[1]] * _CurrOffs) >> 12;
        const sample_t SampleL = SampleR * _PanValue / (256 / 2);

        *frames++ += SampleL;
        *frames++ -= SampleR;

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Center, no interpolation
/// </summary>
void p86_t::MixCenter(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t Sample = _VolumeTable[_Volume][*_CurrAddr];

        *frames++ += Sample;
        *frames++ += Sample;

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Center, no interpolation (Reverse phase)
/// </summary>
void p86_t::MixCenterPhaseReversed(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t Sample = _VolumeTable[_Volume][*_CurrAddr];

        *frames++ += Sample;
        *frames++ -= Sample;

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Left, no interpolation
/// </summary>
void p86_t::MixLeft(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t Sample = _VolumeTable[_Volume][*_CurrAddr];

        *frames++ += Sample;
        *frames++ += Sample * _PanValue / (256 / 2);

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Left, no interpolation (Reverse phase)
/// </summary>
void p86_t::MixLeftPhaseReversed(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t Sample = _VolumeTable[_Volume][*_CurrAddr];

        *frames++ += Sample;
        *frames++ -= Sample * _PanValue / (256 / 2);

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Right, no interpolation
/// </summary>
void p86_t::MixRight(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t SampleR = _VolumeTable[_Volume][*_CurrAddr];
        const sample_t SampleL = SampleR * _PanValue / (256 / 2);

        *frames++ += SampleL;
        *frames++ += SampleR;

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Right, no interpolation (Reverse phase)
/// </summary>
void p86_t::MixRightPhaseReversed(sample_t * frames, size_t frameCount)
{
    for (size_t i = 0; i < frameCount; ++i)
    {
        const sample_t SampleR = _VolumeTable[_Volume][*_CurrAddr];
        const sample_t SampleL = SampleR * _PanValue / (256 / 2);

        *frames++ += SampleL;
        *frames++ -= SampleR;

        if (MoveSamplePointer())
        {
            _IsPlaying = false;
            return;
        }
    }
}

/// <summary>
/// Moves the sample pointer.
/// </summary>
bool p86_t::MoveSamplePointer() noexcept
{
    _CurrOffs += _IncrementLo;

    if (_CurrOffs >= 0x1000)
    {
        _CurrOffs -= 0x1000;
        _CurrAddr++;

        _SizeToDo--;
    }

    _CurrAddr += _IncrementHi;
    _SizeToDo -= _IncrementHi;

    if (_SizeToDo > 1)
        return false;

    if (!_IsLooping || _IsReleasing)
        return true;

    _SizeToDo = _LoopSize;
    _CurrAddr = _LoopAddr;

    return false;
}

/// <summary>
/// 
/// </summary>
void p86_t::CreateVolumeTable(int volume)
{
    const int32_t NewVolumeBase = (int32_t) (0x1000 * std::pow(10.0, volume / 40.0));

    if (NewVolumeBase == _VolumeBase)
        return;

    _VolumeBase = NewVolumeBase;

    for (int32_t i = 0; i < 16; ++i)
    {
        const double Volume = (double) _VolumeBase * i / 256.; // std::pow(2.0, (i + 15) / 2.0) * _VolumeBase / 0x18000;

        for (int32_t j = 0; j < 256; ++j)
            _VolumeTable[i][j] = (sample_t) (Volume * (int8_t) j);
    }
}

/// <summary>
/// 
/// </summary>
void p86_t::ReadHeader(File * file, P86FILEHEADER & header)
{
    uint8_t Data[1552];

    file->Read(Data, sizeof(Data));

    ::memcpy(header.Id, Data, 12);

    header.Version = Data[0x0C];

    ::memcpy(header.Size, Data + 0x0D, 3);

    for (size_t i = 0; i < _countof(P86FILEHEADER::P86Item); ++i)
    {
        ::memcpy(header.P86Item[i].Offset, &Data[0x10 + i * 6], 3);
        ::memcpy(header.P86Item[i].Size,   &Data[0x13 + i * 6], 3);
    }
}
