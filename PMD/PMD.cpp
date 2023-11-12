
// $VER: PMD.cpp (2023.10.29) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara)

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4710 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "PMD.h"

#include "Utility.h"
#include "Table.h"

#include "OPNAW.h"
#include "PPZ.h"
#include "PPS.h"
#include "P86.h"

/// <summary>
/// Initializes an instance.
/// </summary>
PMD::PMD()
{
    _File = new File();

    _OPNAW = new OPNAW(_File);

    _PPZ = new PPZDriver(_File);
    _PPS = new PPSDriver(_File);
    _P86 = new P86Driver(_File);

    Reset();
}

/// <summary>
/// Destroys an instance.
/// </summary>
PMD::~PMD()
{
    delete _P86;
    delete _PPS;
    delete _PPZ;

    delete _OPNAW;

    delete _File;
}

#pragma region(Public)
/// <summary>
/// Initializes the driver.
/// </summary>
bool PMD::Initialize(const WCHAR * directoryPath)
{
    WCHAR DirectoryPath[MAX_PATH] = { 0 };

    if (directoryPath != nullptr)
    {
        ::wcscpy(DirectoryPath, directoryPath);
        AddBackslash(DirectoryPath, _countof(DirectoryPath));
    }

    Reset();

    _PPZ->Initialize(_State.OPNARate, false);
    _PPS->Initialize(_State.OPNARate, false);
    _P86->Initialize(_State.OPNARate, false);

    if (_OPNAW->Initialize(OPNAClock, FREQUENCY_55_5K, false, DirectoryPath) == false)
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

        _PPZ->SetVolume(0);
        _PPS->SetVolume(0);
        _P86->SetVolume(0);

        _OPNAW->SetFMDelay(DEFAULT_REG_WAIT);
        _OPNAW->SetSSGDelay(DEFAULT_REG_WAIT);
        _OPNAW->SetADPCMDelay(DEFAULT_REG_WAIT);
        _OPNAW->SetRhythmDelay(DEFAULT_REG_WAIT);
    }

    // Initial setting of 088/188/288/388 (same INT number only)
    _OPNAW->SetReg(0x29, 0x00);
    _OPNAW->SetReg(0x24, 0x00);
    _OPNAW->SetReg(0x25, 0x00);
    _OPNAW->SetReg(0x26, 0x00);
    _OPNAW->SetReg(0x27, 0x3F);

    // Start the OPN interrupt.
    StartOPNInterrupt();

    return true;
}

/// <summary>
/// Returns true if the data is PMD data.
/// </summary>
bool PMD::IsPMD(const uint8_t * data, size_t size) noexcept
{
    if (size < 3)
        return false;

    if (size > sizeof(_MData))
        return false;

    if (data[0] > 0x0F && data[0] != 0xFF) // 0xFF = FM Towns
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
int PMD::Load(const uint8_t * data, size_t size)
{
    if (!IsPMD(data, size))
        return ERR_UNKNOWN_FORMAT;

    Stop();

    ::memcpy(_MData, data, size);
    ::memset(_MData + size, 0, sizeof(_MData) - size);

    if (_SearchPath.size() == 0)
        return ERR_SUCCESS;

    int Result = ERR_SUCCESS;

    char FileName[MAX_PATH] = { 0 };
    WCHAR FileNameW[MAX_PATH] = { 0 };

    WCHAR FilePath[MAX_PATH] = { 0 };

    {
        _PCMFilePath.clear();

        GetText(data, size, 0, FileName);

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
                    _State.IsUsingP86 = true;

                    _PCMFilePath = FilePath;
                }
            }
            else
            // PPC import (ADPCM, 4-bit sample playback, 256kB max.)
            if (HasExtension(FileNameW, _countof(FileNameW), L".PPC"))
            {
                FindFile(FileNameW, FilePath, _countof(FilePath));

                Result = LoadPPCInternal(FilePath);

                if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
                {
                    _State.IsUsingP86 = false;

                    _PCMFilePath = FilePath;
                }
            }
        }
        else
            _PCMFileName.clear();
    }

    {
        _PPSFilePath.clear();

        GetText(data, size, -1, FileName);

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

    {
        _PPZFilePath[0].clear();
        _PPZFilePath[1].clear();

        GetText(data, size, -2, FileName);

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

                    Result = _PPZ->Load(FilePath, i);

                    if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
                        _PPZFilePath[i] = FilePath;
                }
                else
                // PMB import
                if (HasExtension(p, _countof(FileNameW) - ::wcslen(p), L".PMB"))
                {
                    RenameExtension(p, _countof(FileNameW) - ::wcslen(p), L".PZI");

                    FindFile(p, FilePath, _countof(FilePath));

                    Result = _PPZ->Load(FilePath, i);

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
void PMD::Start()
{
    if (_State.IsTimerABusy || _State.IsTimerBBusy)
    {
        _Driver._Flags |= DriverStartRequested; // Delay the start of the driver until timer processing has finished.

        return;
    }

    DriverStart();
}

/// <summary>
/// Stops the driver.
/// </summary>
void PMD::Stop()
{
    if (_State.IsTimerABusy || _State.IsTimerBBusy)
    {
        _Driver._Flags |= DriverStopRequested; // Delay the start of the driver until timer processing has finished.
    }
    else
    {
        _State.IsFadeOutSpeedSet = false;

        DriverStop();
    }

    ::memset(_SampleSrc, 0, sizeof(_SampleSrc));
    _Position = 0;
}

/// <summary>
/// Renders a chunk of PCM data.
/// </summary>
void PMD::Render(int16_t * sampleData, size_t sampleCount)
{
    size_t SamplesDone = 0;

    do
    {
        if (_SamplesToDo >= sampleCount - SamplesDone)
        {
            ::memcpy(sampleData, _SamplePtr, (sampleCount - SamplesDone) * sizeof(Stereo16bit));
            _SamplesToDo -= (sampleCount - SamplesDone);

            _SamplePtr += (sampleCount - SamplesDone);
            SamplesDone = sampleCount;
        }
        else
        {
            {
                ::memcpy(sampleData, _SamplePtr, _SamplesToDo * sizeof(Stereo16bit));
                sampleData += (_SamplesToDo * 2);

                _SamplePtr = _SampleSrc;
                SamplesDone += _SamplesToDo;
            }

            {
                if (_OPNAW->ReadStatus() & 0x01)
                    HandleTimerA();

                if (_OPNAW->ReadStatus() & 0x02)
                    HandleTimerB();

                _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)
            }

            uint32_t TickCount = _OPNAW->GetNextTick(); // in microseconds

            {
                _SamplesToDo = (size_t) ((double) TickCount * _State.OPNARate / 1000000.0);
                _OPNAW->Count(TickCount);

                ::memset(_SampleDst, 0, _SamplesToDo * sizeof(Stereo32bit));

                if (_State.OPNARate == _State.PPZRate)
                    _PPZ->Mix((Sample *) _SampleDst, _SamplesToDo);
                else
                {
                    // PCM frequency transform of ppz8 (no interpolation)
                    size_t SampleCount = (size_t) (_SamplesToDo * _State.PPZRate / _State.OPNARate + 1);
                    int delta = (int) (8192 * _State.PPZRate / _State.OPNARate);

                    ::memset(_SampleTmp, 0, SampleCount * sizeof(Sample) * 2);

                    _PPZ->Mix((Sample *) _SampleTmp, SampleCount);

                    int carry = 0;

                    // Frequency transform (1 << 13 = 8192)
                    for (size_t i = 0; i < _SamplesToDo; ++i)
                    {
                        _SampleDst[i].Left  = _SampleTmp[(carry >> 13)].Left;
                        _SampleDst[i].Right = _SampleTmp[(carry >> 13)].Right;

                        carry += delta;
                    }
                }
            }

            {
                _OPNAW->Mix((Sample *) _SampleDst, _SamplesToDo);

                if (_Driver.UsePPS)
                    _PPS->Mix((Sample *) _SampleDst, _SamplesToDo);

                if (_State.IsUsingP86)
                    _P86->Mix((Sample *) _SampleDst, _SamplesToDo);
            }

            {
                _Position += TickCount;

                if (_State.FadeOutSpeedHQ > 0)
                {
                    int Factor = (_State.LoopCount != -1) ? (int) ((1 << 10) * ::pow(512, -(double) (_Position - _FadeOutPosition) / 1000 / _State.FadeOutSpeedHQ)) : 0;

                    for (size_t i = 0; i < _SamplesToDo; ++i)
                    {
                        _SampleSrc[i].Left  = (int16_t) Limit(_SampleDst[i].Left  * Factor >> 10, 32767, -32768);
                        _SampleSrc[i].Right = (int16_t) Limit(_SampleDst[i].Right * Factor >> 10, 32767, -32768);
                    }

                    // Fadeout end
                    if ((_Position - _FadeOutPosition > (int64_t) _State.FadeOutSpeedHQ * 1000) && _State.StopAfterFadeout)
                        _Driver._Flags |= DriverStopRequested;
                }
                else
                {
                    for (size_t i = 0; i < _SamplesToDo; ++i)
                    {
                        _SampleSrc[i].Left  = (int16_t) Limit(_SampleDst[i].Left,  32767, -32768);
                        _SampleSrc[i].Right = (int16_t) Limit(_SampleDst[i].Right, 32767, -32768);
                    }
                }
            }
        }
    }
    while (SamplesDone < sampleCount);
}

/// <summary>
/// Gets the current loop number.
/// </summary>
uint32_t PMD::GetLoopNumber()
{
    return (uint32_t) _State.LoopCount;
}

/// <summary>
/// Gets the length of the song and loop part (in ms).
/// </summary>
bool PMD::GetLength(int * songLength, int * loopLength, int * tickCount, int * loopTickCount)
{
    DriverStart();

    _Position = 0;
    *songLength = 0;

    int FMDelay = _OPNAW->GetFMDelay();
    int SSGDelay = _OPNAW->GetSSGDelay();
    int ADPCMDelay = _OPNAW->GetADPCMDelay();
    int RSSDelay = _OPNAW->GetRSSDelay();

    _OPNAW->SetFMDelay(0);
    _OPNAW->SetSSGDelay(0);
    _OPNAW->SetADPCMDelay(0);
    _OPNAW->SetRhythmDelay(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30); // Reset both timer A and B.

            uint32_t TickCount = _OPNAW->GetNextTick();

            _OPNAW->Count(TickCount);
            _Position += TickCount;
        }

        if ((_State.LoopCount == 1) && (*songLength == 0)) // When looping
        {
            *songLength = (int) (_Position / 1000);
            *tickCount = GetPositionInTicks();
        }
        else
        if (_State.LoopCount == -1) // End without loop
        {
            *songLength = (int) (_Position / 1000);
            *loopLength = 0;

            *tickCount = GetPositionInTicks();
            *loopTickCount = 0;

            DriverStop();

            _OPNAW->SetFMDelay(FMDelay);
            _OPNAW->SetSSGDelay(SSGDelay);
            _OPNAW->SetADPCMDelay(ADPCMDelay);
            _OPNAW->SetRhythmDelay(RSSDelay);

            return true;
        }
        else
        if (GetPositionInTicks() >= 65536) // Forced termination.
        {
            *songLength = (int) (_Position / 1000);
            *loopLength = *songLength;

            *tickCount = GetPositionInTicks();
            *loopTickCount = *tickCount;

            return true;
        }
    }
    while (_State.LoopCount < 2);

    *loopLength = (int) (_Position / 1000) - *songLength;
    *loopTickCount = GetPositionInTicks() - *tickCount;

    DriverStop();

    _OPNAW->SetFMDelay(FMDelay);
    _OPNAW->SetSSGDelay(SSGDelay);
    _OPNAW->SetADPCMDelay(ADPCMDelay);
    _OPNAW->SetRhythmDelay(RSSDelay);

    return true;
}

