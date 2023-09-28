﻿
// PMD driver (Based on PMDWin code by C60)

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4710 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "PMD.h"

#include "Utility.h"
#include "Table.h"

#include "OPNAW.h"
#include "PPZ8.h"
#include "PPS.h"
#include "P86.h"

/// <summary>
/// Initializes an instance.
/// </summary>
PMD::PMD()
{
    _File = new File();

    _OPNAW = new OPNAW(_File);

    _PPZ8 = new PPZ8Driver(_File);
    _PPS  = new PPSDriver(_File);
    _P86  = new P86Driver(_File);

    Reset();
}

/// <summary>
/// Destroys an instance.
/// </summary>
PMD::~PMD()
{
    delete _P86;
    delete _PPS;
    delete _PPZ8;

    delete _OPNAW;
}

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

    _PPZ8->Initialize(_State.OPNARate, false);
    _PPS->Initialize(_State.OPNARate, false);
    _P86->Initialize(_State.OPNARate, false);

    if (_OPNAW->Initialize(OPNAClock, SOUND_44K, false, DirectoryPath) == false)
        return false;

    // Initialize ADPCM RAM.
    {
        _OPNAW->SetFMDelay(0);
        _OPNAW->SetSSGDelay(0);
        _OPNAW->SetRSSDelay(0);
        _OPNAW->SetADPCMDelay(0);

        uint8_t Page[0x400]; // 0x400 * 0x100 = 0x40000(256K)

        ::memset(Page, 0x08, sizeof(Page));

        for (int i = 0; i < 0x100; ++i)
            WritePCMData((uint16_t) i * sizeof(Page) / 32, (uint16_t) (i + 1) * sizeof(Page) / 32, Page);
    }

    _OPNAW->SetFMVolume(0);
    _OPNAW->SetSSGVolume(-18);
    _OPNAW->SetADPCMVolume(0);
    _OPNAW->SetRSSVolume(0);

    _PPZ8->SetVolume(0);
    _PPS->SetVolume(0);
    _P86->SetVolume(0);

    _OPNAW->SetFMDelay(DEFAULT_REG_WAIT);
    _OPNAW->SetSSGDelay(DEFAULT_REG_WAIT);
    _OPNAW->SetADPCMDelay(DEFAULT_REG_WAIT);
    _OPNAW->SetRSSDelay(DEFAULT_REG_WAIT);

    pcmends.Count = 0x26;

    for (int i = 0; i < 256; ++i)
    {
        pcmends.Address[i][0] = 0;
        pcmends.Address[i][1] = 0;
    }

    _State.PPCFileName[0] = '\0';

    // Initial setting of 088/188/288/388 (same INT number only)
    _OPNAW->SetReg(0x29, 0x00);
    _OPNAW->SetReg(0x24, 0x00);
    _OPNAW->SetReg(0x25, 0x00);
    _OPNAW->SetReg(0x26, 0x00);
    _OPNAW->SetReg(0x27, 0x3f);

    // Start the OPN interrupt.
    StartOPNInterrupt();

    return true;
}

void PMD::Reset()
{
    ::memset(&_State, 0, sizeof(_State));
    ::memset(&_DriverState, 0, sizeof(_DriverState));
    ::memset(&_EffectState, 0, sizeof(_EffectState));

    ::memset(_FMTrack, 0, sizeof(_FMTrack));
    ::memset(_SSGTrack, 0, sizeof(_SSGTrack));
    ::memset(&_ADPCMTrack, 0, sizeof(_ADPCMTrack));
    ::memset(&_RhythmTrack, 0, sizeof(_RhythmTrack));
    ::memset(_ExtensionTrack, 0, sizeof(_ExtensionTrack));
    ::memset(&_DummyTrack, 0, sizeof(_DummyTrack));
    ::memset(&_EffectTrack, 0, sizeof(_EffectTrack));
    ::memset(_PPZ8Track, 0, sizeof(_PPZ8Track));

    ::memset(&pcmends, 0, sizeof(pcmends));

    ::memset(_SampleSrc, 0, sizeof(_SampleSrc));
    ::memset(_SampleDst, 0, sizeof(_SampleSrc));
    ::memset(_SampleTmp, 0, sizeof(_SampleTmp));

    _SamplePtr = _SampleSrc;
    
    _SamplesToDo = 0;
    _Position = 0;
    _FadeOutPosition = 0;
    _Seed = 0;

    ::memset(_MData, 0, sizeof(_MData));
    ::memset(_VData, 0, sizeof(_VData));
    ::memset(_EData, 0, sizeof(_EData));
    ::memset(&pcmends, 0, sizeof(pcmends));

    // Initialize OPEN_WORK.
    _State.OPNARate = SOUND_44K;
    _State.PPZ8Rate = SOUND_44K;
    _State.RhythmVolume = 0x3c;
    _State.fade_stop_flag = 0;
    _State.IsTimerBBusy = false;

    _State.IsTimerABusy = false;
    _State.TimerBTempo = 0x100;
    _State.port22h = 0;

    _State.IsUsingP86 = false;

    _State.UseInterpolationPPZ8 = false;
    _State.UseInterpolationP86 = false;
    _State.UseInterpolationPPS = false;

    // Initialize variables.
    _State.Track[ 0] = &_FMTrack[0];
    _State.Track[ 1] = &_FMTrack[1];
    _State.Track[ 2] = &_FMTrack[2];
    _State.Track[ 3] = &_FMTrack[3];
    _State.Track[ 4] = &_FMTrack[4];
    _State.Track[ 5] = &_FMTrack[5];

    _State.Track[ 6] = &_SSGTrack[0];
    _State.Track[ 7] = &_SSGTrack[1];
    _State.Track[ 8] = &_SSGTrack[2];

    _State.Track[ 9] = &_ADPCMTrack;

    _State.Track[10] = &_RhythmTrack;

    _State.Track[11] = &_ExtensionTrack[0];
    _State.Track[12] = &_ExtensionTrack[1];
    _State.Track[13] = &_ExtensionTrack[2];

    _State.Track[14] = &_DummyTrack; // Unused
    _State.Track[15] = &_EffectTrack;

    _State.Track[16] = &_PPZ8Track[0];
    _State.Track[17] = &_PPZ8Track[1];
    _State.Track[18] = &_PPZ8Track[2];
    _State.Track[19] = &_PPZ8Track[3];
    _State.Track[20] = &_PPZ8Track[4];
    _State.Track[21] = &_PPZ8Track[5];
    _State.Track[22] = &_PPZ8Track[6];
    _State.Track[23] = &_PPZ8Track[7];

    _State.fm_voldown = fmvd_init;   // FM_VOLDOWN
    _State._fm_voldown = fmvd_init;  // FM_VOLDOWN

    _State.ssg_voldown = 0;          // SSG_VOLDOWN
    _State._ssg_voldown = 0;         // SSG_VOLDOWN

    _State.pcm_voldown = 0;          // PCM_VOLDOWN
    _State._pcm_voldown = 0;         // PCM_VOLDOWN

    _State.ppz_voldown = 0;          // PPZ_VOLDOWN
    _State._ppz_voldown = 0;         // PPZ_VOLDOWN

    _State.rhythm_voldown = 0;       // RHYTHM_VOLDOWN
    _State._rhythm_voldown = 0;      // RHYTHM_VOLDOWN

    _State.UseRhythm = false;        // Use the Rhythm sound source

    _State.rshot_bd = 0;             // Rhythm Sound Source shot inc flag (BD)
    _State.rshot_sd = 0;             // Rhythm Sound Source shot inc flag (SD)
    _State.rshot_sym = 0;            // Rhythm Sound Source shot inc flag (CYM)
    _State.rshot_hh = 0;             // Rhythm Sound Source shot inc flag (HH)
    _State.rshot_tom = 0;            // Rhythm Sound Source shot inc flag (TOM)
    _State.rshot_rim = 0;            // Rhythm Sound Source shot inc flag (RIM)

    _State.rdump_bd = 0;             // Rhythm Sound dump inc flag (BD)
    _State.rdump_sd = 0;             // Rhythm Sound dump inc flag (SD)
    _State.rdump_sym = 0;            // Rhythm Sound dump inc flag (CYM)
    _State.rdump_hh = 0;             // Rhythm Sound dump inc flag (HH)
    _State.rdump_tom = 0;            // Rhythm Sound dump inc flag (TOM)
    _State.rdump_rim = 0;            // Rhythm Sound dump inc flag (RIM)

    _State.pcm86_vol = 0;            // PCM volume adjustment
    _State._pcm86_vol = 0;           // PCM volume adjustment
    _State.fade_stop_flag = 1;       // MSTOP after FADEOUT FLAG

    _DriverState.UsePPS = false;
    _DriverState.music_flag = 0;

    _MData[0] = 0;

    for (size_t i = 0; i < 12; i += 2)
    {
        _MData[i + 1] = 0x18;
        _MData[i + 2] = 0x00;
    }

    _MData[25] = 0x80;

    _State.MData = &_MData[1];
    _State.VData = _VData;
    _State.EData = _EData;

    // Initialize sound effects FMINT/EFCINT.
    _EffectState.effon = 0;
    _EffectState.psgefcnum = 0xff;
}

bool PMD::IsPMD(const uint8_t * data, size_t size) noexcept
{
    if (size < 3)
        return false;

    if (size > sizeof(_MData))
        return false;

    if (data[0] > 0x0F && data[0] != 0xFF)
        return false;

    if (data[1] != 0x18 && data[1] != 0x1A)
        return false;

    if (data[2] != 0x00 && data[2] != 0xE6)
        return false;

    return true;
}

int PMD::Load(const uint8_t * data, size_t size)
{
    if (!IsPMD(data, size))
        return ERR_UNKNOWN_FORMAT;

    Stop();

    ::memcpy(_MData, data, size);
    ::memset(_MData + size, 0, sizeof(_MData) - size);

    if (_State.SearchPath.size() == 0)
        return ERR_SUCCESS;

    int Result = ERR_SUCCESS;

    char FileName[MAX_PATH] = { 0 };
    WCHAR FileNameW[MAX_PATH] = { 0 };

    WCHAR FilePath[MAX_PATH] = { 0 };

    {
        GetText(data, size, 0, FileName);

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

            // P86 import (ADPCM, 8-bit sample playback, stereo, panning)
            if (HasExtension(FileNameW, _countof(FileNameW), L".P86")) // Is it a Professional Music Driver P86 Samples Pack file?
            {
                FindFile(FilePath, FileNameW);

                Result = _P86->Load(FilePath);

                if (Result == P86_SUCCESS || Result == P86_ALREADY_LOADED)
                    _State.IsUsingP86 = true;
            }
            else
            // PPC import (ADPCM, 4-bit sample playback, 256kB max.)
            if (HasExtension(FileNameW, _countof(FileNameW), L".PPC"))
            {
                FindFile(FilePath, FileNameW);

                Result = LoadPPCInternal(FilePath);

                if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
                    _State.IsUsingP86 = false;
            }
        }
    }

    {
        GetText(data, size, -1, FileName);

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

            // PPS import (PCM driver for the SSG, which allows 4-bit 16000Hz PCM playback on the SSG Channel 3. It can also play 2 samples simultanelously, but at a lower quality)
            if (HasExtension(FileNameW, _countof(FileNameW), L".PPS"))
            {
                FindFile(FilePath, FileNameW);

                Result = _PPS->Load(FilePath);
            }
        }
    }

    // 20010120 Ignore if TOWNS
    {
        GetText(data, size, -2, FileName);

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

            // PZI import (Up to 8 PCM channels using the 86PCM, with soft panning possibilities and no memory limit)
            if (HasExtension(FileNameW, _countof(FileNameW), L".PZI") && (data[0] != 0xFF))
            {
                FindFile(FilePath, FileNameW);

                Result = _PPZ8->Load(FilePath, 0);
            }
            else
            // PMB import
            if (HasExtension(FileNameW, _countof(FileNameW), L".PMB") && (data[0] != 0xFF))
            {
                WCHAR * p = ::wcschr(FileNameW, ',');

                if (p == nullptr)
                {
                    if ((p = ::wcschr(FileNameW, '.')) == nullptr)
                        RenameExtension(FileNameW, _countof(FileNameW), L".PZI");

                    FindFile(FilePath, FileNameW);

                    Result = _PPZ8->Load(FilePath, 0);
                }
                else
                {
                    *p = '\0';

                    WCHAR PPZFileName2[MAX_PATH] = { 0 };

                    ::wcscpy(PPZFileName2, p + 1);

                    if ((p = ::wcschr(FileNameW, '.')) == nullptr)
                        RenameExtension(FileNameW, _countof(FileNameW), L".PZI");

                    if ((p = ::wcschr(PPZFileName2, '.')) == nullptr)
                        RenameExtension(PPZFileName2, _countof(PPZFileName2), L".PZI");

                    FindFile(FilePath, FileNameW);

                    Result = _PPZ8->Load(FilePath, 0);

                    FindFile(FilePath, PPZFileName2);

                    Result = _PPZ8->Load(FilePath, 1);
                }
            }
        }
    }

    return ERR_SUCCESS;
}

/// <summary>
/// Gets the length of the song and loop part (in ms).
/// </summary>
bool PMD::GetLength(int * songLength, int * loopLength)
{
    DriverStart();

    _Position = 0; // Time from start of playing (μs)
    *songLength = 0;

    int FMDelay = _OPNAW->GetFMDelay();
    int SSGDelay = _OPNAW->GetSSGDelay();
    int ADPCMDelay = _OPNAW->GetADPCMDelay();
    int RSSDelay = _OPNAW->GetRSSDelay();

    _OPNAW->SetFMDelay(0);
    _OPNAW->SetSSGDelay(0);
    _OPNAW->SetADPCMDelay(0);
    _OPNAW->SetRSSDelay(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNAW->SetReg(0x27, _State.ch3mode | 0x30); // Timer Reset (Both timer A and B)

            uint32_t us = _OPNAW->GetNextEvent();

            _OPNAW->Count(us);
            _Position += us;
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
            _OPNAW->SetRSSDelay(RSSDelay);

            return true;
        }
        else
        if (GetEventNumber() >= 65536) // Forced termination if 65536 clocks or more
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
    _OPNAW->SetRSSDelay(RSSDelay);

    return true;
}

/// <summary>
/// Gets the number of events in the song and loop part.
/// </summary>
bool PMD::GetLengthInEvents(int * eventCount, int * loopEventCount)
{
    DriverStart();

    _Position = 0; // Time from start of playing (μs)
    *eventCount = 0;

    int FMDelay = _OPNAW->GetFMDelay();
    int SSGDelay = _OPNAW->GetSSGDelay();
    int ADPCMDelay = _OPNAW->GetADPCMDelay();
    int RSSDelay = _OPNAW->GetRSSDelay();

    _OPNAW->SetFMDelay(0);
    _OPNAW->SetSSGDelay(0);
    _OPNAW->SetADPCMDelay(0);
    _OPNAW->SetRSSDelay(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNAW->SetReg(0x27, _State.ch3mode | 0x30);  // Timer Reset (Both timer A and B)

            uint32_t us = _OPNAW->GetNextEvent();

            _OPNAW->Count(us);
            _Position += us;
        }

        if ((_State.LoopCount == 1) && (*eventCount == 0)) // When looping
        {
            *eventCount = GetEventNumber();
        }
        else
        if (_State.LoopCount == -1) // End without loop
        {
            *eventCount = GetEventNumber();
            *loopEventCount = 0;

            DriverStop();

            _OPNAW->SetFMDelay(FMDelay);
            _OPNAW->SetSSGDelay(SSGDelay);
            _OPNAW->SetADPCMDelay(ADPCMDelay);
            _OPNAW->SetRSSDelay(RSSDelay);

            return true;
        }
        else
        if (GetEventNumber() >= 65536) // Forced termination if 65536 clocks or more
        {
            *eventCount = GetEventNumber();
            *loopEventCount = *eventCount;

            return true;
        }
    }
    while (_State.LoopCount < 2);

    *loopEventCount = GetEventNumber() - *eventCount;

    DriverStop();

    _OPNAW->SetFMDelay(FMDelay);
    _OPNAW->SetSSGDelay(SSGDelay);
    _OPNAW->SetADPCMDelay(ADPCMDelay);
    _OPNAW->SetRSSDelay(RSSDelay);

    return true;
}

// Gets the current loop number.
uint32_t PMD::GetLoopNumber()
{
    return (uint32_t) _State.LoopCount;
}

// Gets the playback position (in ms)
uint32_t PMD::GetPosition()
{
    return (uint32_t) (_Position / 1000);
}

// Sets the playback position (in ms)
void PMD::SetPosition(uint32_t position)
{
    int64_t NewPosition = (int64_t) position * 1000; // (ms -> conversion to usec)

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

        _OPNAW->SetReg(0x27, _State.ch3mode | 0x30); // Timer Reset (Both timer A and B)

        uint32_t us = _OPNAW->GetNextEvent();

        _OPNAW->Count(us);
        _Position += us;
    }

    if (_State.LoopCount == -1)
        Silence();

    _OPNAW->ClearBuffer();
}

