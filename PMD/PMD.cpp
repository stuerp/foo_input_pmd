
// PMD driver (Based on PMDWin code by C60)

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

    if (_OPNAW->Initialize(OPNAClock, SOUND_44K, false, DirectoryPath) == false)
        return false;

    _OPNAW->SetFMDelay(0);
    _OPNAW->SetSSGDelay(0);
    _OPNAW->SetRhythmDelay(0);
    _OPNAW->SetADPCMDelay(0);

    // Initialize the ADPCM RAM.
    {
        uint8_t Page[0x400]; // 0x400 * 0x100 = 0x40000(256K)

        ::memset(Page, 0x08, sizeof(Page));

        for (int i = 0; i < 0x100; ++i)
            WritePCMData((uint16_t) i * sizeof(Page) / 32, (uint16_t) (i + 1) * sizeof(Page) / 32, Page);
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

    {
        pcmends.Count = 0x26;

        for (int i = 0; i < 256; ++i)
        {
            pcmends.Address[i][0] = 0;
            pcmends.Address[i][1] = 0;
        }
    }

    _State.PPCFileName[0] = '\0';

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
    _State.PPZRate = SOUND_44K;

    _State.fade_stop_flag = 0;
    _State.IsTimerBBusy = false;

    _State.IsTimerABusy = false;
    _State.TimerBTempo = 0x100;
    _State.port22h = 0;

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

    SetFMVolumeDown(fmvd_init);
    SetSSGVolumeDown(0);
    SetADPCMVolumeDown(0);
    _State.RhythmVolumeDown = 0;       // RHYTHM_VOLDOWN
    _State.DefaultRhythmVolumeDown = 0;      // RHYTHM_VOLDOWN
    SetPPZVolumeDown(0);

    _State.RhythmVolume = 0x3C;
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

    SetPMDB2CompatibilityMode(false);

    _State.fade_stop_flag = 1;       // MSTOP after FADEOUT FLAG

    _Driver.UsePPS = false;
    _Driver.music_flag = 0;

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

                Result = _PPZ->Load(FilePath, 0);
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

                    Result = _PPZ->Load(FilePath, 0);
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

                    Result = _PPZ->Load(FilePath, 0);

                    FindFile(FilePath, PPZFileName2);

                    Result = _PPZ->Load(FilePath, 1);
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
    _OPNAW->SetRhythmDelay(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)

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
            _OPNAW->SetRhythmDelay(RSSDelay);

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
    _OPNAW->SetRhythmDelay(RSSDelay);

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
    _OPNAW->SetRhythmDelay(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30);  // Timer Reset (Both timer A and B)

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
            _OPNAW->SetRhythmDelay(RSSDelay);

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
    _OPNAW->SetRhythmDelay(RSSDelay);

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

        _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)

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

                _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)
            }

            uint32_t us = _OPNAW->GetNextEvent(); // in microseconds

            {
                _SamplesToDo = (size_t) ((double) us * _State.OPNARate / 1000000.0);
                _OPNAW->Count(us);

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
                        _Driver.music_flag |= 2;
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
        _State.PPZRate = SOUND_44K;
        _State.UseFM55kHzSynthesis = true;
    }
    else
    {
        _State.OPNARate =
        _State.PPZRate = frequency;
        _State.UseFM55kHzSynthesis = false;
    }

    _OPNAW->SetRate(OPNAClock, _State.OPNARate, _State.UseFM55kHzSynthesis);

    _PPZ->SetRate(_State.PPZRate, _State.UseInterpolationPPZ);
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
    _State.PPZRate = frequency;

    _PPZ->SetRate(frequency, _State.UseInterpolationPPZ);
}