/// <summary>
/// Gets the length of the song and loop part (in ms).
/// </summary>
bool PMD::GetLength(int * songLength, int * loopLength)
{
    DriverStart();

    _Position = 0;
    *songLength = 0;

    int FMDelay = _OPNAW->GetFMDelay();
    int SSGDelay = _OPNAW->GetSSGDelay();
    int ADPCMDelay = _OPNAW->GetADPCMDelay();
    int RSSDelay = _OPNAW->GetRSSDelay();

    _OPNAW->SetFMDelay(0);
    _OPNAW->SetSSGDelay(0);
    _OPNAW->SetADPCMDelay(0);
    _OPNAW->SetRhythmDelay(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)

            uint32_t TickCount = _OPNAW->GetNextTick();

            _OPNAW->Count(TickCount);
            _Position += TickCount;
        }

        if ((_State.LoopCount == 1) && (*songLength == 0)) // When looping
        {
            *songLength = (int) (_Position / 1000);
        }
        else
        if (_State.LoopCount == -1) // End without loop
        {
            *songLength = (int) (_Position / 1000);
            *loopLength = 0;

            DriverStop();

            _OPNAW->SetFMDelay(FMDelay);
            _OPNAW->SetSSGDelay(SSGDelay);
            _OPNAW->SetADPCMDelay(ADPCMDelay);
            _OPNAW->SetRhythmDelay(RSSDelay);

            return true;
        }
        else
        if (GetPositionInTicks() >= 65536)
        {
            *songLength = (int) (_Position / 1000);
            *loopLength = *songLength;

            return true;
        }
    }
    while (_State.LoopCount < 2);

    *loopLength = (int) (_Position / 1000) - *songLength;

    DriverStop();

    _OPNAW->SetFMDelay(FMDelay);
    _OPNAW->SetSSGDelay(SSGDelay);
    _OPNAW->SetADPCMDelay(ADPCMDelay);
    _OPNAW->SetRhythmDelay(RSSDelay);

    return true;
}

/// <summary>
/// Gets the number of ticks in the song and loop part.
/// </summary>
bool PMD::GetLengthInTicks(int * tickCount, int * loopTickCount)
{
    DriverStart();

    _Position = 0;
    *tickCount = 0;

    int FMDelay = _OPNAW->GetFMDelay();
    int SSGDelay = _OPNAW->GetSSGDelay();
    int ADPCMDelay = _OPNAW->GetADPCMDelay();
    int RSSDelay = _OPNAW->GetRSSDelay();

    _OPNAW->SetFMDelay(0);
    _OPNAW->SetSSGDelay(0);
    _OPNAW->SetADPCMDelay(0);
    _OPNAW->SetRhythmDelay(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30);  // Timer Reset (Both timer A and B)

            uint32_t TickCount = _OPNAW->GetNextTick();

            _OPNAW->Count(TickCount);
            _Position += TickCount;
        }

        if ((_State.LoopCount == 1) && (*tickCount == 0)) // When looping
        {
            *tickCount = GetPositionInTicks();
        }
        else
        if (_State.LoopCount == -1) // End without loop
        {
            *tickCount = GetPositionInTicks();
            *loopTickCount = 0;

            DriverStop();

            _OPNAW->SetFMDelay(FMDelay);
            _OPNAW->SetSSGDelay(SSGDelay);
            _OPNAW->SetADPCMDelay(ADPCMDelay);
            _OPNAW->SetRhythmDelay(RSSDelay);

            return true;
        }
        else
        if (GetPositionInTicks() >= 65536) // Forced termination if 65536 clocks or more
        {
            *tickCount = GetPositionInTicks();
            *loopTickCount = *tickCount;

            return true;
        }
    }
    while (_State.LoopCount < 2);

    *loopTickCount = GetPositionInTicks() - *tickCount;

    DriverStop();

    _OPNAW->SetFMDelay(FMDelay);
    _OPNAW->SetSSGDelay(SSGDelay);
    _OPNAW->SetADPCMDelay(ADPCMDelay);
    _OPNAW->SetRhythmDelay(RSSDelay);

    return true;
}

// Gets the playback position (in ms)
/// <summary>
///
/// </summary>
uint32_t PMD::GetPosition()
{
    return (uint32_t) (_Position / 1000);
}

// Sets the playback position (in ms)
/// <summary>
///
/// </summary>
void PMD::SetPosition(uint32_t position)
{
    int64_t NewPosition = (int64_t) position * 1000; // Convert ms to μs.

    if (_Position > NewPosition)
    {
        DriverStart();

        _SamplePtr = _SampleSrc;

        _SamplesToDo = 0;
        _Position = 0;
    }

    while (_Position < NewPosition)
    {
        if (_OPNAW->ReadStatus() & 0x01)
            HandleTimerA();

        if (_OPNAW->ReadStatus() & 0x02)
            HandleTimerB();

        _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)

        uint32_t TickCount = _OPNAW->GetNextTick();

        _OPNAW->Count(TickCount);
        _Position += TickCount;
    }

    if (_State.LoopCount == -1)
        Silence();

    _OPNAW->ClearBuffer();
}

// Gets the playback position (in ticks)
/// <summary>
///
/// </summary>
int PMD::GetPositionInTicks()
{
    return (_State.BarLength * _State.BarCounter) + _State.OpsCounter;
}

// Sets the playback position (in ticks).
/// <summary>
///
/// </summary>
void PMD::SetPositionInTicks(int tickCount)
{
    if (((_State.BarLength * _State.BarCounter) + _State.OpsCounter) > tickCount)
    {
        DriverStart();

        _SamplePtr = _SampleSrc;
        _SamplesToDo = 0;
    }

    while (((_State.BarLength * _State.BarCounter) + _State.OpsCounter) < tickCount)
    {
        if (_OPNAW->ReadStatus() & 0x01)
            HandleTimerA();

        if (_OPNAW->ReadStatus() & 0x02)
            HandleTimerB();

        _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)

        uint32_t TickCount = _OPNAW->GetNextTick();

        _OPNAW->Count(TickCount);
    }

    if (_State.LoopCount == -1)
        Silence();

    _OPNAW->ClearBuffer();
}

// Sets the PCM search directory
/// <summary>
///
/// </summary>
bool PMD::SetSearchPaths(std::vector<const WCHAR *> & paths)
{
    for (std::vector<const WCHAR *>::iterator iter = paths.begin(); iter < paths.end(); iter++)
    {
        WCHAR Path[MAX_PATH];

        ::wcscpy(Path, *iter);
        AddBackslash(Path, _countof(Path));

        _SearchPath.push_back(Path);
    }

    return true;
}

/// <summary>
/// Sets the output frequency at which raw PCM data is synthesized (in Hz, for example 44100).
/// </summary>
void PMD::SetOutputFrequency(uint32_t value) noexcept
{
    if (value == FREQUENCY_55_5K || value == FREQUENCY_55_4K)
    {
        _State.OPNARate =
        _State.PPZRate = FREQUENCY_44_1K;
        _State.UseInterpolation = true;
    }
    else
    {
        _State.OPNARate =
        _State.PPZRate = value;
        _State.UseInterpolation = false;
    }

    _OPNAW->SetOutputFrequency(OPNAClock, _State.OPNARate, _State.UseInterpolation);

    _P86->SetOutputFrequency(_State.OPNARate, _State.UseInterpolationP86);
    _PPS->SetOutputFrequency(_State.OPNARate, _State.UseInterpolationPPS);
    _PPZ->SetOutputFrequency(_State.PPZRate, _State.UseInterpolationPPZ);
}