// Renders a chunk of PCM data.
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

                _OPNAW->SetReg(0x27, _State.ch3mode | 0x30); // Timer Reset (Both timer A and B)
            }

            uint32_t us = _OPNAW->GetNextEvent(); // in microseconds

            {
                _SamplesToDo = (size_t) ((double) us * _State.OPNARate / 1000000.0);
                _OPNAW->Count(us);

                ::memset(_SampleDst, 0, _SamplesToDo * sizeof(Stereo32bit));

                if (_State.OPNARate == _State.PPZ8Rate)
                    _PPZ8->Mix((Sample *) _SampleDst, _SamplesToDo);
                else
                {
                    // PCM frequency transform of ppz8 (no interpolation)
                    size_t SampleCount = (size_t) (_SamplesToDo * _State.PPZ8Rate / _State.OPNARate + 1);
                    int delta = (int) (8192 * _State.PPZ8Rate / _State.OPNARate);

                    ::memset(_SampleTmp, 0, SampleCount * sizeof(Sample) * 2);

                    _PPZ8->Mix((Sample *) _SampleTmp, SampleCount);

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

                if (_DriverState.UsePPS)
                    _PPS->Mix((Sample *) _SampleDst, _SamplesToDo);

                if (_State.IsUsingP86)
                    _P86->Mix((Sample *) _SampleDst, _SamplesToDo);
            }

            {
                _Position += us;

                if (_State.FadeOutSpeedHQ > 0)
                {
                    int Factor = (_State.LoopCount != -1) ? (int) ((1 << 10) * ::pow(512, -(double) (_Position - _FadeOutPosition) / 1000 / _State.FadeOutSpeedHQ)) : 0;

                    for (size_t i = 0; i < _SamplesToDo; ++i)
                    {
                        _SampleSrc[i].Left  = (int16_t) Limit(_SampleDst[i].Left  * Factor >> 10, 32767, -32768);
                        _SampleSrc[i].Right = (int16_t) Limit(_SampleDst[i].Right * Factor >> 10, 32767, -32768);
                    }

                    // Fadeout end
                    if ((_Position - _FadeOutPosition > (int64_t)_State.FadeOutSpeedHQ * 1000) && (_State.fade_stop_flag == 1))
                        _DriverState.music_flag |= 2;
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

// Reload rhythm sound
bool PMD::LoadRythmSample(WCHAR * path)
{
    WCHAR Path[MAX_PATH];

    ::wcscpy(Path, path);
    AddBackslash(Path, _countof(Path));

    Stop();

    return _OPNAW->LoadInstruments(Path);
}

// Sets the PCM search directory
bool PMD::SetSearchPaths(std::vector<const WCHAR *> & paths)
{
    for (std::vector<const WCHAR *>::iterator iter = paths.begin(); iter < paths.end(); iter++)
    {
        WCHAR Path[MAX_PATH];

        ::wcscpy(Path, *iter);
        AddBackslash(Path, _countof(Path));

        _State.SearchPath.push_back(Path);
    }

    return true;
}

/// <summary>
/// Sets the rate at which raw PCM data is synthesized (in Hz, for example 44100)
/// </summary>
void PMD::SetSynthesisRate(uint32_t frequency)
{
    if (frequency == SOUND_55K || frequency == SOUND_55K_2)
    {
        _State.OPNARate =
        _State.PPZ8Rate = SOUND_44K;
        _State.UseFM55kHzSynthesis = true;
    }
    else
    {
        _State.OPNARate =
        _State.PPZ8Rate = frequency;
        _State.UseFM55kHzSynthesis = false;
    }

    _OPNAW->SetRate(OPNAClock, _State.OPNARate, _State.UseFM55kHzSynthesis);

    _PPZ8->SetRate(_State.PPZ8Rate, _State.UseInterpolationPPZ8);
    _PPS->SetRate(_State.OPNARate, _State.UseInterpolationPPS);
    _P86->SetSampleRate(_State.OPNARate, _State.UseInterpolationP86);
}

/// <summary>
/// Enables or disables 55kHz synthesis in FM primary interpolation.
/// </summary>
void PMD::SetFM55kHzSynthesisMode(bool flag)
{
    _State.UseFM55kHzSynthesis = flag;

    _OPNAW->SetRate(OPNAClock, _State.OPNARate, _State.UseFM55kHzSynthesis);
}

/// <summary>
/// Sets the rate at which raw PPZ data is synthesized (in Hz, for example 44100)
/// </summary>
void PMD::SetPPZSynthesisRate(uint32_t frequency)
{
    _State.PPZ8Rate = frequency;

    _PPZ8->SetRate(frequency, _State.UseInterpolationPPZ8);
}

/// <summary>
/// Enables or disables PPZ interpolation.
/// </summary>
void PMD::SetPPZInterpolation(bool flag)
{
    _State.UseInterpolationPPZ8 = flag;

    _PPZ8->SetRate(_State.PPZ8Rate, flag);
}

/// <summary>
/// Enables or disables PPS interpolation.
/// </summary>
void PMD::SetPPSInterpolation(bool flag)
{
    _State.UseInterpolationPPS = flag;

    _PPS->SetRate(_State.OPNARate, flag);
}

/// <summary>
/// Enables or disables P86 interpolation.
/// </summary>
void PMD::SetP86Interpolation(bool flag)
{
    _State.UseInterpolationP86 = flag;

    _P86->SetSampleRate(_State.OPNARate, flag);
}

// Sets FM Wait after register output.
void PMD::SetFMDelay(int nsec)
{
    _OPNAW->SetFMDelay(nsec);
}

// Sets SSG Wait after register output.
void PMD::SetSSGDelay(int nsec)
{
    _OPNAW->SetSSGDelay(nsec);
}

// Sets Rythm Wait after register output.
void PMD::SetRSSDelay(int nsec)
{
    _OPNAW->SetRSSDelay(nsec);
}

// Sets ADPCM Wait after register output.
void PMD::SetADPCMDelay(int nsec)
{
    _OPNAW->SetADPCMDelay(nsec);
}

// Fade out (PMD compatible)
void PMD::SetFadeOutSpeed(int speed)
{
    _State.FadeOutSpeed = speed;
}

// Fade out (High quality sound)
void PMD::SetFadeOutDurationHQ(int speed)
{
    if (speed > 0)
    {
        if (_State.FadeOutSpeedHQ == 0)
            _FadeOutPosition = _Position;

        _State.FadeOutSpeedHQ = speed;
    }
    else
        _State.FadeOutSpeedHQ = 0; // Fadeout forced stop
}

// Sets the playback position (in ticks).
void PMD::SetEventNumber(int eventNumber)
{
    if (((_State.BarLength * _State.BarCounter) + _State.OpsCounter) > eventNumber)
    {
        DriverStart();

        _SamplePtr = _SampleSrc;
        _SamplesToDo = 0;
    }

    while (((_State.BarLength * _State.BarCounter) + _State.OpsCounter) < eventNumber)
    {
        if (_OPNAW->ReadStatus() & 0x01)
            HandleTimerA();

        if (_OPNAW->ReadStatus() & 0x02)
            HandleTimerB();

        _OPNAW->SetReg(0x27, _State.ch3mode | 0x30); // Timer Reset (Both timer A and B)

        uint32_t us = _OPNAW->GetNextEvent();

        _OPNAW->Count(us);
    }

    if (_State.LoopCount == -1)
        Silence();

    _OPNAW->ClearBuffer();
}

// Gets the playback position (in ticks)
int PMD::GetEventNumber()
{
    return (_State.BarLength * _State.BarCounter) + _State.OpsCounter;
}

// Gets PPC / P86 filename.
WCHAR * PMD::GetPCMFileName(WCHAR * filePath)
{
    if (_State.IsUsingP86)
        ::wcscpy(filePath, _P86->_FilePath);
    else
        ::wcscpy(filePath, _State.PPCFileName);

    return filePath;
}

// Gets PPZ filename.
WCHAR * PMD::GetPPZFileName(WCHAR * filePath, int index)
{
    ::wcscpy(filePath, _PPZ8->_FilePath[index]);

    return filePath;
}

/// <summary>
/// Enables or disables the PPS.
/// </summary>
void PMD::UsePPS(bool value) noexcept
{
    _DriverState.UsePPS = value;
}

/// <summary>
/// Enables playing the OPNA rhythm with the Rhythm sound source.
/// </summary>
void PMD::UseRhythm(bool flag) noexcept
{
    _State.UseRhythm = flag;
}

// Make PMD86 PCM compatible with PMDB2?
void PMD::EnablePMDB2CompatibilityMode(bool value)
{
    if (value)
    {
        _State.pcm86_vol =
        _State._pcm86_vol = 1;
    }
    else
    {
        _State.pcm86_vol =
        _State._pcm86_vol = 0;
    }
}

// Get whether PMD86's PCM is PMDB2 compatible
bool PMD::GetPMDB2CompatibilityMode()
{
    return _State.pcm86_vol ? true : false;
}

/// <summary>
/// Enables the specified part.
/// </summary>
int PMD::maskon(int ch)
{
    if (ch >= sizeof(_State.Track) / sizeof(Track *))
        return ERR_WRONG_PARTNO;

    if (TrackTable[ch][0] < 0)
    {
        _State.RhythmMask = 0;  // Rhythm音源をMask
        _OPNAW->SetReg(0x10, 0xff);  // Rhythm音源を全部Dump
    }
    else
    {
        int fmseltmp = _DriverState.fmsel;

        if ((_State.Track[ch]->PartMask == 0) && _State.IsPlaying)
        {
            if (TrackTable[ch][2] == 0)
            {
                _DriverState.CurrentChannel = TrackTable[ch][1];
                _DriverState.fmsel = 0;

                MuteFMPart(_State.Track[ch]);
            }
            else
            if (TrackTable[ch][2] == 1)
            {
                _DriverState.CurrentChannel = TrackTable[ch][1];
                _DriverState.fmsel = 0x100;

                MuteFMPart(_State.Track[ch]);
            }
            else
            if (TrackTable[ch][2] == 2)
            {
                _DriverState.CurrentChannel = TrackTable[ch][1];

                int ah = 1 << (_DriverState.CurrentChannel - 1);

                ah |= (ah << 3);

                // SSG KeyOff
                _OPNAW->SetReg(0x07, ah | _OPNAW->GetReg(0x07));
            }
            else
            if (TrackTable[ch][2] == 3)
            {
                _OPNAW->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
                _OPNAW->SetReg(0x100, 0x01);    // PCM RESET
            }
            else
            if (TrackTable[ch][2] == 4)
            {
                if (_EffectState.psgefcnum < 11)
                    effend();
            }
            else
            if (TrackTable[ch][2] == 5)
                _PPZ8->Stop(TrackTable[ch][1]);
        }

        _State.Track[ch]->PartMask |= 1;
        _DriverState.fmsel = fmseltmp;
    }

    return ERR_SUCCESS;
}

/// <summary>
/// Disables the specified part.
/// </summary>
int PMD::maskoff(int ch)
{
    if (ch >= sizeof(_State.Track) / sizeof(Track *))
        return ERR_WRONG_PARTNO;

    if (TrackTable[ch][0] < 0)
    {
        _State.RhythmMask = 0xff;
    }
    else
    {
        if (_State.Track[ch]->PartMask == 0)
            return ERR_NOT_MASKED;

        // Still masked by sound effects

        if ((_State.Track[ch]->PartMask &= 0xFE) != 0)
            return ERR_EFFECT_USED;

        // The song has stopped.
        if (!_State.IsPlaying)
            return ERR_MUSIC_STOPPED;

        int fmseltmp = _DriverState.fmsel;

        if (_State.Track[ch]->Data)
        {
            if (TrackTable[ch][2] == 0)
            {    // FM音源(表)
                _DriverState.fmsel = 0;
                _DriverState.CurrentChannel = TrackTable[ch][1];
                ResetTone(_State.Track[ch]);
            }
            else
            if (TrackTable[ch][2] == 1)
            {  // FM音源(裏)
                _DriverState.fmsel = 0x100;
                _DriverState.CurrentChannel = TrackTable[ch][1];
                ResetTone(_State.Track[ch]);
            }
        }

        _DriverState.fmsel = fmseltmp;
    }

    return ERR_SUCCESS;
}

//  FM Volume Down の設定
void PMD::setfmvoldown(int voldown)
{
    _State.fm_voldown = _State._fm_voldown = voldown;
}

//  SSG Volume Down の設定
void PMD::setssgvoldown(int voldown)
{
    _State.ssg_voldown = _State._ssg_voldown = voldown;
}

//  Rhythm Volume Down の設定
void PMD::setrhythmvoldown(int voldown)
{
    _State.rhythm_voldown = _State._rhythm_voldown = voldown;
    _State.RhythmVolume   = 48 * 4 * (256 - _State.rhythm_voldown) / 1024;

    _OPNAW->SetReg(0x11, (uint32_t) _State.RhythmVolume);
}

//  ADPCM Volume Down の設定
void PMD::setadpcmvoldown(int voldown)
{
    _State.pcm_voldown = _State._pcm_voldown = voldown;
}

//  PPZ8 Volume Down の設定
void PMD::setppzvoldown(int voldown)
{
    _State.ppz_voldown = _State._ppz_voldown = voldown;
}

//  FM Volume Down の取得
int PMD::getfmvoldown()
{
    return _State.fm_voldown;
}

//  FM Volume Down の取得（その２）
int PMD::getfmvoldown2()
{
    return _State._fm_voldown;
}

//  SSG Volume Down の取得
int PMD::getssgvoldown()
{
    return _State.ssg_voldown;
}

//  SSG Volume Down の取得（その２）
int PMD::getssgvoldown2()
{
    return _State._ssg_voldown;
}

//  Rhythm Volume Down の取得
int PMD::getrhythmvoldown()
{
    return _State.rhythm_voldown;
}

//  Rhythm Volume Down の取得（その２）
int PMD::getrhythmvoldown2()
{
    return _State._rhythm_voldown;
}

//  ADPCM Volume Down の取得
int PMD::getadpcmvoldown()
{
    return _State.pcm_voldown;
}

//  ADPCM Volume Down の取得（その２）
int PMD::getadpcmvoldown2()
{
    return _State._pcm_voldown;
}

//  PPZ8 Volume Down の取得
int PMD::getppzvoldown()
{
    return _State.ppz_voldown;
}

//  PPZ8 Volume Down の取得（その２）
int PMD::getppzvoldown2()
{
    return _State._ppz_voldown;
}

// Gets a note.
bool PMD::GetNote(const uint8_t * data, size_t size, int index, char * text, size_t textSize)
{
    if ((text == nullptr) || (textSize < 1))
        return false;

    text[0] = '\0';

    char a[1024 + 64];

    GetText(data, size, index, a);

    char b[1024 + 64];

    zen2tohan(b, a);

    RemoveEscapeSequences(text, b);

    return true;
}

// Gets a note
void PMD::GetText(const uint8_t * data, size_t size, int index, char * text) const noexcept
{
    *text = '\0';

    const uint8_t * Data;
    size_t Size;

    if (data == nullptr || size == 0)
    {
        Data = _State.MData;
        Size = sizeof(_MData) - 1;
    }
    else
    {
        Data = &data[1];
        Size = size - 1;
    }

    if (Size < 2)
        return;

    if (Data[0] != 0x1a || Data[1] != 0x00)
        return;

    if (Size < (size_t) 0x18 + 1)
        return;

    size_t Offset = (size_t) *(uint16_t *) &Data[0x18] - 4;

    if (Size <  Offset + 3)
        return;

    const uint8_t * Src = &Data[Offset];

    {
        if (Src[2] != 0x40)
        {
            if (Src[3] != 0xFE || Src[2] < 0x41)
                return;
        }

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
            if (Size < (size_t)(Src - Data + 1))
                return;

            Offset = *(uint16_t *) Src;

            if (Offset == 0)
                return;

            if (Size < (size_t) Offset)
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

// Load PPC
int PMD::LoadPPC(const WCHAR * filePath)
{
    Stop();

    int Result = LoadPPCInternal(filePath);

    if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
        _State.IsUsingP86 = false;

    return Result;
}

// Load PPS
int PMD::LoadPPS(const WCHAR * filePath)
{
    Stop();

    int Result = _PPS->Load(filePath);

    switch (Result)
    {
        case PPS_SUCCESS:        return ERR_SUCCESS;
        case PPS_OPEN_FAILED:    return ERR_OPEN_FAILED;
        case PPS_ALREADY_LOADED: return ERR_ALREADY_LOADED;
        case PPZ_OUT_OF_MEMORY:  return ERR_OUT_OF_MEMORY;
        default:                 return ERR_UNKNOWN;
    }
}

// Load P86
int PMD::LoadP86(const WCHAR * filename)
{
    Stop();

    int Result = _P86->Load(filename);

    if (Result == P86_SUCCESS || Result == P86_ALREADY_LOADED)
        _State.IsUsingP86 = true;

    switch (Result)
    {
        case P86_SUCCESS:           return ERR_SUCCESS;
        case P86_OPEN_FAILED:       return ERR_OPEN_FAILED;
        case P86_UNKNOWN_FORMAT:    return ERR_UNKNOWN_FORMAT;
        case P86_ALREADY_LOADED:    return ERR_ALREADY_LOADED;
        case PPZ_OUT_OF_MEMORY:     return ERR_OUT_OF_MEMORY;
        default:                    return ERR_UNKNOWN;
    }
}

// Load .PZI, .PVI
int PMD::LoadPPZ(const WCHAR * filename, int bufnum)
{
    Stop();

    int Result = _PPZ8->Load(filename, bufnum);

    switch (Result)
    {
        case PPZ_SUCCESS:           return ERR_SUCCESS;
        case PPZ_OPEN_FAILED:       return ERR_OPEN_FAILED;
        case PPZ_UNKNOWN_FORMAT:    return ERR_UNKNOWN_FORMAT;
        case PPZ_ALREADY_LOADED:    return ERR_ALREADY_LOADED;
        case PPZ_OUT_OF_MEMORY:     return ERR_OUT_OF_MEMORY;
        default:                    return ERR_UNKNOWN;
    }
}

Track * PMD::GetTrack(int trackNumber)
{
    if (trackNumber >= _countof(_State.Track))
        return nullptr;

    return _State.Track[trackNumber];
}

void PMD::HandleTimerA()
{
    _State.IsTimerABusy = true;
    _State.TimerATime++;

    if ((_State.TimerATime & 7) == 0)
        Fade();

    if (_EffectState.effon && (!_DriverState.UsePPS || _EffectState.psgefcnum == 0x80))
        effplay(); // SSG Sound Source effect processing

    _State.IsTimerABusy = false;
}

void PMD::HandleTimerB()
{
    _State.IsTimerBBusy = true;

    if (_DriverState.music_flag != 0x00)
    {
        if (_DriverState.music_flag & 0x01)
            DriverStart();

        if (_DriverState.music_flag & 0x02)
            DriverStop();
    }

    if (_State.IsPlaying)
    {
        DriverMain();
        SetTimerBTempo();
        IncreaseBarCounter();

        _DriverState.OldTimerATime = _State.TimerATime;
    }

    _State.IsTimerBBusy = false;
}

void PMD::DriverMain()
{
    int i;

    _DriverState.loop_work = 3;

    if (_State.x68_flg == 0)
    {
        for (i = 0; i < 3; ++i)
        {
            _DriverState.CurrentChannel = i + 1;
            SSGMain(&_SSGTrack[i]);
        }
    }

    _DriverState.fmsel = 0x100;

    for (i = 0; i < 3; ++i)
    {
        _DriverState.CurrentChannel = i + 1;
        FMMain(&_FMTrack[i + 3]);
    }

    _DriverState.fmsel = 0;

    for (i = 0; i < 3; ++i)
    {
        _DriverState.CurrentChannel = i + 1;
        FMMain(&_FMTrack[i]);
    }

    for (i = 0; i < 3; ++i)
    {
        _DriverState.CurrentChannel = 3;
        FMMain(&_ExtensionTrack[i]);
    }

    if (_State.x68_flg == 0)
    {
        RhythmMain(&_RhythmTrack);

        if (_State.IsUsingP86)
            PCM86Main(&_ADPCMTrack);
        else
            ADPCMMain(&_ADPCMTrack);
    }

    if (_State.x68_flg != 0xFF)
    {
        for (i = 0; i < 8; ++i)
        {
            _DriverState.CurrentChannel = i;
            PPZ8Main(&_PPZ8Track[i]);
        }
    }

    if (_DriverState.loop_work == 0)
        return;

    for (i = 0; i < 6; ++i)
    {
        if (_FMTrack[i].loopcheck != 3)
            _FMTrack[i].loopcheck = 0;
    }

    for (i = 0; i < 3; ++i)
    {
        if (_SSGTrack[i].loopcheck != 3)
            _SSGTrack[i].loopcheck = 0;

        if (_ExtensionTrack[i].loopcheck != 3)
            _ExtensionTrack[i].loopcheck = 0;
    }

    if (_ADPCMTrack.loopcheck != 3)
        _ADPCMTrack.loopcheck = 0;

    if (_RhythmTrack.loopcheck != 3)
        _RhythmTrack.loopcheck = 0;

    if (_EffectTrack.loopcheck != 3)
        _EffectTrack.loopcheck = 0;

    for (i = 0; i < MaxPPZChannels; ++i)
    {
        if (_PPZ8Track[i].loopcheck != 3)
            _PPZ8Track[i].loopcheck = 0;
    }

    if (_DriverState.loop_work != 3)
    {
        _State.LoopCount++;

        if (_State.LoopCount == 0xFF)
            _State.LoopCount = 1;
    }
    else
        _State.LoopCount = -1;
}

void PMD::FMMain(Track * track)
{
    if (track->Data == nullptr)
        return;

    uint8_t * si = track->Data;

    track->Length--;

    if (track->PartMask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK & Keyoff
        if ((track->keyoff_flag & 3) == 0)
        {
            // Already KeyOff?
            if (track->Length <= track->qdat)
            {
                KeyOff(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->Length == 0)
    {
        if (track->PartMask == 0)
            track->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = ExecuteFMCommand(track, si);
            }
            else
            if (*si == 0x80)
            {
                track->Data = si;
                track->loopcheck = 3;
                track->onkai = 0xFF;

                if (track->LoopData == nullptr)
                {
                    if (track->PartMask)
                    {
                        _DriverState.tieflag = 0;
                        _DriverState.volpush_flag = 0;
                        _DriverState.loop_work &= track->loopcheck;

                        return;
                    }
                    else
                        break;
                }

                si = track->LoopData; // When there was an "L"
                track->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {
                    si = porta(track, ++si); // Portamento

                    _DriverState.loop_work &= track->loopcheck;

                    return;
                }
                else
                if (track->PartMask == 0)
                {
                    // TONE SET
                    fnumset(track, oshift(track, lfoinit(track, *si++)));

                    track->Length = *si++;

                    si = calc_q(track, si);

                    if (track->volpush && (track->onkai != 0xFF))
                    {
                        if (--_DriverState.volpush_flag)
                        {
                            _DriverState.volpush_flag = 0;
                            track->volpush = 0;
                        }
                    }

                    volset(track);
                    Otodasi(track);
                    KeyOn(track);

                    track->keyon_flag++;
                    track->Data = si;

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;

                    if (*si == 0xfb)
                        track->keyoff_flag = 2; // Do not key off if '&' immediately follows
                    else
                        track->keyoff_flag = 0;

                    _DriverState.loop_work &= track->loopcheck;

                    return;
                }
                else
                {
                    si++;

                    track->fnum = 0; // Set to rest
                    track->onkai = 0xFF;
                    track->onkai_def = 0xFF;
                    track->Length = *si++;
                    track->keyon_flag++;
                    track->Data = si;

                    if (--_DriverState.volpush_flag)
                        track->volpush = 0;

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;
                    break;
                }
            }
        }
    }

    if (track->PartMask == 0)
    {
        // Finish with LFO & Portament & Fadeout processing
        if (track->hldelay_c)
        {
            if (--track->hldelay_c == 0)
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + (_DriverState.CurrentChannel - 1 + 0xb4)), (uint32_t) track->fmpan);
        }

        if (track->sdelay_c)
        {
            if (--track->sdelay_c == 0)
            {
                if ((track->keyoff_flag & 1) == 0)
                    KeyOn(track); // Already keyoffed?
            }
        }

        if (track->lfoswi)
        {
            _DriverState.lfo_switch = track->lfoswi & 8;

            if (track->lfoswi & 3)
            {
                if (lfo(track))
                    _DriverState.lfo_switch |= (track->lfoswi & 3);
            }

            if (track->lfoswi & 0x30)
            {
                SwapLFO(track);

                if (lfo(track))
                {
                    SwapLFO(track);

                    _DriverState.lfo_switch |= (track->lfoswi & 0x30);
                }
                else
                    SwapLFO(track);
            }

            if (_DriverState.lfo_switch & 0x19)
            {
                if (_DriverState.lfo_switch & 8)
                    porta_calc(track);

                Otodasi(track);
            }

            if (_DriverState.lfo_switch & 0x22)
            {
                volset(track);
                _DriverState.loop_work &= track->loopcheck;

                return;
            }
        }

        if (_State.FadeOutSpeed != 0)
            volset(track);
    }

    _DriverState.loop_work &= track->loopcheck;
}

void PMD::KeyOff(Track * track)
{
    if (track->onkai == 0xFF)
        return;

    KeyOffEx(track);
}

void PMD::KeyOffEx(Track * track)
{
    if (_DriverState.fmsel == 0)
    {
        _DriverState.omote_key[_DriverState.CurrentChannel - 1] = (~track->SlotMask) & (_DriverState.omote_key[_DriverState.CurrentChannel - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) ((_DriverState.CurrentChannel - 1) | _DriverState.omote_key[_DriverState.CurrentChannel - 1]));
    }
    else
    {
        _DriverState.ura_key[_DriverState.CurrentChannel - 1] = (~track->SlotMask) & (_DriverState.ura_key[_DriverState.CurrentChannel - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) (((_DriverState.CurrentChannel - 1) | _DriverState.ura_key[_DriverState.CurrentChannel - 1]) | 4));
    }
}

void PMD::KeyOn(Track * track)
{
    int  al;

    if (track->onkai == 0xFF)
        return; // ｷｭｳﾌ ﾉ ﾄｷ

    if (_DriverState.fmsel == 0)
    {
        al = _DriverState.omote_key[_DriverState.CurrentChannel - 1] | track->SlotMask;

        if (track->sdelay_c)
            al &= track->sdelay_m;

        _DriverState.omote_key[_DriverState.CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) ((_DriverState.CurrentChannel - 1) | al));
    }
    else
    {
        al = _DriverState.ura_key[_DriverState.CurrentChannel - 1] | track->SlotMask;

        if (track->sdelay_c)
            al &= track->sdelay_m;

        _DriverState.ura_key[_DriverState.CurrentChannel - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) (((_DriverState.CurrentChannel - 1) | al) | 4));
    }
}

//  Set [ FNUM/BLOCK + DETUNE + LFO ]
void PMD::Otodasi(Track * track)
{
    if ((track->fnum == 0) || (track->SlotMask == 0))
        return;

    int cx = (int) (track->fnum & 0x3800);    // cx=BLOCK
    int ax = (int) (track->fnum) & 0x7ff;    // ax=FNUM

    // Portament/LFO/Detune SET
    ax += track->porta_num + track->detune;

    if ((_DriverState.CurrentChannel == 3) && (_DriverState.fmsel == 0) && (_State.ch3mode != 0x3f))
        ch3_special(track, ax, cx);
    else
    {
        if (track->lfoswi & 1)
            ax += track->lfodat;

        if (track->lfoswi & 0x10)
            ax += track->_lfodat;

        fm_block_calc(&cx, &ax);

        // SET BLOCK/FNUM TO OPN

        ax |= cx;

        _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + _DriverState.CurrentChannel + 0xa4 - 1), (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + _DriverState.CurrentChannel + 0xa4 - 5), (uint32_t) LOBYTE(ax));
    }
}

//  FM音源のdetuneでオクターブが変わる時の修正
//    input  CX:block / AX:fnum+detune
//    output  CX:block / AX:fnum
void PMD::fm_block_calc(int * cx, int * ax)
{
    while (*ax >= 0x26a)
    {
        if (*ax < (0x26a * 2)) return;

        *cx += 0x800;      // oct.up
        if (*cx != 0x4000)
        {
            *ax -= 0x26a;    // 4d2h-26ah
        }
        else
        {        // ﾓｳ ｺﾚｲｼﾞｮｳ ｱｶﾞﾝﾅｲﾖﾝ
            *cx = 0x3800;
            if (*ax >= 0x800)
                *ax = 0x7ff;  // 4d2h
            return;
        }
    }

    while (*ax < 0x26a)
    {
        *cx -= 0x800;      // oct.down
        if (*cx >= 0)
        {
            *ax += 0x26a;    // 4d2h-26ah
        }
        else
        {        // ﾓｳ ｺﾚｲｼﾞｮｳ ｻｶﾞﾝﾅｲﾖﾝ
            *cx = 0;
            if (*ax < 8)
            {
                *ax = 8;
            }
            return;
        }
    }
}

// Determine the part and set mode if ch3
int PMD::ch3_setting(Track * qq)
{
    if (_DriverState.CurrentChannel == 3 && _DriverState.fmsel == 0)
    {
        ch3mode_set(qq);

        return 1;
    }

    return 0;
}

void PMD::cm_clear(int * ah, int * al)
{
    *al ^= 0xff;

    if ((_DriverState.slot3_flag &= *al) == 0)
    {
        if (_DriverState.slotdetune_flag != 1)
        {
            *ah = 0x3f;
        }
        else
        {
            *ah = 0x7f;
        }
    }
    else
    {
        *ah = 0x7f;
    }
}

//  Setting FM3 mode
void PMD::ch3mode_set(Track * track)
{
    int al;

    if (track == &_FMTrack[3 - 1])
        al = 1;
    else
    if (track == &_ExtensionTrack[0])
        al = 2;
    else
    if (track == &_ExtensionTrack[1])
        al = 4;
    else
        al = 8;

    int ah;

    if ((track->SlotMask & 0xF0) == 0)
    {
        cm_clear(&ah, &al); // s0
    }
    else
    if (track->SlotMask != 0xF0)
    {
        _DriverState.slot3_flag |= al;
        ah = 0x7f;
    }
    else

    if ((track->VolumeMask1 & 0x0F) == 0)
    {
        cm_clear(&ah, &al);
    }
    else
    if ((track->lfoswi & 1) != 0)
    {
        _DriverState.slot3_flag |= al;
        ah = 0x7f;
    }
    else

    if ((track->VolumeMask2 & 0x0F) == 0)
    {
        cm_clear(&ah, &al);
    }
    else
    if (track->lfoswi & 0x10)
    {
        _DriverState.slot3_flag |= al;
        ah = 0x7f;
    }
    else
    {
        cm_clear(&ah, &al);
    }

    if ((uint32_t) ah == _State.ch3mode)
        return;

    _State.ch3mode = (uint32_t) ah;

    _OPNAW->SetReg(0x27, (uint32_t) (ah & 0xCF)); // Don't reset.

    // When moving to sound effect mode, the pitch is rewritten with the previous FM3 part
    if (ah == 0x3F || track == &_FMTrack[2])
        return;

    if (_FMTrack[2].PartMask == 0)
        Otodasi(&_FMTrack[2]);

    if (track == &_ExtensionTrack[0])
        return;

    if (_ExtensionTrack[0].PartMask == 0)
        Otodasi(&_ExtensionTrack[0]);

    if (track == &_ExtensionTrack[1])
        return;

    if (_ExtensionTrack[1].PartMask == 0)
        Otodasi(&_ExtensionTrack[1]);
}