/// <summary>
/// Enables or disables PPZ interpolation.
/// </summary>
void PMD::SetPPZInterpolation(bool flag)
{
    _State.UseInterpolationPPZ = flag;

    _PPZ->SetRate(_State.PPZRate, flag);
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

        _OPNAW->SetReg(0x27, _State.FMChannel3Mode | 0x30); // Timer Reset (Both timer A and B)

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
    ::wcscpy(filePath, _PPZ->_FilePath[index]);

    return filePath;
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
void PMD::UseRhythm(bool flag) noexcept
{
    _State.UseRhythm = flag;
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

    if ((_State.Channel[channel]->MuteMask == 0x00) && IsPlaying())
    {
        if (ChannelTable[channel][2] == 0)
        {
            _Driver.CurrentChannel = ChannelTable[channel][1];
            _Driver.FMSelector = 0;

            MuteFMChannel(_State.Channel[channel]);
        }
        else
        if (ChannelTable[channel][2] == 1)
        {
            _Driver.CurrentChannel = ChannelTable[channel][1];
            _Driver.FMSelector = 0x100;

            MuteFMChannel(_State.Channel[channel]);
        }
        else
        if (ChannelTable[channel][2] == 2)
        {
            _Driver.CurrentChannel = ChannelTable[channel][1];

            int ah = 1 << (_Driver.CurrentChannel - 1);

            ah |= (ah << 3);

            // SSG SetFMKeyOff
            _OPNAW->SetReg(0x07, ah | _OPNAW->GetReg(0x07));
        }
        else
        if (ChannelTable[channel][2] == 3)
        {
            _OPNAW->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
            _OPNAW->SetReg(0x100, 0x01);    // PCM RESET
        }
        else
        if (ChannelTable[channel][2] == 4)
        {
            if (_Effect.Number < 11)
                StopEffect();
        }
        else
        if (ChannelTable[channel][2] == 5)
            _PPZ->Stop(ChannelTable[channel][1]);
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

// Gets a note.
bool PMD::GetMemo(const uint8_t * data, size_t size, int index, char * text, size_t textSize)
{
    if ((text == nullptr) || (textSize < 1))
        return false;

    text[0] = '\0';

    char Zen2[1024 + 64];

    GetText(data, size, index, Zen2);

    char Han[1024 + 64];

    ZenToHan(Han, Zen2);

    RemoveEscapeSequences(text, Han);

    return true;
}

void PMD::GetText(const uint8_t * data, size_t size, int index, char * text) const noexcept
{
    *text = '\0';

    if (data == nullptr || size < 0x19)
        return;

    const uint8_t * Data = &data[1];
    size_t Size = size - 1;

    if (Data[0] != 0x1a || Data[1] != 0x00)
        return;

    size_t Offset = (size_t) *(uint16_t *) &Data[0x18] - 4;

    if (Offset + 3 >= Size)
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

    int Result = _PPZ->Load(filename, bufnum);

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

Channel * PMD::GetChannel(int channelNumber)
{
    if (channelNumber >= _countof(_State.Channel))
        return nullptr;

    return _State.Channel[channelNumber];
}

void PMD::HandleTimerA()
{
    _State.IsTimerABusy = true;
    _State.TimerATime++;

    if ((_State.TimerATime & 7) == 0)
        Fade();

    if ((_Effect.Priority != 0) && (!_Driver.UsePPS || (_Effect.Number == 0x80)))
        PlayEffect(); // Use the SSG for effect processing.

    _State.IsTimerABusy = false;
}

void PMD::HandleTimerB()
{
    _State.IsTimerBBusy = true;

    if (_Driver.music_flag != 0x00)
    {
        if (_Driver.music_flag & 0x01)
            DriverStart();

        if (_Driver.music_flag & 0x02)
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
uint8_t * PMD::PDRSwitchCommand(Channel *, uint8_t * si)
{
    if (!_Driver.UsePPS)
        return si + 1;

//  ppsdrv->SetParameter((*si & 1) << 1, *si & 1); // Preliminary
    si++;

    return si;
}

uint8_t * PMD::IncreasePCMVolumeCommand(Channel * channel, uint8_t * si)
{
    int al = (int) *si++ + channel->Volume;

    if (al > 254)
        al = 254;

    al++;

    channel->volpush = al;
    _Driver.volpush_flag = 1;

    return si;
}

uint8_t * PMD::SpecialC0ProcessingCommand(Channel * channel, uint8_t * si, uint8_t value)
{
    switch (value)
    {
        case 0xff:
            _State.FMVolumeDown = *si++;
            break;

        case 0xfe:
            si = DecreaseFMVolumeCommand(channel, si);
            break;

        case 0xfd:
            _State.SSGVolumeDown = *si++;
            break;

        case 0xfc:
            si = DecreaseSSGVolumeCommand(channel, si);
            break;

        // Command "&": Tie notes together.
        case 0xFB:
            _State.ADPCMVolumeDown = *si++;
            break;

        case 0xfa:
            si = DecreaseADPCMVolumeCommand(channel, si);
            break;

        case 0xf9:
            _State.RhythmVolumeDown = *si++;
            break;

        case 0xf8:
            si = DecreaseRhythmVolumeCommand(channel, si);
            break;

        case 0xf7:
            _State.PMDB2CompatibilityMode = ((*si++ & 0x01) == 0x01);
            break;

        case 0xf6:
            _State.PPZVolumeDown = *si++;
            break;

        case 0xf5:
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
    channel->lfoswi = (channel->lfoswi & 0x8F) | ((*si++ & 0x07) << 4);

    SwapLFO(channel);

    lfoinit_main(channel);

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
        channel->VolumeMask2 = channel->carrier;

    SetFMChannel3Mode(channel);

    return si;
}

// Command 't' [TEMPO CHANGE1] / COMMAND 'T' [TEMPO CHANGE2] / COMMAND 't±' [TEMPO CHANGE 相対1] / COMMAND 'T±' [TEMPO CHANGE 相対2]
uint8_t * PMD::ChangeTempoCommand(uint8_t * si)
{
    int al = *si++;

    if (al < 251)
    {
        _State.tempo_d = al;    // T (FC)
        _State.tempo_d_push = al;

        calc_tb_tempo();
    }
    else
    if (al == 0xFF)
    {
        al = *si++;          // t (FC FF)

        if (al < 18)
            al = 18;

        _State.tempo_48 = al;
        _State.tempo_48_push = al;

        calc_tempo_tb();
    }
    else
    if (al == 0xFE)
    {
        al = *si++;      // T± (FC FE)

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
        al = *si++;      // t± (FC FD)

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
uint8_t * PMD::SetStartOfLoopCommand(Channel * channel, uint8_t * si)
{
    uint8_t * ax = (channel == &_EffectChannel) ? _State.EData : _State.MData;

    ax[*(uint16_t *) si + 1] = 0;

    si += 2;

    return si;
}

// Command ']': Set end of loop
uint8_t * PMD::SetEndOfLoopCommand(Channel * channel, uint8_t * si)
{
    int ah = *si++;

    if (ah != 0)
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
        channel->loopcheck = 1;
    }

    int ax = *(uint16_t *) si + 2;

    if (channel == &_EffectChannel)
        si = _State.EData + ax;
    else
        si = _State.MData + ax;

    return si;
}

// Command ':': Loop dash
uint8_t * PMD::ExitLoopCommand(Channel * channel, uint8_t * si)
{
    uint8_t * bx = (channel == &_EffectChannel) ? _State.EData : _State.MData;

    bx += *(uint16_t *) si;
    si += 2;

    int dl = *bx++ - 1;

    if (dl != *bx)
        return si;

    si = bx + 3;

    return si;
}

// Sets an LFO parameter.
uint8_t * PMD::SetLFOParameter(Channel * channel, uint8_t * si)
{
    channel->delay = *si;
    channel->delay2 = *si++;
    channel->speed = *si;
    channel->speed2 = *si++;
    channel->step = *(int8_t *) si;
    channel->step2 = *(int8_t *) si++;
    channel->time = *si;
    channel->time2 = *si++;

    lfoinit_main(channel);

    return si;
}

//  SSG ENVELOPE SET
uint8_t * PMD::psgenvset(Channel * qq, uint8_t * si)
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

/// <summary>
/// Decreases the volume of the specified sound source.
/// </summary>
/// <param name="channel"></param>
/// <param name="si"></param>
/// <returns></returns>
uint8_t * PMD::DecreaseVolumeCommand(Channel * channel, uint8_t * si)
{
    int al = channel->Volume - *si++;

    if (al < 0)
        al = 0;
    else
    if (al >= 255)
        al = 254;

    channel->volpush = ++al;
    _Driver.volpush_flag = 1;

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

        channel->_mdc  = al;
        channel->_mdc2 = al;

        return si;
    }

    if (al == 0)
        al = 255;

    channel->mdc  = al;
    channel->mdc2 = al;

    return si;
}
#pragma endregion

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

void PMD::cm_clear(int * ah, int * al)
{
    *al ^= 0xff;

    if ((_Driver.slot3_flag &= *al) == 0)
    {
        if (_Driver.slotdetune_flag != 1)
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

//  Pitch setting when using ch3=sound effect mode (input CX:block AX:fnum)
void PMD::ch3_special(Channel * channel, int ax, int cx)
{
    int shiftmask = 0x80;

    int si = cx;

    int bh;

    if ((channel->VolumeMask1 & 0x0f) == 0)
        bh = 0xf0;      // all
    else
        bh = channel->VolumeMask1;  // bh=lfo1 mask 4321xxxx

    int ch;

    if ((channel->VolumeMask2 & 0x0f) == 0)
        ch = 0xf0;      // all
    else
        ch = channel->VolumeMask2;  // ch=lfo2 mask 4321xxxx

    //  slot  4
    int ax_;

    if (channel->FMSlotMask & 0x80)
    {
        ax_ = ax;
        ax += _State.slot_detune4;

        if ((bh & shiftmask) && (channel->lfoswi & 0x01))  ax += channel->lfodat;
        if ((ch & shiftmask) && (channel->lfoswi & 0x10))  ax += channel->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xa6, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa2, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  3
    if (channel->FMSlotMask & 0x40)
    {
        ax_ = ax;
        ax += _State.slot_detune3;

        if ((bh & shiftmask) && (channel->lfoswi & 0x01))  ax += channel->lfodat;
        if ((ch & shiftmask) && (channel->lfoswi & 0x10))  ax += channel->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xac, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa8, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  2
    if (channel->FMSlotMask & 0x20)
    {
        ax_ = ax;
        ax += _State.slot_detune2;

        if ((bh & shiftmask) && (channel->lfoswi & 0x01))
            ax += channel->lfodat;

        if ((ch & shiftmask) && (channel->lfoswi & 0x10))
            ax += channel->_lfodat;

        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xae, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xaa, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  1
    if (channel->FMSlotMask & 0x10)
    {
        ax_ = ax;
        ax += _State.slot_detune1;

        if ((bh & shiftmask) && (channel->lfoswi & 0x01)) 
            ax += channel->lfodat;

        if ((ch & shiftmask) && (channel->lfoswi & 0x10))
            ax += channel->_lfodat;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xad, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa9, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }
}

// Get the data to set to 0b4h? out.dl
uint8_t PMD::calc_panout(Channel * channel)
{
    int  dl = channel->PanAndVolume;

    if (channel->HardwareLFODelayCounter)
        dl &= 0xc0;  // If HLFO Delay remains, set only pan.

    return (uint8_t) dl;
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

    _OPNAW->SetReg((uint32_t) (_Driver.FMSelector + dh), (uint32_t) al);
}

//  Sub for volume LFO
void PMD::fmlfo_sub(Channel *, int al, int bl, uint8_t * vol_tbl)
{
    if (bl & 0x80) vol_tbl[0] = (uint8_t) Limit(vol_tbl[0] - al, 255, 0);
    if (bl & 0x40) vol_tbl[1] = (uint8_t) Limit(vol_tbl[1] - al, 255, 0);
    if (bl & 0x20) vol_tbl[2] = (uint8_t) Limit(vol_tbl[2] - al, 255, 0);
    if (bl & 0x10) vol_tbl[3] = (uint8_t) Limit(vol_tbl[3] - al, 255, 0);
}

// Completely muting the [PartB] part (TL=127 and RR=15 and KEY-OFF). cy=1 ･･･ All slots are neiromasked
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

    SetFMKeyOff(channel);

    return 0;
}

uint8_t * PMD::SetRhythmPanValue(uint8_t * si)
{
    int * bx = &_State.RhythmPanAndVolume[*si - 1];

    int dh = *si++ + 0x18 - 1;

    int dl = *bx & 0x1F;
    int al = (*(int8_t *) si++ + dl);

    if (al > 31)
        al = 31;
    else
    if (al < 0)
        al = 0;

    dl = (al &= 0x1F);
    dl = *bx = ((*bx & 0xE0) | dl);

    _OPNAW->SetReg((uint32_t) dh, (uint32_t) dl);

    return si;
}

uint8_t * PMD::SetRhythmVolume(uint8_t * si)
{
    int dl = _State.RhythmVolume + *(int8_t *) si++;

    if (dl >= 64)
        dl = (dl & 0x80) ? 0 : 63;

    _State.RhythmVolume = dl;

    if (_State.FadeOutVolume != 0)
        dl = ((256 - _State.FadeOutVolume) * dl) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) dl);

    return si;
}

//  SHIFT[di] transpose
int PMD::oshiftp(Channel * channel, int al)
{
    return oshift(channel, al);
}

int PMD::oshift(Channel * channel, int al)
{
    if (al == 0x0f)
        return al;

    int dl = channel->shift + channel->shift_def;

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

uint8_t * PMD::CalculateQ(Channel * channel, uint8_t * si)
{
    if (*si == 0xc1)
    {
        si++; // &&
        channel->qdat = 0;

        return si;
    }

    int dl = channel->qdata;

    if (channel->qdatb != 0)
        dl += (channel->Length * channel->qdatb) >> 8;

    if (channel->qdat3 != 0)
    {
        int ax = rnd((channel->qdat3 & 0x7f) + 1); // Random-Q

        if ((channel->qdat3 & 0x80) == 0)
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

    if (channel->qdat2 != 0)
    {
        int dh = channel->Length - channel->qdat2;

        if (dh < 0)
        {
            channel->qdat = 0;

            return si;
        }

        if (dl < dh)
            channel->qdat = dl;
        else
            channel->qdat = dh;
    }
    else
        channel->qdat = dl;

    return si;
}

void PMD::CalculatePortamento(Channel * channel)
{
    channel->porta_num += channel->porta_num2;

    if (channel->porta_num3 == 0)
        return;

    if (channel->porta_num3 > 0)
    {
        channel->porta_num3--;
        channel->porta_num++;
    }
    else
    {
        channel->porta_num3++;
        channel->porta_num--;
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
void PMD::SwapLFO(Channel * track)
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
    Swap(&track->MDepth, &track->_mdepth);
    Swap(&track->MDepthSpeedA, &track->_mdspd);
    Swap(&track->MDepthSpeedB, &track->_mdspd2);
    Swap(&track->lfo_wave, &track->_lfo_wave);
    Swap(&track->mdc, &track->_mdc);
    Swap(&track->mdc2, &track->_mdc2);
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

    // PAN/HARDLFO DEFAULT
    _OPNAW->SetReg(0x0b4, 0xc0);
    _OPNAW->SetReg(0x0b5, 0xc0);
    _OPNAW->SetReg(0x0b6, 0xc0);
    _OPNAW->SetReg(0x1b4, 0xc0);
    _OPNAW->SetReg(0x1b5, 0xc0);
    _OPNAW->SetReg(0x1b6, 0xc0);

    _State.port22h = 0x00;

    _OPNAW->SetReg(0x22, 0x00);

    //  Rhythm Default = Pan : 0xC0 (3 << 6, Center) , Volume : 0x0F
    for (int i = 0; i < 6; ++i)
        _State.RhythmPanAndVolume[i] = 0xCF;

    _OPNAW->SetReg(0x10, 0xFF);

    // Set the Rhythm volume.
    _State.RhythmVolume = 48 * 4 * (256 - _State.RhythmVolumeDown) / 1024;

    _OPNAW->SetReg(0x11, (uint32_t) _State.RhythmVolume);

    // PCM reset & LIMIT SET
    _OPNAW->SetReg(0x10c, 0xff);
    _OPNAW->SetReg(0x10d, 0xff);

    for (int i = 0; i < MaxPPZChannels; ++i)
        _PPZ->SetPan(i, 5);
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
    _OPNAW->SetReg(0x27, 0x3F); // Timer Enable

    //　Measure counter reset
    _State.OpsCounter = 0;
    _State.BarCounter = 0;
    _State.BarLength = 96;
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

    for (int i = 0; i < MaxPPZChannels; ++i)
        _PPZ->Stop(i);
}

/// <summary>
/// Starts the driver.
/// </summary>
void PMD::Start()
{
    if (_State.IsTimerABusy || _State.IsTimerBBusy)
    {
        _Driver.music_flag |= 0x01; // Not executed during TA/TB processing

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
        _Driver.music_flag |= 0x02;
    }
    else
    {
        _State.fadeout_flag = 0;
        DriverStop();
    }

    ::memset(_SampleSrc, 0, sizeof(_SampleSrc));
    _Position = 0;
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

    _State.FMChannel3Mode = 0x3F;
    _State.BarLength = 96;

    _State.FMVolumeDown = _State.DefaultFMVolumeDown;
    _State.SSGVolumeDown = _State.DefaultSSGVolumeDown;
    _State.ADPCMVolumeDown = _State.DefaultADPCMVolumeDown;
    _State.PPZVolumeDown = _State.DefaultPPZVolumeDown;
    _State.RhythmVolumeDown = _State.DefaultRhythmVolumeDown;

    _State.PMDB2CompatibilityMode = _State.DefaultPMDB2CompatibilityMode;

    for (int i = 0; i < 6; ++i)
    {
        int PartMask = _FMChannel[i].MuteMask;
        int KeyOnFlag = _FMChannel[i].KeyOnFlag;

        ::memset(&_FMChannel[i], 0, sizeof(Channel));

        _FMChannel[i].MuteMask = PartMask & 0x0f;
        _FMChannel[i].KeyOnFlag = KeyOnFlag;
        _FMChannel[i].Tone = 255;
        _FMChannel[i].DefaultTone = 255;
    }

    for (int i = 0; i < 3; ++i)
    {
        int PartMask = _SSGChannel[i].MuteMask;
        int KeyOnFlag = _SSGChannel[i].KeyOnFlag;

        ::memset(&_SSGChannel[i], 0, sizeof(Channel));

        _SSGChannel[i].MuteMask = PartMask & 0x0f;
        _SSGChannel[i].KeyOnFlag = KeyOnFlag;
        _SSGChannel[i].Tone = 255;
        _SSGChannel[i].DefaultTone = 255;
    }

    {
        int partmask = _ADPCMChannel.MuteMask;
        int KeyOnFlag = _ADPCMChannel.KeyOnFlag;

        ::memset(&_ADPCMChannel, 0, sizeof(Channel));

        _ADPCMChannel.MuteMask = partmask & 0x0f;
        _ADPCMChannel.KeyOnFlag = KeyOnFlag;
        _ADPCMChannel.Tone = 255;
        _ADPCMChannel.DefaultTone = 255;
    }

    {
        int partmask = _RhythmChannel.MuteMask;
        int KeyOnFlag = _RhythmChannel.KeyOnFlag;

        ::memset(&_RhythmChannel, 0, sizeof(Channel));

        _RhythmChannel.MuteMask = partmask & 0x0f;
        _RhythmChannel.KeyOnFlag = KeyOnFlag;
        _RhythmChannel.Tone = 255;
        _RhythmChannel.DefaultTone = 255;
    }

    for (int i = 0; i < 3; ++i)
    {
        int partmask = _FMExtensionChannel[i].MuteMask;
        int KeyOnFlag = _FMExtensionChannel[i].KeyOnFlag;

        ::memset(&_FMExtensionChannel[i], 0, sizeof(Channel));

        _FMExtensionChannel[i].MuteMask = partmask & 0x0f;
        _FMExtensionChannel[i].KeyOnFlag = KeyOnFlag;
        _FMExtensionChannel[i].Tone = 255;
        _FMExtensionChannel[i].DefaultTone = 255;
    }

    for (int i = 0; i < 8; ++i)
    {
        int partmask = _PPZChannel[i].MuteMask;
        int KeyOnFlag = _PPZChannel[i].KeyOnFlag;

        ::memset(&_PPZChannel[i], 0, sizeof(Channel));

        _PPZChannel[i].MuteMask = partmask & 0x0f;
        _PPZChannel[i].KeyOnFlag = KeyOnFlag;
        _PPZChannel[i].Tone = 255;
        _PPZChannel[i].DefaultTone = 255;
    }

    _Driver.Initialize();

    _Effect.PreviousNumber = 0;
}

/// <summary>
/// Sets the start address and initial value of each track.
/// </summary>
void PMD::InitializeTracks()
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
        _FMChannel[i].KeyOffFlag = -1;
        _FMChannel[i].mdc = -1;        // MDepth Counter (無限)
        _FMChannel[i].mdc2 = -1;      // 同上
        _FMChannel[i]._mdc = -1;      // 同上
        _FMChannel[i]._mdc2 = -1;      // 同上
        _FMChannel[i].Tone = 255;      // rest
        _FMChannel[i].DefaultTone = 0xFF;    // rest
        _FMChannel[i].Volume = 108;      // FM  VOLUME DEFAULT= 108
        _FMChannel[i].PanAndVolume = 0xC0;  // 0xC0 (3 << 6, Center)
        _FMChannel[i].FMSlotMask = 0xf0;    // FM SLOT MASK
        _FMChannel[i].ToneMask = 0xff;    // FM Neiro MASK

        Offsets++;
    }

    for (size_t i = 0; i < _countof(_SSGChannel); ++i)
    {
        if (_State.MData[*Offsets] == 0x80) // Do not play.
            _SSGChannel[i].Data = nullptr;
        else
            _SSGChannel[i].Data = &_State.MData[*Offsets];

        _SSGChannel[i].Length = 1;
        _SSGChannel[i].KeyOffFlag = -1;  // 現在SetFMKeyOff中
        _SSGChannel[i].mdc = -1;      // MDepth Counter (無限)
        _SSGChannel[i].mdc2 = -1;      // 同上
        _SSGChannel[i]._mdc = -1;      // 同上
        _SSGChannel[i]._mdc2 = -1;      // 同上
        _SSGChannel[i].Tone = 255;      // rest
        _SSGChannel[i].DefaultTone = 255;    // rest
        _SSGChannel[i].Volume = 8;      // SSG VOLUME DEFAULT= 8
        _SSGChannel[i].psgpat = 7;      // SSG = TONE
        _SSGChannel[i].envf = 3;      // SSG ENV = NONE/normal

        Offsets++;
    }

    {
        if (_State.MData[*Offsets] == 0x80) // Do not play
            _ADPCMChannel.Data = nullptr;
        else
            _ADPCMChannel.Data = &_State.MData[*Offsets];

        _ADPCMChannel.Length = 1;
        _ADPCMChannel.KeyOffFlag = -1;    // 現在SetFMKeyOff中
        _ADPCMChannel.mdc = -1;        // MDepth Counter (無限)
        _ADPCMChannel.mdc2 = -1;      // 同上
        _ADPCMChannel._mdc = -1;      // 同上
        _ADPCMChannel._mdc2 = -1;      // 同上
        _ADPCMChannel.Tone = 255;      // rest
        _ADPCMChannel.DefaultTone = 255;    // rest
        _ADPCMChannel.Volume = 128;      // PCM VOLUME DEFAULT= 128
        _ADPCMChannel.PanAndVolume = 0xC0;  // 0xC0 (3 << 6, Center)

        Offsets++;
    }

    {
        if (_State.MData[*Offsets] == 0x80) // Do not play
            _RhythmChannel.Data = nullptr;
        else
            _RhythmChannel.Data = &_State.MData[*Offsets];

        _RhythmChannel.Length = 1;
        _RhythmChannel.KeyOffFlag = -1;  // 現在SetFMKeyOff中
        _RhythmChannel.mdc = -1;      // MDepth Counter (無限)
        _RhythmChannel.mdc2 = -1;      // 同上
        _RhythmChannel._mdc = -1;      // 同上
        _RhythmChannel._mdc2 = -1;      // 同上
        _RhythmChannel.Tone = 255;      // rest
        _RhythmChannel.DefaultTone = 255;    // rest
        _RhythmChannel.Volume = 15;      // PPSDRV volume

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

    ::free(pcmbuf);

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

// Fade In / Out
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

            if (_State.fade_stop_flag == 1)
                _Driver.music_flag |= 0x02;
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