/// <summary>
/// Enables or disables interpolation to 55kHz output frequency.
/// </summary>
void PMD::SetFMInterpolation(bool value)
{
    if (value == _State.UseInterpolation)
        return;

    _State.UseInterpolation = value;

    _OPNAW->SetOutputFrequency(OPNAClock, _State.OPNARate, _State.UseInterpolation);
}

/// <summary>
/// Sets the output frequency at which raw PPZ data is synthesized (in Hz, for example 44100).
/// </summary>
void PMD::SetPPZOutputFrequency(uint32_t value) noexcept
{
    if (value == _State.PPZRate)
        return;

    _State.PPZRate = value;

    _PPZ->SetOutputFrequency(value, _State.UseInterpolationPPZ);
}

/// <summary>
/// Enables or disables PPZ interpolation.
/// </summary>
void PMD::SetPPZInterpolation(bool flag)
{
    _State.UseInterpolationPPZ = flag;

    _PPZ->SetOutputFrequency(_State.PPZRate, flag);
}

/// <summary>
/// Enables or disables PPS interpolation.
/// </summary>
void PMD::SetPPSInterpolation(bool flag)
{
    _State.UseInterpolationPPS = flag;

    _PPS->SetOutputFrequency(_State.OPNARate, flag);
}

/// <summary>
/// Enables or disables P86 interpolation.
/// </summary>
void PMD::SetP86Interpolation(bool flag)
{
    _State.UseInterpolationP86 = flag;

    _P86->SetOutputFrequency(_State.OPNARate, flag);
}

/// <summary>
/// Sets the fade out speed (PMD compatible)
/// </summary>
void PMD::SetFadeOutSpeed(int speed)
{
    _State.FadeOutSpeed = speed;
}

/// <summary>
/// Sets the fade out speed (High quality sound)
/// </summary>
void PMD::SetFadeOutDurationHQ(int value)
{
    if (value > 0)
    {
        if (_State.FadeOutSpeedHQ == 0)
            _FadeOutPosition = _Position;

        _State.FadeOutSpeedHQ = value;
    }
    else
        _State.FadeOutSpeedHQ = 0; // Fadeout forced stop
}

/// <summary>
/// Enables or disables the use of the PPS.
/// </summary>
void PMD::UsePPS(bool value) noexcept
{
    _Driver.UsePPS = value;
}

/// <summary>
/// Enables playing the OPNA rhythm with the Rhythm sound source.
/// </summary>
void PMD::UseSSG(bool flag) noexcept
{
    _State.UseSSG = flag;
}

/// <summary>
/// Disables the specified channel.
/// </summary>
int PMD::DisableChannel(int channel)
{
    if (channel >= MaxChannels)
        return ERR_WRONG_PARTNO;

    if (ChannelTable[channel][0] < 0)
    {
        _State.RhythmMask = 0;      // Mask the Rhythm sound source.

        _OPNAW->SetReg(0x10, 0xFF); // Dump all Rhythm sound sources.

        return ERR_SUCCESS;
    }

    int OldFMSelector = _Driver.FMSelector;

    if (IsPlaying() && (_State.Channel[channel]->MuteMask == 0x00))
    {
        switch (ChannelTable[channel][2])
        {
            case 0:
            {
                _Driver.CurrentChannel = ChannelTable[channel][1];
                _Driver.FMSelector = 0;

                MuteFMChannel(_State.Channel[channel]);
                break;
            }

            case 1:
            {
                _Driver.CurrentChannel = ChannelTable[channel][1];
                _Driver.FMSelector = 0x100;

                MuteFMChannel(_State.Channel[channel]);
                break;
            }

            case 2:
            {
                _Driver.CurrentChannel = ChannelTable[channel][1];

                int ah = 1 << (_Driver.CurrentChannel - 1);

                ah |= (ah << 3);

                // SSG SetFMKeyOff
                _OPNAW->SetReg(0x07, ah | _OPNAW->GetReg(0x07));
                break;
            }

            case 3:
            {
                _OPNAW->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
                _OPNAW->SetReg(0x100, 0x01);    // PCM RESET
                break;
            }

            case 4:
            {
                if (_Effect.Number < 11)
                    StopEffect();
                break;
            }

            case 5:
            {
                _PPZ->Stop((size_t) ChannelTable[(size_t) channel][1]);
                break;
            }
        }
    }

    _State.Channel[channel]->MuteMask |= 0x01;

    _Driver.FMSelector = OldFMSelector;

    return ERR_SUCCESS;
}

/// <summary>
/// Enables the specified channel.
/// </summary>
int PMD::EnableChannel(int channel)
{
    if (channel >= MaxChannels)
        return ERR_WRONG_PARTNO;

    if (ChannelTable[channel][0] < 0)
    {
        _State.RhythmMask = 0xFF;

        return ERR_SUCCESS;
    }

    if (_State.Channel[channel]->MuteMask == 0x00)
        return ERR_NOT_MASKED;

    if ((_State.Channel[channel]->MuteMask &= 0xFE) != 0)
        return ERR_EFFECT_USED;

    if (!IsPlaying())
        return ERR_MUSIC_STOPPED;

    int OldFMSelector = _Driver.FMSelector;

    if (_State.Channel[channel]->Data)
    {
        if (ChannelTable[channel][2] == 0)
        {   // FM sound source (Front)
            _Driver.FMSelector = 0;
            _Driver.CurrentChannel = ChannelTable[channel][1];

            ResetFMInstrument(_State.Channel[channel]);
        }
        else
        if (ChannelTable[channel][2] == 1)
        {   // FM sound source (Back)
            _Driver.FMSelector = 0x100;
            _Driver.CurrentChannel = ChannelTable[channel][1];

            ResetFMInstrument(_State.Channel[channel]);
        }
    }

    _Driver.FMSelector = OldFMSelector;

    return ERR_SUCCESS;
}

/// <summary>
/// Gets the text of the specified memo.
/// </summary>
bool PMD::GetMemo(const uint8_t * data, size_t size, int index, char * text, size_t textSize)
{
    if ((text == nullptr) || (textSize < 1))
        return false;

    text[0] = '\0';

    char TwoBytezen[1024 + 64];

    GetText(data, size, index, TwoBytezen);

    char Han[1024 + 64];

    ZenToHan(Han, TwoBytezen);

    RemoveEscapeSequences(text, Han);

    return true;
}

/// <summary>
/// Gets the specified channel object.
/// </summary>
Channel * PMD::GetChannel(int channelNumber)
{
    if (channelNumber >= _countof(_State.Channel))
        return nullptr;

    return _State.Channel[channelNumber];
}
#pragma endregion

/// <summary>
/// Resets the driver.
/// </summary>
void PMD::Reset()
{
    _State.Reset();

    ::memset(_FMChannel, 0, sizeof(_FMChannel));
    ::memset(_SSGChannel, 0, sizeof(_SSGChannel));
    ::memset(&_ADPCMChannel, 0, sizeof(_ADPCMChannel));
    ::memset(&_RhythmChannel, 0, sizeof(_RhythmChannel));
    ::memset(_FMExtensionChannel, 0, sizeof(_FMExtensionChannel));
    ::memset(&_DummyChannel, 0, sizeof(_DummyChannel));
    ::memset(&_EffectChannel, 0, sizeof(_EffectChannel));
    ::memset(_PPZChannel, 0, sizeof(_PPZChannel));

    ::memset(&_SampleBank, 0, sizeof(_SampleBank));

    ::memset(_SampleSrc, 0, sizeof(_SampleSrc));
    ::memset(_SampleDst, 0, sizeof(_SampleSrc));
    ::memset(_SampleTmp, 0, sizeof(_SampleTmp));

    _SamplePtr = _SampleSrc;
    
    _SamplesToDo = 0;
    _Position = 0;
    _FadeOutPosition = 0;
    _Seed = 0;

    {
        ::memset(_MData, 0, sizeof(_MData));
        ::memset(_VData, 0, sizeof(_VData));
        ::memset(_EData, 0, sizeof(_EData));

        // Create some mock PMD data.
        for (size_t i = 0; i < 12; i += 2)
        {
            _MData[i + 1] = 0x18;
            _MData[i + 2] = 0x00;
        }

        _MData[25] = 0x80;

        _State.MData = &_MData[1];
        _State.VData = _VData;
        _State.EData = _EData;
    }

    _State.OPNARate = FREQUENCY_44_1K;
    _State.PPZRate = FREQUENCY_44_1K;

    _State.StopAfterFadeout = false;
    _State.IsTimerBBusy = false;

    _State.IsTimerABusy = false;
    _State.TimerBTempo = 0x100;

    _State.IsUsingP86 = false;

    _State.UseInterpolationPPZ = false;
    _State.UseInterpolationP86 = false;
    _State.UseInterpolationPPS = false;

    {
        _State.Channel[ 0] = &_FMChannel[0];
        _State.Channel[ 1] = &_FMChannel[1];
        _State.Channel[ 2] = &_FMChannel[2];
        _State.Channel[ 3] = &_FMChannel[3];
        _State.Channel[ 4] = &_FMChannel[4];
        _State.Channel[ 5] = &_FMChannel[5];

        _State.Channel[ 6] = &_SSGChannel[0];
        _State.Channel[ 7] = &_SSGChannel[1];
        _State.Channel[ 8] = &_SSGChannel[2];

        _State.Channel[ 9] = &_ADPCMChannel;

        _State.Channel[10] = &_RhythmChannel;

        _State.Channel[11] = &_FMExtensionChannel[0];
        _State.Channel[12] = &_FMExtensionChannel[1];
        _State.Channel[13] = &_FMExtensionChannel[2];

        _State.Channel[14] = &_DummyChannel; // Unused
        _State.Channel[15] = &_EffectChannel;

        _State.Channel[16] = &_PPZChannel[0];
        _State.Channel[17] = &_PPZChannel[1];
        _State.Channel[18] = &_PPZChannel[2];
        _State.Channel[19] = &_PPZChannel[3];
        _State.Channel[20] = &_PPZChannel[4];
        _State.Channel[21] = &_PPZChannel[5];
        _State.Channel[22] = &_PPZChannel[6];
        _State.Channel[23] = &_PPZChannel[7];
    }

    SetFMVolumeAdjustment(0);
    SetSSGVolumeAdjustment(0);
    SetADPCMVolumeAdjustment(0);
    _State.RhythmVolumeAdjust = 0;
    _State.DefaultRhythmVolumeAdjust = 0;
    SetPPZVolumeAdjustment(0);

    _State.RhythmVolume = 0x3C;
    _State.UseSSG = false;

    // Initialize the counters.
    _State.RhythmBassDrumOn = 0;
    _State.RhythmSnareDrumOn = 0;
    _State.RhythmCymbalOn = 0;
    _State.RhythmHiHatOn = 0;
    _State.RhythmTomDrumOn = 0;
    _State.RhythmRimShotOn = 0;

    // Initialize the counters.
    _State.RhythmBassDrumOff = 0;
    _State.RhythmSnareDrumOff = 0;
    _State.RhythmCymbalOff = 0;
    _State.RhythmHiHatOff = 0;
    _State.RhythmTomDrumOff = 0;
    _State.RhythmRimShotOff = 0;

    SetPMDB2CompatibilityMode(false);

    _State.StopAfterFadeout = true;

    {
        _Driver._Flags = DriverIdle;
        _Driver.UsePPS = false;
    }
}

