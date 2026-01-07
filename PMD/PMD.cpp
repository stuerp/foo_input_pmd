
// $VER: PMD.cpp (2026.01.07) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <pch.h>

#include "PMD.h"

#include "Utility.h"
#include "Tables.h"

#include "OPNAW.h"

/// <summary>
/// Initializes an instance.
/// </summary>
pmd_driver_t::pmd_driver_t()
{
    _File = new File();

    _OPNAW = new opnaw_t(_File);

    _PPZ8 = new ppz8_t(_File);
    _PPS = new pps_t(_File);
    _P86 = new p86_t(_File);

    Reset();
}

/// <summary>
/// Destroys an instance.
/// </summary>
pmd_driver_t::~pmd_driver_t()
{
    delete _P86;
    delete _PPS;
    delete _PPZ8;

    delete _OPNAW;

    delete _File;
}

#pragma region Public

/// <summary>
/// Initializes the driver.
/// </summary>
bool pmd_driver_t::Initialize(const WCHAR * directoryPathDrums) noexcept
{
    WCHAR DirectoryPathDrums[MAX_PATH] = { 0 };

    if (directoryPathDrums != nullptr)
    {
        ::wcscpy_s(DirectoryPathDrums, _countof(DirectoryPathDrums), directoryPathDrums);
        AddBackslash(DirectoryPathDrums, _countof(DirectoryPathDrums));
    }

    Reset();

    _PCMSampleRate = FREQUENCY_44_1K;
    _PPZSampleRate = FREQUENCY_44_1K;

    _TimerBTempo = 0x100;

    _UseInterpolation    = false;

    _UseInterpolationP86 = false;
    _UseInterpolationPPS = false;
    _UseInterpolationPPZ = false;

    _StopAfterFadeout = true;

    _P86->Initialize(_PCMSampleRate, false);
    _PPS->Initialize(_PCMSampleRate, false);
    _PPZ8->Initialize(_PCMSampleRate, false);

    if (!_OPNAW->Initialize(OPNAClock, FREQUENCY_55_5K, false, DirectoryPathDrums))
        return false;

    {
        _OPNAW->SetFMDelay(0);
        _OPNAW->SetSSGDelay(0);
        _OPNAW->SetRhythmDelay(0);
        _OPNAW->SetADPCMDelay(0);

        // Initialize the ADPCM RAM.
        {
            uint8_t Page[0x400]; // 0x400 * 0x100 = 0x40000(256K)

            ::memset(Page, 0x08, sizeof(Page));

            for (size_t i = 0; i < 0x100; ++i)
                WritePCMData((uint16_t)(i * sizeof(Page) / 32), (uint16_t)((i + 1) * sizeof(Page) / 32), Page);
        }

        _OPNAW->SetFMVolume(0);
        _OPNAW->SetSSGVolume(-18);
        _OPNAW->SetADPCMVolume(0);
        _OPNAW->SetRhythmVolume(0);

        _PPZ8->SetVolume(0);

        _PPS->SetVolume(0);
        _P86->InitializeVolume(0);

        _OPNAW->SetFMDelay(DEFAULT_REG_WAIT);
        _OPNAW->SetSSGDelay(DEFAULT_REG_WAIT);
        _OPNAW->SetADPCMDelay(DEFAULT_REG_WAIT);
        _OPNAW->SetRhythmDelay(DEFAULT_REG_WAIT);
    }

    // Initial setting of 088/188/288/388 (same INT number only)
    _OPNAW->SetReg(0x29, 0x00); // Reset SCH and IRQ Enable

    _OPNAW->SetReg(0x24, 0x00); // Timer A Upper 8 bits
    _OPNAW->SetReg(0x25, 0x00); // Timer A Lower 2 bits

    _OPNAW->SetReg(0x26, 0x00); // Timer B Data
    _OPNAW->SetReg(0x27, 0x3F); // Timer A/B Control (--XXXXXX, Reset A/B, Enable A/B, Load A/B) / Disable 3 CH Mode.

    // Start the OPN interrupt.
    StartOPNInterrupt();

    return true;
}

/// <summary>
/// Returns true if the data is PMD data.
/// </summary>
bool pmd_driver_t::IsPMD(const uint8_t * data, size_t size) noexcept
{
    if (size < 3)
        return false;

    if (size > sizeof(_MData))
        return false;

    // File version. 0x00 for standard .M or .M2 files targeting PC-98/PC-88/X68000 systems, Must be 0xFF for files targeting FM Towns hardware.
    if (data[0] > 0x0F && data[0] != 0xFF)
        return false;

    if (data[1] != 0x18 && data[1] != 0x1A)
        return false;

    if (data[2] != 0x00 && data[2] != 0xE6)
        return false;

    return true;
}

/// <summary>
/// Loads the data into the driver.
/// </summary>
int32_t pmd_driver_t::Load(const uint8_t * data, size_t size) noexcept
{
    if (!IsPMD(data, size))
        return ERR_UNKNOWN_FORMAT;

    Stop();

    ::memcpy(_MData, data, size);
    ::memset(_MData + size, 0, sizeof(_MData) - size);

    if (_SearchPath.size() == 0)
        return ERR_SUCCESS;

    int32_t Result = ERR_SUCCESS;

    char FileName[MAX_PATH] = { 0 };
    WCHAR FileNameW[MAX_PATH] = { 0 };

    WCHAR FilePath[MAX_PATH] = { 0 };

    // Get the PCM file path.
    {
        _PCMFilePath.clear();

        GetText(_MData, size, 0, FileName, _countof(FileName));

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

            _PCMFileName = FileNameW;

            // P86 import (ADPCM, 8-bit sample playback, stereo, panning)
            if (HasExtension(FileNameW, _countof(FileNameW), L".P86")) // Is it a Professional Music Driver P86 Samples Pack file?
            {
                FindFile(FileNameW, FilePath, _countof(FilePath));

                Result = _P86->Load(FilePath);

                if (Result == P86_SUCCESS || Result == P86_ALREADY_LOADED)
                {
                    _IsUsingP86 = true;

                    _PCMFilePath = FilePath;
                }
            }
            else
            // PPC import (ADPCM, 4-bit sample playback, 256kB max.)
            if (HasExtension(FileNameW, _countof(FileNameW), L".PPC"))
            {
                FindFile(FileNameW, FilePath, _countof(FilePath));

                Result = LoadPPC(FilePath);

                if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
                {
                    _IsUsingP86 = false;

                    _PCMFilePath = FilePath;
                }
            }
        }
        else
            _PCMFileName.clear();
    }

    // Get the PPS file path.
    {
        _PPSFilePath.clear();

        GetText(_MData, size, -1, FileName, _countof(FileName));

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

            _PPSFileName = FileNameW;

            // PPS import (PCM driver for the SSG, which allows 4-bit 16000Hz PCM playback on the SSG Channel 3. It can also play 2 samples simultanelously, but at a lower quality)
            if (HasExtension(FileNameW, _countof(FileNameW), L".PPS"))
            {
                FindFile(FileNameW, FilePath, _countof(FilePath));

                Result = _PPS->Load(FilePath);

                if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
                    _PPSFilePath = FilePath;
            }
        }
        else
            _PPSFileName.clear();
    }

    // Get the PPZ file paths.
    {
        _PPZFilePath[0].clear();
        _PPZFilePath[1].clear();

        GetText(_MData, size, -2, FileName, _countof(FileName));

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

            WCHAR * p = FileNameW;

            for (size_t i = 0; (*p != '\0') && (i < _countof(_PPZFilePath)); ++i)
            {
                {
                    WCHAR * q = ::wcschr(p, ',');

                    if (q != nullptr)
                        *q = '\0';
                }

                _PPZFileName[i] = p;

                // PZI import (Up to 8 PCM channels using the 86PCM, with soft panning possibilities and no memory limit) / PVI import (ADPCM)
                if (HasExtension(p, _countof(FileNameW) - ::wcslen(p), L".PZI") || HasExtension(p, _countof(FileNameW) - ::wcslen(p), L".PVI"))
                {
                    FindFile(p, FilePath, _countof(FilePath));

                    Result = _PPZ8->Load(FilePath, i);

                    if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
                        _PPZFilePath[i] = FilePath;
                }
                else
                // PMB import
                if (HasExtension(p, _countof(FileNameW) - ::wcslen(p), L".PMB"))
                {
                    RenameExtension(p, _countof(FileNameW) - ::wcslen(p), L".PZI");

                    FindFile(p, FilePath, _countof(FilePath));

                    Result = _PPZ8->Load(FilePath, i);

                    if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
                        _PPZFilePath[i] = FilePath;
                }

                p += ::wcslen(p) + 1;
            }
        }
    }

    return ERR_SUCCESS;
}

/// <summary>
/// Starts the driver.
/// </summary>
void pmd_driver_t::Start() noexcept
{
    if (_InTimerAInterrupt || _InTimerBInterrupt)
    {
        _Driver._Flags |= DriverStartRequested; // Delay the start of the driver until timer processing has finished.

        return;
    }

    DriverStart();
}

/// <summary>
/// Stops the driver.
/// </summary>
void pmd_driver_t::Stop() noexcept
{
    if (_InTimerAInterrupt || _InTimerBInterrupt)
    {
        _Driver._Flags |= DriverStopRequested; // Delay stopping the driver until timer processing has finished.
    }
    else
    {
        _State._IsFadeOutSpeedSet = false;

        DriverStop();
    }

    ::memset(_SrcFrames, 0, sizeof(_SrcFrames));
    _Position = 0;
}