//  Pitch setting when using ch3=sound effect mode (input CX:block AX:fnum)
void PMD::ch3_special(Track * track, int ax, int cx)
{
    int shiftmask = 0x80;

    int si = cx;

    int bh;

    if ((track->VolumeMask1 & 0x0f) == 0)
        bh = 0xf0;      // all
    else
        bh = track->VolumeMask1;  // bh=lfo1 mask 4321xxxx

    int ch;

    if ((track->VolumeMask2 & 0x0f) == 0)
        ch = 0xf0;      // all
    else
        ch = track->VolumeMask2;  // ch=lfo2 mask 4321xxxx

    //  slot  4
    int ax_;

    if (track->SlotMask & 0x80)
    {
        ax_ = ax;
        ax += _State.slot_detune4;

        if ((bh & shiftmask) && (track->lfoswi & 0x01))  ax += track->lfodat;
        if ((ch & shiftmask) && (track->lfoswi & 0x10))  ax += track->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xa6, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa2, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  3
    if (track->SlotMask & 0x40)
    {
        ax_ = ax;
        ax += _State.slot_detune3;

        if ((bh & shiftmask) && (track->lfoswi & 0x01))  ax += track->lfodat;
        if ((ch & shiftmask) && (track->lfoswi & 0x10))  ax += track->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xac, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa8, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  2
    if (track->SlotMask & 0x20)
    {
        ax_ = ax;
        ax += _State.slot_detune2;

        if ((bh & shiftmask) && (track->lfoswi & 0x01))
            ax += track->lfodat;

        if ((ch & shiftmask) && (track->lfoswi & 0x10))
            ax += track->_lfodat;

        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xae, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xaa, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  1
    if (track->SlotMask & 0x10)
    {
        ax_ = ax;
        ax += _State.slot_detune1;

        if ((bh & shiftmask) && (track->lfoswi & 0x01)) 
            ax += track->lfodat;

        if ((ch & shiftmask) && (track->lfoswi & 0x10))
            ax += track->_lfodat;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xad, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa9, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }
}

//  'p' COMMAND [FM PANNING SET]
uint8_t * PMD::panset(Track * qq, uint8_t * si)
{
    panset_main(qq, *si++);
    return si;
}

void PMD::panset_main(Track * qq, int al)
{
    qq->fmpan = (qq->fmpan & 0x3f) | ((al << 6) & 0xc0);

    if (_DriverState.CurrentChannel == 3 && _DriverState.fmsel == 0)
    {
        //  FM3の場合は 4つのパート総て設定
        _FMTrack[2].fmpan = qq->fmpan;
        _ExtensionTrack[0].fmpan = qq->fmpan;
        _ExtensionTrack[1].fmpan = qq->fmpan;
        _ExtensionTrack[2].fmpan = qq->fmpan;
    }

    if (qq->PartMask == 0)
    {    // パートマスクされているか？
// dl = al;
        _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + _DriverState.CurrentChannel + 0xb4 - 1), calc_panout(qq));
    }
}

//  0b4h?に設定するデータを取得 out.dl
uint8_t PMD::calc_panout(Track * qq)
{
    int  dl;

    dl = qq->fmpan;

    if (qq->hldelay_c)
        dl &= 0xc0;  // HLFO Delayが残ってる場合はパンのみ設定

    return (uint8_t) dl;
}

//  Pan setting Extend
uint8_t * PMD::panset_ex(Track * qq, uint8_t * si)
{
    int    al;

    al = *(int8_t *) si++;
    si++;    // 逆走flagは読み飛ばす

    if (al > 0)
    {
        al = 2;
        panset_main(qq, al);
    }
    else if (al == 0)
    {
        al = 3;
        panset_main(qq, al);
    }
    else
    {
        al = 1;
        panset_main(qq, al);
    }
    return si;
}

//  Pan setting Extend
uint8_t * PMD::panset8_ex(Track * qq, uint8_t * si)
{
    int    flag, data;

    qq->fmpan = (int8_t) *si++;
    _State.revpan = *si++;


    if (qq->fmpan == 0)
    {
        flag = 3;        // Center
        data = 0;
    }
    else if (qq->fmpan > 0)
    {
        flag = 2;        // Right
        data = 128 - qq->fmpan;
    }
    else
    {
        flag = 1;        // Left
        data = 128 + qq->fmpan;
    }

    if (_State.revpan != 1)
    {
        flag |= 4;        // 逆相
    }

    _P86->SetPan(flag, data);

    return si;
}

//  ＦＭ音源用　Entry
int PMD::lfoinit(Track * qq, int al)
{
    int    ah;

    ah = al & 0x0f;
    if (ah == 0x0c)
    {
        al = qq->onkai_def;
        ah = al & 0x0f;
    }

    qq->onkai_def = al;

    if (ah == 0x0f)
    {        // ｷｭｰﾌ ﾉ ﾄｷ ﾊ INIT ｼﾅｲﾖ
        lfo_exit(qq);
        return al;
    }

    qq->porta_num = 0;        // ポルタメントは初期化

    if ((_DriverState.tieflag & 1) == 0)
    {
        lfin1(qq);
    }
    else
    {
        lfo_exit(qq);
    }
    return al;
}

//  ＦＭ　BLOCK,F-NUMBER SET
//    INPUTS  -- AL [KEY#,0-7F]
void PMD::fnumset(Track * qq, int al)
{
    int    ax, bx;

    if ((al & 0x0f) != 0x0f)
    {    // 音符の場合
        qq->onkai = al;

        // BLOCK/FNUM CALICULATE
        bx = al & 0x0f;    // bx=onkai
        ax = fnum_data[bx];

        // BLOCK SET
        ax |= (((al >> 1) & 0x38) << 8);
        qq->fnum = (uint32_t) ax;
    }
    else
    {            // 休符の場合
        qq->onkai = 0xFF;

        if ((qq->lfoswi & 0x11) == 0)
        {
            qq->fnum = 0;      // 音程LFO未使用
        }
    }
}

//  FM volume setting main
void PMD::volset(Track * track)
{
    if (track->SlotMask == 0)
        return;

    int cl = (track->volpush) ? track->volpush - 1 : track->volume;

    if (track != &_EffectTrack)
    {  // 効果音の場合はvoldown/fadeout影響無し
//--------------------------------------------------------------------
//  音量down計算
//--------------------------------------------------------------------
        if (_State.fm_voldown)
            cl = ((256 - _State.fm_voldown) * cl) >> 8;

        //--------------------------------------------------------------------
        //  Fadeout計算
        //--------------------------------------------------------------------
        if (_State.FadeOutVolume >= 2)
            cl = ((256 - (_State.FadeOutVolume >> 1)) * cl) >> 8;
    }

    //  音量をcarrierに設定 & 音量LFO処理
    //    input  cl to Volume[0-127]
    //      bl to SlotMask
    int bh = 0;          // Vol Slot Mask
    int bl = track->SlotMask;    // ch=SlotMask Push

    uint8_t vol_tbl[4] = { 0x80, 0x80, 0x80, 0x80 };

    cl = 255 - cl;      // cl=carrierに設定する音量+80H(add)
    bl &= track->carrier;    // bl=音量を設定するSLOT xxxx0000b
    bh |= bl;

    if (bl & 0x80) vol_tbl[0] = (uint8_t) cl;
    if (bl & 0x40) vol_tbl[1] = (uint8_t) cl;
    if (bl & 0x20) vol_tbl[2] = (uint8_t) cl;
    if (bl & 0x10) vol_tbl[3] = (uint8_t) cl;

    if (cl != 255)
    {
        if (track->lfoswi & 2)
        {
            bl = track->VolumeMask1;
            bl &= track->SlotMask;    // bl=音量LFOを設定するSLOT xxxx0000b
            bh |= bl;

            fmlfo_sub(track, track->lfodat, bl, vol_tbl);
        }

        if (track->lfoswi & 0x20)
        {
            bl = track->VolumeMask2;
            bl &= track->SlotMask;
            bh |= bl;

            fmlfo_sub(track, track->_lfodat, bl, vol_tbl);
        }
    }

    int dh = 0x4c - 1 + _DriverState.CurrentChannel;    // dh=FM Port Address

    if (bh & 0x80) volset_slot(dh,      track->slot4, vol_tbl[0]);
    if (bh & 0x40) volset_slot(dh -  8, track->slot3, vol_tbl[1]);
    if (bh & 0x20) volset_slot(dh -  4, track->slot2, vol_tbl[2]);
    if (bh & 0x10) volset_slot(dh - 12, track->slot1, vol_tbl[3]);
}

//  スロット毎の計算 & 出力 マクロ
//      in.  dl  元のTL値
//        dh  Outするレジスタ
//        al  音量変動値 中心=80h
void PMD::volset_slot(int dh, int dl, int al)
{
    if ((al += dl) > 255)
        al = 255;
    
    if ((al -= 0x80) < 0)
        al = 0;

    _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) al);
}

//  Sub for volume LFO
void PMD::fmlfo_sub(Track *, int al, int bl, uint8_t * vol_tbl)
{
    if (bl & 0x80) vol_tbl[0] = (uint8_t) Limit(vol_tbl[0] - al, 255, 0);
    if (bl & 0x40) vol_tbl[1] = (uint8_t) Limit(vol_tbl[1] - al, 255, 0);
    if (bl & 0x20) vol_tbl[2] = (uint8_t) Limit(vol_tbl[2] - al, 255, 0);
    if (bl & 0x10) vol_tbl[3] = (uint8_t) Limit(vol_tbl[3] - al, 255, 0);
}

void PMD::SSGMain(Track * track)
{
    if (track->Data == nullptr)
        return;

    uint8_t * si = track->Data;
    int    temp;

    track->Length--;

    // KEYOFF CHECK & Keyoff
    if ((track == &_SSGTrack[2]) && _DriverState.UsePPS && _State.kshot_dat && (track->Length <= track->qdat))
    {
        // PPS 使用時 & SSG 3ch & SSG 効果音鳴らしている場合
        keyoffp(track);
        _OPNAW->SetReg((uint32_t) (_DriverState.CurrentChannel + 8 - 1), 0U);    // 強制的に音を止める
        track->keyoff_flag = -1;
    }

    if (track->PartMask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        if ((track->keyoff_flag & 3) == 0)
        {    // 既にKeyOffしたか？
            if (track->Length <= track->qdat)
            {
                keyoffp(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->Length == 0)
    {
        track->lfoswi &= 0xf7;

        // DATA READ
        while (1)
        {
            if ((*si == 0xda) && ssgdrum_check(track, *si))
            {
                si++;
            }
            else
            if (*si > 0x80 && *si != 0xda)
            {
                si = ExecuteSSGCommand(track, si);
            }
            else
            if (*si == 0x80)
            {
                track->Data = si;
                track->loopcheck = 3;
                track->onkai = 255;

                if (track->LoopData == nullptr)
                {
                    if (track->PartMask)
                    {
                        _DriverState.tieflag = 0;
                        _DriverState.volpush_flag = 0;
                        _DriverState.loop_work &= track->loopcheck;
                        return;
                    }
                    else
                    {
                        break;
                    }
                }
                // "L"があった時
                si = track->LoopData;
                track->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {            // ポルタメント
                    si = portap(track, ++si);
                    _DriverState.loop_work &= track->loopcheck;
                    return;
                }
                else
                if (track->PartMask)
                {
                    if (!ssgdrum_check(track, *si))
                    {
                        si++;
                        track->fnum = 0;    //休符に設定
                        track->onkai = 255;
                        track->Length = *si++;
                        track->keyon_flag++;
                        track->Data = si;

                        if (--_DriverState.volpush_flag)
                        {
                            track->volpush = 0;
                        }

                        _DriverState.tieflag = 0;
                        _DriverState.volpush_flag = 0;
                        break;
                    }
                }

                //  TONE SET
                SetSSGTune(track, oshiftp(track, lfoinitp(track, *si++)));

                track->Length = *si++;
                si = calc_q(track, si);

                if (track->volpush && track->onkai != 255)
                {
                    if (--_DriverState.volpush_flag)
                    {
                        _DriverState.volpush_flag = 0;
                        track->volpush = 0;
                    }
                }

                volsetp(track);
                OtodasiP(track);
                keyonp(track);

                track->keyon_flag++;
                track->Data = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;
                track->keyoff_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらKeyOffしない
                    track->keyoff_flag = 2;
                }

                _DriverState.loop_work &= track->loopcheck;

                return;
            }
        }
    }

    _DriverState.lfo_switch = (track->lfoswi & 8);

    if (track->lfoswi)
    {
        if (track->lfoswi & 3)
        {
            if (lfop(track))
            {
                _DriverState.lfo_switch |= (track->lfoswi & 3);
            }
        }

        if (track->lfoswi & 0x30)
        {
            SwapLFO(track);
            if (lfop(track))
            {
                SwapLFO(track);
                _DriverState.lfo_switch |= (track->lfoswi & 0x30);
            }
            else
            {
                SwapLFO(track);
            }
        }

        if (_DriverState.lfo_switch & 0x19)
        {
            if (_DriverState.lfo_switch & 0x08)
                porta_calc(track);

            // SSG 3ch で休符かつ SSG Drum 発音中は操作しない
            if (!(track == &_SSGTrack[2] && track->onkai == 255 && _State.kshot_dat && !_DriverState.UsePPS))
                OtodasiP(track);
        }
    }

    temp = soft_env(track);

    if (temp || _DriverState.lfo_switch & 0x22 || (_State.FadeOutSpeed != 0))
    {
        // SSG 3ch で休符かつ SSG Drum 発音中は volume set しない
        if (!(track == &_SSGTrack[2] && track->onkai == 255 && _State.kshot_dat && !_DriverState.UsePPS))
        {
            volsetp(track);
        }
    }

    _DriverState.loop_work &= track->loopcheck;
}

void PMD::keyoffp(Track * qq)
{
    if (qq->onkai == 255) return;    // ｷｭｳﾌ ﾉ ﾄｷ
    if (qq->envf != -1)
    {
        qq->envf = 2;
    }
    else
    {
        qq->eenv_count = 4;
    }
}

/// <summary>
/// Main rythm processing
/// </summary>
void PMD::RhythmMain(Track * track)
{
    if (track->Data == nullptr)
        return;

    uint8_t * si = track->Data;

    if (--track->Length == 0)
    {
        uint8_t * bx = _State.RhythmData;

        int al;
        int result = 0;

    rhyms00:
        do
        {
            result = 1;

            al = *bx++;

            if (al != 0xff)
            {
                if (al & 0x80)
                {
                    bx = RhythmOn(track, bx, al, &result);

                    if (result == 0)
                        continue;
                }
                else
                    _State.kshot_dat = 0;  //rest

                al = *bx++;

                _State.RhythmData = bx;

                track->Length = al;
                track->keyon_flag++;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;
                _DriverState.loop_work &= track->loopcheck;

                return;
            }
        }
        while (result == 0);

        while (1)
        {
            while ((al = *si++) != 0x80)
            {
                if (al < 0x80)
                {
                    track->Data = si;

                    bx = _State.RhythmData = &_State.MData[_State.RhythmDataTable[al]];
                    goto rhyms00;
                }

                // al > 0x80
                si = ExecuteRhythmCommand(track, si - 1);
            }

            track->Data = --si;
            track->loopcheck = 3;

            bx = track->LoopData;

            if (bx == nullptr)
            {
                _State.RhythmData = (uint8_t *) &_DriverState.DummyRhythmData;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;
                _DriverState.loop_work &= track->loopcheck;

                return;
            }
            else
            {
                // If there's an "L"
                si = bx;

                track->loopcheck = 1;
            }
        }
    }

    _DriverState.loop_work &= track->loopcheck;
}

uint8_t * PMD::RhythmOn(Track * track, uint8_t * bx, int al, int * result)
{
    if (al & 0x40)
    {
        bx = ExecuteRhythmCommand(track, bx - 1);
        *result = 0;

        return bx;
    }

    *result = 1;

    if (track->PartMask)
    {
        _State.kshot_dat = 0;

        return ++bx;
    }

    al = ((al << 8) + *bx++) & 0x3fff;

    _State.kshot_dat = al;

    if (al == 0)
        return bx;

    _State.RhythmData = bx;

    if (_State.UseRhythm)
    {
        for (int cl = 0; cl < 11; cl++)
        {
            if (al & (1 << cl))
            {
                _OPNAW->SetReg((uint32_t) rhydat[cl][0], (uint32_t) rhydat[cl][1]);

                int dl = rhydat[cl][2] & _State.RhythmMask;

                if (dl)
                {
                    if (dl < 0x80)
                        _OPNAW->SetReg(0x10, (uint32_t) dl);
                    else
                    {
                        _OPNAW->SetReg(0x10, 0x84);

                        dl = _State.RhythmMask & 0x08;

                        if (dl)
                            _OPNAW->SetReg(0x10, (uint32_t) dl);
                    }
                }
            }
        }
    }

    if (_State.FadeOutVolume)
    {
        if (_State.UseRhythm)
        {
            int dl = _State.RhythmVolume;

            if (_State.FadeOutVolume)
                dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

            _OPNAW->SetReg(0x11, (uint32_t) dl);
        }

        if (!_DriverState.UsePPS)
            return _State.RhythmData; // No sound during fadeout when using PPS.
    }

    int bx_ = al;

    al = 0;

    do
    {
        while ((bx_ & 1) == 0)
        {
            bx_ >>= 1;
            al++;
        }

        effgo(track, al);

        bx_ >>= 1;
    }
    while (_DriverState.UsePPS && bx_);  // If you use PPSDRV, try playing the second or more notes.

    return _State.RhythmData;
}

//  SSG Drums & Sound Effects Routine (From WT298)
//  AL to sound effect No. Enter and CALL
//  If you have ppsdrv, run it
void PMD::effgo(Track * track, int al)
{
    if (_DriverState.UsePPS)
    {
        al |= 0x80;

        if (_EffectState.last_shot_data == al)
            _PPS->Stop();
        else
            _EffectState.last_shot_data = al;
    }

    _EffectState.hosei_flag = 3; // With pitch/volume correction (K part)

    eff_main(track, al);
}

void PMD::eff_on2(Track * qq, int al)
{
    _EffectState.hosei_flag = 1;        //  音程のみ補正あり (n command)
    eff_main(qq, al);
}

void PMD::eff_main(Track * qq, int al)
{
    int    ah, bh, bl;

    if (_State.SSGEffectFlag)
        return;    //  効果音を使用しないモード

    if (_DriverState.UsePPS && (al & 0x80))
    {  // PPS を鳴らす
        if (_EffectState.effon >= 2)
            return;  // 通常効果音発音時は発声させない

        _SSGTrack[2].PartMask |= 2;

        _EffectState.effon = 1;        // 優先度１(ppsdrv)
        _EffectState.psgefcnum = al;      // 音色番号設定 (80H?)

        bh = 0;
        bl = 15;
        ah = _EffectState.hosei_flag;

        if (ah & 1)
            bh = qq->detune % 256;    // BH = Detuneの下位 8bit

        if (ah & 2)
        {
            if (qq->volume < 15)
                bl = qq->volume;    // BL = volume値 (0?15)

            if (_State.FadeOutVolume)
                bl = (bl * (256 - _State.FadeOutVolume)) >> 8;
        }

        if (bl)
        {
            bl ^= 0x0f;
            ah = 1;
            al &= 0x7f;

            _PPS->Play(al, bh, bl);
        }
    }
    else
    {
        _EffectState.psgefcnum = al;

        if (_EffectState.effon <= efftbl[al].priority)
        {
            if (_DriverState.UsePPS)
                _PPS->Stop();

            _SSGTrack[2].PartMask |= 2;    // Part Mask

            efffor(efftbl[al].table);    // First effect

            _EffectState.effon = efftbl[al].priority; // Set priority
        }
    }
}

//  こーかおん　えんそう　めいん
//   Ｆｒｏｍ　ＶＲＴＣ
void PMD::effplay()
{
    if (--_EffectState.effcnt)
        effsweep();
    else
        efffor(_EffectState.effadr);
}

void PMD::efffor(const int * si)
{
    int al = *si++;

    if (al == -1)
    {
        effend();
    }
    else
    {
        _EffectState.effcnt = al; // Effect count

        int cl = *si;

        _OPNAW->SetReg(4, (uint32_t) (*si++)); // Set frequency

        int ch = *si;

        _OPNAW->SetReg(5, (uint32_t) (*si++)); // Set frequency

        _EffectState.eswthz = (ch << 8) + cl;

        _State.SSGNoiseFrequencyLast = _EffectState.eswnhz = *si;

        _OPNAW->SetReg(6, (uint32_t) *si++); // ノイズ

        _OPNAW->SetReg(7, ((*si++ << 2) & 0x24) | (_OPNAW->GetReg(0x07) & 0xdb));

        _OPNAW->SetReg(10, (uint32_t) *si++); // ボリューム
        _OPNAW->SetReg(11, (uint32_t) *si++); // エンベロープ周波数
        _OPNAW->SetReg(12, (uint32_t) *si++);
        _OPNAW->SetReg(13, (uint32_t) *si++); // エンベロープPATTERN

        _EffectState.eswtst = *si++; // スイープ増分 (TONE)
        _EffectState.eswnst = *si++; // スイープ増分 (NOISE)

        _EffectState.eswnct = _EffectState.eswnst & 15;    // スイープカウント (NOISE)

        _EffectState.effadr = (int *) si;
    }
}

void PMD::effend()
{
    if (_DriverState.UsePPS)
        _PPS->Stop();

    _OPNAW->SetReg(0x0a, 0x00);
    _OPNAW->SetReg(0x07, ((_OPNAW->GetReg(0x07)) & 0xdb) | 0x24);

    _EffectState.effon = 0;
    _EffectState.psgefcnum = -1;
}

// 普段の処理
void PMD::effsweep()
{
    int    dl;

    _EffectState.eswthz += _EffectState.eswtst;
    _OPNAW->SetReg(4, (uint32_t) LOBYTE(_EffectState.eswthz));
    _OPNAW->SetReg(5, (uint32_t) HIBYTE(_EffectState.eswthz));

    if (_EffectState.eswnst == 0) return;    // ノイズスイープ無し
    if (--_EffectState.eswnct) return;

    dl = _EffectState.eswnst;
    _EffectState.eswnct = dl & 15;

    // used to be "dl / 16"
    // with negative value division is different from shifting right
    // division: usually truncated towards zero (mandatory since c99)
    //   same as x86 idiv
    // shift: usually arithmetic shift
    //   same as x86 sar

    _EffectState.eswnhz += dl >> 4;

    _OPNAW->SetReg(6, (uint32_t) _EffectState.eswnhz);
    _State.SSGNoiseFrequencyLast = _EffectState.eswnhz;
}

//  PDRのswitch
uint8_t * PMD::pdrswitch(Track *, uint8_t * si)
{
    if (!_DriverState.UsePPS)
        return si + 1;

//  ppsdrv->SetParam((*si & 1) << 1, *si & 1);    @暫定
    si++;

    return si;
}

/// <summary>
/// Main ADPCM sound source processing
/// </summary>
void PMD::ADPCMMain(Track * track)
{
    if (track->Data == nullptr)
        return;

    uint8_t * si = track->Data;

    track->Length--;

    if (track->PartMask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((track->keyoff_flag & 3) == 0)
        {    // 既にKeyOffしたか？
            if (track->Length <= track->qdat)
            {
                keyoffm(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->Length == 0)
    {
        track->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = ExecuteADPCMCommand(track, si);
            }
            else
            if (*si == 0x80)
            {
                track->Data = si;
                track->loopcheck = 3;
                track->onkai = 255;

                if (track->LoopData == NULL)
                {
                    if (track->PartMask)
                    {
                        _DriverState.tieflag = 0;
                        _DriverState.volpush_flag = 0;
                        _DriverState.loop_work &= track->loopcheck;

                        return;
                    }
                    else
                        break;
                }

                // "L"があった時
                si = track->LoopData;
                track->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {        // ポルタメント
                    si = portam(track, ++si);

                    _DriverState.loop_work &= track->loopcheck;

                    return;
                }
                else
                if (track->PartMask)
                {
                    si++;
                    track->fnum = 0;    //休符に設定
                    track->onkai = 255;
                    //          qq->onkai_def = 255;
                    track->Length = *si++;
                    track->keyon_flag++;
                    track->Data = si;

                    if (--_DriverState.volpush_flag)
                        track->volpush = 0;

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumsetm(track, oshift(track, lfoinitp(track, *si++)));

                track->Length = *si++;
                si = calc_q(track, si);

                if (track->volpush && track->onkai != 255)
                {
                    if (--_DriverState.volpush_flag)
                    {
                        _DriverState.volpush_flag = 0;
                        track->volpush = 0;
                    }
                }

                volsetm(track);
                OtodasiM(track);

                if (track->keyoff_flag & 1)
                    keyonm(track);

                track->keyon_flag++;
                track->Data = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;

                if (*si == 0xfb)
                {   // Do not KeyOff if '&' immediately follows
                    track->keyoff_flag = 2;
                }
                else
                {
                    track->keyoff_flag = 0;
                }

                _DriverState.loop_work &= track->loopcheck;

                return;
            }
        }
    }

    _DriverState.lfo_switch = (track->lfoswi & 8);

    if (track->lfoswi)
    {
        if (track->lfoswi & 3)
        {
            if (lfo(track))
                _DriverState.lfo_switch |= (track->lfoswi & 3);
        }

        if (track->lfoswi & 0x30)
        {
            SwapLFO(track);

            if (lfop(track))
            {
                SwapLFO(track);
                _DriverState.lfo_switch |= (track->lfoswi & 0x30);
            }
            else
                SwapLFO(track);
        }

        if (_DriverState.lfo_switch & 0x19)
        {
            if (_DriverState.lfo_switch & 0x08)
                porta_calc(track);

            OtodasiM(track);
        }
    }

    int temp = soft_env(track);

    if ((temp != 0) || _DriverState.lfo_switch & 0x22 || (_State.FadeOutSpeed != 0))
        volsetm(track);

    _DriverState.loop_work &= track->loopcheck;
}

/// <summary>
/// Main PCM86 processing
/// </summary>
void PMD::PCM86Main(Track * track)
{
    if (track->Data == nullptr)
        return;

    uint8_t * si = track->Data;

    int    temp;

    track->Length--;

    if (track->PartMask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((track->keyoff_flag & 3) == 0)
        {    // 既にKeyOffしたか？
            if (track->Length <= track->qdat)
            {
                keyoff8(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->Length == 0)
    {
        while (1)
        {
            //      if(*si > 0x80 && *si != 0xda) {
            if (*si > 0x80)
            {
                si = ExecutePCM86Command(track, si);
            }
            else if (*si == 0x80)
            {
                track->Data = si;
                track->loopcheck = 3;
                track->onkai = 255;

                if (track->LoopData == nullptr)
                {
                    if (track->PartMask)
                    {
                        _DriverState.tieflag = 0;
                        _DriverState.volpush_flag = 0;
                        _DriverState.loop_work &= track->loopcheck;
                        return;
                    }
                    else
                    {
                        break;
                    }
                }
                // "L"があった時
                si = track->LoopData;
                track->loopcheck = 1;
            }
            else
            {
                if (track->PartMask)
                {
                    si++;
                    track->fnum = 0;    //休符に設定
                    track->onkai = 255;
                    //          qq->onkai_def = 255;
                    track->Length = *si++;
                    track->keyon_flag++;
                    track->Data = si;

                    if (--_DriverState.volpush_flag)
                    {
                        track->volpush = 0;
                    }

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumset8(track, oshift(track, lfoinitp(track, *si++)));

                track->Length = *si++;
                si = calc_q(track, si);

                if (track->volpush && track->onkai != 255)
                {
                    if (--_DriverState.volpush_flag)
                    {
                        _DriverState.volpush_flag = 0;
                        track->volpush = 0;
                    }
                }

                volset8(track);
                Otodasi8(track);
                if (track->keyoff_flag & 1)
                {
                    keyon8(track);
                }

                track->keyon_flag++;
                track->Data = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらKeyOffしない
                    track->keyoff_flag = 2;
                }
                else
                {
                    track->keyoff_flag = 0;
                }
                _DriverState.loop_work &= track->loopcheck;
                return;

            }
        }
    }

    if (track->lfoswi & 0x22)
    {
        _DriverState.lfo_switch = 0;
        if (track->lfoswi & 2)
        {
            lfo(track);
            _DriverState.lfo_switch |= (track->lfoswi & 2);
        }

        if (track->lfoswi & 0x20)
        {
            SwapLFO(track);
            if (lfo(track))
            {
                SwapLFO(track);
                _DriverState.lfo_switch |= (track->lfoswi & 0x20);
            }
            else
            {
                SwapLFO(track);
            }
        }

        temp = soft_env(track);
        if (temp || _DriverState.lfo_switch & 0x22 || _State.FadeOutSpeed)
        {
            volset8(track);
        }
    }
    else
    {
        temp = soft_env(track);
        if (temp || _State.FadeOutSpeed)
        {
            volset8(track);
        }
    }

    _DriverState.loop_work &= track->loopcheck;
}

/// <summary>
/// Main PPZ8 processing
/// </summary>
void PMD::PPZ8Main(Track * track)
{
    if (track->Data == nullptr)
        return;

    uint8_t * si = track->Data;

    int    temp;

    track->Length--;

    if (track->PartMask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((track->keyoff_flag & 3) == 0)
        {    // 既にKeyOffしたか？
            if (track->Length <= track->qdat)
            {
                keyoffz(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->Length == 0)
    {
        track->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = ExecutePPZ8Command(track, si);
            }
            else if (*si == 0x80)
            {
                track->Data = si;
                track->loopcheck = 3;
                track->onkai = 255;

                if (track->LoopData == nullptr)
                {
                    if (track->PartMask)
                    {
                        _DriverState.tieflag = 0;
                        _DriverState.volpush_flag = 0;
                        _DriverState.loop_work &= track->loopcheck;
                        return;
                    }
                    else
                    {
                        break;
                    }
                }
                // "L"があった時
                si = track->LoopData;
                track->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {        // ポルタメント
                    si = portaz(track, ++si);
                    _DriverState.loop_work &= track->loopcheck;
                    return;
                }
                else
                if (track->PartMask)
                {
                    si++;
                    track->fnum = 0;    //休符に設定
                    track->onkai = 255;
                    //          qq->onkai_def = 255;
                    track->Length = *si++;
                    track->keyon_flag++;
                    track->Data = si;

                    if (--_DriverState.volpush_flag)
                    {
                        track->volpush = 0;
                    }

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumsetz(track, oshift(track, lfoinitp(track, *si++)));

                track->Length = *si++;
                si = calc_q(track, si);

                if (track->volpush && track->onkai != 255)
                {
                    if (--_DriverState.volpush_flag)
                    {
                        _DriverState.volpush_flag = 0;
                        track->volpush = 0;
                    }
                }

                volsetz(track);
                OtodasiZ(track);
                if (track->keyoff_flag & 1)
                {
                    keyonz(track);
                }

                track->keyon_flag++;
                track->Data = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらKeyOffしない
                    track->keyoff_flag = 2;
                }
                else
                {
                    track->keyoff_flag = 0;
                }
                _DriverState.loop_work &= track->loopcheck;
                return;

            }
        }
    }

    _DriverState.lfo_switch = (track->lfoswi & 8);
    if (track->lfoswi)
    {
        if (track->lfoswi & 3)
        {
            if (lfo(track))
            {
                _DriverState.lfo_switch |= (track->lfoswi & 3);
            }
        }

        if (track->lfoswi & 0x30)
        {
            SwapLFO(track);
            if (lfop(track))
            {
                SwapLFO(track);
                _DriverState.lfo_switch |= (track->lfoswi & 0x30);
            }
            else
            {
                SwapLFO(track);
            }
        }

        if (_DriverState.lfo_switch & 0x19)
        {
            if (_DriverState.lfo_switch & 0x08)
            {
                porta_calc(track);
            }
            OtodasiZ(track);
        }
    }

    temp = soft_env(track);
    if (temp || _DriverState.lfo_switch & 0x22 || _State.FadeOutSpeed)
    {
        volsetz(track);
    }

    _DriverState.loop_work &= track->loopcheck;
}

//  PCM KEYON
void PMD::keyonm(Track * track)
{
    if (track->onkai == 255)
        return;

    _OPNAW->SetReg(0x101, 0x02);  // PAN=0 / x8 bit mode
    _OPNAW->SetReg(0x100, 0x21);  // PCM RESET

    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_State.PCMStart));
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_State.PCMStart));
    _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_State.PCMStop));
    _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_State.PCMStop));

    if ((_DriverState.PCMRepeat1 | _DriverState.PCMRepeat2) == 0)
    {
        _OPNAW->SetReg(0x100, 0xa0);  // PCM PLAY (non_repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (track->fmpan | 2));  // PAN SET / x8 bit mode
    }
    else
    {
        _OPNAW->SetReg(0x100, 0xb0);  // PCM PLAY (repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (track->fmpan | 2));  // PAN SET / x8 bit mode

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_DriverState.PCMRepeat1));
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_DriverState.PCMRepeat1));
        _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_DriverState.PCMRepeat2));
        _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_DriverState.PCMRepeat2));
    }
}