/// <summary>
/// Starts the OPN interrupt.
/// </summary>
void PMD::StartOPNInterrupt()
{
    ::memset(_FMChannel, 0, sizeof(_FMChannel));
    ::memset(_SSGChannel, 0, sizeof(_SSGChannel));
    ::memset(&_ADPCMChannel, 0, sizeof(_ADPCMChannel));
    ::memset(&_RhythmChannel, 0, sizeof(_RhythmChannel));
    ::memset(&_DummyChannel, 0, sizeof(_DummyChannel));
    ::memset(_FMExtensionChannel, 0, sizeof(_FMExtensionChannel));
    ::memset(&_EffectChannel, 0, sizeof(_EffectChannel));
    ::memset(_PPZChannel, 0, sizeof(_PPZChannel));

    _State.RhythmMask = 0xFF;

    InitializeState();
    InitializeOPN();

    _OPNAW->SetReg(0x07, 0xBF);

    DriverStop();

    InitializeInterrupt();

    _OPNAW->SetReg(0x29, 0x83);
}

/// <summary>
/// Initializes the different state machines.
/// </summary>
void PMD::InitializeState()
{
    _State.FadeOutVolume = 0;
    _State.FadeOutSpeed = 0;
    _State.IsFadeOutSpeedSet = false;
    _State.FadeOutSpeedHQ = 0;

    _State.Status = 0;
    _State.LoopCount = 0;
    _State.BarCounter = 0;
    _State.OpsCounter = 0;
    _State.TimerATime = 0;

    _State.PCMStart = 0;
    _State.PCMStop = 0;

    _State.UseRhythmChannel = false;
    _State.RhythmChannelMask = 0;

    _State.FMSlot1Detune = 0;
    _State.FMSlot2Detune = 0;
    _State.FMSlot3Detune = 0;
    _State.FMSlot4Detune = 0;

    _State.FMChannel3Mode = 0x3F;
    _State.BarLength = 96;

    _State.FMVolumeAdjust = _State.DefaultFMVolumeAdjust;
    _State.SSGVolumeAdjust = _State.DefaultSSGVolumeAdjust;
    _State.ADPCMVolumeAdjust = _State.DefaultADPCMVolumeAdjust;
    _State.PPZVolumeAdjust = _State.DefaultPPZVolumeAdjust;
    _State.RhythmVolumeAdjust = _State.DefaultRhythmVolumeAdjust;

    _State.PMDB2CompatibilityMode = _State.DefaultPMDB2CompatibilityMode;

    for (int i = 0; i < _countof(_FMChannel); ++i)
    {
        int PartMask = _FMChannel[i].MuteMask;
        int KeyOnFlag = _FMChannel[i].KeyOnFlag;

        ::memset(&_FMChannel[i], 0, sizeof(Channel));

        _FMChannel[i].MuteMask = PartMask & 0x0F;
        _FMChannel[i].KeyOnFlag = KeyOnFlag;
        _FMChannel[i].Tone = 0xFF;
        _FMChannel[i].DefaultTone = 0xFF;
    }

    for (int i = 0; i < _countof(_SSGChannel); ++i)
    {
        int PartMask = _SSGChannel[i].MuteMask;
        int KeyOnFlag = _SSGChannel[i].KeyOnFlag;

        ::memset(&_SSGChannel[i], 0, sizeof(Channel));

        _SSGChannel[i].MuteMask = PartMask & 0x0F;
        _SSGChannel[i].KeyOnFlag = KeyOnFlag;
        _SSGChannel[i].Tone = 0xFF;
        _SSGChannel[i].DefaultTone = 0xFF;
    }

    {
        int PartMask = _ADPCMChannel.MuteMask;
        int KeyOnFlag = _ADPCMChannel.KeyOnFlag;

        ::memset(&_ADPCMChannel, 0, sizeof(Channel));

        _ADPCMChannel.MuteMask = PartMask & 0x0F;
        _ADPCMChannel.KeyOnFlag = KeyOnFlag;
        _ADPCMChannel.Tone = 0xFF;
        _ADPCMChannel.DefaultTone = 0xFF;
    }

    {
        int PartMask = _RhythmChannel.MuteMask;
        int KeyOnFlag = _RhythmChannel.KeyOnFlag;

        ::memset(&_RhythmChannel, 0, sizeof(Channel));

        _RhythmChannel.MuteMask = PartMask & 0x0f;
        _RhythmChannel.KeyOnFlag = KeyOnFlag;
        _RhythmChannel.Tone = 0xFF;
        _RhythmChannel.DefaultTone = 0xFF;
    }

    for (int i = 0; i < _countof(_FMExtensionChannel); ++i)
    {
        int PartMask = _FMExtensionChannel[i].MuteMask;
        int KeyOnFlag = _FMExtensionChannel[i].KeyOnFlag;

        ::memset(&_FMExtensionChannel[i], 0, sizeof(Channel));

        _FMExtensionChannel[i].MuteMask = PartMask & 0x0F;
        _FMExtensionChannel[i].KeyOnFlag = KeyOnFlag;
        _FMExtensionChannel[i].Tone = 0xFF;
        _FMExtensionChannel[i].DefaultTone = 0xFF;
    }

    for (int i = 0; i < _countof(_PPZChannel); ++i)
    {
        int PartMask = _PPZChannel[i].MuteMask;
        int KeyOnFlag = _PPZChannel[i].KeyOnFlag;

        ::memset(&_PPZChannel[i], 0, sizeof(Channel));

        _PPZChannel[i].MuteMask = PartMask & 0x0F;
        _PPZChannel[i].KeyOnFlag = KeyOnFlag;
        _PPZChannel[i].Tone = 0xFF;
        _PPZChannel[i].DefaultTone = 0xFF;
    }

    _Driver.Initialize();

    _Effect.PreviousInstrumentNumber = 0;
}

/// <summary>
/// Initializes the OPN.
/// </summary>
void PMD::InitializeOPN()
{
    _OPNAW->ClearBuffer();

    _OPNAW->SetReg(0x29, 0x83);

    _State.SSGNoiseFrequency = 0;

    _OPNAW->SetReg(0x06, 0x00);

    _State.OldSSGNoiseFrequency = 0;

    // Reset SSG-Type Envelope Control (4.8s)
    for (uint32_t i = 0x090; i < 0x09F; ++i)
    {
        if (i % 4 != 3)
            _OPNAW->SetReg(i, 0x00);
    }

    for (uint32_t i = 0x190; i < 0x19F; ++i)
    {
        if (i % 4 != 3)
            _OPNAW->SetReg(i, 0x00);
    }

    // Initialize the hardware LFO.
    _OPNAW->SetReg(0x0b4, 0xc0);
    _OPNAW->SetReg(0x0b5, 0xc0);
    _OPNAW->SetReg(0x0b6, 0xc0);
    _OPNAW->SetReg(0x1b4, 0xc0);
    _OPNAW->SetReg(0x1b5, 0xc0);
    _OPNAW->SetReg(0x1b6, 0xc0);

    _OPNAW->SetReg(0x22, 0x00); // Hardware LFO speed

    //  Rhythm Default = Pan : 0xC0 (3 << 6, Center) , Volume : 0x0F
    for (int i = 0; i < 6; ++i)
        _State.RhythmPanAndVolume[i] = 0xCF;

    _OPNAW->SetReg(0x10, 0xFF);

    // Set the Rhythm volume.
    _State.RhythmVolume = 48 * 4 * (256 - _State.RhythmVolumeAdjust) / 1024;

    _OPNAW->SetReg(0x11, (uint32_t) _State.RhythmVolume);

    // PCM reset & LIMIT SET
    _OPNAW->SetReg(0x10c, 0xff);
    _OPNAW->SetReg(0x10d, 0xff);

    for (size_t i = 0; i < MaxPPZChannels; ++i)
        _PPZ->SetPan(i, 5);
}