/// <summary>
/// Renders a chunk of PCM data.
/// </summary>
void pmd_driver_t::Render(int16_t * frames, size_t frameCount) noexcept
{
    size_t FramesDone = 0;

    do
    {
        if (_FramesToDo >= frameCount - FramesDone)
        {
            ::memcpy(frames, _FramePtr, (frameCount - FramesDone) * sizeof(frame16_t));

            _FramesToDo -= (frameCount - FramesDone);
            _FramePtr   += (frameCount - FramesDone);

            FramesDone = frameCount;
        }
        else
        {
            {
                ::memcpy(frames, _FramePtr, _FramesToDo * sizeof(frame16_t));

                frames += (_FramesToDo * 2);

                _FramePtr = _SrcFrames;
                FramesDone += _FramesToDo;
            }

            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerAInterrupt();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerBInterrupt();

            _OPNAW->SetReg(0x27, _State._FMChannel3Mode | 0x30); // Reset both timer A and B.

            const uint32_t NextTick = _OPNAW->GetNextTick(); // in μs

            _OPNAW->AdvanceTimers(NextTick);

            {
                _FramesToDo = (size_t) ((double) (NextTick * _PCMSampleRate) / 1'000'000.0);

                ::memset(_DstFrames, 0, _FramesToDo * sizeof(frame32_t));

                if (_PCMSampleRate == _PPZSampleRate)
                    _PPZ8->Mix(_DstFrames, _FramesToDo);
                else
                {
                    // PCM frequency transform of ppz8 (no interpolation)
                    size_t SampleCount = (size_t) (_FramesToDo * _PPZSampleRate / _PCMSampleRate + 1);
                    int32_t delta = (int32_t) (8192 * _PPZSampleRate / _PCMSampleRate);

                    ::memset(_TmpFrames, 0, SampleCount * sizeof(sample_t) * 2);

                    _PPZ8->Mix(_TmpFrames, SampleCount);

                    int32_t Carry = 0;

                    // Frequency transform (1 << 13 = 8192)
                    for (size_t i = 0; i < _FramesToDo; ++i)
                    {
                        _DstFrames[i].Left  = _TmpFrames[(Carry >> 13)].Left;
                        _DstFrames[i].Right = _TmpFrames[(Carry >> 13)].Right;

                        Carry += delta;
                    }
                }
            }

            {
                _OPNAW->Mix((sample_t *) _DstFrames, _FramesToDo);

                if (_UsePPSForDrums)
                    _PPS->Mix(_DstFrames, _FramesToDo);

                if (_IsUsingP86)
                    _P86->Mix(_DstFrames, _FramesToDo);
            }

            _Position += NextTick;

            {
                if (_State._FadeOutSpeedHQ > 0)
                {
                    int32_t Factor = (_State._LoopCount != -1) ? (int32_t) ((1 << 10) * ::pow(512, -(double) (_Position - _FadeOutPosition) / 1'000 / _State._FadeOutSpeedHQ)) : 0;

                    for (size_t i = 0; i < _FramesToDo; ++i)
                    {
                        _SrcFrames[i].Left  = (int16_t) std::clamp(_DstFrames[i].Left  * Factor >> 10, -32768, 32767);
                        _SrcFrames[i].Right = (int16_t) std::clamp(_DstFrames[i].Right * Factor >> 10, -32768, 32767);
                    }

                    // Fadeout end
                    if ((_Position - _FadeOutPosition > (int64_t) _State._FadeOutSpeedHQ * 1'000) && _StopAfterFadeout)
                        _Driver._Flags |= DriverStopRequested;
                }
                else
                {
                    for (size_t i = 0; i < _FramesToDo; ++i)
                    {
                        _SrcFrames[i].Left  = (int16_t) std::clamp(_DstFrames[i].Left,  -32768, 32767);
                        _SrcFrames[i].Right = (int16_t) std::clamp(_DstFrames[i].Right, -32768, 32767);
                    }
                }
            }
        }
    }
    while (FramesDone < frameCount);
}

/// <summary>
/// Gets the current loop number.
/// </summary>
uint32_t pmd_driver_t::GetLoopNumber() const noexcept
{
    return (uint32_t) _State._LoopCount;
}

/// <summary>
/// Gets the length of the song and loop part (in ms).
/// </summary>
bool pmd_driver_t::GetLength(uint32_t & songLength, uint32_t & loopLength, uint32_t & songTicks, uint32_t & loopTicks) noexcept
{
    DriverStart();

    _Position = 0;
    songLength = 0;

    // Save the original delay values.
    const int32_t FMDelay    = _OPNAW->GetFMDelay();
    const int32_t SSGDelay   = _OPNAW->GetSSGDelay();
    const int32_t ADPCMDelay = _OPNAW->GetADPCMDelay();
    const int32_t RSSDelay   = _OPNAW->GetRSSDelay();

    _OPNAW->SetFMDelay(0);
    _OPNAW->SetSSGDelay(0);
    _OPNAW->SetADPCMDelay(0);
    _OPNAW->SetRhythmDelay(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerAInterrupt();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerBInterrupt();

            _OPNAW->SetReg(0x27, _State._FMChannel3Mode | 0x30); // Reset both timer A and B.

            const uint32_t NextTick = _OPNAW->GetNextTick(); // in μs

            _OPNAW->AdvanceTimers(NextTick);

            _Position += NextTick;
        }

        if ((_State._LoopCount == 1) && (songLength == 0)) // When looping
        {
            songLength = (uint32_t) _Position / 1'000;
            songTicks  = GetPositionInTicks();
        }
        else
        if (_State._LoopCount == -1) // End without loop
        {
            songLength = (uint32_t) _Position / 1'000; // in ms
            songTicks  = GetPositionInTicks(); // in ticks

            loopLength = 0; // in ms
            loopTicks  = 0; // in ticks

            goto Stop;
        }
        else
        if (GetPositionInTicks() >= 65536) // Forced termination.
        {
            songLength = (uint32_t) _Position / 1'000;
            songTicks  = GetPositionInTicks();

            loopLength = songLength;
            loopTicks  = songTicks;

            goto Stop;
        }
    }
    while (_State._LoopCount < 2);

    loopLength = (uint32_t) (_Position / 1'000) - songLength; // in ms
    loopTicks  = GetPositionInTicks() - songTicks; // in ticks

Stop:
    DriverStop();

    // Restore the original delay values.
    _OPNAW->SetFMDelay(FMDelay);
    _OPNAW->SetSSGDelay(SSGDelay);
    _OPNAW->SetADPCMDelay(ADPCMDelay);
    _OPNAW->SetRhythmDelay(RSSDelay);

    return true;
}

/// <summary>
/// Gets the playback position (in ms)
/// </summary>
uint32_t pmd_driver_t::GetPosition() const noexcept
{
    return (uint32_t) (_Position / 1'000);
}

/// <summary>
/// Sets the playback position (in ms)
/// </summary>
void pmd_driver_t::SetPosition(uint32_t position)
{
    int64_t NewPosition = (int64_t) position * 1'000; // Convert ms to μs.

    if (_Position > NewPosition)
    {
        DriverStart();

        _FramePtr = _SrcFrames;

        _FramesToDo = 0;
        _Position = 0;
    }

    while (_Position < NewPosition)
    {
        if (_OPNAW->ReadStatus() & 0x01)
            HandleTimerAInterrupt();

        if (_OPNAW->ReadStatus() & 0x02)
            HandleTimerBInterrupt();

        _OPNAW->SetReg(0x27, _State._FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)

        const uint32_t TickCount = _OPNAW->GetNextTick();

        _OPNAW->AdvanceTimers(TickCount);

        _Position += TickCount;
    }

    if (_State._LoopCount == -1)
        Mute();

    _OPNAW->ClearBuffer();
}

/// <summary>
/// Gets the playback position (in ticks)
/// </summary>
uint32_t pmd_driver_t::GetPositionInTicks() const noexcept
{
    return (uint32_t) ((_State._BarCounter * _State._BarLength) + _State._OpsCounter);
}

/// <summary>
/// Sets the playback position (in ticks).
/// </summary>
void pmd_driver_t::SetPositionInTicks(int tickCount)
{
    if (((_State._BarLength * _State._BarCounter) + _State._OpsCounter) > tickCount)
    {
        DriverStart();

        _FramePtr = _SrcFrames;
        _FramesToDo = 0;
    }

    while (((_State._BarLength * _State._BarCounter) + _State._OpsCounter) < tickCount)
    {
        if (_OPNAW->ReadStatus() & 0x01)
            HandleTimerAInterrupt();

        if (_OPNAW->ReadStatus() & 0x02)
            HandleTimerBInterrupt();

        _OPNAW->SetReg(0x27, _State._FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)

        const uint32_t TickCount = _OPNAW->GetNextTick();

        _OPNAW->AdvanceTimers(TickCount);
    }

    if (_State._LoopCount == -1)
        Mute();

    _OPNAW->ClearBuffer();
}

/// <summary>
/// Sets the PCM search paths.
/// </summary>
bool pmd_driver_t::SetSearchPaths(std::vector<const WCHAR *> & paths)
{
    for (std::vector<const WCHAR *>::iterator iter = paths.begin(); iter < paths.end(); iter++)
    {
        WCHAR Path[MAX_PATH];

        ::wcscpy_s(Path, _countof(Path), *iter);
        AddBackslash(Path, _countof(Path));

        _SearchPath.push_back(Path);
    }

    return true;
}

/// <summary>
/// Sets the output frequency at which raw PCM data is synthesized (in Hz, for example 44100).
/// </summary>
void pmd_driver_t::SetSampleRate(uint32_t sampleRate) noexcept
{
    if (sampleRate == FREQUENCY_55_5K || sampleRate == FREQUENCY_55_4K)
    {
        _PCMSampleRate =
        _PPZSampleRate = FREQUENCY_44_1K;
        _UseInterpolation = true;
    }
    else
    {
        _PCMSampleRate =
        _PPZSampleRate = sampleRate;
        _UseInterpolation = false;
    }

    _OPNAW->Initialize(OPNAClock, _PCMSampleRate, _UseInterpolation);

    _P86->SetSampleRate(_PCMSampleRate, _UseInterpolationP86);
    _PPS->SetSampleRate(_PCMSampleRate, _UseInterpolationPPS);
    _PPZ8->SetSampleRate(_PPZSampleRate, _UseInterpolationPPZ);
}

/// <summary>
/// Enables or disables interpolation to 55kHz output frequency.
/// </summary>
void pmd_driver_t::SetInterpolation(bool value)
{
    if (value == _UseInterpolation)
        return;

    _UseInterpolation = value;

    _OPNAW->Initialize(OPNAClock, _PCMSampleRate, _UseInterpolation);
}

/// <summary>
/// Sets the output frequency at which raw PPZ data is synthesized (in Hz, for example 44100).
/// </summary>
void pmd_driver_t::SetPPZSampleRate(uint32_t sampleRate) noexcept
{
    if (sampleRate == _PPZSampleRate)
        return;

    _PPZSampleRate = sampleRate;

    _PPZ8->SetSampleRate(sampleRate, _UseInterpolationPPZ);
}

/// <summary>
/// Enables or disables PPZ interpolation.
/// </summary>
void pmd_driver_t::SetPPZInterpolation(bool flag)
{
    _UseInterpolationPPZ = flag;

    _PPZ8->SetSampleRate(_PPZSampleRate, flag);
}

/// <summary>
/// Enables or disables PPS interpolation.
/// </summary>
void pmd_driver_t::SetPPSInterpolation(bool flag)
{
    _UseInterpolationPPS = flag;

    _PPS->SetSampleRate(_PCMSampleRate, flag);
}

/// <summary>
/// Enables or disables P86 interpolation.
/// </summary>
void pmd_driver_t::SetP86Interpolation(bool flag)
{
    _UseInterpolationP86 = flag;

    _P86->SetSampleRate(_PCMSampleRate, flag);
}

/// <summary>
/// Sets the fade out speed (PMD compatible)
/// </summary>
void pmd_driver_t::SetFadeOutSpeed(int speed)
{
    _State._FadeOutSpeed = speed;
}

/// <summary>
/// Sets the fade out speed (High quality sound)
/// </summary>
void pmd_driver_t::SetFadeOutDurationHQ(int value)
{
    if (value > 0)
    {
        if (_State._FadeOutSpeedHQ == 0)
            _FadeOutPosition = _Position;

        _State._FadeOutSpeedHQ = value;
    }
    else
        _State._FadeOutSpeedHQ = 0; // Fadeout forced stop
}

/// <summary>
/// Enables or disables the use of the PPS.
/// </summary>
void pmd_driver_t::UsePPSForDrums(bool value) noexcept
{
    _UsePPSForDrums = value;
}

/// <summary>
/// Enables or disables the use of the SSG for drums.
/// </summary>
void pmd_driver_t::UseSSGForDrums(bool value) noexcept
{
    _UseSSGForDrums = value;
}

/// <summary>
/// Disables the specified channel.
/// </summary>
int pmd_driver_t::DisableChannel(int channel)
{
    if (channel >= MaxChannels)
        return ERR_WRONG_PARTNO;

    if (ChannelTable[channel][0] < 0)
    {
        _RhythmMask = 0x00;

        _OPNAW->SetReg(0x10, 0xFF); // Rhytm Part: Dump and disable all Rhythm Sound Source channels.

        return ERR_SUCCESS;
    }

    int OldFMSelector = _Driver._FMSelector;

    if (IsPlaying() && (_State._Channels[channel]->_PartMask == 0x00))
    {
        switch (ChannelTable[channel][2])
        {
            case 0:
            {
                _Driver._CurrentChannel = ChannelTable[channel][1];
                _Driver._FMSelector = 0x000;

                MuteFMChannel(_State._Channels[channel]);
                break;
            }

            case 1:
            {
                _Driver._CurrentChannel = ChannelTable[channel][1];
                _Driver._FMSelector = 0x100;

                MuteFMChannel(_State._Channels[channel]);
                break;
            }

            case 2:
            {
                _Driver._CurrentChannel = ChannelTable[channel][1];

                uint32_t Mask = 1u << (_Driver._CurrentChannel - 1); // Tone mask (-----xxx)

                Mask |= (Mask << 3); // Noise mask (--xxx---)

                _OPNAW->SetReg(0x07, Mask | _OPNAW->GetReg(0x07)); // Turns the specified SSG channel off (KeyOff)
                break;
            }

            case 3:
            {
                _OPNAW->SetReg(0x101, 0x02); // Set ADPCM Control 2: PAN UNSET / x8 bit mode
                _OPNAW->SetReg(0x100, 0x01); // Set ADPCM Control 1
                break;
            }

            case 4:
            {
                if (_SSGEffect._Number < 11)
                    SSGStopEffect();
                break;
            }

            case 5:
            {
                _PPZ8->Stop((size_t) ChannelTable[(size_t) channel][1]);
                break;
            }
        }
    }

    _State._Channels[channel]->_PartMask |= 0x01;

    _Driver._FMSelector = OldFMSelector;

    return ERR_SUCCESS;
}

/// <summary>
/// Enables the specified channel.
/// </summary>
int pmd_driver_t::EnableChannel(int channel)
{
    if (channel >= MaxChannels)
        return ERR_WRONG_PARTNO;

    if (ChannelTable[channel][0] < 0)
    {
        _RhythmMask = 0xFF;

        return ERR_SUCCESS;
    }

    if (_State._Channels[channel]->_PartMask == 0x00)
        return ERR_NOT_MASKED;

    if ((_State._Channels[channel]->_PartMask &= 0xFE) != 0)
        return ERR_EFFECT_USED;

    if (!IsPlaying())
        return ERR_MUSIC_STOPPED;

    int OldFMSelector = _Driver._FMSelector;

    if (_State._Channels[channel]->_Data != nullptr)
    {
        if (ChannelTable[channel][2] == 0) // FM sound source (Front)
        {   
            _Driver._FMSelector = 0x000;
            _Driver._CurrentChannel = ChannelTable[channel][1];

            ResetFMInstrument(_State._Channels[channel]);
        }
        else
        if (ChannelTable[channel][2] == 1) // FM sound source (Back)
        {   
            _Driver._FMSelector = 0x100;
            _Driver._CurrentChannel = ChannelTable[channel][1];

            ResetFMInstrument(_State._Channels[channel]);
        }
    }

    _Driver._FMSelector = OldFMSelector;

    return ERR_SUCCESS;
}

/// <summary>
/// Gets the text of the specified memo.
/// </summary>
bool pmd_driver_t::GetMemo(const uint8_t * data, size_t size, int index, char * text, size_t textSize)
{
    if ((text == nullptr) || (textSize < 1))
        return false;

    text[0] = '\0';

    char TwoByteZen[1024 + 64];

    GetText(data, size, index, TwoByteZen, _countof(TwoByteZen));

    char Han[1024 + 64];

    Zen2ToHan(Han, TwoByteZen);

    RemoveEscapeSequences(text, Han);

    return true;
}

/// <summary>
/// Gets the specified channel object.
/// </summary>
channel_t * pmd_driver_t::GetChannel(int channelNumber) const noexcept
{
    if (channelNumber >= (int32_t) _countof(_State._Channels))
        return nullptr;

    return _State._Channels[channelNumber];
}

#pragma endregion

/// <summary>
/// Resets the driver.
/// </summary>
void pmd_driver_t::Reset()
{
    UsePPSForDrums(false);
    UseSSGForDrums(false);

    ::memset(_FMChannels, 0, sizeof(_FMChannels));
    ::memset(_SSGChannels, 0, sizeof(_SSGChannels));

    _ADPCMChannel = { };
    _RhythmChannel = { };

    ::memset(_FMExtensionChannels, 0, sizeof(_FMExtensionChannels));

    _SSGEffectChannel = { };

    ::memset(_PPZ8Channels, 0, sizeof(_PPZ8Channels));

    _DummyChannel = { };

    ::memset(_FMSlotKey1, 0, _countof(_FMSlotKey1));
    ::memset(_FMSlotKey2, 0, _countof(_FMSlotKey2));

    ::memset(&_SampleBank, 0, sizeof(_SampleBank));

    ::memset(_SrcFrames, 0, sizeof(_SrcFrames));
    ::memset(_DstFrames, 0, sizeof(_SrcFrames));
    ::memset(_TmpFrames, 0, sizeof(_TmpFrames));

    _FramePtr = _SrcFrames;
    
    _FramesToDo = 0;
    _Position = 0;
    _FadeOutPosition = 0;
    _Seed = 0;

    // Create some mock PMD data.
    {
        ::memset(_MData, 0, sizeof(_MData));
        ::memset(_VData, 0, sizeof(_VData));
        ::memset(_EData, 0, sizeof(_EData));

        for (size_t i = 0; i < 12; i += 2)
        {
            _MData[i + 1] = 0x18;
            _MData[i + 2] = 0x00;
        }

        _MData[25] = 0x80;
    }

    _InTimerAInterrupt = false;
    _InTimerBInterrupt = false;

    _IsUsingP86 = false;

    _RhythmVolume = 0x3C;

    // Initialize the state.
    _State.Initialize();

    _State._MData = &_MData[1];
    _State._VData = _VData;
    _State._EData = _EData;

    {
        _State._Channels[ 0] = &_FMChannels[0];
        _State._Channels[ 1] = &_FMChannels[1];
        _State._Channels[ 2] = &_FMChannels[2];
        _State._Channels[ 3] = &_FMChannels[3];
        _State._Channels[ 4] = &_FMChannels[4];
        _State._Channels[ 5] = &_FMChannels[5];

        _State._Channels[ 6] = &_SSGChannels[0];
        _State._Channels[ 7] = &_SSGChannels[1];
        _State._Channels[ 8] = &_SSGChannels[2];

        _State._Channels[ 9] = &_ADPCMChannel;

        _State._Channels[10] = &_RhythmChannel;

        _State._Channels[11] = &_FMExtensionChannels[0];
        _State._Channels[12] = &_FMExtensionChannels[1];
        _State._Channels[13] = &_FMExtensionChannels[2];

        _State._Channels[14] = &_DummyChannel; // Unused
        _State._Channels[15] = &_SSGEffectChannel;

        _State._Channels[16] = &_PPZ8Channels[0];
        _State._Channels[17] = &_PPZ8Channels[1];
        _State._Channels[18] = &_PPZ8Channels[2];
        _State._Channels[19] = &_PPZ8Channels[3];
        _State._Channels[20] = &_PPZ8Channels[4];
        _State._Channels[21] = &_PPZ8Channels[5];
        _State._Channels[22] = &_PPZ8Channels[6];
        _State._Channels[23] = &_PPZ8Channels[7];
    }

    _Driver._Flags = DriverIdle;
}

/// <summary>
/// Starts the OPN interrupt.
/// </summary>
void pmd_driver_t::StartOPNInterrupt()
{
    ::memset(_FMChannels, 0, sizeof(_FMChannels));
    ::memset(_SSGChannels, 0, sizeof(_SSGChannels));

    _ADPCMChannel = { };
    _RhythmChannel = { };

    ::memset(_FMExtensionChannels, 0, sizeof(_FMExtensionChannels));

    _SSGEffectChannel = { };

    ::memset(_PPZ8Channels, 0, sizeof(_PPZ8Channels));

    _DummyChannel = { };

    _RhythmMask = 0xFF;

    InitializeState();
    InitializeOPNA();

    // Disable the SSG channels (X-XXXXXX, IOB + Noise A, B and C + Tone A, B and C
    _OPNAW->SetReg(0x07, 0xBF);

    DriverStop();

    InitializeTimers();

    // Enable 3-channel mode with enhanced features (SCH, X-------) and IRQ (------XX)
    _OPNAW->SetReg(0x29, 0x83);
}

/// <summary>
/// Initializes the different state machines.
/// </summary>
void pmd_driver_t::InitializeState()
{
    _State._FadeOutVolume = 0;
    _State._FadeOutSpeed = 0;
    _State._IsFadeOutSpeedSet = false;
    _State._FadeOutSpeedHQ = 0;

    _State._LoopCount = 0;
    _State._BarCounter = 0;
    _State._OpsCounter = 0;
    _State._TimerACounter = 0;

    _State._PCMBegin = 0;
    _State._PCMEnd = 0;

    _State._UseRhythmChannel = false;
    _RhythmChannelMask = 0;

    _State._FMSlot1Detune = 0;
    _State._FMSlot2Detune = 0;
    _State._FMSlot3Detune = 0;
    _State._FMSlot4Detune = 0;

    _State._FMChannel3Mode = 0x3F;
    _State._BarLength = 96;

    _State._FMVolumeAdjust = _State._FMVolumeAdjustDefault;
    _State._SSGVolumeAdjust = _State._SSGVolumeAdjustDefault;
    _State._ADPCMVolumeAdjust = _State._ADPCMVolumeAdjustDefault;
    _State._PPZ8VolumeAdjust = _State._PPZ8VolumeAdjustDefault;
    _State._RhythmVolumeAdjust = _State._RhythmVolumeAdjustDefault;

    _State._IsCompatibleWithPMDB2 = _State._IsCompatibleWithPMDB2Default;

    for (auto & FMChannel : _FMChannels)
    {
        const int32_t PartMask  = FMChannel._PartMask;
        const int32_t KeyOnFlag = FMChannel._KeyOnFlag;

        FMChannel =
        {
            ._PartMask    = PartMask & 0x0F,
            ._KeyOnFlag   = KeyOnFlag,
            ._Tone        = 0xFF,
            ._DefaultTone = 0xFF
        };
    }

    for (auto & SSGChannel : _SSGChannels)
    {
        const int32_t PartMask  = SSGChannel._PartMask;
        const int32_t KeyOnFlag = SSGChannel._KeyOnFlag;

        SSGChannel =
        {
            ._PartMask    = PartMask & 0x0F,
            ._KeyOnFlag   = KeyOnFlag,
            ._Tone        = 0xFF,
            ._DefaultTone = 0xFF
        };
    }

    {
        const int32_t PartMask  = _ADPCMChannel._PartMask;
        const int32_t KeyOnFlag = _ADPCMChannel._KeyOnFlag;

        _ADPCMChannel =
        {
            ._PartMask    = PartMask & 0x0F,
            ._KeyOnFlag   = KeyOnFlag,
            ._Tone        = 0xFF,
            ._DefaultTone = 0xFF
        };
    }

    {
        const int32_t PartMask  = _RhythmChannel._PartMask;
        const int32_t KeyOnFlag = _RhythmChannel._KeyOnFlag;

        _RhythmChannel =
        {
            ._PartMask    = PartMask & 0x0F,
            ._KeyOnFlag   = KeyOnFlag,
            ._Tone        = 0xFF,
            ._DefaultTone = 0xFF
        };
    }

    for (auto & FMExtensionChannel : _FMExtensionChannels)
    {
        const int32_t PartMask  = FMExtensionChannel._PartMask;
        const int32_t KeyOnFlag = FMExtensionChannel._KeyOnFlag;

        FMExtensionChannel =
        {
            ._PartMask    = PartMask & 0x0F,
            ._KeyOnFlag   = KeyOnFlag,
            ._Tone        = 0xFF,
            ._DefaultTone = 0xFF
        };
    }

    for (auto & PPZChannel : _PPZ8Channels)
    {
        const int32_t PartMask  = PPZChannel._PartMask;
        const int32_t KeyOnFlag = PPZChannel._KeyOnFlag;

        PPZChannel =
        {
            ._PartMask    = PartMask & 0x0F,
            ._KeyOnFlag   = KeyOnFlag,
            ._Tone        = 0xFF,
            ._DefaultTone = 0xFF
        };
    }

    _Driver.Initialize();

    _OldInstrumentNumber = 0;
}

/// <summary>
/// Initializes the OPNA.
/// </summary>
void pmd_driver_t::InitializeOPNA()
{
    _OPNAW->ClearBuffer();

    // Enable 3-channel mode with enhanced features (SCH, X-------) and IRQ (------XX)
    _OPNAW->SetReg(0x29, 0x83);

    // Initialize the noise period (Noise Period Control, ---xxxxx)
    _SSGNoiseFrequency = 0;

    _OPNAW->SetReg(0x06, 0x00);

    _OldSSGNoiseFrequency = 0;

    // Reset the SSG Envelope Control (SSG-EG, 0x93, 0x97 and 0x9B are not used)
    for (uint32_t i = 0x090; i < 0x09F; ++i)
    {
        if (i % 4 != 0x03)
            _OPNAW->SetReg(i, 0x00);
    }

    // Reset the SSG Envelope Control (SSG-EG, 0x193, 0x197 and 0x19B are not used)
    for (uint32_t i = 0x190; i < 0x19F; ++i)
    {
        if (i % 4 != 0x03)
            _OPNAW->SetReg(i, 0x00);
    }

    // Initialize the hardware LFO.
    {
        _OPNAW->SetReg(0x0B4, 0xC0); // L/R (11000000)
        _OPNAW->SetReg(0x0B5, 0xC0); // L/R (11000000)
        _OPNAW->SetReg(0x0B6, 0xC0); // L/R (11000000)

        _OPNAW->SetReg(0x1B4, 0xC0); // L/R (11000000)
        _OPNAW->SetReg(0x1B5, 0xC0); // L/R (11000000)
        _OPNAW->SetReg(0x1B6, 0xC0); // L/R (11000000)

        _OPNAW->SetReg(0x22, 0x00); // Reset LFO FREQ CONTROL
    }

    // Initialize the Rhythm driver. Default = Pan : 0xC0 (3 << 6, Center) , Volume : 0x0F
    {
        for (auto & RhythmPanAndVolume : _State._RhythmPanAndVolumes)
            RhythmPanAndVolume = 0xCF;

        _OPNAW->SetReg(0x10, 0xFF); // Dump / Rhythm KON (X-XXXXXX)

        // Set the Rhythm volume.
        _RhythmVolume = 48 * 4 * (256 - _State._RhythmVolumeAdjust) / 1024;

        _OPNAW->SetReg(0x11, (uint32_t) _RhythmVolume);  // Rhythm Part: Set RTL (Total Level)
    }

    // PCM reset & LIMIT SET
    _OPNAW->SetReg(0x10C, 0xFF); // Set ADPCM Limit Address (Lo)
    _OPNAW->SetReg(0x10D, 0xFF); // Set ADPCM Limit Address (Hi)

    for (size_t i = 0; i < MaxPPZ8Channels; ++i)
        _PPZ8->SetPan(i, 5);
}

/// <summary>
/// Handles an interrupt from timer A.
/// </summary>
void pmd_driver_t::HandleTimerAInterrupt()
{
    _InTimerAInterrupt = true;

    {
        _State._TimerACounter++;

        if ((_State._TimerACounter & 0x07) == 0)
            Fade();

        if ((_SSGEffect._Priority != 0) && (!_UsePPSForDrums || (_SSGEffect._Number == 0x80)))
            SSGPlayEffect();
    }

    _InTimerAInterrupt = false;
}

/// <summary>
/// Handles an interrupt from timer B.
/// </summary>
void pmd_driver_t::HandleTimerBInterrupt()
{
    _InTimerBInterrupt = true;

    if (_Driver._Flags != DriverIdle)
    {
        if (_Driver._Flags & DriverStartRequested)
            DriverStart();

        if (_Driver._Flags & DriverStopRequested)
            DriverStop();
    }

    if (IsPlaying())
    {
        DriverMain();

        SetTimerBTempo();
        IncreaseBarCounter();

        _Driver._OldTimerACounter = _State._TimerACounter;
    }

    _InTimerBInterrupt = false;
}

/// <summary>
/// Executes a command. The execution is the same for all sound sources.
/// </summary>
uint8_t * pmd_driver_t::ExecuteCommand(channel_t * channel, uint8_t * si, uint8_t command)
{
    switch (command)
    {
        case 0xFF: si++; break;
        case 0xFE: si++; break;

        // 5.2. Volume Setting 2, Set the volume finely, 0–127 (FM) / 0–255 (PCM), 0–15 (SSG, SSG rhythm, PPZ), Command 'V number'
        case 0xFD:
        {
            channel->_Volume = *si++;
            break;
        }

        // 11.1. Tempo Setting 1 / 11.2. Tempo Setting 2
        case 0xFC:
        {
            si = SetTempoCommand(si);
            break;
        }

        // 4.10. Tie/Slur Setting, Connects the sound before and after as a tie (&). Keyoff will not be done on the previous note. 
        case 0xFB:
        {
            _Driver._IsTieSet = true;
            break;
        }

        // 7.1. Detune Setting, Sets the detune (frequency shift value), Command 'D number' / Command 'DD number'
        case 0xFA:
        {
            channel->_DetuneValue = *(int16_t *) si;
            si += 2;
            break;
        }

        // 10.1. Local Loop Setting, Start loop, Command '['
        case 0xF9:
        {
            si = SetStartOfLoopCommand(channel, si);
            break;
        }

        // 10.1. Local Loop Setting, End loop, Command ']'
        case 0xF8:
        {
            si = SetEndOfLoopCommand(channel, si);
            break;
        }

        // 10.1. Local Loop Setting, Exit loop, Command ':'
        case 0xF7:
        {
            si = ExitLoopCommand(channel, si);
            break;
        }

        // 10.2. Global Loop Setting, Command 'L'
        case 0xF6:
        {
            channel->_LoopData = si;

            // Prevent an endless loop.
            if (*channel->_LoopData == 0x80)
                channel->_LoopData = nullptr;
            break;
        }

        // 4.14. Modulation Setting, Command '_ number'
        case 0xF5:
        {
            channel->Transposition1 = *(int8_t *) si++;
            break;
        }

        // 5.5. Relative Volume Change, Increase volume by 3dB.
        case 0xF4:
        {
            channel->_Volume += 16;

            if (channel->_Volume > 255)
                channel->_Volume = 255;
            break;
        }

        // 5.5. Relative Volume Change, Decrease volume by 3dB.
        case 0xF3:
        {
            channel->_Volume -= 16;

            if (channel->_Volume < 16)
                channel->_Volume = 0;
            break;
        }

        // 9.1. Software LFO Setting, Set software LFO 1, Command 'MA number1, number2, number3, number4'
        case 0xF2:
        {
            si = LFO1SetModulation(channel, si);
            break;
        }

        // 9.3. Software LFO Switch, Controls on/off and keyon synchronization of the software LFO, Command '*A number'
        case 0xF1:
        {
            si = LFO1SetSwitch(channel, si);
            break;
        }

        // 8.1. SSG/PCM Software Envelope Setting, Sets a software envelope (only for OPN/OPNA's SSG/ADPCM channels), Command 'E number1, number2, number3, number4'
        case 0xF0:
        {
            si = SSGSetEnvelope1(channel, si);
            break;
        }

        // 14.1. Rhythm Sound Source Shot/Dump Control
        case 0xEB:
        {
            si = RhythmControl(si);
            break;
        }

        // 14.3. Rhythm Sound Source Individual Volume Setting, Set the volume for an individual rhythm channel, Command 'v number'
        case 0xEA:
        {
            si = RhythmSetVolume(si);
            break;
        }

        // 14.4. Rhythm Sound Source Output Position Setting, Command '\lb \ls \lc \lh \lt \li \mb \ms \mc \mh \mt \mi \rb \rs \rc \rh \rt \ri'
        case 0xE9:
        {
            si = RhythmSetPan(si);
            break;
        }

        // 14.2. Rhythm Sound Source Master Volume Setting, Sets the master volume of the rhythm sound source, Command 'V number'
        case 0xE8:
        {
            si = RhythmSetMasterVolume(si);
            break;
        }

        // 4.14. Modulation Setting, Command '__ number'
        case 0xE7:
        {
            channel->Transposition1 += *(int8_t *) si++;
            break;
        }

        // 14.2. Rhythm Sound Source Master Volume Setting
        case 0xE6:
        {
            si = RhythmSetRelativeMasterVolume(si);
            break;
        }

        // 14.3. Rhythm Sound Source Individual Volume Setting
        case 0xE5:
        {
            si = RhythmSetRelativeVolume(si);
            break;
        }

        case 0xE4: si++; break;
        case 0xE1: si++; break;
        case 0xE0: si++; break;

        // 4.11. Whole Note Length Setting, Sets the length of a whole note. Equivalent to the #Zenlen command, Command 'C number'
        case 0xDF:
        {
            _State._BarLength = *si++;
            break;
        }

        // 5.5. Relative Volume Change, Command '( ^%number'
        case 0xDD:
        {
            si = DecreaseVolumeForNextNote(channel, si);
            break;
        }

        // 15.9. Write to Status1, Command '~ number'
        case 0xDC:
        {
            _Status1 = *si++;
            break;
        }

        // 15.9. Write to Status1, Command '~ ±number'
        case 0xDB:
        {
            _Status1 += *si++;
            break;
        }

        case 0xD9: si++; break;
        case 0xD8: si++; break;
        case 0xD7: si++; break;

        // 9.7. LFO Depth Temporal Change Setting, Sets the temporal change of depth (depth A) of LFO 1, Command 'MDA number1[,±number2[,number3]]'
        case 0xD6:
        {
            channel->_LFO1DepthSpeedCounter1 =
            channel->_LFO1DepthSpeedCounter2 = *si++;
            channel->_LFO1Depth              = *(int8_t *) si++;
            break;
        }

        // 7.1. Detune Setting, Sets the detune (frequency shift value), Command 'DD number'
        case 0xD5:
        {
            channel->_DetuneValue += *(int16_t *) si;
            si += 2;
            break;
        }

        // 15.5. SSG Sound Effect Playback, Play SSG sound effect, Command 'n number'
        case 0xD4:
        {
            si = SetSSGEffect(channel, si);
            break;
        }

        // 15.5. FM Sound Effect Playback, Play FM sound effect, Command 'N number'
        case 0xD3:
        {
            si = SetFMEffect(channel, si);
            break;
        }

        // 15.3. Fade Out Setting, Fades out from the specified position, Command 'F number'
        case 0xD2:
        {
            _State._FadeOutSpeed      = *si++;
            _State._IsFadeOutSpeedSet = true;
            break;
        }

        case 0xD1: si++; break;
        case 0xD0: si++; break;
        case 0xCF: si++; break;
        case 0xCE: si += 6; break;

        // 8.1. SSG/PCM Software Envelope Setting, Sets a software envelope (only for OPN/OPNA's SSG/ADPCM channels), Command 'E number1, number2, number3, number4, number5, number6'
        case 0xCD:
        {
            si = SSGSetEnvelope2(channel, si);
            break;
        }

        case 0xCC: si++; break;

        // 9.2. Software LFO Waveform Setting, Sets the LFO waveform, Command 'MW number' / 'MWA number'
        case 0xCB:
        {
            channel->_LFO1Waveform = *si++;
            break;
        }

        /// 9.5. Software LFO Speed Setting, Set the LFO 1 speed, Command 'MXA number'
        case 0xCA:
        {
            // If set to 0, the LFO speed is dependent on tempo. So if the tempo is slow the LFO will also be slow.
            // If set to 1, change the LFO by the M MA MB commands to an extended specification that makes it constant speed independent of tempo.
            channel->_ExtendMode = (channel->_ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);
            break;
        }

        case 0xC8: si += 3; break;
        case 0xC7: si += 3; break;
        case 0xC6: si += 6; break;
        case 0xC5: si++; break;

        // 4.12. Sound Cut Setting 1, Shortens the sustain duration of following notes by x/8-th's of the actual note length. Valid range for x = 0 - 8, Command 'Q [%] numerical value'
        case 0xC4:
        {
            // Set Early Key Off Timeout Percentage. Stops note (length * pp / 100h) ticks early, added to value of command FE.
            channel->EarlyKeyOffTimeoutPercentage = *si++;
            break;
        }

        // 9.13. Hardware LFO Delay Setting, Sets the hardware LFO delay, Command '#D number'
        case 0xC2:
        {
            channel->_LFO1DelayCounter =
            channel->_LFO1Delay = *si++;

            LFOReset(channel);
            break;
        }

        // 4.10. Tie/Slur Setting, Connects the sound before and after as a slur (&&). Keyoff will be done on the previous note. 
        case 0xC1: break;

        // 9.1. Software LFO Setting, Set software LFO 2, Command 'MB number1, number2, number3, number4'
        case 0xBF:
        {
            LFOSwap(channel);

            si = LFO1SetModulation(channel, si);

            LFOSwap(channel);
            break;
        }

        // 9.7. LFO Depth Temporal Change Setting, Sets the temporal change of depth (depth A) of LFO 2, Command 'MDB number1[,±number2[,number3]]'
        case 0xBD:
        {
            LFOSwap(channel);

            channel->_LFO1DepthSpeedCounter1 =
            channel->_LFO1DepthSpeedCounter2 = *si++;
            channel->_LFO1Depth              = *(int8_t *) si++;

            LFOSwap(channel);
            break;
        }

        // 9.2. Software LFO Waveform Setting, Sets the LFO waveform, Command 'MWB number'
        case 0xBC:
        {
            LFOSwap(channel);

            channel->_LFO1Waveform = *si++;

            LFOSwap(channel);
            break;
        }

        // 9.5. Software LFO Speed Setting, Set the LFO 2 speed, Command 'MXB number'
        case 0xBB:
        {
            LFOSwap(channel);

            // If set to 0, the LFO speed is dependent on tempo. So if the tempo is slow the LFO will also be slow.
            // If set to 1, change the LFO by the M MA MB commands to an extended specification that makes it constant speed independent of tempo.
            channel->_ExtendMode = (channel->_ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);

            LFOSwap(channel);
            break;
        }

        case 0xBA: si++; break;

        // 9.13. Hardware LFO Delay Setting, Sets the hardware LFO delay, Command '#D number'
        case 0xB9:
        {
            LFOSwap(channel);

            channel->_LFO1DelayCounter =
            channel->_LFO1Delay = *si++;

            LFOReset(channel);

            LFOSwap(channel);
            break;
        }

        case 0xB8: si += 2; break;

        /// 9.6. Dedicated Rise/Fall LFO Setting, Selects the rise/fall-type software LFO and turns it on, Command 'MPA ±number1', Range -128–+127
        case 0xB7:
        {
            si = LFOSetRiseFallType(channel, si);
            break;
        }

        case 0xB6: si++; break;
        case 0xB5: si += 2; break;
        case 0xB4: si += 16; break;

        // 4.13. Sound Cut Setting 2, Command 'q [number1][-[number2]] [,number3]' / Command 'q [l length[.]][-[l length]] [,l length[.]]'
        case 0xB3:
        {
            // Set Early Key Off Timeout 2. Stop note after n ticks or earlier depending on the result of B1/C4/FE happening first.
            channel->_EarlyKeyOffTimeout2 = *si++;
            break;
        }

        // 4.16. Master Modulation Setting, Default global transpose, at the beginning of all channels except the rhythm channel, Command '_M number'
        case 0xB2:
        {
            channel->Transposition2 = *(int8_t *) si++;
            break;
        }

        // 4.13. Sound Cut Setting 2, Command 'q [number1][-[number2]] [,number3]' / Command 'q [l length[.]][-[l length]] [,l length[.]]'
        case 0xB1:
        {
            // Set Early Key Off Timeout Randomizer Range. (0..tt ticks, added to the value of command C4 and FE)
            channel->EarlyKeyOffTimeoutRandomRange = *si++;
            break;
        }

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

/// <summary>
/// Increases the volume for the next note only. 5.5. Relative Volume Change, Command ') ^%number', Range 0 - 255
/// </summary>
uint8_t * pmd_driver_t::IncreaseVolumeForNextNote(channel_t * channel, uint8_t * si, int maxVolume)
{
    int32_t Volume = channel->_Volume + *si++ + 1;

    if (Volume > maxVolume)
        Volume = maxVolume;

    channel->VolumeBoost = Volume;
    _Driver._VolumeBoostCount = 1;

    return si;
}

/// <summary>
/// Decreases the volume for the next note only. 5.5. Relative Volume Change, Command '( ^%number', Range 0 - 255
/// </summary>
uint8_t * pmd_driver_t::DecreaseVolumeForNextNote(channel_t * channel, uint8_t * si)
{
    int32_t Volume = channel->_Volume - *si++;

    if (Volume < 1)
        Volume = 1;

    channel->VolumeBoost = Volume;
    _Driver._VolumeBoostCount = 1;

    return si;
}

/// <summary>
/// 15.4. Individual Sound Source Volume Down Setting, Command 'DF [±]number', Command 'DS [±]number', Command 'DP [±]number', Command 'DR [±]number', Command 'A number'
/// </summary>
uint8_t * pmd_driver_t::SpecialC0ProcessingCommand(channel_t * channel, uint8_t * si, uint8_t value) noexcept
{
    switch (value)
    {
        // 15.4. Individual Sound Source Volume Down Setting, Changes an individual sound source's volume down, Command 'DF number'
        case 0xFF:
            _State._FMVolumeAdjust = *si++; // 0..255 or -128..127
            break;

        // 15.4. Individual Sound Source Volume Down Setting, Changes an individual sound source's volume down, Command 'DF ±number'
        case 0xFE:
            si = DecreaseFMVolumeCommand(channel, si);
            break;

        // 15.4. Individual Sound Source Volume Down Setting, Changes an individual sound source's volume down, Command 'DS number'
        case 0xFD:
            _State._SSGVolumeAdjust = *si++; // 0..255 or -128..127
            break;

        // 15.4. Individual Sound Source Volume Down Setting, Changes an individual sound source's volume down, Command 'DS ±number'
        case 0xFC:
            si = SSGDecreaseVolume(channel, si);
            break;

        // 15.4. Individual Sound Source Volume Down Setting, Changes an individual sound source's volume down, Command 'DP number'
        case 0xFB:
            _State._ADPCMVolumeAdjust = *si++; // 0..255 or -128..127
            break;

        // 15.4. Individual Sound Source Volume Down Setting, Changes an individual sound source's volume down, Command 'DP ±number'
        case 0xFA:
            si = ADPCMDecreaseVolume(channel, si);
            break;

        // 15.4. Individual Sound Source Volume Down Setting, Changes an individual sound source's volume down, Command 'DR number'
        case 0xF9:
            _State._RhythmVolumeAdjust = *si++; // 0..255 or -128..127
            break;

        // 15.4. Individual Sound Source Volume Down Setting, Changes an individual sound source's volume down, Command 'DR ±number'
        case 0xF8:
            si = RhythmDecreaseVolume(channel, si);
            break;

        // 15.10. PCM Method Selection, Changes the PCM channel method, Command 'A number'
        case 0xF7:
            _State._IsCompatibleWithPMDB2 = ((*si++ & 0x01) == 0x01);
            break;

        case 0xF6:
            _State._PPZ8VolumeAdjust = *si++; // 0..255 or -128..127
            break;

        case 0xF5:
            si = PPZ8DecreaseVolume(channel, si);
            break;

        default: // Unknown value. Force the end of the part by replacing the code with <End of Part>.
            si--;
            *si = 0x80;
    }

    return si;
}

/// <summary>
/// Command 't' [Tempo Change 1] / Command 'T' [Tempo Change 2] / Command 't±' [Relative Tempo Change 1] / Command 'T±' [Relative Tempo Change 2] (11.1. Tempo setting 1) (11.2. Tempo setting 2)
/// </summary>
uint8_t * pmd_driver_t::SetTempoCommand(uint8_t * si)
{
    int32_t Value = *si++;

    // Add to Ticks per Quarter (T (FC)).
    if (Value < 0xFB)
    {
        _State._Tempo =
        _State._TempoDefault = Value;

        ConvertTimerBTempoToMetronomeTempo();
    }
    else
    // Set Ticks per Quarter (t (FC FF)).
    if (Value == 0xFF)
    {
        Value = *si++;

        if (Value < 18)
            Value = 18;

        _State._MetronomeTempo =
        _State._MetronomeTempoDefault = Value;

        ConvertMetronomeTempoToTimerBTempo();
    }
    else
    // Relative Tempo Change (T± (FC FE)).
    if (Value == 0xFE)
    {
        Value = (int8_t) *si++;

        if (Value >= 0)
            Value += _State._TempoDefault;
        else
        {
            Value += _State._TempoDefault;

            if (Value < 0)
                Value = 0;
        }

        if (Value > 0xFA)
            Value = 0xFA;

        _State._Tempo =
        _State._TempoDefault = Value;

        ConvertTimerBTempoToMetronomeTempo();
    }
    // Relative Tempo Change (t± (FC FD)).
    else
    {
        Value = (int8_t) *si++;

        if (Value >= 0)
        {
            Value += _State._MetronomeTempoDefault;

            if (Value > 255)
                Value = 255;
        }
        else
        {
            Value += _State._MetronomeTempoDefault;

            if (Value < 0)
                Value = 18;
        }

        _State._MetronomeTempo =
        _State._MetronomeTempoDefault = Value;

        ConvertMetronomeTempoToTimerBTempo();
    }

    return si;
}

/// <summary>
/// 10.1. Local loop setting, Set start of loop, Command '['
/// </summary>
uint8_t * pmd_driver_t::SetStartOfLoopCommand(channel_t * channel, uint8_t * si)
{
    uint8_t * Data = (channel == &_SSGEffectChannel) ? _State._EData : _State._MData;

    Data[*(uint16_t *) si + 1] = 0;

    si += 2;

    return si;
}

/// <summary>
/// 10.1. Local loop setting, Set end of loop, Command ']'
/// </summary>
uint8_t * pmd_driver_t::SetEndOfLoopCommand(channel_t * channel, uint8_t * si)
{
    const int32_t MaxLoopCount = *si++; // Range 0 - 255, 0 = Repeat indefinitely

    if (MaxLoopCount != 0)
    {
        (*si)++; // Increase loop count.

        const int32_t LoopCount = *si++;

        if (LoopCount == MaxLoopCount)
        {
            si += 2; // Skip offset.

            return si;
        }
    }
    else // Repeat indefinitely
    {
        si++; // Skip loop count.
        channel->_LoopCheck = 0x01;
    }

    // Jump to offset + 2.
    const int32_t Offset = *(uint16_t *) si + 2;

    si = ((channel == &_SSGEffectChannel) ? _State._EData : _State._MData) + Offset;

    return si;
}

/// <summary>
/// 10.1. Local Loop Setting, Exit loop, Command ':'
/// </summary>
uint8_t * pmd_driver_t::ExitLoopCommand(channel_t * channel, uint8_t * si)
{
    uint8_t * Data = (channel == &_SSGEffectChannel) ? _State._EData : _State._MData;

    Data += *(uint16_t *) si;
    si += 2;

    int32_t dl = *Data++ - 1;

    if (dl != *Data)
        return si;

    si = Data + 3;

    return si;
}

#pragma endregion

/// <summary>
/// Transposes the specified note.
/// </summary>
int32_t pmd_driver_t::Transpose(channel_t * channel, int srcTone)
{
    if (srcTone == 0x0F)
        return srcTone;

    const int32_t Transposition = channel->Transposition1 + channel->Transposition2;

    if (Transposition == 0)
        return srcTone;

    int32_t Octave = (srcTone & 0xF0) >> 4;
    int32_t Note   = (srcTone & 0x0F);

    Note += Transposition;

    if (Transposition < 0)
    {
        if (Note < 0)
        {
            do
            {
                Octave--;
            }
            while ((Note += 12) < 0);
        }

        if (Octave < 0)
            Octave = 0;

        return (Octave << 4) | Note;
    }
    else
    {
        while (Note >= 12)
        {
            Octave++;
            Note -= 12;
        }

        if (Octave > 7)
            Octave = 7;

        return (Octave << 4) | Note;
    }
}

/// <summary>
///
/// </summary>
uint8_t * pmd_driver_t::CalculateQ(channel_t * channel, uint8_t * si)
{
    if (*si == 0xC1)
    {
        si++;
        channel->_GateTime = 0;

        return si;
    }

    int32_t EarlyKeyOffTimeOut1 = channel->_EarlyKeyOffTimeout1;

    if (channel->EarlyKeyOffTimeoutPercentage != 0)
        EarlyKeyOffTimeOut1 += (channel->_Size * channel->EarlyKeyOffTimeoutPercentage) >> 8;

    if (channel->EarlyKeyOffTimeoutRandomRange != 0)
    {
        const int32_t Value = rnd((channel->EarlyKeyOffTimeoutRandomRange & 0x7F) + 1);

        if ((channel->EarlyKeyOffTimeoutRandomRange & 0x80) == 0)
        {
            EarlyKeyOffTimeOut1 += Value;
        }
        else
        {
            EarlyKeyOffTimeOut1 -= Value;

            if (EarlyKeyOffTimeOut1 < 0)
                EarlyKeyOffTimeOut1 = 0;
        }
    }

    if (channel->_EarlyKeyOffTimeout2 != 0)
    {
        int32_t EarlyKeyOffTimeOut2 = channel->_Size - channel->_EarlyKeyOffTimeout2;

        if (EarlyKeyOffTimeOut2 < 0)
        {
            channel->_GateTime = 0;

            return si;
        }

        channel->_GateTime = std::min(EarlyKeyOffTimeOut1, EarlyKeyOffTimeOut2);
    }
    else
        channel->_GateTime = EarlyKeyOffTimeOut1;

    return si;
}

/// <summary>
///
/// </summary>
void pmd_driver_t::CalculatePortamento(channel_t * channel)
{
    channel->_Portamento += channel->_PortamentoQuotient;

    if (channel->_PortamentoRemainder == 0)
        return;

    if (channel->_PortamentoRemainder > 0)
    {
        channel->_PortamentoRemainder--;
        channel->_Portamento++;
    }
    else
    {
        channel->_PortamentoRemainder++;
        channel->_Portamento--;
    }
}

/// <summary>
/// Gets a random number.
/// </summary>
int pmd_driver_t::rnd(int32_t range) noexcept
{
    _Seed = (259 * _Seed + 3) & 0x7fff;

    return _Seed * range / 32767;
}

/// <summary>
/// Set the tempo.
/// </summary>
void pmd_driver_t::SetTimerBTempo()
{
    if (_TimerBTempo != _State._Tempo)
    {
        _TimerBTempo = _State._Tempo;

        _OPNAW->SetReg(0x26, (uint32_t) _TimerBTempo); // Timer B DATA
    }
}

/// <summary>
/// Intializes the timers (FM Source Source only)
/// </summary>
void pmd_driver_t::InitializeTimers()
{
    // OPN interrupt initial setting
    _State._Tempo =
    _State._TempoDefault = 200;

    ConvertTimerBTempoToMetronomeTempo();
    SetTimerBTempo();

    _OPNAW->SetReg(0x25, 0x00); // Timer A Lower 2 bits (9216μs fixed)
    _OPNAW->SetReg(0x24, 0x00); // Timer A Upper 8 bits
    _OPNAW->SetReg(0x27, 0x3F); // Enable timer A and B (--XXXXXX), RESET A/B, ENABLE A/B, LOAD A/B

    _State._OpsCounter = 0;
    _State._BarCounter = 0;
    _State._BarLength = 96;
}

/// <summary>
/// Increase bar counter.
/// </summary>
void pmd_driver_t::IncreaseBarCounter()
{
    if (_State._OpsCounter + 1 == _State._BarLength)
    {
        _State._BarCounter++;
        _State._OpsCounter = 0;
    }
    else
        _State._OpsCounter++;
}

/// <summary>
/// Mutes all sound sources.
/// </summary>
void pmd_driver_t::Mute()
{
    _OPNAW->SetReg(0x80, 0xFF);
    _OPNAW->SetReg(0x81, 0xFF);
    _OPNAW->SetReg(0x82, 0xFF);
    _OPNAW->SetReg(0x84, 0xFF);
    _OPNAW->SetReg(0x85, 0xFF);
    _OPNAW->SetReg(0x86, 0xFF);
    _OPNAW->SetReg(0x88, 0xFF);
    _OPNAW->SetReg(0x89, 0xFF);
    _OPNAW->SetReg(0x8a, 0xFF);
    _OPNAW->SetReg(0x8c, 0xFF);
    _OPNAW->SetReg(0x8d, 0xFF);
    _OPNAW->SetReg(0x8e, 0xFF);

    _OPNAW->SetReg(0x180, 0xFF);
    _OPNAW->SetReg(0x181, 0xFF);
    _OPNAW->SetReg(0x184, 0xFF);
    _OPNAW->SetReg(0x185, 0xFF);
    _OPNAW->SetReg(0x188, 0xFF);
    _OPNAW->SetReg(0x189, 0xFF);
    _OPNAW->SetReg(0x18c, 0xFF);
    _OPNAW->SetReg(0x18d, 0xFF);

    _OPNAW->SetReg(0x182, 0xFF);
    _OPNAW->SetReg(0x186, 0xFF);
    _OPNAW->SetReg(0x18a, 0xFF);
    _OPNAW->SetReg(0x18e, 0xFF);

    _OPNAW->SetReg(0x28, 0x00); // FM KEYOFF
    _OPNAW->SetReg(0x28, 0x01);
    _OPNAW->SetReg(0x28, 0x02);
    _OPNAW->SetReg(0x28, 0x04); // FM KEYOFF [URA]
    _OPNAW->SetReg(0x28, 0x05);
    _OPNAW->SetReg(0x28, 0x06);

    // PPS
    _PPS->Stop();

    // P86
    _P86->Stop();
    _P86->SetPan(3, 0); // Center pan

    // SSG
    _OPNAW->SetReg(0x07, 0xBF); // Disable the SSG channels (X-XXXXXX, IOB + Noise A, B and C + Tone A, B and C
    _OPNAW->SetReg(0x08, 0x00); // SSG channel A amplitude and mode
    _OPNAW->SetReg(0x09, 0x00); // SSG channel B amplitude and mode
    _OPNAW->SetReg(0x0A, 0x00); // SSG channel C amplitude and mode

    // Rhythm
    _OPNAW->SetReg(0x10, 0xFF); // Rhythm DM / RKON

    // ADPCM
    _OPNAW->SetReg(0x101, 0x02); // Set ADPCM Control 2: PAN UNSET / x8 bit mode
    _OPNAW->SetReg(0x100, 0x01); // Set ADPCM Control 1
    _OPNAW->SetReg(0x110, 0x80); // Set ADPCM Flag Control: TA/TB/EOS を RESET
    _OPNAW->SetReg(0x110, 0x18); // Set ADPCM Flag Control: Bit change only for TIMERB/A/EOS

    for (size_t i = 0; i < MaxPPZ8Channels; ++i)
        _PPZ8->Stop(i);
}

/// <summary>
/// Fade In / Out
/// </summary>
void pmd_driver_t::Fade()
{
    if (_State._FadeOutSpeed == 0)
        return;

    if (_State._FadeOutSpeed > 0)
    {
        if ((_State._FadeOutVolume + _State._FadeOutSpeed) < 256)
        {
            _State._FadeOutVolume += _State._FadeOutSpeed;
        }
        else
        {
            _State._FadeOutVolume = 255;
            _State._FadeOutSpeed  =   0;

            if (_StopAfterFadeout)
                _Driver._Flags |= DriverStopRequested;
        }
    }
    else
    {   // Fade in
        if ((_State._FadeOutVolume + _State._FadeOutSpeed) > 255)
        {
            _State._FadeOutVolume += _State._FadeOutSpeed;
        }
        else
        {
            _State._FadeOutVolume = 0;
            _State._FadeOutSpeed  = 0;

            _OPNAW->SetReg(0x11, (uint32_t) _RhythmVolume);  // Rhythm Part: Set RTL (Total Level)
        }
    }
}

/// <summary>
/// Sets the start address and initial value of each channel.
/// </summary>
void pmd_driver_t::InitializeChannels()
{
    _Version = _State._MData[-1];

    const uint16_t * Offsets = (const uint16_t *) _State._MData;

    for (size_t i = 0; i < _countof(_FMChannels); ++i)
    {
        _FMChannels[i]._Data = (_State._MData[*Offsets] != 0x80) ? &_State._MData[*Offsets] : nullptr;
        _FMChannels[i]._Size = 1;
        _FMChannels[i]._KeyOffFlag = -1;
        _FMChannels[i]._LFO1DepthSpeed1 = -1;   // LFO1MDepth Counter (-1 = infinite)
        _FMChannels[i]._LFO1DepthSpeed2 = -1;
        _FMChannels[i]._LFO2DepthSpeed1 = -1;
        _FMChannels[i]._LFO2DepthSpeed2 = -1;
        _FMChannels[i]._Tone = 0xFF;            // Rest
        _FMChannels[i]._DefaultTone = 0xFF;     // Rest
        _FMChannels[i]._Volume = 108;

        _FMChannels[i]._PanAndVolume = 0xC0;    // 3 << 6, Center
        _FMChannels[i]._FMSlotMask = 0xF0;
        _FMChannels[i]._ToneMask = 0xFF;

        Offsets++;
    }

    for (size_t i = 0; i < _countof(_SSGChannels); ++i)
    {
        _SSGChannels[i]._Data = (_State._MData[*Offsets] != 0x80) ? &_State._MData[*Offsets] : nullptr;
        _SSGChannels[i]._Size = 1;
        _SSGChannels[i]._KeyOffFlag = -1;
        _SSGChannels[i]._LFO1DepthSpeed1 = -1;   // LFO1MDepth Counter (-1 = infinite)
        _SSGChannels[i]._LFO1DepthSpeed2 = -1;
        _SSGChannels[i]._LFO2DepthSpeed1 = -1;
        _SSGChannels[i]._LFO2DepthSpeed2 = -1;
        _SSGChannels[i]._Tone = 0xFF;            // Rest
        _SSGChannels[i]._DefaultTone = 0xFF;     // Rest
        _SSGChannels[i]._Volume = 8;

        _SSGChannels[i]._SSGMask = 0x07;          // Tone
        _SSGChannels[i]._SSGEnvelopFlag = 3;      // SSG ENV = NONE/normal

        Offsets++;
    }

    {
        _ADPCMChannel._Data = (_State._MData[*Offsets] != 0x80) ? &_State._MData[*Offsets] : nullptr;
        _ADPCMChannel._Size = 1;
        _ADPCMChannel._KeyOffFlag = -1;
        _ADPCMChannel._LFO1DepthSpeed1 = -1;
        _ADPCMChannel._LFO1DepthSpeed2 = -1;
        _ADPCMChannel._LFO2DepthSpeed1 = -1;
        _ADPCMChannel._LFO2DepthSpeed2 = -1;
        _ADPCMChannel._Tone = 0xFF;             // Rest
        _ADPCMChannel._DefaultTone = 0xFF;      // Rest
        _ADPCMChannel._Volume = 128;

        _ADPCMChannel._PanAndVolume = 0xC0;     // 3 << 6, Center

        Offsets++;
    }

    {
        _RhythmChannel._Data = (_State._MData[*Offsets] != 0x80) ? &_State._MData[*Offsets] : nullptr;
        _RhythmChannel._Size = 1;
        _RhythmChannel._KeyOffFlag = -1;
        _RhythmChannel._LFO1DepthSpeed1 = -1;
        _RhythmChannel._LFO1DepthSpeed2 = -1;
        _RhythmChannel._LFO2DepthSpeed1 = -1;
        _RhythmChannel._LFO2DepthSpeed2 = -1;
        _RhythmChannel._Tone = 0xFF;             // Rest
        _RhythmChannel._DefaultTone = 0xFF;      // Rest
        _RhythmChannel._Volume = 15;

        Offsets++;
    }

    {
        _State._RhythmDataTable = (uint16_t *) &_State._MData[*Offsets];
        _State._RhythmData = (uint8_t *) &_DummyRhythmData;
    }

    {
        Offsets = (const uint16_t *) _State._MData;

        const uint16_t MaxParts = 12;

        if (Offsets[0] != sizeof(uint16_t) * MaxParts) // 0x0018
            _State._InstrumentData = _State._MData + Offsets[12];
    }
}

/// <summary>
/// Tempo conversion (input: tempo_d, output: tempo_48)
/// </summary>
void pmd_driver_t::ConvertTimerBTempoToMetronomeTempo()
{
    int32_t Value;

    if (_State._Tempo != 256)
    {
        Value = 0x112C / (256 - _State._Tempo) + 1;

        if (Value > 255)
            Value = 255;
    }
    else
        Value = 255;

    _State._MetronomeTempo =
    _State._MetronomeTempoDefault = Value;
}

/// <summary>
/// Tempo conversion (input: tempo_48, output: tempo_d)
/// </summary>
void pmd_driver_t::ConvertMetronomeTempoToTimerBTempo()
{
    int32_t Value;

    if (_State._MetronomeTempo >= 18)
    {
        Value = 256 - (0x112C / _State._MetronomeTempo);

        if (0x112C % _State._MetronomeTempo >= 128)
            Value--;
    }
    else
        Value = 0;

    _State._Tempo =
    _State._TempoDefault = Value;
}

/// <summary>
/// Loads the PPC file.
/// </summary>
int pmd_driver_t::LoadPPC(const WCHAR * filePath)
{
    _PCMFilePath.clear();

    if ((*filePath == '\0') || !_File->Open(filePath))
        return ERR_OPEN_FAILED;

    int64_t Size = _File->GetFileSize(filePath);

    if (Size < 0)
        return ERR_OPEN_FAILED;

    uint8_t * Data = (uint8_t *) ::malloc((size_t) Size);

    if (Data == nullptr)
        return ERR_OUT_OF_MEMORY;

    _File->Read(Data, (uint32_t) Size);
    _File->Close();

    int32_t Result = LoadPPCInternal(Data, (size_t) Size);

    if (Result == ERR_SUCCESS)
        _PCMFilePath = filePath;

    ::free(Data);

    return Result;
}

/// <summary>
/// Loads the PPC data.
/// </summary>
int pmd_driver_t::LoadPPCInternal(uint8_t * data, size_t size) noexcept
{
    if (size < 0x10)
        return ERR_UNKNOWN_FORMAT;

    const char * PVIIdentifier = "PVI2";
    const size_t PVIIdentifierSize = ::strlen(PVIIdentifier);

    const char * PPCIdentifier = "ADPCM DATA for  PMD ver.4.4-  ";
    const size_t PPCIdentifierSize = ::strlen(PPCIdentifier);

    const size_t PPCSize = PPCIdentifierSize + sizeof(uint16_t) + (sizeof(uint16_t) * 2 * 256);

    bool IsPVIData = false;

    if ((::memcmp((const char *) data, PVIIdentifier, PVIIdentifierSize) == 0) && (data[10] == 2)) // PVI
    {
        IsPVIData = true;

        int32_t NumSamples = 0;

        // Convert from PVI to PMD format.
        for (size_t i = 0; i < 128; ++i)
        {
            auto & Address = _SampleBank.Address[i];

            if (*((uint16_t *) &data[18 + (i * 4)]) == 0)
            {
                Address[0] = data[16 + (i * 4)];
                Address[1] = 0;
            }
            else
            {
                Address[0] = (uint16_t) (*(uint16_t *) &data[16 + (i * 4)] + 0x26);
                Address[1] = (uint16_t) (*(uint16_t *) &data[18 + (i * 4)] + 0x26);
            }

            if (NumSamples < Address[1])
                NumSamples = Address[1] + 1;
        }

        // The remaining 128 bytes are undefined.
        for (size_t i = 128; i < 256; ++i)
        { 
            _SampleBank.Address[i][0] = 0;
            _SampleBank.Address[i][1] = 0;
        }

        _SampleBank.Count = (uint16_t) NumSamples;
    }
    else
    if (::memcmp((const char *) data, PPCIdentifier, PPCIdentifierSize) == 0) // PPC
    {
        if (size < PPCSize)
            return ERR_UNKNOWN_FORMAT;

        uint16_t * Data = (uint16_t *)(data + PPCIdentifierSize);

        _SampleBank.Count = *Data++;

        for (int32_t i = 0; i < 256; ++i)
        {
            _SampleBank.Address[i][0] = *Data++;
            _SampleBank.Address[i][1] = *Data++;
        }
    }
    else
        return ERR_UNKNOWN_FORMAT;

    {
        uint8_t Data[0x26 * 32];

        // Compare the data with the PCMRAM header.
        ReadPCMData(0x0000, 0x0025, Data);

        if (::memcmp(Data + PPCIdentifierSize, &_SampleBank, sizeof(_SampleBank)) == 0)
            return ERR_ALREADY_LOADED;
    }

    {
        std::vector<uint8_t> Data;

        Data.resize(PPCSize + 128);

        // Write the PCM data.
        ::memcpy(Data.data(),                     PPCIdentifier,      PPCIdentifierSize);
        ::memcpy(Data.data() + PPCIdentifierSize, &_SampleBank.Count, Data.size() - PPCIdentifierSize);

        WritePCMData(0x0000, 0x0025, Data.data());
    }

    // Write the data to PCMRAM.
    {
        uint16_t * Data;

        if (IsPVIData)
        {
            if (size < (_SampleBank.Count - (0x10 + sizeof(uint16_t) * 2 * 128)) * 32)
                return ERR_UNKNOWN_FORMAT;

            Data = (uint16_t *)(data + 0x10 + (sizeof(uint16_t) * 2 * 128));
        }
        else
        {
            if (size < (_SampleBank.Count - (PPCSize / 2)) * 32)
                return ERR_UNKNOWN_FORMAT;

            Data = (uint16_t *)(data + PPCSize);
        }

        const uint16_t StartOffset = 0x0026;
        const uint16_t StopOffset  = _SampleBank.Count;

        WritePCMData(StartOffset, StopOffset, (const uint8_t *) Data);
    }

    return ERR_SUCCESS;
}

/// <summary>
/// Read data from PCM memory to main memory.
/// </summary>
void pmd_driver_t::ReadPCMData(uint16_t startAddress, uint16_t stopAddress, uint8_t * data)
{
    _OPNAW->SetReg(0x100, 0x01);                            // Set ADPCM Control 1
    _OPNAW->SetReg(0x110, 0x00);                            // Set ADPCM Flag Control
    _OPNAW->SetReg(0x110, 0x80);                            // Set ADPCM Flag Control
    _OPNAW->SetReg(0x100, 0x20);                            // Set ADPCM Control 1
    _OPNAW->SetReg(0x101, 0x02);                            // Set ADPCM Control 2: PAN UNSET / x8 bit mode

    _OPNAW->SetReg(0x10C, 0xFF);                            // Set ADPCM Limit Address (Lo)
    _OPNAW->SetReg(0x10D, 0xFF);                            // Set ADPCM Limit Address (Hi)

    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(startAddress)); // Set ADPCM Start Address (Lo)
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(startAddress)); // Set ADPCM Start Address (Hi)

    _OPNAW->SetReg(0x104, 0xFF);                            // Set ADPCM Stop Address (Lo)
    _OPNAW->SetReg(0x105, 0xFF);                            // Set ADPCM Stop Address (Hi)

    *data = (uint8_t) _OPNAW->GetReg(0x108);                // Get ADPCM Data
    *data = (uint8_t) _OPNAW->GetReg(0x108);                // Get ADPCM Data

    for (int32_t i = 0; i < (stopAddress - startAddress) * 32; ++i)
    {
        *data++ = (uint8_t) _OPNAW->GetReg(0x108);          // Get ADPCM Data

        _OPNAW->SetReg(0x110, 0x80);                        // Set ADPCM Flag Control
    }
}

/// <summary>
/// Write data from main memory to PCM memory.
/// </summary>
void pmd_driver_t::WritePCMData(uint16_t startAddress, uint16_t stopAddress, const uint8_t * data)
{
    _OPNAW->SetReg(0x100, 0x01);                            // Set ADPCM Control 1
//  _OPNAW->SetReg(0x110, 0x17);                            // Set ADPCM Flag Control: Mask everything except brdy (=timer interrupt does not occur)
    _OPNAW->SetReg(0x110, 0x80);                            // Set ADPCM Flag Control
    _OPNAW->SetReg(0x100, 0x60);                            // Set ADPCM Control 1
    _OPNAW->SetReg(0x101, 0x02);                            // Set ADPCM Control 2: PAN UNSET / x8 bit mode

    _OPNAW->SetReg(0x10C, 0xFF);                            // Set ADPCM Limit Address (Lo)
    _OPNAW->SetReg(0x10D, 0xFF);                            // Set ADPCM Limit Address (Hi)

    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(startAddress)); // Set ADPCM Start Address (Lo)
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(startAddress)); // Set ADPCM Start Address (Hi)

    _OPNAW->SetReg(0x104, 0xFF);                            // Set ADPCM Stop Address (Lo)
    _OPNAW->SetReg(0x105, 0xFF);                            // Set ADPCM Stop Address (Hi)

    for (int32_t i = 0; i < (stopAddress - startAddress) * 32; ++i)
        _OPNAW->SetReg(0x108, *data++);                     // Set ADPCM Data
}

/// <summary>
/// Finds a PCM sample in the specified search path.
/// </summary>
void pmd_driver_t::FindFile(const WCHAR * filename, WCHAR * filePath, size_t size) const noexcept
{
    filePath[0] = '\0';

    WCHAR FilePath[MAX_PATH];

    for (size_t i = 0; i < _SearchPath.size(); ++i)
    {
        CombinePath(FilePath, _countof(FilePath), _SearchPath[i].c_str(), filename);

        if (_File->GetFileSize(FilePath) > 0)
            ::wcscpy_s(filePath, size, FilePath);
    }
}

/// <summary>
/// Gets the text with the specified index from PMD data.
/// </summary>
void pmd_driver_t::GetText(const uint8_t * data, size_t size, int index, char * text, size_t max) const noexcept
{
    *text = '\0';

    if ((data == nullptr) || size < 0x0019)
        return;

    const uint8_t * Data = &data[1];
    size_t Size = size - 1;

    // The first offset should be 0x001A.
    if (Data[0] != 0x1A || Data[1] != 0x00)
        return;

    // Get the 13th offset.
    size_t Offset = (size_t) *(uint16_t *) &Data[sizeof(uint16_t) * 12] - 4;

    if (Offset + 3 >= Size)
        return;

    const uint8_t * Src = &Data[Offset];

    {
        if ((Src[2] != 0x40) && ((Src[2] < 0x41) || (Src[3] != 0xFE)))
            return;

        if (Src[2] >= 0x42)
            index++;

        if (Src[2] >= 0x48)
            index++;

        if (index < 0)
            return;
    }

    Src = &Data[*(uint16_t *) Src];

    size_t i;

    {
        Offset = 0;

        for (i = 0; i <= (size_t) index; ++i)
        {
            if ((size_t)(Src - Data + 1) >= Size)
                return;

            Offset = *(uint16_t *) Src;

            if ((Offset == 0) || ((size_t) Offset >= Size))
                return;

            if (Data[Offset] == '/')
                return;

            Src += 2;
        }

        // Determine the offset of the terminating '\0', if any.
        for (i = Offset; (i < Size) && Data[i]; ++i)
            ;
    }

    if (i == Size)
    {
        ::memcpy(text, &Data[Offset], (size_t) Size - Offset);
        text[Size - Offset - 1] = '\0';
    }
    else
        ::strcpy_s(text, max, (char *) &Data[Offset]);
}