//  PCM KEYON (PMD86)
void PMD::keyon8(Track * qq)
{
    if (qq->onkai == 255)
        return;

    _P86->Play();
}

//  PPZ KEYON
void PMD::keyonz(Track * qq)
{
    if (qq->onkai == 255) return;

    if ((qq->SampleNumber & 0x80) == 0)
    {
        _PPZ8->Play(_DriverState.CurrentChannel, 0, qq->SampleNumber, 0, 0);
    }
    else
    {
        _PPZ8->Play(_DriverState.CurrentChannel, 1, qq->SampleNumber & 0x7f, 0, 0);
    }
}

//  PCM OTODASI
void PMD::OtodasiM(Track * track)
{
    if (track->fnum == 0)
        return;

    // Portament/LFO/Detune SET
    int bx = (int) (track->fnum + track->porta_num);
    int dx = (int) (((track->lfoswi & 0x11) && (track->lfoswi & 1)) ? dx = track->lfodat : 0);

    if (track->lfoswi & 0x10)
        dx += track->_lfodat;

    dx *= 4;  // PCM ﾊ LFO ｶﾞ ｶｶﾘﾆｸｲ ﾉﾃﾞ depth ｦ 4ﾊﾞｲ ｽﾙ

    dx += track->detune;

    if (dx >= 0)
    {
        bx += dx;

        if (bx > 0xffff)
            bx = 0xffff;
    }
    else
    {
        bx += dx;

        if (bx < 0)
            bx = 0;
    }

    // TONE SET
    _OPNAW->SetReg(0x109, (uint32_t) LOBYTE(bx));
    _OPNAW->SetReg(0x10a, (uint32_t) HIBYTE(bx));
}

//  PCM OTODASI (PMD86)
void PMD::Otodasi8(Track * track)
{
    if (track->fnum == 0)
        return;

    int bl = (int) ((track->fnum & 0x0e00000) >> (16 + 5));
    int cx = (int) ( track->fnum & 0x01fffff);

    if ((_State.pcm86_vol == 0) && track->detune)
        cx = Limit((cx >> 5) + track->detune, 65535, 1) << 5;

    _P86->SetPitch(bl, (uint32_t) cx);
}

//  PPZ OTODASI
void PMD::OtodasiZ(Track * track)
{
    uint32_t cx = track->fnum;

    if (cx == 0)
        return;

    cx += track->porta_num * 16;

    int ax = (track->lfoswi & 1) ? track->lfodat : 0;

    if (track->lfoswi & 0x10)
        ax += track->_lfodat;

    ax += track->detune;

    int64_t cx2 = cx + ((int64_t) cx) / 256 * ax;

    if (cx2 > 0xffffffff)
        cx = 0xffffffff;
    else
    if (cx2 < 0)
        cx = 0;
    else
        cx = (uint32_t) cx2;

    _PPZ8->SetPitch(_DriverState.CurrentChannel, cx);
}