/// <summary>
/// Executes a command. The execution is the same for all sound sources.
/// </summary>
uint8_t * PMD::ExecuteCommand(Channel * channel, uint8_t * si, uint8_t command)
{
    switch (command)
    {
        case 0xFF: si++; break;
        case 0xFE: si++; break;

        // Set the volume.
        case 0xFD:
            channel->Volume = *si++;
            break;

        case 0xFC:
            si = ChangeTempoCommand(si);
            break;

        // Command "&": Tie notes together.
        case 0xFB:
            _Driver.TieNotesTogether = true;
            break;

        // Set detune.
        case 0xFA:
            channel->DetuneValue = *(int16_t *) si;
            si += 2;
            break;

        // Set loop start.
        case 0xF9:
            si = SetStartOfLoopCommand(channel, si);
            break;

        // Set loop end.
        case 0xF8:
            si = SetEndOfLoopCommand(channel, si);
            break;

        // Exit loop.
        case 0xF7:
            si = ExitLoopCommand(channel, si);
            break;

        // Command "L": Set the loop data.
        case 0xF6:
            channel->LoopData = si;
            break;

        // Set transposition.
        case 0xF5:
            channel->Transposition1 = *(int8_t *) si++;
            break;

        // Increase volume by 3dB.
        case 0xF4:
            channel->Volume += 16;

            if (channel->Volume > 255)
                channel->Volume = 255;
            break;

        // Decrease volume by 3dB.
        case 0xF3:
            channel->Volume -= 16;

            if (channel->Volume < 16)
                channel->Volume = 0;
            break;

        case 0xF2:
            si = SetModulation(channel, si);
            break;

        case 0xF1:
            si = SetModulationMask(channel, si);
            break;

        case 0xF0:
            si = SetSSGEnvelopeFormat1Command(channel, si);
            break;

        case 0xEB:
            si = OPNARhythmKeyOn(si);
            break;

        case 0xEA:
            si = SetOPNARhythmVolumeCommand(si);
            break;

        case 0xE9:
            si = SetOPNARhythmPanningCommand(si);
            break;

        case 0xE8:
            si = SetOPNARhythmMasterVolumeCommand(si);
            break;

        // Modify transposition.
        case 0xE7:
            channel->Transposition1 += *(int8_t *) si++;
            break;

        case 0xE6:
            si = ModifyOPNARhythmMasterVolume(si);
            break;

        case 0xE5:
            si = ModifyOPNARhythmVolume(si);
            break;

        // Set hardware LFO delay.
        case 0xE4: si++; break;

        // Set hardware LFO AM/FM (register B4)
        case 0xE1: si++; break;

        // Set hardware LFO speed (register 22)
        case 0xE0: si++; break;

        // Command "Z number": Set ticks per measure.
        case 0xDF:
            _State.BarLength = *si++;
            break;

        case 0xDD:
            si = DecreaseVolumeForNextNote(channel, si);
            break;

        // Set status.
        case 0xDC:
            _State.Status = *si++;
            break;

        // Increment status.
        case 0xDB:
            _State.Status += *si++;
            break;

        case 0xD9: si++; break; // Unused
        case 0xD8: si++; break; // Unused
        case 0xD7: si++; break; // Unused

        // Command "MD", "MDA", "MDB": Set LFO Depth Temporal Change
        case 0xD6:
            channel->LFO1MDepthSpeed1 = channel->LFO1MDepthSpeed2 = *si++;
            channel->LFO1MDepth = *(int8_t *) si++;
            break;

        // Modify detune.
        case 0xD5:
            channel->DetuneValue += *(int16_t *) si;
            si += 2;
            break;

        case 0xD4:
            si = SetSSGEffect(channel, si);
            break;

        case 0xD3:
            si = SetFMEffect(channel, si);
            break;

        // Set fade-out speed.
        case 0xD2:
            _State.FadeOutSpeed = *si++;
            _State.IsFadeOutSpeedSet = true;
            break;

        case 0xD1: si++; break; // Unused
        case 0xD0: si++; break;
        case 0xCF: si++; break;

        // Set PCM Repeat.
        case 0xCE: si += 6; break;

        case 0xCD:
            si = SetSSGEnvelopeFormat2Command(channel, si);
            break;

        // Set SSG Extend Mode.
        case 0xCC: si++; break;

        // Set modulation waveform.
        case 0xCB:
            channel->LFO1Waveform = *si++;
            break;

        case 0xC8: si += 3; break;
        case 0xC7: si += 3; break;
        case 0xC6: si += 6; break;
        case 0xC5: si++; break;

        // Set Early Key Off Timeout Percentage. Stops note (length * pp / 100h) ticks early, added to value of command FE.
        case 0xC4:
            channel->EarlyKeyOffTimeoutPercentage = *si++;
            break;

        case 0xC2:
            channel->LFO1Delay1 = channel->LFO1Delay2 = *si++;
            InitializeLFOMain(channel);
            break;

        case 0xBF:
            SwapLFO(channel);

            si = SetModulation(channel, si);

            SwapLFO(channel);
            break;

        case 0xBD:
            SwapLFO(channel);

            channel->LFO1MDepthSpeed1 = channel->LFO1MDepthSpeed2 = *si++;
            channel->LFO1MDepth = *(int8_t *) si++;

            SwapLFO(channel);
            break;

        case 0xBC:
            SwapLFO(channel);

            channel->LFO1Waveform = *si++;

            SwapLFO(channel);
            break;

        case 0xBB:
            SwapLFO(channel);

            channel->ExtendMode = (channel->ExtendMode & 0xFD) | ((*si++ & 0x01) << 1);

            SwapLFO(channel);
            break;

        case 0xBA:
            si = SetVolumeMask(channel, si);
            break;

        case 0xB9:
            SwapLFO(channel);

            channel->LFO1Delay1 = channel->LFO1Delay2 = *si++;
            InitializeLFOMain(channel);

            SwapLFO(channel);
            break;

        case 0xB8: si += 2; break;

        case 0xB7:
            si = SetMDepthCountCommand(channel, si);
            break;

        case 0xB6: si++; break;
        case 0xB5: si += 2; break;
        case 0xB4: si += 16; break;

        // Set Early Key Off Timeout 2. Stop note after n ticks or earlier depending on the result of B1/C4/FE happening first.
        case 0xB3:
            channel->EarlyKeyOffTimeout2 = *si++;
            break;

        // Set secondary transposition.
        case 0xB2:
            channel->Transposition2 = *(int8_t *) si++;
            break;

        // Set Early Key Off Timeout Randomizer Range. (0..tt ticks, added to the value of command C4 and FE)
        case 0xB1:
            channel->EarlyKeyOffTimeoutRandomRange = *si++;
            break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}



















/// <summary>
///
/// </summary>
void PMD::HandleTimerA()
{
    _State.IsTimerABusy = true;

    {
        _State.TimerATime++;

        if ((_State.TimerATime & 0x07) == 0)
            Fade();

        if ((_Effect.Priority != 0) && (!_Driver.UsePPS || (_Effect.Number == 0x80)))
            PlayEffect(); // Use the SSG for effect processing.
    }

    _State.IsTimerABusy = false;
}

/// <summary>
///
/// </summary>
void PMD::HandleTimerB()
{
    _State.IsTimerBBusy = true;

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

        _Driver.OldTimerATime = _State.TimerATime;
    }

    _State.IsTimerBBusy = false;
}

#pragma region(Commands)
/// <summary>
/// Increases the volume for the next note only (Command "v").
/// </summary>
uint8_t * PMD::IncreaseVolumeForNextNote(Channel * channel, uint8_t * si, int maxVolume)
{
    int Volume = (int) channel->Volume + *si++ + 1;

    if (Volume > maxVolume)
        Volume = maxVolume;

    channel->VolumeBoost = Volume;
    _Driver.IsVolumeBoostSet = 1;

    return si;
}

/// <summary>
/// Decreases the volume for the next note only.
/// </summary>
uint8_t * PMD::DecreaseVolumeForNextNote(Channel * channel, uint8_t * si)
{
    int Volume = channel->Volume - *si++;

    if (Volume < 1)
        Volume = 1;

    channel->VolumeBoost = Volume;
    _Driver.IsVolumeBoostSet = 1;

    return si;
}

uint8_t * PMD::SpecialC0ProcessingCommand(Channel * channel, uint8_t * si, uint8_t value)
{
    switch (value)
    {
        case 0xFF:
            _State.FMVolumeAdjust = *si++; // 0..255 or -128..127
            break;

        case 0xFE:
            si = DecreaseFMVolumeCommand(channel, si);
            break;

        case 0xFD:
            _State.SSGVolumeAdjust = *si++; // 0..255 or -128..127
            break;

        case 0xFC:
            si = DecreaseSSGVolumeCommand(channel, si);
            break;

        case 0xFB:
            _State.ADPCMVolumeAdjust = *si++; // 0..255 or -128..127
            break;

        case 0xFA:
            si = DecreaseADPCMVolumeCommand(channel, si);
            break;

        case 0xF9:
            _State.RhythmVolumeAdjust = *si++; // 0..255 or -128..127
            break;

        case 0xF8:
            si = DecreaseRhythmVolumeCommand(channel, si);
            break;

        case 0xF7:
            _State.PMDB2CompatibilityMode = ((*si++ & 0x01) == 0x01);
            break;

        case 0xF6:
            _State.PPZVolumeAdjust = *si++; // 0..255 or -128..127
            break;

        case 0xF5:
            si = DecreasePPZVolumeCommand(channel, si);
            break;

        default: // Unknown value. Force the end of the part by replacing the code with <End of Part>.
            si--;
            *si = 0x80;
    }

    return si;
}

// Command "# number1, [number2]": Sets the hardware LFO on (1) or off (0). (OPNA FM sound source only). Number2 = depth. Can be omitted only when switch is 0.
uint8_t * PMD::SetHardwareLFOSwitchCommand(Channel * channel, uint8_t * si)
{
    channel->ModulationMode = (channel->ModulationMode & 0x8F) | ((*si++ & 0x07) << 4);

    SwapLFO(channel);

    InitializeLFOMain(channel);

    SwapLFO(channel);

    return si;
}

uint8_t * PMD::SetVolumeMask(Channel * channel, uint8_t * si)
{
    int al = *si++ & 0x0F;

    if (al != 0)
    {
        al = (al << 4) | 0x0F;

        channel->VolumeMask2 = al;
    }
    else
        channel->VolumeMask2 = channel->FMCarrier;

    SetFMChannel3Mode(channel);

    return si;
}

// Command 't' [TEMPO CHANGE1] / COMMAND 'T' [TEMPO CHANGE2] / COMMAND 't±' [TEMPO CHANGE 相対1] / COMMAND 'T±' [TEMPO CHANGE 相対2]
uint8_t * PMD::ChangeTempoCommand(uint8_t * si)
{
    int al = *si++;

    // Add to Ticks per Quarter (T (FC)).
    if (al < 0xFB)
    {
        _State.Tempo = al;
        _State.TempoPush = al;

        ConvertTimerBTempoToMetronomeTempo();
    }
    else
    // Set Ticks per Quarter (t (FC FF)).
    if (al == 0xFF)
    {
        al = *si++;

        if (al < 18)
            al = 18;

        _State.MetronomeTempo = al;
        _State.MetronomeTempoPush = al;

        ConvertMetronomeTempoToTimerBTempo();
    }
    else
    // Add to Tempo (T± (FC FE)).
    if (al == 0xFE)
    {
        al = *si++;

        if (al >= 0)
            al += _State.TempoPush;
        else
        {
            al += _State.TempoPush;

            if (al < 0)
                al = 0;
        }

        if (al > 0xFA)
            al = 0xFA;

        _State.Tempo = al;
        _State.TempoPush = al;

        ConvertTimerBTempoToMetronomeTempo();
    }
    // Set Tempo to ticks (t± (FC FD)).
    else
    {
        al = *si++;

        if (al >= 0)
        {
            al += _State.MetronomeTempoPush;

            if (al > 255)
                al = 255;
        }
        else
        {
            al += _State.MetronomeTempoPush;

            if (al < 0)
                al = 18;
        }

        _State.MetronomeTempo = al;
        _State.MetronomeTempoPush = al;

        ConvertMetronomeTempoToTimerBTempo();
    }

    return si;
}

// Command '[': Set start of loop
uint8_t * PMD::SetStartOfLoopCommand(Channel * channel, uint8_t * si)
{
    uint8_t * Data = (channel == &_EffectChannel) ? _State.EData : _State.MData;

    Data[*(uint16_t *) si + 1] = 0;

    si += 2;

    return si;
}

// Command ']': Set end of loop
uint8_t * PMD::SetEndOfLoopCommand(Channel * channel, uint8_t * si)
{
    int MaxLoopCount = *si++;

    if (MaxLoopCount != 0)
    {
        (*si)++; // Increase loop count.

        int LoopCount = *si++;

        if (LoopCount == MaxLoopCount)
        {
            si += 2; // Skip offset.

            return si;
        }
    }
    else // Loop endlessly.
    {
        si++; // Skip loop count.
        channel->loopcheck = 1;
    }

    // Jump to offset + 2.
    int Offset = *(uint16_t *) si + 2;

    si = ((channel == &_EffectChannel) ? _State.EData : _State.MData) + Offset;

    return si;
}

// Command ':': Exit Loop
uint8_t * PMD::ExitLoopCommand(Channel * channel, uint8_t * si)
{
    uint8_t * Data = (channel == &_EffectChannel) ? _State.EData : _State.MData;

    Data += *(uint16_t *) si;
    si += 2;

    int dl = *Data++ - 1;

    if (dl != *Data)
        return si;

    si = Data + 3;

    return si;
}

/// <summary>
/// Sets the modulation parameters.
/// </summary>
uint8_t * PMD::SetModulation(Channel * channel, uint8_t * si)
{
    channel->LFO1Delay1 = *si;
    channel->LFO1Delay2 = *si++;
    channel->LFO1Speed1 = *si;
    channel->LFO1Speed2 = *si++;
    channel->LFO1Step1 = *(int8_t *) si;
    channel->LFO1Step2 = *(int8_t *) si++;
    channel->LFO1Time1 = *si;
    channel->LFO1Time2 = *si++;

    InitializeLFOMain(channel);

    return si;
}

uint8_t * PMD::SetMDepthCountCommand(Channel * channel, uint8_t * si)
{
    int al = *si++;

    if (al >= 0x80)
    {
        al &= 0x7f;

        if (al == 0)
            al = 255;

        channel->LFO2MDepthCount1  = al;
        channel->LFO2MDepthCount2 = al;

        return si;
    }

    if (al == 0)
        al = 255;

    channel->LFO1MDepthCount1  = al;
    channel->LFO1MDepthCount2 = al;

    return si;
}
#pragma endregion

/// <summary>
/// Completely muting the [PartB] part (TL=127 and RR=15 and KEY-OFF). cy=1 ･･･ All slots are neiromasked
/// </summary>
int PMD::MuteFMChannel(Channel * channel)
{
    if (channel->ToneMask == 0)
        return 1;

    int dh = _Driver.CurrentChannel + 0x40 - 1;

    if (channel->ToneMask & 0x80)
    {
        _OPNAW->SetReg((uint32_t) ( _Driver.FMSelector         + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver.FMSelector + 0x40) + dh), 127);
    }

    dh += 4;

    if (channel->ToneMask & 0x40)
    {
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver.FMSelector + 0x40) + dh), 127);
    }

    dh += 4;

    if (channel->ToneMask & 0x20)
    {
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver.FMSelector + 0x40) + dh), 127);
    }

    dh += 4;

    if (channel->ToneMask & 0x10)
    {
        _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_Driver.FMSelector + 0x40) + dh), 127);
    }

    FMKeyOff(channel);

    return 0;
}

/// <summary>
///
/// </summary>
int PMD::TransposeSSG(Channel * channel, int srcTone)
{
    return Transpose(channel, srcTone);
}