//  PCM VOLUME SET
void PMD::volsetm(Track * qq)
{
    int al = qq->volpush ? qq->volpush : qq->volume;

    //------------------------------------------------------------------------
    //  音量down計算
    //------------------------------------------------------------------------
    al = ((256 - _State.pcm_voldown) * al) >> 8;

    //------------------------------------------------------------------------
    //  Fadeout計算
    //------------------------------------------------------------------------
    if (_State.FadeOutVolume)
        al = (((256 - _State.FadeOutVolume) * (256 - _State.FadeOutVolume) >> 8) * al) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        _OPNAW->SetReg(0x10b, 0);
        return;
    }

    if (qq->envf == -1)
    {
        //  拡張版 音量=al*(eenv_vol+1)/16
        if (qq->eenv_volume == 0)
        {
            _OPNAW->SetReg(0x10b, 0);
            return;
        }

        al = ((((al * (qq->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        if (qq->eenv_volume < 0)
        {
            int ah = -qq->eenv_volume * 16;

            if (al < ah)
            {
                _OPNAW->SetReg(0x10b, 0);
                return;
            }
            else
                al -= ah;
        }
        else
        {
            int ah = qq->eenv_volume * 16;

            if (al + ah > 255)
                al = 255;
            else
                al += ah;
        }
    }

    //--------------------------------------------------------------------
    //  音量LFO計算
    //--------------------------------------------------------------------

    if ((qq->lfoswi & 0x22) == 0)
    {
        _OPNAW->SetReg(0x10b, (uint32_t) al);
        return;
    }

    int dx = (qq->lfoswi & 2) ? qq->lfodat : 0;

    if (qq->lfoswi & 0x20)
        dx += qq->_lfodat;

    if (dx >= 0)
    {
        al += dx;

        if (al & 0xff00)
            _OPNAW->SetReg(0x10b, 255);
        else
            _OPNAW->SetReg(0x10b, (uint32_t) al);
    }
    else
    {
        al += dx;

        if (al < 0)
            _OPNAW->SetReg(0x10b, 0);
        else
            _OPNAW->SetReg(0x10b, (uint32_t) al);
    }
}

//  PCM VOLUME SET(PMD86)
void PMD::volset8(Track * qq)
{
    int al = qq->volpush ? qq->volpush : qq->volume;

    //------------------------------------------------------------------------
    //  音量down計算
    //------------------------------------------------------------------------
    al = ((256 - _State.pcm_voldown) * al) >> 8;

    //------------------------------------------------------------------------
    //  Fadeout計算
    //------------------------------------------------------------------------
    if (_State.FadeOutVolume != 0)
        al = ((256 - _State.FadeOutVolume) * al) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        _OPNAW->SetReg(0x10b, 0);
        return;
    }

    if (qq->envf == -1)
    {
        //  拡張版 音量=al*(eenv_vol+1)/16
        if (qq->eenv_volume == 0)
        {
            _OPNAW->SetReg(0x10b, 0);
            return;
        }

        al = ((((al * (qq->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        if (qq->eenv_volume < 0)
        {
            int ah = -qq->eenv_volume * 16;

            if (al < ah)
            {
                _OPNAW->SetReg(0x10b, 0);
                return;
            }
            else
                al -= ah;
        }
        else
        {
            int ah = qq->eenv_volume * 16;

            if (al + ah > 255)
                al = 255;
            else
                al += ah;
        }
    }

    //--------------------------------------------------------------------
    //  音量LFO計算
    //--------------------------------------------------------------------

    int dx = (qq->lfoswi & 2) ? qq->lfodat : 0;

    if (qq->lfoswi & 0x20)
        dx += qq->_lfodat;

    if (dx >= 0)
    {
        if ((al += dx) > 255)
            al = 255;
    }
    else
    {
        if ((al += dx) < 0)
            al = 0;
    }

    if (_State.pcm86_vol)
        al = (int) ::sqrt(al); //  SPBと同様の音量設定
    else
        al >>= 4;

    _P86->SetVol(al);
}

//  PPZ VOLUME SET
void PMD::volsetz(Track * qq)
{
    int al = qq->volpush ? qq->volpush : qq->volume;

    //------------------------------------------------------------------------
    //  音量down計算
    //------------------------------------------------------------------------
    al = ((256 - _State.ppz_voldown) * al) >> 8;

    //------------------------------------------------------------------------
    //  Fadeout計算
    //------------------------------------------------------------------------
    if (_State.FadeOutVolume != 0)
        al = ((256 - _State.FadeOutVolume) * al) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        _PPZ8->SetVolume(_DriverState.CurrentChannel, 0);
        _PPZ8->Stop(_DriverState.CurrentChannel);
        return;
    }

    if (qq->envf == -1)
    {
        //  拡張版 音量=al*(eenv_vol+1)/16
        if (qq->eenv_volume == 0)
        {
            //*@    ppz8->SetVol(pmdwork._CurrentPart, 0);
            _PPZ8->Stop(_DriverState.CurrentChannel);
            return;
        }

        al = ((((al * (qq->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        if (qq->eenv_volume < 0)
        {
            int ah = -qq->eenv_volume * 16;

            if (al < ah)
            {
                //*@      ppz8->SetVol(pmdwork._CurrentPart, 0);
                _PPZ8->Stop(_DriverState.CurrentChannel);
                return;
            }
            else
                al -= ah;
        }
        else
        {
            int ah = qq->eenv_volume * 16;

            if (al + ah > 255)
                al = 255;
            else
                al += ah;
        }
    }

    // Calculate the LFO volume.
    if ((qq->lfoswi & 0x22))
    {
        int dx = (qq->lfoswi & 2) ? qq->lfodat : 0;

        if (qq->lfoswi & 0x20)
            dx += qq->_lfodat;

        al += dx;

        if (dx >= 0)
        {
            if (al & 0xff00)
                al = 255;
        }
        else
        {
            if (al < 0)
                al = 0;
        }
    }

    if (al != 0)
        _PPZ8->SetVolume(_DriverState.CurrentChannel, al >> 4);
    else
        _PPZ8->Stop(_DriverState.CurrentChannel);
}

//  ADPCM FNUM SET
void PMD::fnumsetm(Track * qq, int al)
{
    if ((al & 0x0f) != 0x0f)
    {      // 音符の場合
        qq->onkai = al;

        int bx = al & 0x0f;          // bx=onkai
        int ch = (al >> 4) & 0x0f;    // cl = octarb
        int cl = ch;

        if (cl > 5)
            cl = 0;
        else
            cl = 5 - cl;        // cl=5-octarb

        int ax = pcm_tune_data[bx];

        if (ch >= 6)
        {          // o7以上?
            ch = 0x50;

            if (ax < 0x8000)
            {
                ax *= 2;        // o7以上で2倍できる場合は2倍
                ch = 0x60;
            }

            qq->onkai = (qq->onkai & 0x0f) | ch;  // onkai値修正
        }
        else
            ax >>= cl;          // ax=ax/[2^OCTARB]

        qq->fnum = (uint32_t) ax;
    }
    else
    {            // 休符の場合
        qq->onkai = 255;

        if ((qq->lfoswi & 0x11) == 0)
            qq->fnum = 0;      // 音程LFO未使用
    }
}

//  PCM FNUM SET(PMD86)
void PMD::fnumset8(Track * qq, int al)
{
    int    ah, bl;

    ah = al & 0x0f;
    if (ah != 0x0f)
    {      // 音符の場合
        if (_State.pcm86_vol && al >= 0x65)
        {    // o7e?
            if (ah < 5)
            {
                al = 0x60;    // o7
            }
            else
            {
                al = 0x50;    // o6
            }
            al |= ah;
        }

        qq->onkai = al;
        bl = ((al & 0xf0) >> 4) * 12 + ah;
        qq->fnum = p86_tune_data[bl];
    }
    else
    {            // 休符の場合
        qq->onkai = 255;
        if ((qq->lfoswi & 0x11) == 0)
        {
            qq->fnum = 0;      // 音程LFO未使用
        }
    }
}

//  PPZ FNUM SET
void PMD::fnumsetz(Track * qq, int al)
{
    if ((al & 0x0f) != 0x0f)
    {      // 音符の場合
        qq->onkai = al;

        int bx = al & 0x0f;          // bx=onkai
        int cl = (al >> 4) & 0x0f;    // cl = octarb

        uint32_t ax = (uint32_t) ppz_tune_data[bx];

        if ((cl -= 4) < 0)
        {
            cl = -cl;
            ax >>= cl;
        }
        else
            ax <<= cl;

        qq->fnum = ax;
    }
    else
    {            // 休符の場合
        qq->onkai = 255;

        if ((qq->lfoswi & 0x11) == 0)
            qq->fnum = 0;      // 音程LFO未使用
    }
}

//  ポルタメント(PCM)
uint8_t * PMD::portam(Track * qq, uint8_t * si)
{
    int    ax, al_, bx_;

    if (qq->PartMask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->Length = *(si + 2);
        qq->keyon_flag++;
        qq->Data = si + 3;

        if (--_DriverState.volpush_flag)
        {
            qq->volpush = 0;
        }

        _DriverState.tieflag = 0;
        _DriverState.volpush_flag = 0;
        _DriverState.loop_work &= qq->loopcheck;
        return si + 3;    // 読み飛ばす  (Mask時)
    }

    fnumsetm(qq, oshift(qq, lfoinitp(qq, *si++)));

    bx_ = (int) qq->fnum;
    al_ = (int) qq->onkai;

    fnumsetm(qq, oshift(qq, *si++));

    ax = (int) qq->fnum;       // ax = ポルタメント先のdelta_n値

    qq->onkai = al_;
    qq->fnum = (uint32_t) bx_;      // bx = ポルタメント元のdekta_n値
    ax -= bx_;        // ax = delta_n差

    qq->Length = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->Length;    // 商
    qq->porta_num3 = ax % qq->Length;    // 余り
    qq->lfoswi |= 8;        // Porta ON

    if (qq->volpush && qq->onkai != 255)
    {
        if (--_DriverState.volpush_flag)
        {
            _DriverState.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volsetm(qq);
    OtodasiM(qq);
    if (qq->keyoff_flag & 1)
    {
        keyonm(qq);
    }

    qq->keyon_flag++;
    qq->Data = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {      // '&'が直後にあったらKeyOffしない
        qq->keyoff_flag = 2;
    }

    _DriverState.loop_work &= qq->loopcheck;
    return si;
}

//  ポルタメント(PPZ)
uint8_t * PMD::portaz(Track * qq, uint8_t * si)
{
    int    ax, al_, bx_;

    if (qq->PartMask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->Length = *(si + 2);
        qq->keyon_flag++;
        qq->Data = si + 3;

        if (--_DriverState.volpush_flag)
        {
            qq->volpush = 0;
        }

        _DriverState.tieflag = 0;
        _DriverState.volpush_flag = 0;
        _DriverState.loop_work &= qq->loopcheck;
        return si + 3;    // 読み飛ばす  (Mask時)
    }

    fnumsetz(qq, oshift(qq, lfoinitp(qq, *si++)));

    bx_ = (int) qq->fnum;
    al_ = qq->onkai;
    fnumsetz(qq, oshift(qq, *si++));
    ax = (int) qq->fnum;       // ax = ポルタメント先のdelta_n値

    qq->onkai = al_;
    qq->fnum = (uint32_t) bx_;      // bx = ポルタメント元のdekta_n値
    ax -= bx_;        // ax = delta_n差
    ax /= 16;

    qq->Length = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->Length;    // 商
    qq->porta_num3 = ax % qq->Length;    // 余り
    qq->lfoswi |= 8;        // Porta ON

    if (qq->volpush && qq->onkai != 255)
    {
        if (--_DriverState.volpush_flag)
        {
            _DriverState.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volsetz(qq);
    OtodasiZ(qq);
    if (qq->keyoff_flag & 1)
    {
        keyonz(qq);
    }

    qq->keyon_flag++;
    qq->Data = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {      // '&'が直後にあったらKeyOffしない
        qq->keyoff_flag = 2;
    }

    _DriverState.loop_work &= qq->loopcheck;
    return si;
}

void PMD::keyoffm(Track * qq)
{
    if (qq->envf != -1)
    {
        if (qq->envf == 2) return;
    }
    else
    {
        if (qq->eenv_count == 4) return;
    }

    if (_DriverState.PCMRelease != 0x8000)
    {
        // PCM RESET
        _OPNAW->SetReg(0x100, 0x21);

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_DriverState.PCMRelease));
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_DriverState.PCMRelease));

        // Stop ADDRESS for Release
        _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_State.PCMStop));
        _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_State.PCMStop));

        // PCM PLAY(non_repeat)
        _OPNAW->SetReg(0x100, 0xa0);
    }

    keyoffp(qq);
    return;
}

void PMD::keyoff8(Track * qq)
{
    _P86->Keyoff();

    if (qq->envf != -1)
    {
        if (qq->envf != 2)
        {
            keyoffp(qq);
        }
        return;
    }

    if (qq->eenv_count != 4)
    {
        keyoffp(qq);
    }
    return;
}

//  ppz KEYOFF
void PMD::keyoffz(Track * qq)
{
    if (qq->envf != -1)
    {
        if (qq->envf == 2) return;
    }
    else
    {
        if (qq->eenv_count == 4) return;
    }

    keyoffp(qq);
    return;
}

//  Pan setting Extend
uint8_t * PMD::pansetm_ex(Track * qq, uint8_t * si)
{
    if (*si == 0)
    {
        qq->fmpan = 0xc0;
    }
    else if (*si < 0x80)
    {
        qq->fmpan = 0x80;
    }
    else
    {
        qq->fmpan = 0x40;
    }

    return si + 2;  // 逆走flagは読み飛ばす
}

/// <summary>
/// PCM Repeat Settings
/// </summary>
uint8_t * PMD::pcmrepeat_set(Track *, uint8_t * si)
{
    int ax = *(int16_t *) si;
    si += 2;

    if (ax >= 0)
        ax += _State.PCMStart;
    else
        ax += _State.PCMStop;

    _DriverState.PCMRepeat1 = ax;

    ax = *(int16_t *) si;
    si += 2;

    if (ax > 0)
        ax += _State.PCMStart;
    else
        ax += _State.PCMStop;

    _DriverState.PCMRepeat2 = ax;

    ax = *(uint16_t *) si;
    si += 2;

    if (ax < 0x8000)
        ax += _State.PCMStart;
    else
    if (ax > 0x8000)
        ax += _State.PCMStop;

    _DriverState.PCMRelease = ax;

    return si;
}

//  リピート設定(PMD86)
uint8_t * PMD::pcmrepeat_set8(Track *, uint8_t * si)
{
    int16_t loop_start, loop_end, release_start;

    loop_start = *(int16_t *) si;
    si += 2;

    loop_end = *(int16_t *) si;
    si += 2;

    release_start = *(int16_t *) si;

    if (_State.pcm86_vol)
    {
        _P86->SetLoop(loop_start, loop_end, release_start, true);
    }
    else
    {
        _P86->SetLoop(loop_start, loop_end, release_start, false);
    }

    return si + 2;
}

//  リピート設定
uint8_t * PMD::ppzrepeat_set(Track * qq, uint8_t * data)
{
    int LoopStart, LoopEnd;

    if ((qq->SampleNumber & 0x80) == 0)
    {
        LoopStart = *(int16_t *) data;
        data += 2;

        if (LoopStart < 0)
            LoopStart = (int) (_PPZ8->PCME_WORK[0].PZIItem[qq->SampleNumber].Size - LoopStart);

        LoopEnd = *(int16_t *) data;
        data += 2;

        if (LoopEnd < 0)
            LoopEnd = (int) (_PPZ8->PCME_WORK[0].PZIItem[qq->SampleNumber].Size - LoopStart);
    }
    else
    {
        LoopStart = *(int16_t *) data;
        data += 2;

        if (LoopStart < 0)
            LoopStart = (int) (_PPZ8->PCME_WORK[1].PZIItem[qq->SampleNumber & 0x7f].Size - LoopStart);

        LoopEnd = *(int16_t *) data;
        data += 2;

        if (LoopEnd < 0)
            LoopEnd = (int) (_PPZ8->PCME_WORK[1].PZIItem[qq->SampleNumber & 0x7f].Size - LoopEnd);
    }

    _PPZ8->SetLoop(_DriverState.CurrentChannel, (uint32_t) LoopStart, (uint32_t) LoopEnd);

    return data + 2;
}

uint8_t * PMD::vol_one_up_pcm(Track * qq, uint8_t * si)
{
    int    al;

    al = (int) *si++ + qq->volume;
    if (al > 254) al = 254;
    al++;
    qq->volpush = al;
    _DriverState.volpush_flag = 1;
    return si;
}

//  COMMAND 'p' [Panning Set]
uint8_t * PMD::pansetm(Track * qq, uint8_t * si)
{
    qq->fmpan = (*si << 6) & 0xc0;
    return si + 1;
}

//  COMMAND 'p' [Panning Set]
//  p0    逆相
//  p1    右
//  p2    左
//  p3    中
uint8_t * PMD::panset8(Track *, uint8_t * si)
{
    int    flag, data;

    data = 0;

    switch (*si++)
    {
        case 1:          // Right
            flag = 2;
            data = 1;
            break;

        case 2:          // Left
            flag = 1;
            data = 0;
            break;

        case 3:          // Center
            flag = 3;
            data = 0;
            break;

        default:          // 逆相
            flag = 3 | 4;
            data = 0;

    }

    _P86->SetPan(flag, data);

    return si;
}

//  COMMAND 'p' [Panning Set]
//    0=0  無音
//    1=9  右
//    2=1  左
//    3=5  中央
uint8_t * PMD::pansetz(Track * qq, uint8_t * si)
{
    qq->fmpan = ppzpandata[*si++];
    _PPZ8->SetPan(_DriverState.CurrentChannel, qq->fmpan);
    return si;
}

//  Pan setting Extend
//    px -4?+4
uint8_t * PMD::pansetz_ex(Track * qq, uint8_t * si)
{
    int    al;

    al = *(int8_t *) si++;
    si++;    // 逆相flagは読み飛ばす

    if (al >= 5)
    {
        al = 4;
    }
    else if (al < -4)
    {
        al = -4;
    }

    qq->fmpan = al + 5;
    _PPZ8->SetPan(_DriverState.CurrentChannel, qq->fmpan);
    return si;
}

uint8_t * PMD::comatm(Track * track, uint8_t * si)
{
    track->SampleNumber = *si++;

    _State.PCMStart = pcmends.Address[track->SampleNumber][0];
    _State.PCMStop = pcmends.Address[track->SampleNumber][1];

    _DriverState.PCMRepeat1 = 0;
    _DriverState.PCMRepeat2 = 0;
    _DriverState.PCMRelease = 0x8000;

    return si;
}

uint8_t * PMD::comat8(Track * track, uint8_t * si)
{
    track->SampleNumber = *si++;

    _P86->SelectSample(track->SampleNumber);

    return si;
}

uint8_t * PMD::comatz(Track * qq, uint8_t * si)
{
    qq->SampleNumber = *si++;

    if ((qq->SampleNumber & 0x80) == 0)
    {
        _PPZ8->SetLoop(_DriverState.CurrentChannel, _PPZ8->PCME_WORK[0].PZIItem[qq->SampleNumber].LoopStart, _PPZ8->PCME_WORK[0].PZIItem[qq->SampleNumber].LoopEnd);
        _PPZ8->SetSourceRate(_DriverState.CurrentChannel, _PPZ8->PCME_WORK[0].PZIItem[qq->SampleNumber].SampleRate);
    }
    else
    {
        _PPZ8->SetLoop(_DriverState.CurrentChannel, _PPZ8->PCME_WORK[1].PZIItem[qq->SampleNumber & 0x7f].LoopStart, _PPZ8->PCME_WORK[1].PZIItem[qq->SampleNumber & 0x7f].LoopEnd);
        _PPZ8->SetSourceRate(_DriverState.CurrentChannel, _PPZ8->PCME_WORK[1].PZIItem[qq->SampleNumber & 0x7f].SampleRate);
    }
    return si;
}

/// <summary>
/// Decides to stop the SSG drum and reset the SSG.
/// </summary>
bool PMD::ssgdrum_check(Track * track, int al)
{
    // Do not stop the drum during the SSG mask. SSG drums are not playing.
    if ((track->PartMask & 1) || ((track->PartMask & 2) == 0))
        return false;

    // Do not turn off normal sound effects.
    if (_EffectState.effon >= 2)
        return false;

    // Don't stop the drums during rests.
    if ((al & 0x0F) == 0x0F)
        return false;

    // Is the SSG drum still playing?
    if (_EffectState.effon == 1)
        effend(); // Turn off the SSG drum.

    track->PartMask &= 0xFD;

    return (track->PartMask == 0);
}

uint8_t * PMD::ExecuteFMCommand(Track * track, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = ProgramChange(track, si); break;
        case 0xfe: track->qdata = *si++; track->qdat3 = 0; break;
        case 0xfd: track->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: track->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = CommandSetStartOfLoop(track, si); break;
        case 0xf8: si = CommandSetEndOfLoop(track, si); break;
        case 0xf7: si = CommandExitLoop(track, si); break;
        case 0xf6: track->LoopData = si; break;
        case 0xf5: track->shift = *(int8_t *) si++; break;
        case 0xf4: if ((track->volume += 4) > 127) track->volume = 127; break;
        case 0xf3: if (track->volume < 4) track->volume = 0; else track->volume -= 4; break;
        case 0xf2: si = lfoset(track, si); break;
        case 0xf1: si = lfoswitch(track, si); ch3_setting(track); break;
        case 0xf0: si += 4; break;

        case 0xef:
            _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + *si), (uint32_t) (*(si + 1)));
            si += 2;
            break;

        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = panset(track, si); break;        // FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: track->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: track->hldelay = *si++; break;
            //追加 for V2.3
        case 0xe3:
            if ((track->volume += *si++) > 127)
                track->volume = 127;
            break;

        case 0xe2:
            if (track->volume < *si)
                track->volume = 0;
            else
                track->volume -= *si;
            si++;
            break;

        case 0xe1: si = hlfo_set(track, si); break;
        case 0xe0: _State.port22h = *si; _OPNAW->SetReg(0x22, *si++); break;
            //
        case 0xdf: _State.BarLength = *si++; break;
            //
        case 0xde: si = vol_one_up_fm(track, si); break;
        case 0xdd: si = vol_one_down(track, si); break;
            //
        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;
            //
        case 0xda: si = porta(track, si); break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6:
            track->mdspd = track->mdspd2 = *si++;
            track->mdepth = *(int8_t *) si++;
            break;

        case 0xd5: track->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(track, si); break;
        case 0xd3: si = fm_efct_set(track, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
        case 0xcf:
            si = SetSlotMask(track, si);
            break;

        case 0xce: si += 6; break;
        case 0xcd: si += 5; break;
        case 0xcc: si++; break;
        case 0xcb: track->lfo_wave = *si++; break;
        case 0xca:
            track->extendmode = (track->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9: si++; break;
        case 0xc8: si = slotdetune_set(track, si); break;
        case 0xc7: si = slotdetune_set2(track, si); break;
        case 0xc6: si = fm3_extpartset(track, si); break;
        case 0xc5: si = volmask_set(track, si); break;
        case 0xc4: track->qdatb = *si++; break;
        case 0xc3: si = panset_ex(track, si); break;
        case 0xc2: track->delay = track->delay2 = *si++; lfoinit_main(track); break;
        case 0xc1: break;
        case 0xc0: si = fm_mml_part_mask(track, si); break;
        case 0xbf: SwapLFO(track); si = lfoset(track, si); SwapLFO(track); break;
        case 0xbe: si = _lfoswitch(track, si); ch3_setting(track); break;
        case 0xbd:
            SwapLFO(track);

            track->mdspd = track->mdspd2 = *si++;
            track->mdepth = *(int8_t *) si++;

            SwapLFO(track);
            break;

        case 0xbc: SwapLFO(track); track->lfo_wave = *si++; SwapLFO(track); break;
        case 0xbb:
            SwapLFO(track);

            track->extendmode = (track->extendmode & 0xfd) | ((*si++ & 1) << 1);

            SwapLFO(track);
            break;

        case 0xba: si = _volmask_set(track, si); break;
        case 0xb9:
            SwapLFO(track);
            track->delay = track->delay2 = *si++; lfoinit_main(track);
            SwapLFO(track);
            break;

        case 0xb8: si = tl_set(track, si); break;
        case 0xb7: si = mdepth_count(track, si); break;
        case 0xb6: si = fb_set(track, si); break;
        case 0xb5:
            track->sdelay_m = (~(*si++) << 4) & 0xf0;
            track->sdelay_c = track->sdelay = *si++;
            break;

        case 0xb4: si += 16; break;
        case 0xb3: track->qdat2 = *si++; break;
        case 0xb2: track->shift_def = *(int8_t *) si++; break;
        case 0xb1: track->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::ExecuteSSGCommand(Track * track, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si++; break;
        case 0xfe: track->qdata = *si++; track->qdat3 = 0; break;
        case 0xfd: track->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: track->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = CommandSetStartOfLoop(track, si); break;
        case 0xf8: si = CommandSetEndOfLoop(track, si); break;
        case 0xf7: si = CommandExitLoop(track, si); break;
        case 0xf6: track->LoopData = si; break;
        case 0xf5: track->shift = *(int8_t *) si++; break;
        case 0xf4: if (track->volume < 15) track->volume++; break;
        case 0xf3: if (track->volume > 0) track->volume--; break;
        case 0xf2: si = lfoset(track, si); break;
        case 0xf1: si = lfoswitch(track, si); break;
        case 0xf0: si = psgenvset(track, si); break;

        case 0xef: _OPNAW->SetReg(*si, *(si + 1)); si += 2; break;
        case 0xee: _State.SSGNoiseFrequency = *si++; break;
        case 0xed: track->psgpat = *si++; break;

        case 0xec: si++; break;
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;

        case 0xe7: track->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: si++; break;

        case 0xe3: track->volume += *si++; if (track->volume > 15) track->volume = 15; break;
        case 0xe2: track->volume -= *si++; if (track->volume < 0) track->volume = 0; break;


        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = vol_one_up_psg(track, si); break;
        case 0xdd: si = vol_one_down(track, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si = portap(track, si); break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6: track->mdspd = track->mdspd2 = *si++; track->mdepth = *(int8_t *) si++; break;
        case 0xd5: track->detune += *(int16_t *) si; si += 2; break;

        case 0xd4: si = ssg_efct_set(track, si); break;
        case 0xd3: si = fm_efct_set(track, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si = psgnoise_move(si); break;

        case 0xcf: si++; break;
        case 0xce: si += 6; break;
        case 0xcd: si = extend_psgenvset(track, si); break;
        case 0xcc:
            track->extendmode = (track->extendmode & 0xfe) | (*si++ & 1);
            break;

        case 0xcb: track->lfo_wave = *si++; break;
        case 0xca:
            track->extendmode = (track->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            track->extendmode = (track->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: track->qdatb = *si++; break;
        case 0xc3: si += 2; break;
        case 0xc2: track->delay = track->delay2 = *si++; lfoinit_main(track); break;
        case 0xc1: break;
        case 0xc0: si = ssg_mml_part_mask(track, si); break;
        case 0xbf: SwapLFO(track); si = lfoset(track, si); SwapLFO(track); break;
        case 0xbe:
            track->lfoswi = (track->lfoswi & 0x8f) | ((*si++ & 7) << 4);

            SwapLFO(track);
            lfoinit_main(track);
            SwapLFO(track);
            break;

        case 0xbd:
            SwapLFO(track);
            track->mdspd = track->mdspd2 = *si++;
            track->mdepth = *(int8_t *) si++;
            SwapLFO(track);
            break;

        case 0xbc:
            SwapLFO(track);

            track->lfo_wave = *si++;

            SwapLFO(track);
            break;

        case 0xbb:
            SwapLFO(track);

            track->extendmode = (track->extendmode & 0xfd) | ((*si++ & 1) << 1);

            SwapLFO(track);
            break;

        case 0xba: si++; break;
        case 0xb9:
            SwapLFO(track);

            track->delay = track->delay2 = *si++;
            lfoinit_main(track);

// FIXME    break;

            SwapLFO(track);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(track, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;
        case 0xb3: track->qdat2 = *si++; break;
        case 0xb2: track->shift_def = *(int8_t *) si++; break;
        case 0xb1: track->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::ExecuteRhythmCommand(Track * track, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si++; break;
        case 0xfe: si++; break;
        case 0xfd: track->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: track->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = CommandSetStartOfLoop(track, si); break;
        case 0xf8: si = CommandSetEndOfLoop(track, si); break;
        case 0xf7: si = CommandExitLoop(track, si); break;
        case 0xf6: track->LoopData = si; break;
        case 0xf5: si++; break;
        case 0xf4: if (track->volume < 15) track->volume++; break;
        case 0xf3: if (track->volume > 0) track->volume--; break;
        case 0xf2: si += 4; break;
        case 0xf1: si = pdrswitch(track, si); break;
        case 0xf0: si += 4; break;

        case 0xef: _OPNAW->SetReg(*si, *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;

        case 0xec: si++; break;
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;

        case 0xe7: si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: si++; break;

        case 0xe3: if ((track->volume + *si) < 16) track->volume += *si; si++; break;
        case 0xe2: if ((track->volume - *si) >= 0) track->volume -= *si; si++; break;

        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = vol_one_up_psg(track, si); break;
        case 0xdd: si = vol_one_down(track, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si++; break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6: si += 2; break;
        case 0xd5: track->detune += *(int16_t *) si; si += 2; break;

        case 0xd4: si = ssg_efct_set(track, si); break;
        case 0xd3: si = fm_efct_set(track, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si++; break;

        case 0xcf: si++; break;
        case 0xce: si += 6; break;
        case 0xcd: si += 5; break;
        case 0xcc: si++; break;
        case 0xcb: si++; break;
        case 0xca: si++; break;
        case 0xc9: si++; break;
        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: si++; break;
        case 0xc3: si += 2; break;
        case 0xc2: si++; break;
        case 0xc1: break;
        case 0xc0: si = rhythm_mml_part_mask(track, si); break;
        case 0xbf: si += 4; break;
        case 0xbe: si++; break;
        case 0xbd: si += 2; break;
        case 0xbc: si++; break;
        case 0xbb: si++; break;
        case 0xba: si++; break;
        case 0xb9: si++; break;
        case 0xb8: si += 2; break;
        case 0xb7: si++; break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;
        case 0xb3: si++; break;
        case 0xb2: si++; break;
        case 0xb1: si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::ExecuteADPCMCommand(Track * track, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comatm(track, si); break;
        case 0xfe: track->qdata = *si++; break;
        case 0xfd: track->volume = *si++; break;
        case 0xfc: si = comt(si); break;
        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: track->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = CommandSetStartOfLoop(track, si); break;
        case 0xf8: si = CommandSetEndOfLoop(track, si); break;
        case 0xf7: si = CommandExitLoop(track, si); break;
        case 0xf6: track->LoopData = si; break;
        case 0xf5: track->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (track->volume < (255 - 16)) track->volume += 16;
            else track->volume = 255;
            break;

        case 0xf3: if (track->volume < 16) track->volume = 0; else track->volume -= 16; break;
        case 0xf2: si = lfoset(track, si); break;
        case 0xf1: si = lfoswitch(track, si); break;
        case 0xf0: si = psgenvset(track, si); break;

        case 0xef: _OPNAW->SetReg((uint32_t) (0x100 + *si), (uint32_t) (*(si + 1))); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = pansetm(track, si); break;        // FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;

        case 0xe7: track->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: si++; break;

        case 0xe3:
            if (track->volume < (255 - (*si))) track->volume += (*si);
            else track->volume = 255;
            si++;
            break;

        case 0xe2:
            if (track->volume < *si) track->volume = 0; else track->volume -= *si;
            si++;
            break;

        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = vol_one_up_pcm(track, si); break;
        case 0xdd: si = vol_one_down(track, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si = portam(track, si); break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6: track->mdspd = track->mdspd2 = *si++; track->mdepth = *(int8_t *) si++; break;
        case 0xd5: track->detune += *(int16_t *) si; si += 2; break;

        case 0xd4: si = ssg_efct_set(track, si); break;
        case 0xd3: si = fm_efct_set(track, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si++; break;

        case 0xcf: si++; break;
        case 0xce: si = pcmrepeat_set(track, si); break;
        case 0xcd: si = extend_psgenvset(track, si); break;
        case 0xcc: si++; break;
        case 0xcb: track->lfo_wave = *si++; break;
        case 0xca:
            track->extendmode = (track->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            track->extendmode = (track->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: track->qdatb = *si++; break;
        case 0xc3: si = pansetm_ex(track, si); break;
        case 0xc2: track->delay = track->delay2 = *si++; lfoinit_main(track); break;
        case 0xc1: break;
        case 0xc0: si = pcm_mml_part_mask(track, si); break;
        case 0xbf: SwapLFO(track); si = lfoset(track, si); SwapLFO(track); break;
        case 0xbe: si = _lfoswitch(track, si); break;
        case 0xbd:
            SwapLFO(track);

            track->mdspd = track->mdspd2 = *si++;
            track->mdepth = *(int8_t *) si++;

            SwapLFO(track);
            break;

        case 0xbc:
            SwapLFO(track);

            track->lfo_wave = *si++;

            SwapLFO(track);
            break;

        case 0xbb:
            SwapLFO(track);

            track->extendmode = (track->extendmode & 0xfd) | ((*si++ & 1) << 1);

            SwapLFO(track);
            break;

        case 0xba: si = _volmask_set(track, si); break;
        case 0xb9:
            SwapLFO(track);

            track->delay = track->delay2 = *si++;
            lfoinit_main(track);
// FIXME    break;

            SwapLFO(track);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(track, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si = ppz_extpartset(track, si); break;
        case 0xb3: track->qdat2 = *si++; break;
        case 0xb2: track->shift_def = *(int8_t *) si++; break;
        case 0xb1: track->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::ExecutePCM86Command(Track * track, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comat8(track, si); break;
        case 0xfe: track->qdata = *si++; break;
        case 0xfd: track->volume = *si++; break;
        case 0xfc: si = comt(si); break;
        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: track->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = CommandSetStartOfLoop(track, si); break;
        case 0xf8: si = CommandSetEndOfLoop(track, si); break;
        case 0xf7: si = CommandExitLoop(track, si); break;
        case 0xf6: track->LoopData = si; break;
        case 0xf5: track->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (track->volume < (255 - 16)) track->volume += 16;
            else track->volume = 255;
            break;

        case 0xf3: if (track->volume < 16) track->volume = 0; else track->volume -= 16; break;
        case 0xf2: si = lfoset(track, si); break;
        case 0xf1: si = lfoswitch(track, si); break;
        case 0xf0: si = psgenvset(track, si); break;

        case 0xef: _OPNAW->SetReg((uint32_t) (0x100 + *si), (uint32_t) (*(si + 1))); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = panset8(track, si); break;        // FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;

        case 0xe7: track->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: si++; break;

        case 0xe3:
            if (track->volume < (255 - (*si))) track->volume += (*si);
            else track->volume = 255;
            si++;
            break;

        case 0xe2:
            if (track->volume < *si) track->volume = 0; else track->volume -= *si;
            si++;
            break;

        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = vol_one_up_pcm(track, si); break;
        case 0xdd: si = vol_one_down(track, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si++; break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6: track->mdspd = track->mdspd2 = *si++; track->mdepth = *(int8_t *) si++; break;
        case 0xd5: track->detune += *(int16_t *) si; si += 2; break;

        case 0xd4: si = ssg_efct_set(track, si); break;
        case 0xd3: si = fm_efct_set(track, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si++; break;

        case 0xcf: si++; break;
        case 0xce: si = pcmrepeat_set8(track, si); break;
        case 0xcd: si = extend_psgenvset(track, si); break;
        case 0xcc: si++; break;
        case 0xcb: track->lfo_wave = *si++; break;
        case 0xca:
            track->extendmode = (track->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            track->extendmode = (track->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: track->qdatb = *si++; break;
        case 0xc3: si = panset8_ex(track, si); break;
        case 0xc2: track->delay = track->delay2 = *si++; lfoinit_main(track); break;
        case 0xc1: break;
        case 0xc0: si = pcm_mml_part_mask8(track, si); break;
        case 0xbf: si += 4; break;
        case 0xbe: si++; break;
        case 0xbd: si += 2; break;
        case 0xbc: si++; break;
        case 0xbb: si++; break;
        case 0xba: si++; break;
        case 0xb9: si++; break;
        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(track, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si = ppz_extpartset(track, si); break;
        case 0xb3: track->qdat2 = *si++; break;
        case 0xb2: track->shift_def = *(int8_t *) si++; break;
        case 0xb1: track->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::ExecutePPZ8Command(Track * track, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comatz(track, si); break;
        case 0xfe: track->qdata = *si++; break;
        case 0xfd: track->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: track->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = CommandSetStartOfLoop(track, si); break;
        case 0xf8: si = CommandSetEndOfLoop(track, si); break;
        case 0xf7: si = CommandExitLoop(track, si); break;
        case 0xf6: track->LoopData = si; break;
        case 0xf5: track->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (track->volume < (255 - 16))
                track->volume += 16;
            else
                track->volume = 255;
            break;

        case 0xf3: if (track->volume < 16) track->volume = 0; else track->volume -= 16; break;
        case 0xf2: si = lfoset(track, si); break;
        case 0xf1: si = lfoswitch(track, si); break;
        case 0xf0: si = psgenvset(track, si); break;

        case 0xef: _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + *si), (uint32_t) *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = pansetz(track, si); break;        // FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;

        case 0xe7: track->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;

        case 0xe4: si++; break;

        case 0xe3:
            if (track->volume < (255 - (*si))) track->volume += (*si);
            else track->volume = 255;
            si++;
            break;

        case 0xe2:
            if (track->volume < *si) track->volume = 0; else track->volume -= *si;
            si++;
            break;

        case 0xe1: si++; break;
        case 0xe0: si++; break;

        case 0xdf: _State.BarLength = *si++; break;

        case 0xde: si = vol_one_up_pcm(track, si); break;
        case 0xdd: si = vol_one_down(track, si); break;

        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;

        case 0xda: si = portaz(track, si); break;

        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;

        case 0xd6: track->mdspd = track->mdspd2 = *si++; track->mdepth = *(int8_t *) si++; break;
        case 0xd5: track->detune += *(int16_t *) si; si += 2; break;

        case 0xd4: si = ssg_efct_set(track, si); break;
        case 0xd3: si = fm_efct_set(track, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State.FadeOutSpeed = *si++;
            break;

        case 0xd1: si++; break;
        case 0xd0: si++; break;

        case 0xcf: si++; break;
        case 0xce: si = ppzrepeat_set(track, si); break;
        case 0xcd: si = extend_psgenvset(track, si); break;
        case 0xcc: si++; break;
        case 0xcb: track->lfo_wave = *si++; break;
        case 0xca:
            track->extendmode = (track->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            track->extendmode = (track->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: track->qdatb = *si++; break;
        case 0xc3: si = pansetz_ex(track, si); break;
        case 0xc2: track->delay = track->delay2 = *si++; lfoinit_main(track); break;
        case 0xc1: break;
        case 0xc0: si = ppz_mml_part_mask(track, si); break;
        case 0xbf: SwapLFO(track); si = lfoset(track, si); SwapLFO(track); break;
        case 0xbe: si = _lfoswitch(track, si); break;
        case 0xbd:
            SwapLFO(track);
            track->mdspd = track->mdspd2 = *si++;
            track->mdepth = *(int8_t *) si++;
            SwapLFO(track);
            break;

        case 0xbc: SwapLFO(track); track->lfo_wave = *si++; SwapLFO(track); break;
        case 0xbb:
            SwapLFO(track);
            track->extendmode = (track->extendmode & 0xfd) | ((*si++ & 1) << 1);
            SwapLFO(track);
            break;

        case 0xba: si = _volmask_set(track, si); break;
        case 0xb9:
            SwapLFO(track);

            track->delay = track->delay2 = *si++;
            lfoinit_main(track);
// FIXME     break;
            SwapLFO(track);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(track, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;
        case 0xb3: track->qdat2 = *si++; break;
        case 0xb2: track->shift_def = *(int8_t *) si++; break;
        case 0xb1: track->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

//  COMMAND '@' [PROGRAM CHANGE]
uint8_t * PMD::ProgramChange(Track * track, uint8_t * si)
{
    int al = *si++;

    track->SampleNumber = al;

    int dl = track->SampleNumber;

    if (track->PartMask == 0)
    {
        SetTone(track, dl); // Is the part masked?

        return si;
    }

    uint8_t * bx = toneadr_calc(track, dl);

    track->alg_fb = dl = bx[24];
    bx += 4;

    // tl設定
    track->slot1 = bx[0];
    track->slot3 = bx[1];
    track->slot2 = bx[2];
    track->slot4 = bx[3];

    //  Set fm3_alg_fb if masked in FM3ch
    if ((_DriverState.CurrentChannel == 3) && track->ToneMask)
    {
        if (_DriverState.fmsel == 0)
        {
            // in. dl = alg/fb
            if ((track->SlotMask & 0x10) == 0)
            {
                al = _DriverState.fm3_alg_fb & 0x38;    // fbは前の値を使用
                dl = (dl & 7) | al;
            }

            _DriverState.fm3_alg_fb = dl;
            track->alg_fb = al;
        }
    }

    return si;
}

// Tone Settings (TONE_NUMBER / PART_DATA_ADDRESS)
void PMD::SetTone(Track * track, int dl)
{
    uint8_t * bx = toneadr_calc(track, dl);

    if (MuteFMPart(track))
    {
        // When _ToneMask=0 (Only TL work is set)
        bx += 4;

        // tl setting
        track->slot1 = bx[0];
        track->slot3 = bx[1];
        track->slot2 = bx[2];
        track->slot4 = bx[3];

        return;
    }

    // Set AL/FB
    int dh = 0xb0 - 1 + _DriverState.CurrentChannel;

    if (_DriverState.af_check)
        dl = track->alg_fb; // Is the mode not setting ALG/FB?
    else
        dl = bx[24];

    if (_DriverState.CurrentChannel == 3 && _DriverState.fmsel == 0)
    {
        if (_DriverState.af_check != 0)
            dl = _DriverState.fm3_alg_fb; // Is the mode not setting ALG/FB?
        else
        {
            if ((track->SlotMask & 0x10) == 0)
                dl = (_DriverState.fm3_alg_fb & 0x38) | (dl & 7); // Are you using slot1?

            _DriverState.fm3_alg_fb = dl;
        }
    }

    _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);

    track->alg_fb = dl;
    dl &= 7;    // dl = algo

    // Check the position of Carrier (also set in VolMask)
    if ((track->VolumeMask1 & 0x0f) == 0)
        track->VolumeMask1 = carrier_table[dl];

    if ((track->VolumeMask2 & 0x0f) == 0)
        track->VolumeMask2 = carrier_table[dl];

    track->carrier = carrier_table[dl];

    int ah = carrier_table[dl + 8];  // Reversed data of slot2/3 (not completed)
    int al = track->ToneMask;

    ah &= al; // AH = mask for TL / AL = mask for others

    // Set each tone parameter (TL is modulator only)
    dh = 0x30 - 1 + _DriverState.CurrentChannel;

    dl = *bx++;        // DT/ML
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // TL
    if (ah & 0x80) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x40) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh),(uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x20) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x10) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // KS/AR
    if (al & 0x08) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // AM/DR
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // SR
    if (al & 0x08) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // SL/RR
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;
/*
    dl = *bx++;        // SL/RR
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh+=4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh+=4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh+=4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh+=4;
*/
    // Save TL for each SLOT in workpiece
    bx -= 20;
    track->slot1 = bx[0];
    track->slot3 = bx[1];
    track->slot2 = bx[2];
    track->slot4 = bx[3];
}

// Completely muting the [PartB] part (TL=127 and RR=15 and KEY-OFF). cy=1 ･･･ All slots are neiromasked
int PMD::MuteFMPart(Track * track)
{
    if (track->ToneMask == 0)
        return 1;

    int dh = _DriverState.CurrentChannel + 0x40 - 1;

    if (track->ToneMask & 0x80)
    {
        _OPNAW->SetReg((uint32_t) ( _DriverState.fmsel         + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_DriverState.fmsel + 0x40) + dh), 127);
    }

    dh += 4;

    if (track->ToneMask & 0x40)
    {
        _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_DriverState.fmsel + 0x40) + dh), 127);
    }

    dh += 4;

    if (track->ToneMask & 0x20)
    {
        _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_DriverState.fmsel + 0x40) + dh), 127);
    }

    dh += 4;

    if (track->ToneMask & 0x10)
    {
        _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_DriverState.fmsel + 0x40) + dh), 127);
    }

    KeyOffEx(track);

    return 0;
}

//  TONE DATA START ADDRESS を計算
//    input  dl  tone_number
//    output  bx  address
uint8_t * PMD::toneadr_calc(Track * track, int dl)
{
    if (!_State.HasToneData && (track != &_EffectTrack))
        return _State.VData + ((size_t) dl << 5);

    uint8_t * bx = _State.ToneData;

    while (*bx != dl)
    {
        bx += 26;

        if (bx > _MData + sizeof(_MData) - 26)
            return _State.ToneData + 1; // Set first timbre if not found.
    }

    return bx + 1;
}

// FM tone generator hard LFO setting (V2.4 expansion)
uint8_t * PMD::hlfo_set(Track * qq, uint8_t * si)
{
    qq->fmpan = (qq->fmpan & 0xc0) | *si++;

    if (_DriverState.CurrentChannel == 3 && _DriverState.fmsel == 0)
    {
        // Part_e is impossible because it is only for 2608
        // For FM3, set all four parts
        _FMTrack[2].fmpan = qq->fmpan;
        _ExtensionTrack[0].fmpan = qq->fmpan;
        _ExtensionTrack[1].fmpan = qq->fmpan;
        _ExtensionTrack[2].fmpan = qq->fmpan;
    }

    if (qq->PartMask == 0)
    {    // パートマスクされているか？
        _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + _DriverState.CurrentChannel + 0xb4 - 1), calc_panout(qq));
    }
    return si;
}

// Change only one volume (V2.7 expansion)
uint8_t * PMD::vol_one_up_fm(Track * qq, uint8_t * si)
{
    int    al;

    al = (int) qq->volume + 1 + *si++;
    if (al > 128) al = 128;

    qq->volpush = al;
    _DriverState.volpush_flag = 1;
    return si;
}

// Portamento (FM)
uint8_t * PMD::porta(Track * track, uint8_t * si)
{
    int ax;

    if (track->PartMask)
    {
        track->fnum = 0;    //休符に設定
        track->onkai = 255;
        track->Length = *(si + 2);
        track->keyon_flag++;
        track->Data = si + 3;

        if (--_DriverState.volpush_flag)
        {
            track->volpush = 0;
        }

        _DriverState.tieflag = 0;
        _DriverState.volpush_flag = 0;
        _DriverState.loop_work &= track->loopcheck;

        return si + 3;    // 読み飛ばす  (Mask時)
    }

    fnumset(track, oshift(track, lfoinit(track, *si++)));

    int cx = (int) track->fnum;
    int cl = track->onkai;

    fnumset(track, oshift(track, *si++));

    int bx = (int) track->fnum;      // bx=ポルタメント先のfnum値

    track->onkai = cl;
    track->fnum = (uint32_t) cx;      // cx=ポルタメント元のfnum値

    int bh = (int) ((bx / 256) & 0x38) - ((cx / 256) & 0x38);  // 先のoctarb - 元のoctarb

    if (bh)
    {
        bh /= 8;
        ax = bh * 0x26a;      // ax = 26ah * octarb差
    }
    else
    {
        ax = 0;
    }

    bx = (bx & 0x7ff) - (cx & 0x7ff);
    ax += bx;        // ax=26ah*octarb差 + 音程差

    track->Length = *si++;
    si = calc_q(track, si);

    track->porta_num2 = ax / track->Length;  // 商
    track->porta_num3 = ax % track->Length;  // 余り
    track->lfoswi |= 8;        // Porta ON

    if (track->volpush && track->onkai != 255)
    {
        if (--_DriverState.volpush_flag)
        {
            _DriverState.volpush_flag = 0;
            track->volpush = 0;
        }
    }

    volset(track);
    Otodasi(track);
    KeyOn(track);

    track->keyon_flag++;
    track->Data = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;

    if (*si == 0xfb)
    {    // '&'が直後にあったらKeyOffしない
        track->keyoff_flag = 2;
    }
    else
    {
        track->keyoff_flag = 0;
    }
    _DriverState.loop_work &= track->loopcheck;
    return si;
}

//  FM slotmask set
uint8_t * PMD::SetSlotMask(Track * track, uint8_t * si)
{
    int al = *si++;
    int ah = al;

    if (al &= 0x0f)
    {
        track->carrier = al << 4;
    }
    else
    {
        int bl;

        if ((_DriverState.CurrentChannel == 3) && (_DriverState.fmsel == 0))
        {
            bl = _DriverState.fm3_alg_fb;
        }
        else
        {
            uint8_t * bx = toneadr_calc(track, track->SampleNumber);

            bl = bx[24];
        }

        track->carrier = carrier_table[bl & 7];
    }

    ah &= 0xf0;

    if (track->SlotMask != ah)
    {
        track->SlotMask = ah;

        if ((ah & 0xf0) == 0)
            track->PartMask |= 0x20;  // Part mask at s0
        else
            track->PartMask &= 0xdf;  // Unmask part when other than s0

        if (ch3_setting(track))
        {
            // Change process of ch3mode only for FM3ch. If it is ch3, keyon processing in the previous FM3 part
            if (track != &_FMTrack[2])
            {
                if (_FMTrack[2].PartMask == 0 && (_FMTrack[2].keyoff_flag & 1) == 0)
                    KeyOn(&_FMTrack[2]);

                if (track != &_ExtensionTrack[0])
                {
                    if (_ExtensionTrack[0].PartMask == 0 && (_ExtensionTrack[0].keyoff_flag & 1) == 0)
                        KeyOn(&_ExtensionTrack[0]);

                    if (track != &_ExtensionTrack[1])
                    {
                        if (_ExtensionTrack[1].PartMask == 0 && (_ExtensionTrack[1].keyoff_flag & 1) == 0)
                            KeyOn(&_ExtensionTrack[1]);
                    }
                }
            }
        }

        ah = 0;

        if (track->SlotMask & 0x80) ah += 0x11;    // slot4
        if (track->SlotMask & 0x40) ah += 0x44;    // slot3
        if (track->SlotMask & 0x20) ah += 0x22;    // slot2
        if (track->SlotMask & 0x10) ah += 0x88;    // slot1

        track->ToneMask = ah;
    }

    return si;
}

//  Slot Detune Set
uint8_t * PMD::slotdetune_set(Track * qq, uint8_t * si)
{
    int    ax, bl;

    if (_DriverState.CurrentChannel != 3 || _DriverState.fmsel)
    {
        return si + 3;
    }

    bl = *si++;
    ax = *(int16_t *) si;
    si += 2;

    if (bl & 1)
    {
        _State.slot_detune1 = ax;
    }

    if (bl & 2)
    {
        _State.slot_detune2 = ax;
    }

    if (bl & 4)
    {
        _State.slot_detune3 = ax;
    }

    if (bl & 8)
    {
        _State.slot_detune4 = ax;
    }

    if (_State.slot_detune1 || _State.slot_detune2 ||
        _State.slot_detune3 || _State.slot_detune4)
    {
        _DriverState.slotdetune_flag = 1;
    }
    else
    {
        _DriverState.slotdetune_flag = 0;
        _State.slot_detune1 = 0;
    }
    ch3mode_set(qq);
    return si;
}

//  Slot Detune Set(相対)
uint8_t * PMD::slotdetune_set2(Track * qq, uint8_t * si)
{
    int    ax, bl;

    if (_DriverState.CurrentChannel != 3 || _DriverState.fmsel)
    {
        return si + 3;
    }

    bl = *si++;
    ax = *(int16_t *) si;
    si += 2;

    if (bl & 1)
    {
        _State.slot_detune1 += ax;
    }

    if (bl & 2)
    {
        _State.slot_detune2 += ax;
    }

    if (bl & 4)
    {
        _State.slot_detune3 += ax;
    }

    if (bl & 8)
    {
        _State.slot_detune4 += ax;
    }

    if (_State.slot_detune1 || _State.slot_detune2 ||
        _State.slot_detune3 || _State.slot_detune4)
    {
        _DriverState.slotdetune_flag = 1;
    }
    else
    {
        _DriverState.slotdetune_flag = 0;
        _State.slot_detune1 = 0;
    }
    ch3mode_set(qq);
    return si;
}

void PMD::fm3_partinit(Track * qq, uint8_t * ax)
{
    qq->Data = ax;
    qq->Length = 1;          // ｱﾄ 1ｶｳﾝﾄ ﾃﾞ ｴﾝｿｳ ｶｲｼ
    qq->keyoff_flag = -1;      // 現在KeyOff中
    qq->mdc = -1;          // MDepth Counter (無限)
    qq->mdc2 = -1;          //
    qq->_mdc = -1;          //
    qq->_mdc2 = -1;          //
    qq->onkai = 255;        // rest
    qq->onkai_def = 255;      // rest
    qq->volume = 108;        // FM  VOLUME DEFAULT= 108
    qq->fmpan = _FMTrack[2].fmpan;  // FM PAN = CH3と同じ
    qq->PartMask |= 0x20;      // s0用 partmask
}

//  FM3ch 拡張パートセット
uint8_t * PMD::fm3_extpartset(Track *, uint8_t * si)
{
    int16_t ax;

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&_ExtensionTrack[0], &_State.MData[ax]);

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&_ExtensionTrack[1], &_State.MData[ax]);

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&_ExtensionTrack[2], &_State.MData[ax]);
    return si;
}

//  ppz 拡張パートセット
uint8_t * PMD::ppz_extpartset(Track *, uint8_t * si)
{
    for (size_t i = 0; i < _countof(_PPZ8Track); ++i)
    {
        int16_t ax = *(int16_t *) si;
        si += 2;

        if (ax)
        {
            _PPZ8Track[i].Data = &_State.MData[ax];
            _PPZ8Track[i].Length = 1;          // ｱﾄ 1ｶｳﾝﾄ ﾃﾞ ｴﾝｿｳ ｶｲｼ
            _PPZ8Track[i].keyoff_flag = -1;      // 現在KeyOff中
            _PPZ8Track[i].mdc = -1;          // MDepth Counter (無限)
            _PPZ8Track[i].mdc2 = -1;          //
            _PPZ8Track[i]._mdc = -1;          //
            _PPZ8Track[i]._mdc2 = -1;          //
            _PPZ8Track[i].onkai = 255;        // rest
            _PPZ8Track[i].onkai_def = 255;      // rest
            _PPZ8Track[i].volume = 128;        // PCM VOLUME DEFAULT= 128
            _PPZ8Track[i].fmpan = 5;          // PAN=Middle
        }
    }
    return si;
}

//  音量マスクslotの設定
uint8_t * PMD::volmask_set(Track * qq, uint8_t * si)
{
    int    al;

    al = *si++ & 0x0f;
    if (al)
    {
        al = (al << 4) | 0x0f;
        qq->VolumeMask1 = al;
    }
    else
    {
        qq->VolumeMask1 = qq->carrier;
    }
    ch3_setting(qq);
    return si;
}

//  0c0hの追加special命令
uint8_t * PMD::special_0c0h(Track * qq, uint8_t * si, uint8_t al)
{
    switch (al)
    {
        case 0xff: _State.fm_voldown = *si++; break;
        case 0xfe: si = _vd_fm(qq, si); break;
        case 0xfd: _State.ssg_voldown = *si++; break;
        case 0xfc: si = _vd_ssg(qq, si); break;
        case 0xfb: _State.pcm_voldown = *si++; break;
        case 0xfa: si = _vd_pcm(qq, si); break;
        case 0xf9: _State.rhythm_voldown = *si++; break;
        case 0xf8: si = _vd_rhythm(qq, si); break;
        case 0xf7: _State.pcm86_vol = (*si++ & 1); break;
        case 0xf6: _State.ppz_voldown = *si++; break;
        case 0xf5: si = _vd_ppz(qq, si); break;
        default:
            si--;
            *si = 0x80;
    }
    return si;
}

uint8_t * PMD::_vd_fm(Track *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.fm_voldown = Limit(al + _State.fm_voldown, 255, 0);
    else
        _State.fm_voldown = _State._fm_voldown;

    return si;
}

uint8_t * PMD::_vd_ssg(Track *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.ssg_voldown = Limit(al + _State.ssg_voldown, 255, 0);
    else
        _State.ssg_voldown = _State._ssg_voldown;

    return si;
}

uint8_t * PMD::_vd_pcm(Track *, uint8_t * si)
{
    int  al = *(int8_t *) si++;

    if (al)
        _State.pcm_voldown = Limit(al + _State.pcm_voldown, 255, 0);
    else
        _State.pcm_voldown = _State._pcm_voldown;

    return si;
}

uint8_t * PMD::_vd_rhythm(Track *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.rhythm_voldown = Limit(al + _State.rhythm_voldown, 255, 0);
    else
        _State.rhythm_voldown = _State._rhythm_voldown;

    return si;
}

uint8_t * PMD::_vd_ppz(Track *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _State.ppz_voldown = Limit(al + _State.ppz_voldown, 255, 0);
    else
        _State.ppz_voldown = _State._ppz_voldown;

    return si;
}

// Mask on/off for playing parts
uint8_t * PMD::fm_mml_part_mask(Track * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al != 0)
    {
        qq->PartMask |= 0x40;

        if (qq->PartMask == 0x40)
            MuteFMPart(qq);  // 音消去
    }
    else
    {
        if ((qq->PartMask &= 0xbf) == 0)
            ResetTone(qq); // Tone reset
    }

    return si;
}

uint8_t * PMD::ssg_mml_part_mask(Track * qq, uint8_t * si)
{
    uint8_t b = *si++;

    if (b >= 2)
        si = special_0c0h(qq, si, b);
    else
    if (b != 0)
    {
        qq->PartMask |= 0x40;

        if (qq->PartMask == 0x40)
        {
            int ah = ((1 << (_DriverState.CurrentChannel - 1)) | (4 << _DriverState.CurrentChannel));
            uint32_t al = _OPNAW->GetReg(0x07);

            _OPNAW->SetReg(0x07, ah | al);    // SSG KeyOff
        }
    }
    else
        qq->PartMask &= 0xbf;

    return si;
}

uint8_t * PMD::rhythm_mml_part_mask(Track * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al != 0)
        qq->PartMask |= 0x40;
    else
        qq->PartMask &= 0xbf;

    return si;
}

uint8_t * PMD::pcm_mml_part_mask(Track * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al)
    {
        qq->PartMask |= 0x40;

        if (qq->PartMask == 0x40)
        {
            _OPNAW->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
            _OPNAW->SetReg(0x100, 0x01);    // PCM RESET
        }
    }
    else
        qq->PartMask &= 0xbf;

    return si;
}

uint8_t * PMD::pcm_mml_part_mask8(Track * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al)
    {
        qq->PartMask |= 0x40;

        if (qq->PartMask == 0x40)
            _P86->Stop();
    }
    else
        qq->PartMask &= 0xbf;

    return si;
}

uint8_t * PMD::ppz_mml_part_mask(Track * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al != 0)
    {
        qq->PartMask |= 0x40;

        if (qq->PartMask == 0x40)
            _PPZ8->Stop(_DriverState.CurrentChannel);
    }
    else
        qq->PartMask &= 0xbf;

    return si;
}

// Reset the tone of the FM sound source
void PMD::ResetTone(Track * track)
{
    if (track->ToneMask == 0)
        return;

    int s1 = track->slot1;
    int s2 = track->slot2;
    int s3 = track->slot3;
    int s4 = track->slot4;

    _DriverState.af_check = 1;

    SetTone(track, track->SampleNumber);

    _DriverState.af_check = 0;

    track->slot1 = s1;
    track->slot2 = s2;
    track->slot3 = s3;
    track->slot4 = s4;

    int dh;

    int al = ((~track->carrier) & track->SlotMask) & 0xf0;

    // al<- TLを再設定していいslot 4321xxxx
    if (al)
    {
        dh = 0x4c - 1 + _DriverState.CurrentChannel;  // dh=TL FM Port Address

        if (al & 0x80) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) s4);

        dh -= 8;

        if (al & 0x40) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) s3);

        dh += 4;

        if (al & 0x20) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) s2);

        dh -= 8;

        if (al & 0x10) _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) s1);
    }

    dh = _DriverState.CurrentChannel + 0xb4 - 1;

    _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), calc_panout(track));
}

uint8_t * PMD::_lfoswitch(Track * track, uint8_t * si)
{
    track->lfoswi = (track->lfoswi & 0x8f) | ((*si++ & 7) << 4);

    SwapLFO(track);
    lfoinit_main(track);
    SwapLFO(track);

    return si;
}

uint8_t * PMD::_volmask_set(Track * qq, uint8_t * si)
{
    int al = *si++ & 0x0f;

    if (al)
    {
        al = (al << 4) | 0x0f;
        qq->VolumeMask2 = al;
    }
    else
    {
        qq->VolumeMask2 = qq->carrier;
    }

    ch3_setting(qq);

    return si;
}

//  TL変化
uint8_t * PMD::tl_set(Track * qq, uint8_t * si)
{
    int dh = 0x40 - 1 + _DriverState.CurrentChannel;    // dh=TL FM Port Address
    int al = *(int8_t *) si++;
    int ah = al & 0x0f;
    int ch = (qq->SlotMask >> 4) | ((qq->SlotMask << 4) & 0xf0);

    ah &= ch; // ah=変化させるslot 00004321

    int dl = *(int8_t *) si++;

    if (al >= 0)
    {
        dl &= 127;

        if (ah & 1)
        {
            qq->slot1 = dl;
            if (qq->PartMask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
        }

        dh += 8;
        if (ah & 2)
        {
            qq->slot2 = dl;
            if (qq->PartMask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
        }

        dh -= 4;
        if (ah & 4)
        {
            qq->slot3 = dl;
            if (qq->PartMask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
        }

        dh += 8;
        if (ah & 8)
        {
            qq->slot4 = dl;
            if (qq->PartMask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
        }
    }
    else
    {
        //  相対変化
        al = dl;
        if (ah & 1)
        {
            if ((dl = (int) qq->slot1 + al) < 0)
            {
                dl = 0;
                if (al >= 0) dl = 127;
            }
            if (qq->PartMask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
            qq->slot1 = dl;
        }

        dh += 8;
        if (ah & 2)
        {
            if ((dl = (int) qq->slot2 + al) < 0)
            {
                dl = 0;
                if (al >= 0) dl = 127;
            }
            if (qq->PartMask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
            qq->slot2 = dl;
        }

        dh -= 4;
        if (ah & 4)
        {
            if ((dl = (int) qq->slot3 + al) < 0)
            {
                dl = 0;
                if (al >= 0) dl = 127;
            }
            if (qq->PartMask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
            qq->slot3 = dl;
        }

        dh += 8;
        if (ah & 8)
        {
            if ((dl = (int) qq->slot4 + al) < 0)
            {
                dl = 0;
                if (al >= 0) dl = 127;
            }
            if (qq->PartMask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
            qq->slot4 = dl;
        }
    }
    return si;
}

//  FB変化
uint8_t * PMD::fb_set(Track * qq, uint8_t * si)
{
    int dl;

    int dh = _DriverState.CurrentChannel + 0xb0 - 1;  // dh=ALG/FB port address
    int al = *(int8_t *) si++;

    if (al >= 0)
    {
        // in  al 00000xxx 設定するFB
        al = ((al << 3) & 0xff) | (al >> 5);

        // in  al 00xxx000 設定するFB
        if (_DriverState.CurrentChannel == 3 && _DriverState.fmsel == 0)
        {
            if ((qq->SlotMask & 0x10) == 0) return si;
            dl = (_DriverState.fm3_alg_fb & 7) | al;
            _DriverState.fm3_alg_fb = dl;
        }
        else
        {
            dl = (qq->alg_fb & 7) | al;
        }

        _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
        qq->alg_fb = dl;

        return si;
    }
    else
    {
        if ((al & 0x40) == 0)
            al &= 7;

        if (_DriverState.CurrentChannel == 3 && _DriverState.fmsel == 0)
        {
            dl = _DriverState.fm3_alg_fb;
        }
        else
        {
            dl = qq->alg_fb;

        }

        dl = (dl >> 3) & 7;

        if ((al += dl) >= 0)
        {
            if (al >= 8)
            {
                al = 0x38;
                if (_DriverState.CurrentChannel == 3 && _DriverState.fmsel == 0)
                {
                    if ((qq->SlotMask & 0x10) == 0) return si;

                    dl = (_DriverState.fm3_alg_fb & 7) | al;
                    _DriverState.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (qq->alg_fb & 7) | al;
                }
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
                qq->alg_fb = dl;
                return si;
            }
            else
            {
                // in  al 00000xxx 設定するFB
                al = ((al << 3) & 0xff) | (al >> 5);

                // in  al 00xxx000 設定するFB
                if (_DriverState.CurrentChannel == 3 && _DriverState.fmsel == 0)
                {
                    if ((qq->SlotMask & 0x10) == 0) return si;
                    dl = (_DriverState.fm3_alg_fb & 7) | al;
                    _DriverState.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (qq->alg_fb & 7) | al;
                }
                _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
                qq->alg_fb = dl;
                return si;
            }
        }
        else
        {
            al = 0;
            if (_DriverState.CurrentChannel == 3 && _DriverState.fmsel == 0)
            {
                if ((qq->SlotMask & 0x10) == 0) return si;

                dl = (_DriverState.fm3_alg_fb & 7) | al;
                _DriverState.fm3_alg_fb = dl;
            }
            else
            {
                dl = (qq->alg_fb & 7) | al;
            }
            _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            qq->alg_fb = dl;
            return si;
        }
    }
}

//  COMMAND 't' [TEMPO CHANGE1]
//  COMMAND 'T' [TEMPO CHANGE2]
//  COMMAND 't±' [TEMPO CHANGE 相対1]
//  COMMAND 'T±' [TEMPO CHANGE 相対2]
uint8_t * PMD::comt(uint8_t * si)
{
    int al = *si++;

    if (al < 251)
    {
        _State.tempo_d = al;    // T (FC)
        _State.tempo_d_push = al;

        calc_tb_tempo();
    }
    else
    if (al == 0xff)
    {
        al = *si++;          // t (FC FF)

        if (al < 18)
            al = 18;

        _State.tempo_48 = al;
        _State.tempo_48_push = al;

        calc_tempo_tb();
    }
    else
    if (al == 0xfe)
    {
        al = int8_t(*si++);      // T± (FC FE)

        if (al >= 0)
            al += _State.tempo_d_push;
        else
        {
            al += _State.tempo_d_push;

            if (al < 0)
                al = 0;
        }

        if (al > 250)
            al = 250;

        _State.tempo_d = al;
        _State.tempo_d_push = al;

        calc_tb_tempo();
    }
    else
    {
        al = int8_t(*si++);      // t± (FC FD)

        if (al >= 0)
        {
            al += _State.tempo_48_push;

            if (al > 255)
                al = 255;
        }
        else
        {
            al += _State.tempo_48_push;

            if (al < 0)
                al = 18;
        }

        _State.tempo_48 = al;
        _State.tempo_48_push = al;

        calc_tempo_tb();
    }

    return si;
}

// Command '[': Set start of loop
uint8_t * PMD::CommandSetStartOfLoop(Track * track, uint8_t * si)
{
    uint8_t * ax = (track == &_EffectTrack) ? _State.EData : _State.MData;

    ax[*(uint16_t *) si + 1] = 0;

    si += 2;

    return si;
}

// Command ']': Set end of loop
uint8_t * PMD::CommandSetEndOfLoop(Track * track, uint8_t * si)
{
    int ah = *si++;

    if (ah)
    {
        (*si)++;

        int al = *si++;

        if (ah == al)
        {
            si += 2;
            return si;
        }
    }
    else
    {   // 0 Nara Mujouken Loop
        si++;
        track->loopcheck = 1;
    }

    int ax = *(uint16_t *) si + 2;

    if (track == &_EffectTrack)
        si = _State.EData + ax;
    else
        si = _State.MData + ax;

    return si;
}

// Command ':': Loop dash
uint8_t * PMD::CommandExitLoop(Track * track, uint8_t * si)
{
    uint8_t * bx = (track == &_EffectTrack) ? _State.EData : _State.MData;

    bx += *(uint16_t *) si;
    si += 2;

    int dl = *bx++ - 1;

    if (dl != *bx)
        return si;

    si = bx + 3;

    return si;
}

//  LFO ﾊﾟﾗﾒｰﾀ ｾｯﾄ
uint8_t * PMD::lfoset(Track * qq, uint8_t * si)
{
    qq->delay = *si;
    qq->delay2 = *si++;
    qq->speed = *si;
    qq->speed2 = *si++;
    qq->step = *(int8_t *) si;
    qq->step2 = *(int8_t *) si++;
    qq->time = *si;
    qq->time2 = *si++;
    lfoinit_main(qq);
    return si;
}

//  LFO SWITCH
uint8_t * PMD::lfoswitch(Track * qq, uint8_t * si)
{
    int al;

    al = *si++;

    if (al & 0xf8)
    {
        al = 1;
    }
    al &= 7;
    qq->lfoswi = (qq->lfoswi & 0xf8) | al;
    lfoinit_main(qq);
    return si;
}

//  SSG ENVELOPE SET
uint8_t * PMD::psgenvset(Track * qq, uint8_t * si)
{
    qq->eenv_ar = *si; qq->eenv_arc = *si++;
    qq->eenv_dr = *(int8_t *) si++;
    qq->eenv_sr = *si; qq->eenv_src = *si++;
    qq->eenv_rr = *si; qq->eenv_rrc = *si++;

    if (qq->envf == -1)
    {
        qq->envf = 2;    // RR
        qq->eenv_volume = -15;    // volume
    }

    return si;
}

//  "\?" COMMAND [ OPNA Rhythm Keyon/Dump ]
uint8_t * PMD::rhykey(uint8_t * si)
{
    int dl = *si++ & _State.RhythmMask;

    if (dl == 0)
        return si;

    if (_State.FadeOutVolume != 0)
    {
        int al = ((256 - _State.FadeOutVolume) * _State.RhythmVolume) >> 8;

        _OPNAW->SetReg(0x11, (uint32_t) al);
    }

    if (dl < 0x80)
    {
        if (dl & 0x01) _OPNAW->SetReg(0x18, (uint32_t) _State.rdat[0]);
        if (dl & 0x02) _OPNAW->SetReg(0x19, (uint32_t) _State.rdat[1]);
        if (dl & 0x04) _OPNAW->SetReg(0x1a, (uint32_t) _State.rdat[2]);
        if (dl & 0x08) _OPNAW->SetReg(0x1b, (uint32_t) _State.rdat[3]);
        if (dl & 0x10) _OPNAW->SetReg(0x1c, (uint32_t) _State.rdat[4]);
        if (dl & 0x20) _OPNAW->SetReg(0x1d, (uint32_t) _State.rdat[5]);
    }

    _OPNAW->SetReg(0x10, (uint32_t) dl);

    if (dl >= 0x80)
    {
        if (dl & 0x01) _State.rdump_bd++;
        if (dl & 0x02) _State.rdump_sd++;
        if (dl & 0x04) _State.rdump_sym++;
        if (dl & 0x08) _State.rdump_hh++;
        if (dl & 0x10) _State.rdump_tom++;
        if (dl & 0x20) _State.rdump_rim++;

        _State.rshot_dat &= (~dl);
    }
    else
    {
        if (dl & 0x01) _State.rshot_bd++;
        if (dl & 0x02) _State.rshot_sd++;
        if (dl & 0x04) _State.rshot_sym++;
        if (dl & 0x08) _State.rshot_hh++;
        if (dl & 0x10) _State.rshot_tom++;
        if (dl & 0x20) _State.rshot_rim++;

        _State.rshot_dat |= dl;
    }

    return si;
}

//  "\v?n" COMMAND
uint8_t * PMD::rhyvs(uint8_t * si)
{
    int dl = *si & 0x1f;
    int dh = *si++ >> 5;
    int * bx = &_State.rdat[dh - 1];

    dh = 0x18 - 1 + dh;
    dl |= (*bx & 0xc0);
    *bx = dl;

    _OPNAW->SetReg((uint32_t) dh, (uint32_t) dl);

    return si;
}

uint8_t * PMD::rhyvs_sft(uint8_t * si)
{
    int * bx = &_State.rdat[*si - 1];
    int dh = *si++ + 0x18 - 1;
    int dl = *bx & 0x1f;
    int al = (*(int8_t *) si++ + dl);

    if (al >= 32)
    {
        al = 31;
    }
    else
    if (al < 0)
    {
        al = 0;
    }

    dl = (al &= 0x1f);
    dl = *bx = ((*bx & 0xe0) | dl);

    _OPNAW->SetReg((uint32_t) dh, (uint32_t) dl);

    return si;
}

//  "\p?" COMMAND
uint8_t * PMD::rpnset(uint8_t * si)
{
    int * bx;
    int    dh, dl;

    dl = (*si & 3) << 6;

    dh = (*si++ >> 5) & 0x07;
    bx = &_State.rdat[dh - 1];

    dh += 0x18 - 1;
    dl |= (*bx & 0x1f);
    *bx = dl;
    _OPNAW->SetReg((uint32_t) dh, (uint32_t) dl);

    return si;
}

//  "\Vn" COMMAND
uint8_t * PMD::rmsvs(uint8_t * si)
{
    int dl = *si++;

    if (_State.rhythm_voldown != 0)
        dl = ((256 - _State.rhythm_voldown) * dl) >> 8;

    _State.RhythmVolume = dl;

    if (_State.FadeOutVolume != 0)
        dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) dl);

    return si;
}

uint8_t * PMD::rmsvs_sft(uint8_t * si)
{
    int dl = _State.RhythmVolume + *(int8_t *) si++;

    if (dl >= 64)
    {
        if (dl & 0x80)
            dl = 0;
        else
            dl = 63;
    }

    _State.RhythmVolume = dl;

    if (_State.FadeOutVolume != 0)
        dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) dl);

    return si;
}

// Change only one volume (V2.7 expansion)
uint8_t * PMD::vol_one_up_psg(Track * track, uint8_t * si)
{
    int al = track->volume + *si++;

    if (al > 15)
        al = 15;

    track->volpush = ++al;
    _DriverState.volpush_flag = 1;

    return si;
}

uint8_t * PMD::vol_one_down(Track * track, uint8_t * si)
{
    int al = track->volume - *si++;

    if (al < 0)
        al = 0;
    else
    if (al >= 255)
        al = 254;

    track->volpush = ++al;
    _DriverState.volpush_flag = 1;

    return si;
}

//  ポルタメント(SSG)
uint8_t * PMD::portap(Track * track, uint8_t * si)
{
    if (track->PartMask)
    {
        track->fnum = 0;    //休符に設定
        track->onkai = 255;
        track->Length = *(si + 2);
        track->keyon_flag++;
        track->Data = si + 3;

        if (--_DriverState.volpush_flag)
            track->volpush = 0;

        _DriverState.tieflag = 0;
        _DriverState.volpush_flag = 0;
        _DriverState.loop_work &= track->loopcheck;

        return si + 3;    // 読み飛ばす  (Mask時)
    }

    SetSSGTune(track, oshiftp(track, lfoinitp(track, *si++)));

    int bx_ = (int) track->fnum;
    int al_ = track->onkai;

    SetSSGTune(track, oshiftp(track, *si++));

    int ax = (int) track->fnum;       // ax = ポルタメント先のpsg_tune値

    track->onkai = al_;
    track->fnum = (uint32_t) bx_;      // bx = ポルタメント元のpsg_tune値
    ax -= bx_;

    track->Length = *si++;
    si = calc_q(track, si);

    track->porta_num2 = ax / track->Length;    // 商
    track->porta_num3 = ax % track->Length;    // 余り
    track->lfoswi |= 8;        // Porta ON

    if (track->volpush && track->onkai != 255)
    {
        if (--_DriverState.volpush_flag)
        {
            _DriverState.volpush_flag = 0;
            track->volpush = 0;
        }
    }

    volsetp(track);
    OtodasiP(track);
    keyonp(track);

    track->keyon_flag++;
    track->Data = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;
    track->keyoff_flag = 0;

    if (*si == 0xfb) // If there is '&' immediately after, KeyOff will not be done.
        track->keyoff_flag = 2;

    _DriverState.loop_work &= track->loopcheck;

    return si;
}

//  'w' COMMAND [SSG NOISE ﾍｲｷﾝ ｼｭｳﾊｽｳ]
uint8_t * PMD::psgnoise_move(uint8_t * si)
{
    _State.SSGNoiseFrequency += *(int8_t *) si++;

    if (_State.SSGNoiseFrequency < 0)
        _State.SSGNoiseFrequency = 0;

    if (_State.SSGNoiseFrequency > 31)
        _State.SSGNoiseFrequency = 31;

    return si;
}

//  SSG Envelope set (Extend)
uint8_t * PMD::extend_psgenvset(Track * track, uint8_t * si)
{
    track->eenv_ar = *si++ & 0x1f;
    track->eenv_dr = *si++ & 0x1f;
    track->eenv_sr = *si++ & 0x1f;
    track->eenv_rr = *si & 0x0f;
    track->eenv_sl = ((*si++ >> 4) & 0x0f) ^ 0x0f;
    track->eenv_al = *si++ & 0x0f;

    if (track->envf != -1)
    {  // Did you move from normal to expanded?
        track->envf = -1;
        track->eenv_count = 4;    // RR
        track->eenv_volume = 0;  // Volume
    }
    return si;
}

uint8_t * PMD::mdepth_count(Track * track, uint8_t * si)
{
    int al = *si++;

    if (al >= 0x80)
    {
        al &= 0x7f;

        if (al == 0)
            al = 255;

        track->_mdc  = al;
        track->_mdc2 = al;

        return si;
    }

    if (al == 0)
        al = 255;

    track->mdc  = al;
    track->mdc2 = al;

    return si;
}

// Initialization of LFO and SSG/PCM software envelopes

// Entry for SSG/PCM sound source
int PMD::lfoinitp(Track * track, int al)
{
    int    ah;

    ah = al & 0x0f;

    if (ah == 0x0c)
    {
        al = track->onkai_def;
        ah = al & 0x0f;
    }

    track->onkai_def = al;

    // 4.8r 修正
    if (ah == 0x0f)
    {      // ｷｭｰﾌ ﾉ ﾄｷ ﾊ INIT ｼﾅｲﾖ
// PMD 4.8r 修正
        soft_env(track);
        lfo_exit(track);
        return al;
    }

    track->porta_num = 0;      // ポルタメントは初期化

    if (_DriverState.tieflag & 1)
    {  // ﾏｴ ｶﾞ & ﾉ ﾄｷ ﾓ INIT ｼﾅｲ｡
// PMD 4.8r 修正
        soft_env(track);      // 前が & の場合 -> 1回 SofeEnv処理
        lfo_exit(track);
        return al;
    }

    //------------------------------------------------------------------------
    //  ソフトウエアエンベロープ初期化
    //------------------------------------------------------------------------
    if (track->envf != -1)
    {
        track->envf = 0;
        track->eenv_volume = 0;
        track->eenv_ar = track->eenv_arc;

        if (track->eenv_ar == 0)
        {
            track->envf = 1;  // ATTACK=0 ... ｽｸﾞ Decay ﾆ
            track->eenv_volume = track->eenv_dr;
        }

        track->eenv_sr = track->eenv_src;
        track->eenv_rr = track->eenv_rrc;
        lfin1(track);

    }
    else
    {
        //  拡張ssg_envelope用

        track->eenv_arc = track->eenv_ar - 16;
        if (track->eenv_dr < 16)
        {
            track->eenv_drc = (track->eenv_dr - 16) * 2;
        }
        else
        {
            track->eenv_drc = track->eenv_dr - 16;
        }

        if (track->eenv_sr < 16)
        {
            track->eenv_src = (track->eenv_sr - 16) * 2;
        }
        else
        {
            track->eenv_src = track->eenv_sr - 16;
        }

        track->eenv_rrc = (track->eenv_rr) * 2 - 16;
        track->eenv_volume = track->eenv_al;
        track->eenv_count = 1;
        ext_ssgenv_main(track);
        lfin1(track);
    }
    return al;
}

void PMD::lfo_exit(Track * track)
{
    if ((track->lfoswi & 0x03) != 0)
        lfo(track); // If the previous is & -> LFO processing once

    if ((track->lfoswi & 0x30) != 0)
    {  // If the previous is & -> LFO processing once
        SwapLFO(track);
        lfo(track);
        SwapLFO(track);
    }
}

//  ＬＦＯ初期化
void PMD::lfin1(Track * track)
{
    track->hldelay_c = track->hldelay;

    if (track->hldelay)
        _OPNAW->SetReg((uint32_t) (_DriverState.fmsel + _DriverState.CurrentChannel + 0xb4 - 1), (uint32_t) (track->fmpan & 0xc0));

    track->sdelay_c = track->sdelay;

    if (track->lfoswi & 0x03)
    {   // LFO not used
        if ((track->lfoswi & 4) == 0)
            lfoinit_main(track); // Is keyon asynchronous?

        lfo(track);
    }

    if (track->lfoswi & 0x30)
    {   // LFO not used
        if ((track->lfoswi & 0x40) == 0)
        {
            SwapLFO(track); // Is keyon asynchronous?

            lfoinit_main(track);

            SwapLFO(track);
        }

        SwapLFO(track);

        lfo(track);

        SwapLFO(track);
    }
}

void PMD::lfoinit_main(Track * track)
{
    track->lfodat = 0;
    track->delay = track->delay2;
    track->speed = track->speed2;
    track->step = track->step2;
    track->time = track->time2;
    track->mdc = track->mdc2;

    if (track->lfo_wave == 2 || track->lfo_wave == 3)
    {   // Square wave or random wave?
        track->speed = 1;  // Make the LFO apply immediately after the delay
    }
    else
        track->speed++; // Otherwise, +1 to the speed value immediately after the delay
}

//  SHIFT[di] transpose
int PMD::oshiftp(Track * track, int al)
{
    return oshift(track, al);
}

int PMD::oshift(Track * track, int al)
{
    if (al == 0x0f)
        return al;

    int dl = track->shift + track->shift_def;

    if (dl == 0)
        return al;

    int bl = (al & 0x0f);    // bl = ONKAI
    int bh = (al & 0xf0) >> 4;  // bh = OCT

    if (dl < 0)
    {
        // - ﾎｳｺｳ ｼﾌﾄ
        if ((bl += dl) < 0)
        {
            do
            {
                bh--;
            }
            while ((bl += 12) < 0);
        }

        if (bh < 0)
            bh = 0;

        return (bh << 4) | bl;

    }
    else
    {
        // + ﾎｳｺｳ ｼﾌﾄ
        bl += dl;

        while (bl >= 0x0c)
        {
            bh++;
            bl -= 12;
        }

        if (bh > 7)
            bh = 7;

        return (bh << 4) | bl;
    }
}

void PMD::SetSSGTune(Track * track, int al)
{
    if ((al & 0x0f) == 0x0f)
    {    // ｷｭｳﾌ ﾅﾗ FNUM ﾆ 0 ｦ ｾｯﾄ
        track->onkai = 255;

        if (track->lfoswi & 0x11)
            return;

        track->fnum = 0;  // 音程LFO未使用

        return;
    }

    track->onkai = al;

    int cl = (al >> 4) & 0x0f;  // cl=oct
    int bx = al & 0x0f;      // bx=onkai
    int ax = psg_tune_data[bx];

    if (cl > 0)
    {
        int carry;
        ax >>= cl - 1;
        carry = ax & 1;
        ax = (ax >> 1) + carry;
    }

    track->fnum = (uint32_t) ax;
}

//  Q値の計算
//    break  dx
uint8_t * PMD::calc_q(Track * track, uint8_t * si)
{
    if (*si == 0xc1)
    {
        si++; // &&
        track->qdat = 0;

        return si;
    }

    int dl = track->qdata;

    if (track->qdatb)
        dl += (track->Length * track->qdatb) >> 8;

    if (track->qdat3)
    {
        int ax = rnd((track->qdat3 & 0x7f) + 1); // Random-Q

        if ((track->qdat3 & 0x80) == 0)
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

    if (track->qdat2)
    {
        int dh = track->Length - track->qdat2;

        if (dh < 0)
        {
            track->qdat = 0;

            return si;
        }

        if (dl < dh)
            track->qdat = dl;
        else
            track->qdat = dh;
    }
    else
        track->qdat = dl;

    return si;
}

// Set SSG volume.
void PMD::volsetp(Track * track)
{
    if (track->envf == 3 || (track->envf == -1 && track->eenv_count == 0))
        return;

    int dl = (track->volpush) ? track->volpush - 1 : track->volume;

    //------------------------------------------------------------------------
    //  音量down計算
    //------------------------------------------------------------------------
    dl = ((256 - _State.ssg_voldown) * dl) >> 8;

    //------------------------------------------------------------------------
    //  Fadeout計算
    //------------------------------------------------------------------------
    dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (dl <= 0)
    {
        _OPNAW->SetReg((uint32_t) (_DriverState.CurrentChannel + 8 - 1), 0);
        return;
    }

    if (track->envf == -1)
    {
        if (track->eenv_volume == 0)
        {
            _OPNAW->SetReg((uint32_t) (_DriverState.CurrentChannel + 8 - 1), 0);
            return;
        }

        dl = ((((dl * (track->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        dl += track->eenv_volume;

        if (dl <= 0)
        {
            _OPNAW->SetReg((uint32_t) (_DriverState.CurrentChannel + 8 - 1), 0);
            return;
        }

        if (dl > 15)
            dl = 15;
    }

    //--------------------------------------------------------------------
    //  音量LFO計算
    //--------------------------------------------------------------------
    if ((track->lfoswi & 0x22) == 0)
    {
        _OPNAW->SetReg((uint32_t) (_DriverState.CurrentChannel + 8 - 1), (uint32_t) dl);
        return;
    }

    int ax = (track->lfoswi & 2) ? track->lfodat : 0;

    if (track->lfoswi & 0x20)
        ax += track->_lfodat;

    dl += ax;

    if (dl < 0)
    {
        _OPNAW->SetReg((uint32_t) (_DriverState.CurrentChannel + 8 - 1), 0);
        return;
    }

    if (dl > 15)
        dl = 15;

    //------------------------------------------------------------------------
    //  出力
    //------------------------------------------------------------------------
    _OPNAW->SetReg((uint32_t) (_DriverState.CurrentChannel + 8 - 1), (uint32_t) dl);
}

// Set SSG pitch.
void PMD::OtodasiP(Track * track)
{
    if (track->fnum == 0)
        return;

    // SSG Portament set
    int ax = (int) (track->fnum + track->porta_num);
    int dx = 0;

    // SSG Detune/LFO set
    if ((track->extendmode & 1) == 0)
    {
        ax -= track->detune;

        if (track->lfoswi & 1)
            ax -= track->lfodat;

        if (track->lfoswi & 0x10)
            ax -= track->_lfodat;
    }
    else
    {
        // 拡張DETUNE(DETUNE)の計算
        if (track->detune)
        {
            dx = (ax * track->detune) >> 12;    // dx:ax=ax * qq->detune

            if (dx >= 0)
                dx++;
            else
                dx--;

            ax -= dx;
        }

        // 拡張DETUNE(LFO)の計算
        if (track->lfoswi & 0x11)
        {
            if (track->lfoswi & 1)
                dx = track->lfodat;
            else
                dx = 0;

            if (track->lfoswi & 0x10)
                dx += track->_lfodat;

            if (dx)
            {
                dx = (ax * dx) >> 12;

                if (dx >= 0)
                    dx++;
                else
                    dx--;
            }

            ax -= dx;
        }
    }

    // TONE SET

    if (ax >= 0x1000)
    {
        if (ax >= 0)
            ax = 0xfff;
        else
            ax = 0;
    }

    _OPNAW->SetReg((uint32_t) ((_DriverState.CurrentChannel - 1) * 2),     (uint32_t) LOBYTE(ax));
    _OPNAW->SetReg((uint32_t) ((_DriverState.CurrentChannel - 1) * 2 + 1), (uint32_t) HIBYTE(ax));
}

//  ＰＳＧ　ＫＥＹＯＮ
void PMD::keyonp(Track * qq)
{
    if (qq->onkai == 255)
        return;    // ｷｭｳﾌ ﾉ ﾄｷ

    int ah = (1 << (_DriverState.CurrentChannel - 1)) | (1 << (_DriverState.CurrentChannel + 2));
    int al = ((int32_t) _OPNAW->GetReg(0x07) | ah);

    ah = ~(ah & qq->psgpat);
    al &= ah;

    _OPNAW->SetReg(7, (uint32_t) al);

    // SSG ﾉｲｽﾞ ｼｭｳﾊｽｳ ﾉ ｾｯﾄ

    if (_State.SSGNoiseFrequency != _State.SSGNoiseFrequencyLast && _EffectState.effon == 0)
    {
        _OPNAW->SetReg(6, (uint32_t) _State.SSGNoiseFrequency);
        _State.SSGNoiseFrequencyLast = _State.SSGNoiseFrequency;
    }
}

//  ＬＦＯ処理
//    Don't Break cl
//    output    cy=1  変化があった
int PMD::lfo(Track * track)
{
    return lfop(track);
}

int PMD::lfop(Track * track)
{
    if (track->delay)
    {
        track->delay--;

        return 0;
    }

    int ax, ch;

    if (track->extendmode & 2)
    {
        // Match with TimerA? If not, unconditionally process lfo
        ch = _State.TimerATime - _DriverState.OldTimerATime;

        if (ch == 0)
            return 0;

        ax = track->lfodat;

        for (; ch > 0; ch--)
            lfo_main(track);
    }
    else
    {
        ax = track->lfodat;

        lfo_main(track);
    }

    return (ax == track->lfodat) ? 0 : 1;
}

void PMD::lfo_main(Track * track)
{
    if (track->speed != 1)
    {
        if (track->speed != 255)
            track->speed--;

        return;
    }

    track->speed = track->speed2;

    int al, ax;

    if (track->lfo_wave == 0 || track->lfo_wave == 4 || track->lfo_wave == 5)
    {
        // Triangle wave lwowave = 0,4,5
        if (track->lfo_wave == 5)
            ax = abs(track->step) * track->step;
        else
            ax = track->step;

        if ((track->lfodat += ax) == 0)
            md_inc(track);

        al = track->time;

        if (al != 255)
        {
            if (--al == 0)
            {
                al = track->time2;

                if (track->lfo_wave != 4)
                    al += al;  // lfowave=0,5の場合 timeを反転時２倍にする

                track->time = al;
                track->step = -track->step;

                return;
            }
        }

        track->time = al;
    }
    else
    if (track->lfo_wave == 2)
    {
        //  矩形波    lfowave = 2
        track->lfodat = (track->step * track->time);

        md_inc(track);

        track->step = -track->step;

    }
    else
    if (track->lfo_wave == 6)
    {
        //  ワンショット  lfowave = 6
        if (track->time)
        {
            if (track->time != 255)
            {
                track->time--;
            }
            track->lfodat += track->step;
        }
    }
    else
    if (track->lfo_wave == 1)
    {
        //ノコギリ波  lfowave = 1
        track->lfodat += track->step;
        al = track->time;
        if (al != -1)
        {
            al--;
            if (al == 0)
            {
                track->lfodat = -track->lfodat;
                md_inc(track);
                al = (track->time2) * 2;
            }
        }
        track->time = al;

    }
    else
    {
        //  ランダム波  lfowave = 3
        ax = abs(track->step) * track->time;
        track->lfodat = ax - rnd(ax * 2);
        md_inc(track);
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

// Change STEP value by value of MD command
void PMD::md_inc(Track * track)
{
    if (--track->mdspd)
        return;

    track->mdspd = track->mdspd2;

    if (track->mdc == 0)
        return;    // count = 0

    if (track->mdc <= 127)
        track->mdc--;

    int al;

    if (track->step < 0)
    {
        al = track->mdepth - track->step;

        if (al < 128)
            track->step = -al;
        else
            track->step = (track->mdepth < 0) ? 0 : -127;
    }
    else
    {
        al = track->step + track->mdepth;

        if (al < 128)
            track->step = al;
        else
            track->step = (track->mdepth < 0) ? 0 : 127;
    }
}

/// <summary>
/// Swap between LFO 1 and LFO 2.
/// </summary>
void PMD::SwapLFO(Track * track)
{
    Swap(&track->lfodat, &track->_lfodat);

    track->lfoswi = ((track->lfoswi & 0x0f) << 4) + (track->lfoswi >> 4);
    track->extendmode = ((track->extendmode & 0x0f) << 4) + (track->extendmode >> 4);

    Swap(&track->delay, &track->_delay);
    Swap(&track->speed, &track->_speed);
    Swap(&track->step, &track->_step);
    Swap(&track->time, &track->_time);
    Swap(&track->delay2, &track->_delay2);
    Swap(&track->speed2, &track->_speed2);
    Swap(&track->step2, &track->_step2);
    Swap(&track->time2, &track->_time2);
    Swap(&track->mdepth, &track->_mdepth);
    Swap(&track->mdspd, &track->_mdspd);
    Swap(&track->mdspd2, &track->_mdspd2);
    Swap(&track->lfo_wave, &track->_lfo_wave);
    Swap(&track->mdc, &track->_mdc);
    Swap(&track->mdc2, &track->_mdc2);
}

// Portamento calculation
void PMD::porta_calc(Track * track)
{
    track->porta_num += track->porta_num2;

    if (track->porta_num3 == 0)
        return;

    if (track->porta_num3 > 0)
    {
        track->porta_num3--;
        track->porta_num++;
    }
    else
    {
        track->porta_num3++;
        track->porta_num--;
    }
}

// SSG/PCM Software Envelope
int PMD::soft_env(Track * track)
{
    if (track->extendmode & 4)
    {
        if (_State.TimerATime == _DriverState.OldTimerATime) return 0;

        int cl = 0;

        for (int i = 0; i < _State.TimerATime - _DriverState.OldTimerATime; ++i)
        {
            if (soft_env_main(track))
                cl = 1;
        }

        return cl;
    }
    else
        return soft_env_main(track);
}

int PMD::soft_env_main(Track * track)
{
    if (track->envf == -1)
        return ext_ssgenv_main(track);

    int dl = track->eenv_volume;

    soft_env_sub(track);

    if (dl == track->eenv_volume)
        return 0;

    return -1;
}

int PMD::soft_env_sub(Track * track)
{
    if (track->envf == 0)
    {
        // Attack
        if (--track->eenv_ar != 0)
            return 0;

        track->envf = 1;
        track->eenv_volume = track->eenv_dr;

        return 1;
    }

    if (track->envf != 2)
    {
        // Decay
        if (track->eenv_sr == 0) return 0;  // No attenuation when DR=0
        if (--track->eenv_sr != 0) return 0;

        track->eenv_sr = track->eenv_src;
        track->eenv_volume--;

        if (track->eenv_volume >= -15 || track->eenv_volume < 15)
            return 0;

        track->eenv_volume = -15;

        return 0;
    }

    // Release
    if (track->eenv_rr == 0)
    {
        track->eenv_volume = -15; // When RR = 0, immediately mute
        return 0;
    }

    if (--track->eenv_rr != 0)
        return 0;

    track->eenv_rr = track->eenv_rrc;
    track->eenv_volume--;

    if (track->eenv_volume >= -15 && track->eenv_volume < 15)
        return 0;

    track->eenv_volume = -15;

    return 0;
}

// Extended version
int PMD::ext_ssgenv_main(Track * track)
{
    if (track->eenv_count == 0)
        return 0;

    int dl = track->eenv_volume;

    esm_sub(track, track->eenv_count);

    if (dl == track->eenv_volume)
        return 0;

    return -1;
}

void PMD::esm_sub(Track * track, int ah)
{
    if (--ah == 0)
    {
        // Attack Rate
        if (track->eenv_arc > 0)
        {
            track->eenv_volume += track->eenv_arc;

            if (track->eenv_volume < 15)
            {
                track->eenv_arc = track->eenv_ar - 16;
                return;
            }

            track->eenv_volume = 15;
            track->eenv_count++;

            if (track->eenv_sl != 15)
                return;    // If SL=0, immediately go to SR

            track->eenv_count++;

            return;
        }
        else
        {
            if (track->eenv_ar == 0)
                return;

            track->eenv_arc++;

            return;
        }
    }

    if (--ah == 0)
    {
        // Decay Rate
        if (track->eenv_drc > 0)
        {
            track->eenv_volume -= track->eenv_drc; // Count CHECK if less than 0

            if (track->eenv_volume < 0 || track->eenv_volume < track->eenv_sl)
            {
                track->eenv_volume = track->eenv_sl;
                track->eenv_count++;

                return;
            }

            if (track->eenv_dr < 16)
                track->eenv_drc = (track->eenv_dr - 16) * 2;
            else
                track->eenv_drc = track->eenv_dr - 16;

            return;
        }

        if (track->eenv_dr == 0)
            return;

        track->eenv_drc++;

        return;
    }

    if (--ah == 0)
    {
        // Sustain Rate
        if (track->eenv_src > 0)
        {
            // Count CHECK if less than 0
            if ((track->eenv_volume -= track->eenv_src) < 0)
                track->eenv_volume = 0;

            if (track->eenv_sr < 16)
                track->eenv_src = (track->eenv_sr - 16) * 2;
            else
                track->eenv_src = track->eenv_sr - 16;

            return;
        }

        if (track->eenv_sr == 0)
            return;  // SR=0?

        track->eenv_src++;

        return;
    }

    // Release Rate
    if (track->eenv_rrc > 0)
    {
        // Count CHECK if less than 0
        if ((track->eenv_volume -= track->eenv_rrc) < 0)
            track->eenv_volume = 0;

        track->eenv_rrc = (track->eenv_rr) * 2 - 16;

        return;
    }

    if (track->eenv_rr == 0)
        return;

    track->eenv_rrc++;
}

// Tempo setting
void PMD::SetTimerBTempo()
{
    if (_State.TimerBTempo != _State.tempo_d)
    {
        _State.TimerBTempo = _State.tempo_d;

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
/// Starts the OPN interrupt.
/// </summary>
void PMD::StartOPNInterrupt()
{
    ::memset(_FMTrack, 0, sizeof(_FMTrack));
    ::memset(_SSGTrack, 0, sizeof(_SSGTrack));
    ::memset(&_ADPCMTrack, 0, sizeof(_ADPCMTrack));
    ::memset(&_RhythmTrack, 0, sizeof(_RhythmTrack));
    ::memset(&_DummyTrack, 0, sizeof(_DummyTrack));
    ::memset(_ExtensionTrack, 0, sizeof(_ExtensionTrack));
    ::memset(&_EffectTrack, 0, sizeof(_EffectTrack));
    ::memset(_PPZ8Track, 0, sizeof(_PPZ8Track));

    _State.RhythmMask = 255;
    _DriverState.DummyRhythmData = 255;

    InitializeState();
    InitializeOPN();

    _OPNAW->SetReg(0x07, 0xbf);

    DriverStop();
    InitializeInterrupt();

    _OPNAW->SetReg(0x29, 0x83);
}

void PMD::InitializeOPN()
{
    _OPNAW->ClearBuffer();
    _OPNAW->SetReg(0x29, 0x83);

    _State.SSGNoiseFrequency = 0;

    _OPNAW->SetReg(0x06, 0x00);

    _State.SSGNoiseFrequencyLast = 0;

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

    // PAN/HARDLFO DEFAULT
    _OPNAW->SetReg(0x0b4, 0xc0);
    _OPNAW->SetReg(0x0b5, 0xc0);
    _OPNAW->SetReg(0x0b6, 0xc0);
    _OPNAW->SetReg(0x1b4, 0xc0);
    _OPNAW->SetReg(0x1b5, 0xc0);
    _OPNAW->SetReg(0x1b6, 0xc0);

    _State.port22h = 0x00;

    _OPNAW->SetReg(0x22, 0x00);

    //  Rhythm Default = Pan : Mid , Vol : 15
    for (int i = 0; i < 6; ++i)
        _State.rdat[i] = 0xcf;

    _OPNAW->SetReg(0x10, 0xff);

    // Rhythm total level set
    _State.RhythmVolume = 48 * 4 * (256 - _State.rhythm_voldown) / 1024;

    _OPNAW->SetReg(0x11, (uint32_t) _State.RhythmVolume);

    // PCM reset & LIMIT SET
    _OPNAW->SetReg(0x10c, 0xff);
    _OPNAW->SetReg(0x10d, 0xff);

    for (int i = 0; i < MaxPPZChannels; ++i)
        _PPZ8->SetPan(i, 5);
}

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
    _P86->SetPan(3, 0);

    // 2003.11.30 For small noise measures
//@  if(effwork.effon == 0) {
    _OPNAW->SetReg(0x07, 0xBF);
    _OPNAW->SetReg(0x08, 0x00);
    _OPNAW->SetReg(0x09, 0x00);
    _OPNAW->SetReg(0x0a, 0x00);
//@  } else {
//@ opna->SetReg(0x07, (opna->GetReg(0x07) & 0x3f) | 0x9b);
//@  }

    _OPNAW->SetReg(0x10, 0xff);   // Rhythm dump

    _OPNAW->SetReg(0x101, 0x02);  // PAN=0 / x8 bit mode
    _OPNAW->SetReg(0x100, 0x01);  // PCM RESET
    _OPNAW->SetReg(0x110, 0x80);  // TA/TB/EOS を RESET
    _OPNAW->SetReg(0x110, 0x18);  // Bit change only for TIMERB/A/EOS

    for (int i = 0; i < MaxPPZChannels; ++i)
        _PPZ8->Stop(i);
}

/// <summary>
/// Starts the driver.
/// </summary>
void PMD::Start()
{
    if (_State.IsTimerABusy || _State.IsTimerBBusy)
    {
        _DriverState.music_flag |= 0x01; // Not executed during TA/TB processing

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
        _DriverState.music_flag |= 0x02;
    }
    else
    {
        _State.fadeout_flag = 0;
        DriverStop();
    }

    ::memset(_SampleSrc, 0, sizeof(_SampleSrc));
    _Position = 0;
}

void PMD::DriverStart()
{
    // Set Timer B = 0 and Timer Reset (to match the length of the song every time)
    _State.tempo_d = 0;

    SetTimerBTempo();

    _OPNAW->SetReg(0x27, 0x00); // Timer reset (both timer A and B)

    _DriverState.music_flag &= 0xFE;

    DriverStop();

    _SamplePtr = _SampleSrc;
    _SamplesToDo = 0;
    _Position = 0;

    InitializeState();
    InitializeTracks();

    InitializeOPN();
    InitializeInterrupt();

    _State.IsPlaying = true;
}

void PMD::DriverStop()
{
    _DriverState.music_flag &= 0xFD;

    _State.IsPlaying = false;
    _State.LoopCount = -1;
    _State.FadeOutSpeed = 0;
    _State.FadeOutVolume = 0xFF;

    Silence();
}

/// <summary>
/// Initializes the different state machines.
/// </summary>
void PMD::InitializeState()
{
    _State.FadeOutVolume = 0;
    _State.FadeOutSpeed = 0;
    _State.fadeout_flag = 0;
    _State.FadeOutSpeedHQ = 0;

    _State.status = 0;
    _State.LoopCount = 0;
    _State.BarCounter = 0;
    _State.OpsCounter = 0;
    _State.TimerATime = 0;

    _State.PCMStart = 0;
    _State.PCMStop = 0;

    _State.kshot_dat = 0;
    _State.rshot_dat = 0;

    _State.slot_detune1 = 0;
    _State.slot_detune2 = 0;
    _State.slot_detune3 = 0;
    _State.slot_detune4 = 0;

    _State.ch3mode = 0x3F;
    _State.BarLength = 96;

    _State.fm_voldown = _State._fm_voldown;
    _State.ssg_voldown = _State._ssg_voldown;
    _State.pcm_voldown = _State._pcm_voldown;
    _State.ppz_voldown = _State._ppz_voldown;
    _State.rhythm_voldown = _State._rhythm_voldown;
    _State.pcm86_vol = _State._pcm86_vol;

    for (int i = 0; i < 6; ++i)
    {
        int PartMask = _FMTrack[i].PartMask;
        int keyon_flag = _FMTrack[i].keyon_flag;

        ::memset(&_FMTrack[i], 0, sizeof(Track));

        _FMTrack[i].PartMask = PartMask & 0x0f;
        _FMTrack[i].keyon_flag = keyon_flag;
        _FMTrack[i].onkai = 255;
        _FMTrack[i].onkai_def = 255;
    }

    for (int i = 0; i < 3; ++i)
    {
        int PartMask = _SSGTrack[i].PartMask;
        int keyon_flag = _SSGTrack[i].keyon_flag;

        ::memset(&_SSGTrack[i], 0, sizeof(Track));

        _SSGTrack[i].PartMask = PartMask & 0x0f;
        _SSGTrack[i].keyon_flag = keyon_flag;
        _SSGTrack[i].onkai = 255;
        _SSGTrack[i].onkai_def = 255;
    }

    {
        int partmask = _ADPCMTrack.PartMask;
        int keyon_flag = _ADPCMTrack.keyon_flag;

        ::memset(&_ADPCMTrack, 0, sizeof(Track));

        _ADPCMTrack.PartMask = partmask & 0x0f;
        _ADPCMTrack.keyon_flag = keyon_flag;
        _ADPCMTrack.onkai = 255;
        _ADPCMTrack.onkai_def = 255;
    }

    {
        int partmask = _RhythmTrack.PartMask;
        int keyon_flag = _RhythmTrack.keyon_flag;

        ::memset(&_RhythmTrack, 0, sizeof(Track));

        _RhythmTrack.PartMask = partmask & 0x0f;
        _RhythmTrack.keyon_flag = keyon_flag;
        _RhythmTrack.onkai = 255;
        _RhythmTrack.onkai_def = 255;
    }

    for (int i = 0; i < 3; ++i)
    {
        int partmask = _ExtensionTrack[i].PartMask;
        int keyon_flag = _ExtensionTrack[i].keyon_flag;

        ::memset(&_ExtensionTrack[i], 0, sizeof(Track));

        _ExtensionTrack[i].PartMask = partmask & 0x0f;
        _ExtensionTrack[i].keyon_flag = keyon_flag;
        _ExtensionTrack[i].onkai = 255;
        _ExtensionTrack[i].onkai_def = 255;
    }

    for (int i = 0; i < 8; ++i)
    {
        int partmask = _PPZ8Track[i].PartMask;
        int keyon_flag = _PPZ8Track[i].keyon_flag;

        ::memset(&_PPZ8Track[i], 0, sizeof(Track));

        _PPZ8Track[i].PartMask = partmask & 0x0f;
        _PPZ8Track[i].keyon_flag = keyon_flag;
        _PPZ8Track[i].onkai = 255;
        _PPZ8Track[i].onkai_def = 255;
    }

    _DriverState.tieflag = 0;
    _DriverState.OldTimerATime = 0;

    _DriverState.omote_key[0] = 0;
    _DriverState.omote_key[1] = 0;
    _DriverState.omote_key[2] = 0;

    _DriverState.ura_key[0] = 0;
    _DriverState.ura_key[1] = 0;
    _DriverState.ura_key[2] = 0;

    _DriverState.fm3_alg_fb = 0;
    _DriverState.af_check = 0;

    _DriverState.PCMRepeat1 = 0;
    _DriverState.PCMRepeat2 = 0;
    _DriverState.PCMRelease = 0x8000;

    _DriverState.slotdetune_flag = 0;
    _DriverState.slot3_flag = 0;
    _DriverState.fmsel = 0;

    _EffectState.last_shot_data = 0;
}

/// <summary>
/// Sets the start address and initial value of each track.
/// </summary>
void PMD::InitializeTracks()
{
    _State.x68_flg = _State.MData[-1];

    {
        const size_t Offset = 2 * (max_part2 + 1);

        if (_State.MData[0] != Offset)
        {
            _State.ToneData = _State.MData + *(uint16_t *) (&_State.MData[Offset]);
            _State.HasToneData = true;
        }
        else
            _State.HasToneData = false;
    }

    uint16_t * p = (uint16_t *) _State.MData;

    for (size_t i = 0; i < _countof(_FMTrack); ++i)
    {
        if (_State.MData[*p] == 0x80) // Do not play.
            _FMTrack[i].Data = nullptr;
        else
            _FMTrack[i].Data = &_State.MData[*p];

        _FMTrack[i].Length = 1;
        _FMTrack[i].keyoff_flag = -1;    // 現在KeyOff中
        _FMTrack[i].mdc = -1;        // MDepth Counter (無限)
        _FMTrack[i].mdc2 = -1;      // 同上
        _FMTrack[i]._mdc = -1;      // 同上
        _FMTrack[i]._mdc2 = -1;      // 同上
        _FMTrack[i].onkai = 255;      // rest
        _FMTrack[i].onkai_def = 255;    // rest
        _FMTrack[i].volume = 108;      // FM  VOLUME DEFAULT= 108
        _FMTrack[i].fmpan = 0xc0;      // FM PAN = Middle
        _FMTrack[i].SlotMask = 0xf0;    // FM SLOT MASK
        _FMTrack[i].ToneMask = 0xff;    // FM Neiro MASK

        p++;
    }

    for (size_t i = 0; i < _countof(_SSGTrack); ++i)
    {
        if (_State.MData[*p] == 0x80) // Do not play.
            _SSGTrack[i].Data = nullptr;
        else
            _SSGTrack[i].Data = &_State.MData[*p];

        _SSGTrack[i].Length = 1;
        _SSGTrack[i].keyoff_flag = -1;  // 現在KeyOff中
        _SSGTrack[i].mdc = -1;      // MDepth Counter (無限)
        _SSGTrack[i].mdc2 = -1;      // 同上
        _SSGTrack[i]._mdc = -1;      // 同上
        _SSGTrack[i]._mdc2 = -1;      // 同上
        _SSGTrack[i].onkai = 255;      // rest
        _SSGTrack[i].onkai_def = 255;    // rest
        _SSGTrack[i].volume = 8;      // SSG VOLUME DEFAULT= 8
        _SSGTrack[i].psgpat = 7;      // SSG = TONE
        _SSGTrack[i].envf = 3;      // SSG ENV = NONE/normal

        p++;
    }

    if (_State.MData[*p] == 0x80) // Do not play
        _ADPCMTrack.Data = nullptr;
    else
        _ADPCMTrack.Data = &_State.MData[*p];

    _ADPCMTrack.Length = 1;
    _ADPCMTrack.keyoff_flag = -1;    // 現在KeyOff中
    _ADPCMTrack.mdc = -1;        // MDepth Counter (無限)
    _ADPCMTrack.mdc2 = -1;      // 同上
    _ADPCMTrack._mdc = -1;      // 同上
    _ADPCMTrack._mdc2 = -1;      // 同上
    _ADPCMTrack.onkai = 255;      // rest
    _ADPCMTrack.onkai_def = 255;    // rest
    _ADPCMTrack.volume = 128;      // PCM VOLUME DEFAULT= 128
    _ADPCMTrack.fmpan = 0xc0;      // PCM PAN = Middle
    p++;

    if (_State.MData[*p] == 0x80) // Do not play
        _RhythmTrack.Data = nullptr;
    else
        _RhythmTrack.Data = &_State.MData[*p];

    _RhythmTrack.Length = 1;
    _RhythmTrack.keyoff_flag = -1;  // 現在KeyOff中
    _RhythmTrack.mdc = -1;      // MDepth Counter (無限)
    _RhythmTrack.mdc2 = -1;      // 同上
    _RhythmTrack._mdc = -1;      // 同上
    _RhythmTrack._mdc2 = -1;      // 同上
    _RhythmTrack.onkai = 255;      // rest
    _RhythmTrack.onkai_def = 255;    // rest
    _RhythmTrack.volume = 15;      // PPSDRV volume
    p++;

    _State.RhythmDataTable = (uint16_t *) &_State.MData[*p];

    _State.RhythmData = (uint8_t *) &_DriverState.DummyRhythmData;
}

//  Interrupt settings. FM tone generator only
void PMD::InitializeInterrupt()
{
    // OPN interrupt initial setting
    _State.tempo_d = 200;
    _State.tempo_d_push = 200;

    calc_tb_tempo();
    SetTimerBTempo();

    _OPNAW->SetReg(0x25, 0x00); // Timer A Set (9216μs fixed)
    _OPNAW->SetReg(0x24, 0x00); // The slowest and just right
    _OPNAW->SetReg(0x27, 0x3f); // Timer Enable

    //　Measure counter reset
    _State.OpsCounter = 0;
    _State.BarCounter = 0;
    _State.BarLength = 96;
}

/// <summary>
/// Tempo conversion (input: tempo_d, output: tempo_48)
/// </summary>
void PMD::calc_tb_tempo()
{
    int al;

    if (256 - _State.tempo_d == 0)
    {
        al = 255;
    }
    else
    {
        al = (0x112c * 2 / (256 - _State.tempo_d) + 1) / 2; // Tempo = 0x112C / [ 256 - TB ]  timerB -> tempo

        if (al > 255)
            al = 255;
    }

    _State.tempo_48 = al;
    _State.tempo_48_push = al;
}

/// <summary>
/// Tempo conversion (input: tempo_48, output: tempo_d)
/// </summary>
void PMD::calc_tempo_tb()
{
    int al;

    if (_State.tempo_48 >= 18)
    {
        al = 256 - 0x112c / _State.tempo_48; // TB = 256 - [ 112CH / TEMPO ]  tempo -> timerB

        if (0x112c % _State.tempo_48 >= 128)
            al--;

    //  al = 256 - (0x112c * 2 / _State.tempo_48 + 1) / 2;
    }
    else
        al = 0;

    _State.tempo_d = al;
    _State.tempo_d_push = al;
}

/// <summary>
/// Loads the PPC file.
/// </summary>
int PMD::LoadPPCInternal(const WCHAR * filePath)
{
    if (*filePath == '\0')
        return ERR_OPEN_FAILED;

    if (!_File->Open(filePath))
        return ERR_OPEN_FAILED;

    int64_t Size = _File->GetFileSize(filePath);

    if (Size < 0)
        return ERR_OPEN_FAILED;

    uint8_t * pcmbuf = (uint8_t *) ::malloc((size_t) Size);

    if (pcmbuf == NULL)
        return ERR_OUT_OF_MEMORY;

    _File->Read(pcmbuf, (uint32_t) Size);
    _File->Close();

    int Result = LoadPPCInternal(pcmbuf, (int) Size);

    ::wcscpy(_State.PPCFileName, filePath);

    free(pcmbuf);

    return Result;
}

/// <summary>
/// Loads the PPC data.
/// </summary>
int PMD::LoadPPCInternal(uint8_t * pcmdata, int size)
{
    if (size < 0x10)
    {
        _State.PPCFileName[0] = '\0';

        return ERR_UNKNOWN_FORMAT;
    }

    bool FoundPVI;

    int i;
    uint16_t * pcmdata2;
    int bx = 0;

    if ((::strncmp((char *) pcmdata, PVIHeader, sizeof(PVIHeader) - 1) == 0) && (pcmdata[10] == 2))
    {   // PVI, x8
        FoundPVI = true;

        // Transfer from pvi tone information to pmd
        for (i = 0; i < 128; ++i)
        {
            if (*((uint16_t *) &pcmdata[18 + i * 4]) == 0)
            {
                pcmends.Address[i][0] = pcmdata[16 + i * 4];
                pcmends.Address[i][1] = 0;
            }
            else
            {
                pcmends.Address[i][0] = (uint16_t) (*(uint16_t *) &pcmdata[16 + i * 4] + 0x26);
                pcmends.Address[i][1] = (uint16_t) (*(uint16_t *) &pcmdata[18 + i * 4] + 0x26);
            }

            if (bx < pcmends.Address[i][1])
                bx = pcmends.Address[i][1] + 1;
        }

        // The remaining 128 are undefined
        for (i = 128; i < 256; ++i)
        { 
            pcmends.Address[i][0] = 0;
            pcmends.Address[i][1] = 0;
        }

        pcmends.Count = (uint16_t) bx;
    }
    else
    if (::strncmp((char *) pcmdata, PPCHeader, sizeof(PPCHeader) - 1) == 0)
    {   // PPC
        FoundPVI = false;

        pcmdata2 = (uint16_t *) pcmdata + 30 / 2;

        if (size < 30 + 4 * 256 + 2)
        {
            _State.PPCFileName[0] = '\0';

            return ERR_UNKNOWN_FORMAT;
        }

        pcmends.Count = *pcmdata2++;

        for (i = 0; i < 256; ++i)
        {
            pcmends.Address[i][0] = *pcmdata2++;
            pcmends.Address[i][1] = *pcmdata2++;
        }
    }
    else
    {
        _State.PPCFileName[0] = '\0';

        return ERR_UNKNOWN_FORMAT;
    }

    uint8_t tempbuf[0x26 * 32];

    // Compare PMD work and PCMRAM header
    ReadPCMData(0, 0x25, tempbuf);

    // Skip the "ADPCM?" header
    // Ignore file name (PMDWin specification)
    if (::memcmp(&tempbuf[30], &pcmends, sizeof(pcmends)) == 0)
        return ERR_ALREADY_LOADED;

    uint8_t tempbuf2[30 + 4 * 256 + 128 + 2];

    // Write PMD work to PCMRAM head
    ::memcpy(tempbuf2, PPCHeader, sizeof(PPCHeader) - 1);
    ::memcpy(&tempbuf2[sizeof(PPCHeader) - 1], &pcmends.Count, sizeof(tempbuf2) - (sizeof(PPCHeader) - 1));

    WritePCMData(0, 0x25, tempbuf2);

    // Write PCMDATA to PCMRAM
    if (FoundPVI)
    {
        pcmdata2 = (uint16_t *) (pcmdata + 0x10 + sizeof(uint16_t) * 2 * 128);

        if (size < (int) (pcmends.Count - (0x10 + sizeof(uint16_t) * 2 * 128)) * 32)
        {
            _State.PPCFileName[0] = '\0';

            return ERR_UNKNOWN_FORMAT;
        }
    }
    else
    {
        pcmdata2 = (uint16_t *) pcmdata + (30 + 4 * 256 + 2) / 2;

        if (size < (pcmends.Count - ((30 + 4 * 256 + 2) / 2)) * 32)
        {
            _State.PPCFileName[0] = '\0';

            return ERR_UNKNOWN_FORMAT;
        }
    }

    uint16_t pcmstart = 0x26;
    uint16_t pcmstop = pcmends.Count;

    WritePCMData(pcmstart, pcmstop, (uint8_t *) pcmdata2);

    return ERR_SUCCESS;
}

// Read data from PCM memory to main memory.
void PMD::ReadPCMData(uint16_t startAddress, uint16_t stopAddress, uint8_t * pcmData)
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

    *pcmData = (uint8_t) _OPNAW->GetReg(0x108);
    *pcmData = (uint8_t) _OPNAW->GetReg(0x108);

    for (int i = 0; i < (stopAddress - startAddress) * 32; ++i)
    {
        *pcmData++ = (uint8_t) _OPNAW->GetReg(0x108);

        _OPNAW->SetReg(0x110, 0x80);
    }
}

// Send data from main memory to PCM memory (x8, high speed version)
void PMD::WritePCMData(uint16_t startAddress, uint16_t stopAddress, const uint8_t * pcmData)
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
        _OPNAW->SetReg(0x108, *pcmData++);
}

/// <summary>
/// Finds a PCM sample in the specified search path.
/// </summary>
WCHAR * PMD::FindFile(WCHAR * filePath, const WCHAR * filename)
{
    WCHAR FilePath[MAX_PATH];

    for (size_t i = 0; i < _State.SearchPath.size(); ++i)
    {
        CombinePath(FilePath, _countof(FilePath), _State.SearchPath[i].c_str(), filename);

        if (_File->GetFileSize(FilePath) > 0)
        {
            ::wcscpy(filePath, FilePath);

            return filePath;
        }
    }

    return nullptr;
}

//  fm effect
uint8_t * PMD::fm_efct_set(Track *, uint8_t * si)
{
    return si + 1;
}

uint8_t * PMD::ssg_efct_set(Track * qq, uint8_t * si)
{
    int al = *si++;

    if (qq->PartMask)
        return si;

    if (al)
        eff_on2(qq, al);
    else
        effend();

    return si;
}

// Fade In / Out
void PMD::Fade()
{
    if (_State.FadeOutSpeed == 0)
        return;

    if (_State.FadeOutSpeed > 0)
    {
        if (_State.FadeOutSpeed + _State.FadeOutVolume < 256)
        {
            _State.FadeOutVolume += _State.FadeOutSpeed;
        }
        else
        {
            _State.FadeOutVolume = 255;
            _State.FadeOutSpeed  =   0;

            if (_State.fade_stop_flag == 1)
                _DriverState.music_flag |= 0x02;
        }
    }
    else
    {   // Fade in
        if (_State.FadeOutSpeed + _State.FadeOutVolume > 255)
        {
            _State.FadeOutVolume += _State.FadeOutSpeed;
        }
        else
        {
            _State.FadeOutVolume = 0;
            _State.FadeOutSpeed = 0;

            _OPNAW->SetReg(0x11, (uint32_t) _State.RhythmVolume);
        }
    }
}