/// <summary>
///
/// </summary>
int PMD::Transpose(Channel * channel, int srcTone)
{
    if (srcTone == 0x0F)
        return srcTone;

    int Transposition = channel->Transposition1 + channel->Transposition2;

    if (Transposition == 0)
        return srcTone;

    int Octave = (srcTone & 0xF0) >> 4;
    int Note   = (srcTone & 0x0F);

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
uint8_t * PMD::CalculateQ(Channel * channel, uint8_t * si)
{
    if (*si == 0xC1)
    {
        si++;
        channel->GateTime = 0;

        return si;
    }

    int dl = channel->EarlyKeyOffTimeout;

    if (channel->EarlyKeyOffTimeoutPercentage != 0)
        dl += (channel->Length * channel->EarlyKeyOffTimeoutPercentage) >> 8;

    if (channel->EarlyKeyOffTimeoutRandomRange != 0)
    {
        int ax = rnd((channel->EarlyKeyOffTimeoutRandomRange & 0x7F) + 1);

        if ((channel->EarlyKeyOffTimeoutRandomRange & 0x80) == 0)
        {
            dl += ax;
        }
        else
        {
            dl -= ax;

            if (dl < 0)
                dl = 0;
        }
    }

    if (channel->EarlyKeyOffTimeout2 != 0)
    {
        int dh = channel->Length - channel->EarlyKeyOffTimeout2;

        if (dh < 0)
        {
            channel->GateTime = 0;

            return si;
        }

        channel->GateTime = (dl < dh) ? dl : dh;
    }
    else
        channel->GateTime = dl;

    return si;
}

/// <summary>
///
/// </summary>
void PMD::CalculatePortamento(Channel * channel)
{
    channel->Portamento += channel->PortamentoQuotient;

    if (channel->PortamentoRemainder == 0)
        return;

    if (channel->PortamentoRemainder > 0)
    {
        channel->PortamentoRemainder--;
        channel->Portamento++;
    }
    else
    {
        channel->PortamentoRemainder++;
        channel->Portamento--;
    }
}

/// <summary>
/// Gets a random number.
/// </summary>
/// <param name="ax">MAX_RANDOM</param>
int PMD::rnd(int ax)
{
    _Seed = (259 * _Seed + 3) & 0x7fff;

    return _Seed * ax / 32767;
}

/// <summary>
/// Swap between LFO 1 and LFO 2.
/// </summary>
void PMD::SwapLFO(Channel * channel)
{
    Swap(&channel->LFO1Data, &channel->LFO2Data);

    channel->ModulationMode  = ((channel->ModulationMode & 0x0F) << 4) + (channel->ModulationMode >> 4);
    channel->ExtendMode = ((channel->ExtendMode & 0x0F) << 4) + (channel->ExtendMode >> 4);

    Swap(&channel->LFO1Delay1, &channel->LFO2Delay1);
    Swap(&channel->LFO1Speed1, &channel->LFO2Speed1);
    Swap(&channel->LFO1Step1, &channel->LFO2Step1);
    Swap(&channel->LFO1Time1, &channel->LFO2Time1);
    Swap(&channel->LFO1Delay2, &channel->LFO2Delay2);
    Swap(&channel->LFO1Speed2, &channel->LFO2Speed2);
    Swap(&channel->LFO1Step2, &channel->LFO2Step2);
    Swap(&channel->LFO1Time2, &channel->LFO2Time2);
    Swap(&channel->LFO1MDepth, &channel->LFO2MDepth);
    Swap(&channel->LFO1MDepthSpeed1, &channel->LFO2MDepthSpeed1);
    Swap(&channel->LFO1MDepthSpeed2, &channel->LFO2MDepthSpeed2);
    Swap(&channel->LFO1Waveform, &channel->LFO2Waveform);
    Swap(&channel->LFO1MDepthCount1, &channel->LFO2MDepthCount1);
    Swap(&channel->LFO1MDepthCount2, &channel->LFO2MDepthCount2);
}

/// <summary>
/// Set the tempo.
/// </summary>
void PMD::SetTimerBTempo()
{
    if (_State.TimerBTempo != _State.Tempo)
    {
        _State.TimerBTempo = _State.Tempo;

        _OPNAW->SetReg(0x26, (uint32_t) _State.TimerBTempo);
    }
}

/// <summary>
/// Increase bar counter.
/// </summary>
void PMD::IncreaseBarCounter()
{
    if (_State.OpsCounter + 1 == _State.BarLength)
    {
        _State.BarCounter++;
        _State.OpsCounter = 0;
    }
    else
        _State.OpsCounter++;
}

/// <summary>
/// Interrupt settings. FM tone generator only
/// </summary>
void PMD::InitializeInterrupt()
{
    // OPN interrupt initial setting
    _State.Tempo = 200;
    _State.TempoPush = 200;

    ConvertTimerBTempoToMetronomeTempo();
    SetTimerBTempo();

    _OPNAW->SetReg(0x25, 0x00); // Timer A Set (9216μs fixed)
    _OPNAW->SetReg(0x24, 0x00); // The slowest and just right
    _OPNAW->SetReg(0x27, 0x3F); // Timer Enable

    //　Measure counter reset
    _State.OpsCounter = 0;
    _State.BarCounter = 0;
    _State.BarLength = 96;
}

/// <summary>
///
/// </summary>
void PMD::Silence()
{
    _OPNAW->SetReg(0x80, 0xff); // FM Release = 15
    _OPNAW->SetReg(0x81, 0xff);
    _OPNAW->SetReg(0x82, 0xff);
    _OPNAW->SetReg(0x84, 0xff);
    _OPNAW->SetReg(0x85, 0xff);
    _OPNAW->SetReg(0x86, 0xff);
    _OPNAW->SetReg(0x88, 0xff);
    _OPNAW->SetReg(0x89, 0xff);
    _OPNAW->SetReg(0x8a, 0xff);
    _OPNAW->SetReg(0x8c, 0xff);
    _OPNAW->SetReg(0x8d, 0xff);
    _OPNAW->SetReg(0x8e, 0xff);

    _OPNAW->SetReg(0x180, 0xff);
    _OPNAW->SetReg(0x181, 0xff);
    _OPNAW->SetReg(0x184, 0xff);
    _OPNAW->SetReg(0x185, 0xff);
    _OPNAW->SetReg(0x188, 0xff);
    _OPNAW->SetReg(0x189, 0xff);
    _OPNAW->SetReg(0x18c, 0xff);
    _OPNAW->SetReg(0x18d, 0xff);

    _OPNAW->SetReg(0x182, 0xff);
    _OPNAW->SetReg(0x186, 0xff);
    _OPNAW->SetReg(0x18a, 0xff);
    _OPNAW->SetReg(0x18e, 0xff);

    _OPNAW->SetReg(0x28, 0x00); // FM KEYOFF
    _OPNAW->SetReg(0x28, 0x01);
    _OPNAW->SetReg(0x28, 0x02);
    _OPNAW->SetReg(0x28, 0x04); // FM KEYOFF [URA]
    _OPNAW->SetReg(0x28, 0x05);
    _OPNAW->SetReg(0x28, 0x06);

    _PPS->Stop();
    _P86->Stop();
    _P86->SetPan(3, 0); // Center

    _OPNAW->SetReg(0x07, 0xBF);
    _OPNAW->SetReg(0x08, 0x00);
    _OPNAW->SetReg(0x09, 0x00);
    _OPNAW->SetReg(0x0a, 0x00);

    _OPNAW->SetReg(0x10, 0xff);   // Rhythm dump

    _OPNAW->SetReg(0x101, 0x02);  // PAN=0 / x8 bit mode
    _OPNAW->SetReg(0x100, 0x01);  // PCM RESET
    _OPNAW->SetReg(0x110, 0x80);  // TA/TB/EOS を RESET
    _OPNAW->SetReg(0x110, 0x18);  // Bit change only for TIMERB/A/EOS

    for (size_t i = 0; i < MaxPPZChannels; ++i)
        _PPZ->Stop(i);
}

/// <summary>
/// Fade In / Out
/// </summary>
void PMD::Fade()
{
    if (_State.FadeOutSpeed == 0)
        return;

    if (_State.FadeOutSpeed > 0)
    {
        if ((_State.FadeOutVolume + _State.FadeOutSpeed) < 256)
        {
            _State.FadeOutVolume += _State.FadeOutSpeed;
        }
        else
        {
            _State.FadeOutVolume = 255;
            _State.FadeOutSpeed  =   0;

            if (_State.StopAfterFadeout)
                _Driver._Flags |= DriverStopRequested;
        }
    }
    else
    {   // Fade in
        if ((_State.FadeOutVolume + _State.FadeOutSpeed) > 255)
        {
            _State.FadeOutVolume += _State.FadeOutSpeed;
        }
        else
        {
            _State.FadeOutVolume = 0;
            _State.FadeOutSpeed  = 0;

            _OPNAW->SetReg(0x11, (uint32_t) _State.RhythmVolume);
        }
    }
}

/// <summary>
/// Sets the start address and initial value of each track.
/// </summary>
void PMD::InitializeChannels()
{
    _State.x68_flg = _State.MData[-1];

    const uint16_t * Offsets = (const uint16_t *) _State.MData;

    for (size_t i = 0; i < _countof(_FMChannel); ++i)
    {
        if (_State.MData[*Offsets] == 0x80) // Do not play.
            _FMChannel[i].Data = nullptr;
        else
            _FMChannel[i].Data = &_State.MData[*Offsets];

        _FMChannel[i].Length = 1;
        _FMChannel[i].KeyOffFlag = 0xFF;
        _FMChannel[i].LFO1MDepthCount1 = -1;    // LFO1MDepth Counter (-1 = infinite)
        _FMChannel[i].LFO1MDepthCount2 = -1;
        _FMChannel[i].LFO2MDepthCount1 = -1;
        _FMChannel[i].LFO2MDepthCount2 = -1;
        _FMChannel[i].Tone = 0xFF;              // Rest
        _FMChannel[i].DefaultTone = 0xFF;       // Rest
        _FMChannel[i].Volume = 108;
        _FMChannel[i].PanAndVolume = 0xC0;      // 3 << 6, Center
        _FMChannel[i].FMSlotMask = 0xF0;
        _FMChannel[i].ToneMask = 0xFF;

        Offsets++;
    }

    for (size_t i = 0; i < _countof(_SSGChannel); ++i)
    {
        if (_State.MData[*Offsets] == 0x80) // Do not play.
            _SSGChannel[i].Data = nullptr;
        else
            _SSGChannel[i].Data = &_State.MData[*Offsets];

        _SSGChannel[i].Length = 1;
        _SSGChannel[i].KeyOffFlag = 0xFF;
        _SSGChannel[i].LFO1MDepthCount1 = -1;   // LFO1MDepth Counter (-1 = infinite)
        _SSGChannel[i].LFO1MDepthCount2 = -1;
        _SSGChannel[i].LFO2MDepthCount1 = -1;
        _SSGChannel[i].LFO2MDepthCount2 = -1;
        _SSGChannel[i].Tone = 0xFF;             // Rest
        _SSGChannel[i].DefaultTone = 0xFF;      // Rest
        _SSGChannel[i].Volume = 8;
        _SSGChannel[i].SSGMask = 0x07;          // Tone
        _SSGChannel[i].SSGEnvelopFlag = 3;      // SSG ENV = NONE/normal

        Offsets++;
    }

    {
        if (_State.MData[*Offsets] == 0x80) // Do not play
            _ADPCMChannel.Data = nullptr;
        else
            _ADPCMChannel.Data = &_State.MData[*Offsets];

        _ADPCMChannel.Length = 1;
        _ADPCMChannel.KeyOffFlag = 0xFF;
        _ADPCMChannel.LFO1MDepthCount1 = -1;
        _ADPCMChannel.LFO1MDepthCount2 = -1;
        _ADPCMChannel.LFO2MDepthCount1 = -1;
        _ADPCMChannel.LFO2MDepthCount2 = -1;
        _ADPCMChannel.Tone = 0xFF;
        _ADPCMChannel.DefaultTone = 0xFF;
        _ADPCMChannel.Volume = 128;
        _ADPCMChannel.PanAndVolume = 0xC0;      // 3 << 6, Center

        Offsets++;
    }

    {
        if (_State.MData[*Offsets] == 0x80) // Do not play
            _RhythmChannel.Data = nullptr;
        else
            _RhythmChannel.Data = &_State.MData[*Offsets];

        _RhythmChannel.Length = 1;
        _RhythmChannel.KeyOffFlag = 0xFF;
        _RhythmChannel.LFO1MDepthCount1 = -1;
        _RhythmChannel.LFO1MDepthCount2 = -1;
        _RhythmChannel.LFO2MDepthCount1 = -1;
        _RhythmChannel.LFO2MDepthCount2 = -1;
        _RhythmChannel.Tone = 0xFF;             // Rest
        _RhythmChannel.DefaultTone = 0xFF;      // Rest
        _RhythmChannel.Volume = 15;

        Offsets++;
    }

    {
        _State.RhythmDataTable = (uint16_t *) &_State.MData[*Offsets];

        _State.DummyRhythmData = 0xFF;
        _State.RhythmData = &_State.DummyRhythmData;
    }

    {
        Offsets = (const uint16_t *) _State.MData;

        if (Offsets[0] != sizeof(uint16_t) * MaxParts) // 0x0018
            _State.InstrumentDefinitions = _State.MData + Offsets[12];
    }
}

/// <summary>
/// Tempo conversion
/// </summary>
void PMD::ConvertTimerBTempoToMetronomeTempo()
{
    int al;

    if (256 - _State.Tempo == 0)
    {
        al = 255;
    }
    else
    {
        al = (0x112c * 2 / (256 - _State.Tempo) + 1) / 2; // Tempo = 0x112C / [ 256 - TB ]  timerB -> tempo

        if (al > 255)
            al = 255;
    }

    _State.MetronomeTempo = al;
    _State.MetronomeTempoPush = al;
}

/// <summary>
/// Tempo conversion (input: tempo_48, output: tempo_d)
/// </summary>
void PMD::ConvertMetronomeTempoToTimerBTempo()
{
    int al;

    if (_State.MetronomeTempo >= 18)
    {
        al = 256 - 0x112c / _State.MetronomeTempo; // TB = 256 - [ 112CH / TEMPO ]  tempo -> timerB

        if (0x112c % _State.MetronomeTempo >= 128)
            al--;

    //  al = 256 - (0x112c * 2 / _State.tempo_48 + 1) / 2;
    }
    else
        al = 0;

    _State.Tempo = al;
    _State.TempoPush = al;
}

/// <summary>
/// Loads the PPC file.
/// </summary>
int PMD::LoadPPCInternal(const WCHAR * filePath)
{
    _PCMFilePath.clear();

    if (*filePath == '\0')
        return ERR_OPEN_FAILED;

    if (!_File->Open(filePath))
        return ERR_OPEN_FAILED;

    int64_t Size = _File->GetFileSize(filePath);

    if (Size < 0)
        return ERR_OPEN_FAILED;

    uint8_t * Data = (uint8_t *) ::malloc((size_t) Size);

    if (Data == NULL)
        return ERR_OUT_OF_MEMORY;

    _File->Read(Data, (uint32_t) Size);
    _File->Close();

    int Result = LoadPPCInternal(Data, (int) Size);

    if (Result == ERR_SUCCESS)
        _PCMFilePath = filePath;

    ::free(Data);

    return Result;
}

/// <summary>
/// Loads the PPC data.
/// </summary>
int PMD::LoadPPCInternal(uint8_t * data, int size)
{
    if (size < 0x10)
        return ERR_UNKNOWN_FORMAT;

    bool FoundPVI;

    int i;
    int bx = 0;

    if ((::strncmp((char *) data, PVIHeader, sizeof(PVIHeader) - 1) == 0) && (data[10] == 2))
    {   // PVI, x8
        FoundPVI = true;

        // Convert from PVI to PMD format.
        for (i = 0; i < 128; ++i)
        {
            if (*((uint16_t *) &data[18 + i * 4]) == 0)
            {
                _SampleBank.Address[i][0] = data[16 + i * 4];
                _SampleBank.Address[i][1] = 0;
            }
            else
            {
                _SampleBank.Address[i][0] = (uint16_t) (*(uint16_t *) &data[16 + i * 4] + 0x26);
                _SampleBank.Address[i][1] = (uint16_t) (*(uint16_t *) &data[18 + i * 4] + 0x26);
            }

            if (bx < _SampleBank.Address[i][1])
                bx = _SampleBank.Address[i][1] + 1;
        }

        // The remaining 128 are undefined
        for (i = 128; i < 256; ++i)
        { 
            _SampleBank.Address[i][0] = 0;
            _SampleBank.Address[i][1] = 0;
        }

        _SampleBank.Count = (uint16_t) bx;
    }
    else
    if (::strncmp((char *) data, PPCHeader, sizeof(PPCHeader) - 1) == 0)
    {   // PPC
        FoundPVI = false;

        if (size < sizeof(PPCHeader) + sizeof(uint16_t) + (sizeof(uint16_t) * 2 * 256))
            return ERR_UNKNOWN_FORMAT;

        uint16_t * Data = (uint16_t *)(data + (sizeof(PPCHeader) - 1));

        _SampleBank.Count = *Data++;

        for (i = 0; i < 256; ++i)
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
        ReadPCMData(0, 0x25, Data);

        if (::memcmp(Data + sizeof(PPCHeader), &_SampleBank, sizeof(_SampleBank)) == 0)
            return ERR_ALREADY_LOADED;
    }

    {
        uint8_t Data[sizeof(PPCHeader) + sizeof(uint16_t) + (sizeof(uint16_t) * 2 * 256) + 128];

        // Compare the data with the PCMRAM header.
        ::memcpy(Data, PPCHeader, sizeof(PPCHeader) - 1);
        ::memcpy(Data + (sizeof(PPCHeader) - 1), &_SampleBank.Count, sizeof(Data) - (sizeof(PPCHeader) - 1));

        WritePCMData(0, 0x25, Data);
    }

    // Write the data to PCMRAM.
    {
        uint16_t * Data;

        if (FoundPVI)
        {
            Data = (uint16_t *)(data + 0x10 + (sizeof(uint16_t) * 2 * 128));

            if (size < (int)(_SampleBank.Count - (0x10 + sizeof(uint16_t) * 2 * 128)) * 32)
                return ERR_UNKNOWN_FORMAT;
        }
        else
        {
            Data = (uint16_t *)(data + sizeof(PPCHeader) + sizeof(uint16_t) + (sizeof(uint16_t) * 2 * 256));

            if (size < (int)(_SampleBank.Count - (sizeof(PPCHeader) + sizeof(uint16_t) + (sizeof(uint16_t) * 2 * 256) / 2) * 32))
                return ERR_UNKNOWN_FORMAT;
        }

        uint16_t pcmstart = 0x26;
        uint16_t pcmstop = _SampleBank.Count;

        WritePCMData(pcmstart, pcmstop, (const uint8_t *) Data);
    }

    return ERR_SUCCESS;
}

/// <summary>
/// Read data from PCM memory to main memory.
/// </summary>
void PMD::ReadPCMData(uint16_t startAddress, uint16_t stopAddress, uint8_t * data)
{
    _OPNAW->SetReg(0x100, 0x01);
    _OPNAW->SetReg(0x110, 0x00);
    _OPNAW->SetReg(0x110, 0x80);
    _OPNAW->SetReg(0x100, 0x20);
    _OPNAW->SetReg(0x101, 0x02); // x8
    _OPNAW->SetReg(0x10c, 0xff);
    _OPNAW->SetReg(0x10d, 0xff);
    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(startAddress));
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(startAddress));
    _OPNAW->SetReg(0x104, 0xff);
    _OPNAW->SetReg(0x105, 0xff);

    *data = (uint8_t) _OPNAW->GetReg(0x108);
    *data = (uint8_t) _OPNAW->GetReg(0x108);

    for (int i = 0; i < (stopAddress - startAddress) * 32; ++i)
    {
        *data++ = (uint8_t) _OPNAW->GetReg(0x108);

        _OPNAW->SetReg(0x110, 0x80);
    }
}

/// <summary>
/// Send data from main memory to PCM memory (x8, high speed version)
/// </summary>
void PMD::WritePCMData(uint16_t startAddress, uint16_t stopAddress, const uint8_t * data)
{
    _OPNAW->SetReg(0x100, 0x01);
//  _OPNAW->SetReg(0x110, 0x17); // Mask everything except brdy (=timer interrupt does not occur)
    _OPNAW->SetReg(0x110, 0x80);
    _OPNAW->SetReg(0x100, 0x60);
    _OPNAW->SetReg(0x101, 0x02); // x8
    _OPNAW->SetReg(0x10c, 0xff);
    _OPNAW->SetReg(0x10d, 0xff);
    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(startAddress));
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(startAddress));
    _OPNAW->SetReg(0x104, 0xff);
    _OPNAW->SetReg(0x105, 0xff);

    for (int i = 0; i < (stopAddress - startAddress) * 32; ++i)
        _OPNAW->SetReg(0x108, *data++);
}

/// <summary>
/// Finds a PCM sample in the specified search path.
/// </summary>
void PMD::FindFile(const WCHAR * filename, WCHAR * filePath, size_t size) const noexcept
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
void PMD::GetText(const uint8_t * data, size_t size, int index, char * text) const noexcept
{
    *text = '\0';

    if (data == nullptr || size < 0x0019)
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
        ::strcpy(text, (char *) &Data[Offset]);
}
