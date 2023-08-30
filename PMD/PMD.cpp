
// Based on PMDWin code by C60

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

    _OPNA = new OPNAW(_File);
    _PPZ8 = new PPZ8(_File);
    _PPS  = new PPSDRV(_File);
    _P86  = new P86DRV(_File);

    InitializeInternal();
}

/// <summary>
/// Destroys an instance.
/// </summary>
PMD::~PMD()
{
    delete _P86;
    delete _PPS;
    delete _PPZ8;
    delete _OPNA;
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

    InitializeInternal();

    _PPZ8->Init(_State._OPNARate, false);
    _PPS->Init(_State._OPNARate, false);
    _P86->Init(_State._OPNARate, false);

    if (_OPNA->Init(OPNAClock, SOUND_44K, false, DirectoryPath) == false)
        return false;

    // Initialize ADPCM RAM.
    {
        _OPNA->SetFMWait(0);
        _OPNA->SetSSGWait(0);
        _OPNA->SetRhythmWait(0);
        _OPNA->SetADPCMWait(0);

        uint8_t  Page[0x400]; // 0x400 * 0x100 = 0x40000(256K)

        ::memset(Page, 0x08, sizeof(Page));

        for (int i = 0; i < 0x100; i++)
            pcmstore((uint16_t) i * sizeof(Page) / 32, (uint16_t) (i + 1) * sizeof(Page) / 32, Page);
    }

    _OPNA->SetFMVolume(0);
    _OPNA->SetPSGVolume(-18);
    _OPNA->SetADPCMVolume(0);
    _OPNA->SetRhythmMasterVolume(0);

    _PPZ8->SetVolume(0);
    _PPS->SetVolume(0);
    _P86->SetVolume(0);

    _OPNA->SetFMWait(DEFAULT_REG_WAIT);
    _OPNA->SetSSGWait(DEFAULT_REG_WAIT);
    _OPNA->SetRhythmWait(DEFAULT_REG_WAIT);
    _OPNA->SetADPCMWait(DEFAULT_REG_WAIT);

    pcmends.Count = 0x26;

    for (int i = 0; i < 256; i++)
    {
        pcmends.Address[i][0] = 0;
        pcmends.Address[i][1] = 0;
    }

    _State._PPCFileName[0] = '\0';

    // Initial setting of 088/188/288/388 (same INT number only)
    _OPNA->SetReg(0x29, 0x00);
    _OPNA->SetReg(0x24, 0x00);
    _OPNA->SetReg(0x25, 0x00);
    _OPNA->SetReg(0x26, 0x00);
    _OPNA->SetReg(0x27, 0x3f);

    // Start the OPN interrupt.
    StartOPNInterrupt();

    return true;
}

// Initialization (internal processing)
void PMD::InitializeInternal()
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
    _State._OPNARate = SOUND_44K;
    _State._PPZ8Rate = SOUND_44K;
    _State.rhyvol = 0x3c;
    _State.fade_stop_flag = 0;
    _State._IsTimerBBusy = false;

    _State._IsTimerABusy = false;
    _State._TimerBSpeed = 0x100;
    _State.port22h = 0;

    _State._IsUsingP86 = false;

    _State._UseInterpolationPPZ8 = false;
    _State._UseInterpolationP86 = false;
    _State._UseInterpolationPPS = false;

    // Initialize variables.
    _State._Track[ 0] = &_FMTrack[0];
    _State._Track[ 1] = &_FMTrack[1];
    _State._Track[ 2] = &_FMTrack[2];
    _State._Track[ 3] = &_FMTrack[3];
    _State._Track[ 4] = &_FMTrack[4];
    _State._Track[ 5] = &_FMTrack[5];

    _State._Track[ 6] = &_SSGTrack[0];
    _State._Track[ 7] = &_SSGTrack[1];
    _State._Track[ 8] = &_SSGTrack[2];

    _State._Track[ 9] = &_ADPCMTrack;

    _State._Track[10] = &_RhythmTrack;

    _State._Track[11] = &_ExtensionTrack[0];
    _State._Track[12] = &_ExtensionTrack[1];
    _State._Track[13] = &_ExtensionTrack[2];

    _State._Track[14] = &_DummyTrack; // Unused
    _State._Track[15] = &_EffectTrack;

    _State._Track[16] = &_PPZ8Track[0];
    _State._Track[17] = &_PPZ8Track[1];
    _State._Track[18] = &_PPZ8Track[2];
    _State._Track[19] = &_PPZ8Track[3];
    _State._Track[20] = &_PPZ8Track[4];
    _State._Track[21] = &_PPZ8Track[5];
    _State._Track[22] = &_PPZ8Track[6];
    _State._Track[23] = &_PPZ8Track[7];

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

    _State._UseSSG = false;         // Use SSG (Rhytm Sound Source)

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

    _DriverState._UsePPS = false;
    _DriverState.music_flag = 0;

    _MData[0] = 0;

    for (size_t i = 0; i < 12; ++i)
    {
        _MData[i * 2 + 1] = 0x18;
        _MData[i * 2 + 2] = 0x00;
    }

    _MData[25] = 0x80;

    _State._MData = &_MData[1];
    _State._VData = _VData;
    _State._EData = _EData;

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

    if (_State._SearchPath.size() == 0)
        return ERR_SUCCESS;

    int Result = ERR_SUCCESS;

    char FileName[MAX_PATH] = { 0 };
    WCHAR FileNameW[MAX_PATH] = { 0 };

    WCHAR FilePath[MAX_PATH] = { 0 };

    // P86/PPC reading
    {
        GetText(data, size, 0, FileName);

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

            if (HasExtension(FileNameW, _countof(FileNameW), L".P86")) // Is it a Professional Music Driver P86 Samples Pack file?
            {
                FindFile(FilePath, FileNameW);

                Result = _P86->Load(FilePath);

                if (Result == P86_SUCCESS || Result == P86_ALREADY_LOADED)
                    _State._IsUsingP86 = true;
            }
            else
            if (HasExtension(FileNameW, _countof(FileNameW), L".PPC"))
            {
                FindFile(FilePath, FileNameW);

                Result = LoadPPCInternal(FilePath);

                if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
                    _State._IsUsingP86 = false;
            }
        }
    }

    // PPS import
    {
        GetText(data, size, -1, FileName);

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

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

            if (HasExtension(FileNameW, _countof(FileNameW), L".PZI") && (data[0] != 0xff))
            {
                FindFile(FilePath, FileNameW);

                Result = _PPZ8->Load(FilePath, 0);
            }
            else
            if (HasExtension(FileNameW, _countof(FileNameW), L".PMB") && (data[0] != 0xff))
            {
                WCHAR * p = ::wcschr(FileNameW, ',');

                if (p == NULL)
                {
                    if ((p = ::wcschr(FileNameW, '.')) == NULL)
                        RenameExtension(FileNameW, _countof(FileNameW), L".PZI");

                    FindFile(FilePath, FileNameW);

                    Result = _PPZ8->Load(FilePath, 0);
                }
                else
                {
                    *p = '\0';

                    WCHAR PPZFileName2[MAX_PATH] = { 0 };

                    ::wcscpy(PPZFileName2, p + 1);

                    if ((p = ::wcschr(FileNameW, '.')) == NULL)
                        RenameExtension(FileNameW, _countof(FileNameW), L".PZI");

                    if ((p = ::wcschr(PPZFileName2, '.')) == NULL)
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

    int FMWait = _OPNA->GetFMWait();
    int SSGWait = _OPNA->GetSSGWait();
    int RhythmWait = _OPNA->GetRhythmWait();
    int ADPCMWait = _OPNA->GetADPCMWait();

    _OPNA->SetFMWait(0);
    _OPNA->SetSSGWait(0);
    _OPNA->SetRhythmWait(0);
    _OPNA->SetADPCMWait(0);

    do
    {
        {
            if (_OPNA->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNA->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNA->SetReg(0x27, _State.ch3mode | 0x30); // Timer Reset (Both timer A and B)

            uint32_t us = _OPNA->GetNextEvent();

            _OPNA->Count(us);
            _Position += us;
        }

        if ((_State._LoopCount == 1) && (*songLength == 0)) // When looping
        {
            *songLength = (int) (_Position / 1000);
        }
        else
        if (_State._LoopCount == -1) // End without loop
        {
            *songLength = (int) (_Position / 1000);
            *loopLength = 0;

            DriverStop();

            _OPNA->SetFMWait(FMWait);
            _OPNA->SetSSGWait(SSGWait);
            _OPNA->SetRhythmWait(RhythmWait);
            _OPNA->SetADPCMWait(ADPCMWait);

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
    while (_State._LoopCount < 2);

    *loopLength = (int) (_Position / 1000) - *songLength;

    DriverStop();

    _OPNA->SetFMWait(FMWait);
    _OPNA->SetSSGWait(SSGWait);
    _OPNA->SetRhythmWait(RhythmWait);
    _OPNA->SetADPCMWait(ADPCMWait);

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

    int FMWait = _OPNA->GetFMWait();
    int SSGWait = _OPNA->GetSSGWait();
    int RhythmWait = _OPNA->GetRhythmWait();
    int ADPCMWait = _OPNA->GetADPCMWait();

    _OPNA->SetFMWait(0);
    _OPNA->SetSSGWait(0);
    _OPNA->SetRhythmWait(0);
    _OPNA->SetADPCMWait(0);

    do
    {
        {
            if (_OPNA->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNA->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNA->SetReg(0x27, _State.ch3mode | 0x30);  // Timer Reset (Both timer A and B)

            uint32_t us = _OPNA->GetNextEvent();

            _OPNA->Count(us);
            _Position += us;
        }

        if ((_State._LoopCount == 1) && (*eventCount == 0)) // When looping
        {
            *eventCount = GetEventNumber();
        }
        else
        if (_State._LoopCount == -1) // End without loop
        {
            *eventCount = GetEventNumber();
            *loopEventCount = 0;

            DriverStop();

            _OPNA->SetFMWait(FMWait);
            _OPNA->SetSSGWait(SSGWait);
            _OPNA->SetRhythmWait(RhythmWait);
            _OPNA->SetADPCMWait(ADPCMWait);

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
    while (_State._LoopCount < 2);

    *loopEventCount = GetEventNumber() - *eventCount;

    DriverStop();

    _OPNA->SetFMWait(FMWait);
    _OPNA->SetSSGWait(SSGWait);
    _OPNA->SetRhythmWait(RhythmWait);
    _OPNA->SetADPCMWait(ADPCMWait);

    return true;
}

// Gets the current loop number.
uint32_t PMD::GetLoopNumber()
{
    return (uint32_t) _State._LoopCount;
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
        if (_OPNA->ReadStatus() & 0x01)
            HandleTimerA();

        if (_OPNA->ReadStatus() & 0x02)
            HandleTimerB();

        _OPNA->SetReg(0x27, _State.ch3mode | 0x30); // Timer Reset (Both timer A and B)

        uint32_t us = _OPNA->GetNextEvent();

        _OPNA->Count(us);
        _Position += us;
    }

    if (_State._LoopCount == -1)
        Silence();

    _OPNA->ClearBuffer();
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
                if (_OPNA->ReadStatus() & 0x01)
                    HandleTimerA();

                if (_OPNA->ReadStatus() & 0x02)
                    HandleTimerB();

                _OPNA->SetReg(0x27, _State.ch3mode | 0x30); // Timer Reset (Both timer A and B)
            }

            uint32_t us = _OPNA->GetNextEvent(); // in microseconds

            {
                _SamplesToDo = (size_t) ((double) us * _State._OPNARate / 1000000.0);
                _OPNA->Count(us);

                ::memset(_SampleDst, 0, _SamplesToDo * sizeof(Stereo32bit));

                if (_State._OPNARate == _State._PPZ8Rate)
                    _PPZ8->Mix((Sample *) _SampleDst, _SamplesToDo);
                else
                {
                    // PCM frequency transform of ppz8 (no interpolation)
                    size_t SampleCount = (size_t) (_SamplesToDo * _State._PPZ8Rate / _State._OPNARate + 1);
                    int delta     = (int) (8192         * _State._PPZ8Rate / _State._OPNARate);

                    ::memset(_SampleTmp, 0, SampleCount * sizeof(Sample) * 2);

                    _PPZ8->Mix((Sample *) _SampleTmp, SampleCount);

                    int carry = 0;

                    // Frequency transform (1 << 13 = 8192)
                    for (size_t i = 0; i < _SamplesToDo; i++)
                    {
                        _SampleDst[i].Left  = _SampleTmp[(carry >> 13)].Left;
                        _SampleDst[i].Right = _SampleTmp[(carry >> 13)].Right;

                        carry += delta;
                    }
                }
            }

            {
                _OPNA->Mix((Sample *) _SampleDst, _SamplesToDo);

                if (_DriverState._UsePPS)
                    _PPS->Mix((Sample *) _SampleDst, _SamplesToDo);

                if (_State._IsUsingP86)
                    _P86->Mix((Sample *) _SampleDst, _SamplesToDo);
            }

            {
                _Position += us;

                if (_State._FadeOutSpeedHQ > 0)
                {
                    int Factor = (_State._LoopCount != -1) ? (int) ((1 << 10) * ::pow(512, -(double) (_Position - _FadeOutPosition) / 1000 / _State._FadeOutSpeedHQ)) : 0;

                    for (size_t i = 0; i < _SamplesToDo; i++)
                    {
                        _SampleSrc[i].Left  = (int16_t) Limit(_SampleDst[i].Left  * Factor >> 10, 32767, -32768);
                        _SampleSrc[i].Right = (int16_t) Limit(_SampleDst[i].Right * Factor >> 10, 32767, -32768);
                    }

                    // Fadeout end
                    if ((_Position - _FadeOutPosition > (int64_t)_State._FadeOutSpeedHQ * 1000) && (_State.fade_stop_flag == 1))
                        _DriverState.music_flag |= 2;
                }
                else
                {
                    for (size_t i = 0; i < _SamplesToDo; i++)
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

    return _OPNA->LoadInstruments(Path);
}

// Sets the PCM search directory
bool PMD::SetSearchPaths(std::vector<const WCHAR *> & paths)
{
    for (std::vector<const WCHAR *>::iterator iter = paths.begin(); iter < paths.end(); iter++)
    {
        WCHAR Path[MAX_PATH];

        ::wcscpy(Path, *iter);
        AddBackslash(Path, _countof(Path));

        _State._SearchPath.push_back(Path);
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
        _State._OPNARate =
        _State._PPZ8Rate = SOUND_44K;
        _State._UseFM55kHzSynthesis = true;
    }
    else
    {
        _State._OPNARate =
        _State._PPZ8Rate = frequency;
        _State._UseFM55kHzSynthesis = false;
    }

    _OPNA->SetRate(OPNAClock, _State._OPNARate, _State._UseFM55kHzSynthesis);

    _PPZ8->SetRate(_State._PPZ8Rate, _State._UseInterpolationPPZ8);
    _PPS->SetRate(_State._OPNARate, _State._UseInterpolationPPS);
    _P86->SetSampleRate(_State._OPNARate, _State._UseInterpolationP86);
}

/// <summary>
/// Enables or disables 55kHz synthesis in FM primary interpolation.
/// </summary>
void PMD::SetFM55kHzSynthesisMode(bool flag)
{
    _State._UseFM55kHzSynthesis = flag;

    _OPNA->SetRate(OPNAClock, _State._OPNARate, _State._UseFM55kHzSynthesis);
}

/// <summary>
/// Sets the rate at which raw PPZ data is synthesized (in Hz, for example 44100)
/// </summary>
void PMD::SetPPZSynthesisRate(uint32_t frequency)
{
    _State._PPZ8Rate = frequency;

    _PPZ8->SetRate(frequency, _State._UseInterpolationPPZ8);
}

/// <summary>
/// Enables or disables PPZ interpolation.
/// </summary>
void PMD::SetPPZInterpolation(bool flag)
{
    _State._UseInterpolationPPZ8 = flag;

    _PPZ8->SetRate(_State._PPZ8Rate, flag);
}

/// <summary>
/// Enables or disables PPS interpolation.
/// </summary>
void PMD::SetPPSInterpolation(bool flag)
{
    _State._UseInterpolationPPS = flag;

    _PPS->SetRate(_State._OPNARate, flag);
}

/// <summary>
/// Enables or disables P86 interpolation.
/// </summary>
void PMD::SetP86Interpolation(bool flag)
{
    _State._UseInterpolationP86 = flag;

    _P86->SetSampleRate(_State._OPNARate, flag);
}

// Sets FM Wait after register output.
void PMD::SetFMWait(int nsec)
{
    _OPNA->SetFMWait(nsec);
}

// Sets SSG Wait after register output.
void PMD::SetSSGWait(int nsec)
{
    _OPNA->SetSSGWait(nsec);
}

// Sets Rythm Wait after register output.
void PMD::SetRhythmWait(int nsec)
{
    _OPNA->SetRhythmWait(nsec);
}

// Sets ADPCM Wait after register output.
void PMD::SetADPCMWait(int nsec)
{
    _OPNA->SetADPCMWait(nsec);
}

// Fade out (PMD compatible)
void PMD::SetFadeOutSpeed(int speed)
{
    _State._FadeOutSpeed = speed;
}

// Fade out (High quality sound)
void PMD::SetFadeOutDurationHQ(int speed)
{
    if (speed > 0)
    {
        if (_State._FadeOutSpeedHQ == 0)
            _FadeOutPosition = _Position;

        _State._FadeOutSpeedHQ = speed;
    }
    else
        _State._FadeOutSpeedHQ = 0; // Fadeout forced stop
}

// Sets the playback position (in ticks).
void PMD::SetEventNumber(int pos)
{
    if (_State.syousetu_lng * _State.syousetu + _State.opncount > pos)
    {
        DriverStart();

        _SamplePtr = _SampleSrc;
        _SamplesToDo = 0;
    }

    while (_State.syousetu_lng * _State.syousetu + _State.opncount < pos)
    {
        if (_OPNA->ReadStatus() & 0x01)
            HandleTimerA();

        if (_OPNA->ReadStatus() & 0x02)
            HandleTimerB();

        _OPNA->SetReg(0x27, _State.ch3mode | 0x30); // Timer Reset (Both timer A and B)

        uint32_t us = _OPNA->GetNextEvent();
        _OPNA->Count(us);
    }

    if (_State._LoopCount == -1)
        Silence();

    _OPNA->ClearBuffer();
}

// Gets the playback position (in ticks)
int PMD::GetEventNumber()
{
    return _State.syousetu_lng * _State.syousetu + _State.opncount;
}

// Gets PPC / P86 filename.
WCHAR * PMD::GetPCMFileName(WCHAR * filePath)
{
    if (_State._IsUsingP86)
        ::wcscpy(filePath, _P86->_FilePath);
    else
        ::wcscpy(filePath, _State._PPCFileName);

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
    _DriverState._UsePPS = value;
}

/// <summary>
/// Enables playing the OPNA Rhythm with the SSG Sound Source.
/// </summary>
void PMD::UseSSG(bool flag) noexcept
{
    _State._UseSSG = flag;
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
    if (ch >= sizeof(_State._Track) / sizeof(Track *))
        return ERR_WRONG_PARTNO;

    if (TrackTable[ch][0] < 0)
    {
        _State.rhythmmask = 0;  // Rhythm音源をMask
        _OPNA->SetReg(0x10, 0xff);  // Rhythm音源を全部Dump
    }
    else
    {
        int fmseltmp = _DriverState.fmsel;

        if ((_State._Track[ch]->Mask == 0) && _State._IsPlaying)
        {
            if (TrackTable[ch][2] == 0)
            {
                _DriverState._CurrentTrack = TrackTable[ch][1];
                _DriverState.fmsel = 0;

                MuteFMPart(_State._Track[ch]);
            }
            else
            if (TrackTable[ch][2] == 1)
            {
                _DriverState._CurrentTrack = TrackTable[ch][1];
                _DriverState.fmsel = 0x100;

                MuteFMPart(_State._Track[ch]);
            }
            else
            if (TrackTable[ch][2] == 2)
            {
                _DriverState._CurrentTrack = TrackTable[ch][1];

                int ah = 1 << (_DriverState._CurrentTrack - 1);

                ah |= (ah << 3);

                // PSG keyoff
                _OPNA->SetReg(0x07, ah | _OPNA->GetReg(0x07));
            }
            else
            if (TrackTable[ch][2] == 3)
            {
                _OPNA->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
                _OPNA->SetReg(0x100, 0x01);    // PCM RESET
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

        _State._Track[ch]->Mask |= 1;
        _DriverState.fmsel = fmseltmp;
    }

    return ERR_SUCCESS;
}

/// <summary>
/// Disables the specified part.
/// </summary>
int PMD::maskoff(int ch)
{
    if (ch >= sizeof(_State._Track) / sizeof(Track *))
        return ERR_WRONG_PARTNO;

    if (TrackTable[ch][0] < 0)
    {
        _State.rhythmmask = 0xff;
    }
    else
    {
        if (_State._Track[ch]->Mask == 0)
            return ERR_NOT_MASKED;

        // Still masked by sound effects

        if ((_State._Track[ch]->Mask &= 0xfe) != 0)
            return ERR_EFFECT_USED;

        // The song has stopped.
        if (!_State._IsPlaying)
            return ERR_MUSIC_STOPPED;

        int fmseltmp = _DriverState.fmsel;

        if (_State._Track[ch]->address)
        {
            if (TrackTable[ch][2] == 0)
            {    // FM音源(表)
                _DriverState.fmsel = 0;
                _DriverState._CurrentTrack = TrackTable[ch][1];
                neiro_reset(_State._Track[ch]);
            }
            else
            if (TrackTable[ch][2] == 1)
            {  // FM音源(裏)
                _DriverState.fmsel = 0x100;
                _DriverState._CurrentTrack = TrackTable[ch][1];
                neiro_reset(_State._Track[ch]);
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
    _State.rhyvol         = 48 * 4 * (256 - _State.rhythm_voldown) / 1024;

    _OPNA->SetReg(0x11, (uint32_t) _State.rhyvol);
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
        Data = _State._MData;
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

    if (Size < (size_t)*(uint16_t *) &Data[0x18] - 4 + 3)
        return;

    const uint8_t * Src = &Data[*(uint16_t *) &Data[0x18] - 4];

    if (*(Src + 2) != 0x40)
    {
        if (*(Src + 3) != 0xfe || *(Src + 2) < 0x41)
            return;
    }

    if (*(Src + 2) >= 0x42)
        index++;

    if (*(Src + 2) >= 0x48)
        index++;

    if (index < 0)
        return;

    Src = &Data[*(uint16_t *) Src];

    size_t i;

    uint16_t Offset = 0;

    for (i = 0; i <= (size_t) index; i++)
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

    for (i = Offset; i < Size; i++)
    {
        if (Data[i] == '\0')
            break;
    }

    // Without the terminating \0
    if (i >= Size)
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
        _State._IsUsingP86 = false;

    return Result;
}

// Load PPS
int PMD::LoadPPS(const WCHAR * filename)
{
    Stop();

    int Result = _PPS->Load(filename);

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
        _State._IsUsingP86 = true;

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
    if (trackNumber >= _countof(_State._Track))
        return nullptr;

    return _State._Track[trackNumber];
}

void PMD::HandleTimerA()
{
    _State._IsTimerABusy = true;
    _State._TimerATime++;

    if ((_State._TimerATime & 7) == 0)
        Fade();

    if (_EffectState.effon && (!_DriverState._UsePPS || _EffectState.psgefcnum == 0x80))
        effplay(); // SSG Sound Source effect processing

    _State._IsTimerABusy = false;
}

void PMD::HandleTimerB()
{
    _State._IsTimerBBusy = true;

    if (_DriverState.music_flag)
    {
        if (_DriverState.music_flag & 1)
            DriverStart();

        if (_DriverState.music_flag & 2)
            DriverStop();
    }

    if (_State._IsPlaying)
    {
        DriverMain();
        settempo_b();
        syousetu_count();

        _DriverState._OldTimerATime = _State._TimerATime;
    }

    _State._IsTimerBBusy = false;
}

void PMD::DriverMain()
{
    int i;

    _DriverState.loop_work = 3;

    if (_State.x68_flg == 0)
    {
        for (i = 0; i < 3; i++)
        {
            _DriverState._CurrentTrack = i + 1;
            PSGMain(&_SSGTrack[i]);
        }
    }

    _DriverState.fmsel = 0x100;

    for (i = 0; i < 3; i++)
    {
        _DriverState._CurrentTrack = i + 1;
        FMMain(&_FMTrack[i + 3]);
    }

    _DriverState.fmsel = 0;

    for (i = 0; i < 3; i++)
    {
        _DriverState._CurrentTrack = i + 1;
        FMMain(&_FMTrack[i]);
    }

    for (i = 0; i < 3; i++)
    {
        _DriverState._CurrentTrack = 3;
        FMMain(&_ExtensionTrack[i]);
    }

    if (_State.x68_flg == 0)
    {
        RhythmMain(&_RhythmTrack);

        if (_State._IsUsingP86)
            PCM86Main(&_ADPCMTrack);
        else
            ADPCMMain(&_ADPCMTrack);
    }

    if (_State.x68_flg != 0xff)
    {
        for (i = 0; i < 8; i++)
        {
            _DriverState._CurrentTrack = i;
            PPZ8Main(&_PPZ8Track[i]);
        }
    }

    if (_DriverState.loop_work == 0)
        return;

    for (i = 0; i < 6; i++)
    {
        if (_FMTrack[i].loopcheck != 3)
            _FMTrack[i].loopcheck = 0;
    }

    for (i = 0; i < 3; i++)
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

    for (i = 0; i < PCM_CNL_MAX; i++)
    {
        if (_PPZ8Track[i].loopcheck != 3)
            _PPZ8Track[i].loopcheck = 0;
    }

    if (_DriverState.loop_work != 3)
    {
        _State._LoopCount++;

        if (_State._LoopCount == 255)
            _State._LoopCount = 1;
    }
    else
        _State._LoopCount = -1;
}

void PMD::FMMain(Track * track)
{
    if (track->address == nullptr)
        return;

    uint8_t * si = track->address;

    track->leng--;

    if (track->Mask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK & Keyoff
        if ((track->keyoff_flag & 3) == 0)
        {   // Already keyoff?
            if (track->leng <= track->qdat)
            {
                keyoff(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->leng == 0)
    {
        if (track->Mask == 0)
            track->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = FMCommands(track, si);
            }
            else
            if (*si == 0x80)
            {
                track->address = si;
                track->loopcheck = 3;
                track->onkai = 255;

                if (track->partloop == NULL)
                {
                    if (track->Mask)
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
                si = track->partloop;
                track->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {   // ポルタメント
                    si = porta(track, ++si);
                    _DriverState.loop_work &= track->loopcheck;
                    return;
                }
                else
                if (track->Mask == 0)
                {
                    // TONE SET
                    fnumset(track, oshift(track, lfoinit(track, *si++)));

                    track->leng = *si++;
                    si = calc_q(track, si);

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
                    keyon(track);

                    track->keyon_flag++;
                    track->address = si;

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;

                    if (*si == 0xfb)
                    {   // '&'が直後にあったらkeyoffしない
                        track->keyoff_flag = 2;
                    }
                    else
                    {
                        track->keyoff_flag = 0;
                    }
                    _DriverState.loop_work &= track->loopcheck;
                    return;
                }
                else
                {
                    si++;

                    track->fnum = 0;       //休符に設定
                    track->onkai = 255;
                    track->onkai_def = 255;
                    track->leng = *si++;
                    track->keyon_flag++;
                    track->address = si;

                    if (--_DriverState.volpush_flag)
                    {
                        track->volpush = 0;
                    }

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;
                    break;
                }
            }
        }
    }

    if (track->Mask == 0)
    {
        // Finish with LFO & Portament & Fadeout processing
        if (track->hldelay_c)
        {
            if (--track->hldelay_c == 0)
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + (_DriverState._CurrentTrack - 1 + 0xb4)), (uint32_t) track->fmpan);
        }

        if (track->sdelay_c)
        {
            if (--track->sdelay_c == 0)
            {
                if ((track->keyoff_flag & 1) == 0)
                {   // Already keyoffed?
                    keyon(track);
                }
            }
        }

        if (track->lfoswi)
        {
            _DriverState.lfo_switch = track->lfoswi & 8;

            if (track->lfoswi & 3)
            {
                if (lfo(track))
                {
                    _DriverState.lfo_switch |= (track->lfoswi & 3);
                }
            }

            if (track->lfoswi & 0x30)
            {
                lfo_change(track);

                if (lfo(track))
                {
                    lfo_change(track);

                    _DriverState.lfo_switch |= (track->lfoswi & 0x30);
                }
                else
                {
                    lfo_change(track);
                }
            }

            if (_DriverState.lfo_switch & 0x19)
            {
                if (_DriverState.lfo_switch & 8)
                {
                    porta_calc(track);

                }

                Otodasi(track);
            }

            if (_DriverState.lfo_switch & 0x22)
            {
                volset(track);
                _DriverState.loop_work &= track->loopcheck;

                return;
            }
        }

        if (_State._FadeOutSpeed != 0)
            volset(track);
    }

    _DriverState.loop_work &= track->loopcheck;
}

//  KEY OFF
void PMD::keyoff(Track * qq)
{
    if (qq->onkai == 255)
        return;

    kof1(qq);
}

void PMD::kof1(Track * qq)
{
    if (_DriverState.fmsel == 0)
    {
        _DriverState.omote_key[_DriverState._CurrentTrack - 1] = (~qq->slotmask) & (_DriverState.omote_key[_DriverState._CurrentTrack - 1]);
        _OPNA->SetReg(0x28, (uint32_t) ((_DriverState._CurrentTrack - 1) | _DriverState.omote_key[_DriverState._CurrentTrack - 1]));
    }
    else
    {
        _DriverState.ura_key[_DriverState._CurrentTrack - 1] = (~qq->slotmask) & (_DriverState.ura_key[_DriverState._CurrentTrack - 1]);
        _OPNA->SetReg(0x28, (uint32_t) (((_DriverState._CurrentTrack - 1) | _DriverState.ura_key[_DriverState._CurrentTrack - 1]) | 4));
    }
}

// FM Key On
void PMD::keyon(Track * qq)
{
    int  al;

    if (qq->onkai == 255)
        return; // ｷｭｳﾌ ﾉ ﾄｷ

    if (_DriverState.fmsel == 0)
    {
        al = _DriverState.omote_key[_DriverState._CurrentTrack - 1] | qq->slotmask;

        if (qq->sdelay_c)
            al &= qq->sdelay_m;

        _DriverState.omote_key[_DriverState._CurrentTrack - 1] = al;
        _OPNA->SetReg(0x28, (uint32_t) ((_DriverState._CurrentTrack - 1) | al));
    }
    else
    {
        al = _DriverState.ura_key[_DriverState._CurrentTrack - 1] | qq->slotmask;

        if (qq->sdelay_c)
            al &= qq->sdelay_m;

        _DriverState.ura_key[_DriverState._CurrentTrack - 1] = al;
        _OPNA->SetReg(0x28, (uint32_t) (((_DriverState._CurrentTrack - 1) | al) | 4));
    }
}

//  Set [ FNUM/BLOCK + DETUNE + LFO ]
void PMD::Otodasi(Track * track)
{
    if ((track->fnum == 0) || (track->slotmask == 0))
        return;

    int cx = (int) (track->fnum & 0x3800);    // cx=BLOCK
    int ax = (int) (track->fnum) & 0x7ff;    // ax=FNUM

    // Portament/LFO/Detune SET
    ax += track->porta_num + track->detune;

    if ((_DriverState._CurrentTrack == 3) && (_DriverState.fmsel == 0) && (_State.ch3mode != 0x3f))
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

        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + _DriverState._CurrentTrack + 0xa4 - 1), (uint32_t) HIBYTE(ax));
        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + _DriverState._CurrentTrack + 0xa4 - 5), (uint32_t) LOBYTE(ax));
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
    if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
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

//  FM3のmodeを設定する
void PMD::ch3mode_set(Track * qq)
{
    int al;

    if (qq == &_FMTrack[3 - 1])
    {
        al = 1;
    }
    else
    if (qq == &_ExtensionTrack[0])
    {
        al = 2;
    }
    else
    if (qq == &_ExtensionTrack[1])
    {
        al = 4;
    }
    else
    {
        al = 8;
    }

    int ah;

    if ((qq->slotmask & 0xf0) == 0)
    {  // s0
        cm_clear(&ah, &al);
    }
    else
    if (qq->slotmask != 0xf0)
    {
        _DriverState.slot3_flag |= al;
        ah = 0x7f;
    }
    else
    if ((qq->volmask & 0x0f) == 0)
    {
        cm_clear(&ah, &al);
    }
    else
    if ((qq->lfoswi & 1) != 0)
    {
        _DriverState.slot3_flag |= al;
        ah = 0x7f;
    }
    else
    if ((qq->_volmask & 0x0f) == 0)
    {
        cm_clear(&ah, &al);
    }
    else
    if (qq->lfoswi & 0x10)
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

    _OPNA->SetReg(0x27, (uint32_t) (ah & 0xcf)); // Don't reset.

    // When moving to sound effect mode, the pitch is rewritten with the previous FM3 part
    if (ah == 0x3f || qq == &_FMTrack[2])
        return;

    if (_FMTrack[2].Mask == 0)
        Otodasi(&_FMTrack[2]);

    if (qq == &_ExtensionTrack[0])
        return;

    if (_ExtensionTrack[0].Mask == 0)
        Otodasi(&_ExtensionTrack[0]);

    if (qq == &_ExtensionTrack[1])
        return;

    if (_ExtensionTrack[1].Mask == 0)
        Otodasi(&_ExtensionTrack[1]);
}

//  ch3=効果音モード を使用する場合の音程設定
//      input CX:block AX:fnum
void PMD::ch3_special(Track * qq, int ax, int cx)
{
    int    ax_, bh, ch, si;
    int    shiftmask = 0x80;

    si = cx;

    if ((qq->volmask & 0x0f) == 0)
    {
        bh = 0xf0;      // all
    }
    else
    {
        bh = qq->volmask;  // bh=lfo1 mask 4321xxxx
    }

    if ((qq->_volmask & 0x0f) == 0)
    {
        ch = 0xf0;      // all
    }
    else
    {
        ch = qq->_volmask;  // ch=lfo2 mask 4321xxxx
    }

    //  slot  4
    if (qq->slotmask & 0x80)
    {
        ax_ = ax;
        ax += _State.slot_detune4;
        if ((bh & shiftmask) && (qq->lfoswi & 0x01))  ax += qq->lfodat;
        if ((ch & shiftmask) && (qq->lfoswi & 0x10))  ax += qq->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNA->SetReg(0xa6, (uint32_t) HIBYTE(ax));
        _OPNA->SetReg(0xa2, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  3
    if (qq->slotmask & 0x40)
    {
        ax_ = ax;
        ax += _State.slot_detune3;
        if ((bh & shiftmask) && (qq->lfoswi & 0x01))  ax += qq->lfodat;
        if ((ch & shiftmask) && (qq->lfoswi & 0x10))  ax += qq->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNA->SetReg(0xac, (uint32_t) HIBYTE(ax));
        _OPNA->SetReg(0xa8, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  2
    if (qq->slotmask & 0x20)
    {
        ax_ = ax;
        ax += _State.slot_detune2;

        if ((bh & shiftmask) && (qq->lfoswi & 0x01))
            ax += qq->lfodat;

        if ((ch & shiftmask) && (qq->lfoswi & 0x10))
            ax += qq->_lfodat;

        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNA->SetReg(0xae, (uint32_t) HIBYTE(ax));
        _OPNA->SetReg(0xaa, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  1
    if (qq->slotmask & 0x10)
    {
        ax_ = ax;
        ax += _State.slot_detune1;

        if ((bh & shiftmask) && (qq->lfoswi & 0x01)) 
            ax += qq->lfodat;

        if ((ch & shiftmask) && (qq->lfoswi & 0x10))
            ax += qq->_lfodat;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNA->SetReg(0xad, (uint32_t) HIBYTE(ax));
        _OPNA->SetReg(0xa9, (uint32_t) LOBYTE(ax));

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

    if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
    {
        //  FM3の場合は 4つのパート総て設定
        _FMTrack[2].fmpan = qq->fmpan;
        _ExtensionTrack[0].fmpan = qq->fmpan;
        _ExtensionTrack[1].fmpan = qq->fmpan;
        _ExtensionTrack[2].fmpan = qq->fmpan;
    }

    if (qq->Mask == 0)
    {    // パートマスクされているか？
// dl = al;
        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + _DriverState._CurrentTrack + 0xb4 - 1), calc_panout(qq));
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
        qq->onkai = 255;

        if ((qq->lfoswi & 0x11) == 0)
        {
            qq->fnum = 0;      // 音程LFO未使用
        }
    }
}

//  ＦＭ音量設定メイン
void PMD::volset(Track * qq)
{
    if (qq->slotmask == 0)
        return;

    int cl = (qq->volpush) ? qq->volpush - 1 : qq->volume;

    if (qq != &_EffectTrack)
    {  // 効果音の場合はvoldown/fadeout影響無し
//--------------------------------------------------------------------
//  音量down計算
//--------------------------------------------------------------------
        if (_State.fm_voldown)
            cl = ((256 - _State.fm_voldown) * cl) >> 8;

        //--------------------------------------------------------------------
        //  Fadeout計算
        //--------------------------------------------------------------------
        if (_State._FadeOutVolume >= 2)
            cl = ((256 - (_State._FadeOutVolume >> 1)) * cl) >> 8;
    }

    //  音量をcarrierに設定 & 音量LFO処理
    //    input  cl to Volume[0-127]
    //      bl to SlotMask
    int bh = 0;          // Vol Slot Mask
    int bl = qq->slotmask;    // ch=SlotMask Push

    uint8_t vol_tbl[4] = { 0x80, 0x80, 0x80, 0x80 };

    cl = 255 - cl;      // cl=carrierに設定する音量+80H(add)
    bl &= qq->carrier;    // bl=音量を設定するSLOT xxxx0000b
    bh |= bl;

    if (bl & 0x80) vol_tbl[0] = (uint8_t) cl;
    if (bl & 0x40) vol_tbl[1] = (uint8_t) cl;
    if (bl & 0x20) vol_tbl[2] = (uint8_t) cl;
    if (bl & 0x10) vol_tbl[3] = (uint8_t) cl;

    if (cl != 255)
    {
        if (qq->lfoswi & 2)
        {
            bl = qq->volmask;
            bl &= qq->slotmask;    // bl=音量LFOを設定するSLOT xxxx0000b
            bh |= bl;
            fmlfo_sub(qq, qq->lfodat, bl, vol_tbl);
        }

        if (qq->lfoswi & 0x20)
        {
            bl = qq->_volmask;
            bl &= qq->slotmask;
            bh |= bl;
            fmlfo_sub(qq, qq->_lfodat, bl, vol_tbl);
        }
    }

    int dh = 0x4c - 1 + _DriverState._CurrentTrack;    // dh=FM Port Address

    if (bh & 0x80) volset_slot(dh,      qq->slot4, vol_tbl[0]);
    if (bh & 0x40) volset_slot(dh -  8, qq->slot3, vol_tbl[1]);
    if (bh & 0x20) volset_slot(dh -  4, qq->slot2, vol_tbl[2]);
    if (bh & 0x10) volset_slot(dh - 12, qq->slot1, vol_tbl[3]);
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

    _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) al);
}

//  音量LFO用サブ
void PMD::fmlfo_sub(Track *, int al, int bl, uint8_t * vol_tbl)
{
    if (bl & 0x80) vol_tbl[0] = (uint8_t) Limit(vol_tbl[0] - al, 255, 0);
    if (bl & 0x40) vol_tbl[1] = (uint8_t) Limit(vol_tbl[1] - al, 255, 0);
    if (bl & 0x20) vol_tbl[2] = (uint8_t) Limit(vol_tbl[2] - al, 255, 0);
    if (bl & 0x10) vol_tbl[3] = (uint8_t) Limit(vol_tbl[3] - al, 255, 0);
}

// SSG Sound Source Main
void PMD::PSGMain(Track * track)
{
    if (track->address == nullptr)
        return;

    uint8_t * si = track->address;
    int    temp;

    track->leng--;

    // KEYOFF CHECK & Keyoff
    if (track == &_SSGTrack[2] && _DriverState._UsePPS && _State.kshot_dat && track->leng <= track->qdat)
    {
        // PPS 使用時 & SSG 3ch & SSG 効果音鳴らしている場合
        keyoffp(track);
        _OPNA->SetReg((uint32_t) (_DriverState._CurrentTrack + 8 - 1), 0U);    // 強制的に音を止める
        track->keyoff_flag = -1;
    }

    if (track->Mask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        if ((track->keyoff_flag & 3) == 0)
        {    // 既にkeyoffしたか？
            if (track->leng <= track->qdat)
            {
                keyoffp(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->leng == 0)
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
                si = PSGCommands(track, si);
            }
            else
            if (*si == 0x80)
            {
                track->address = si;
                track->loopcheck = 3;
                track->onkai = 255;

                if (track->partloop == nullptr)
                {
                    if (track->Mask)
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
                si = track->partloop;
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
                if (track->Mask)
                {
                    if (!ssgdrum_check(track, *si))
                    {
                        si++;
                        track->fnum = 0;    //休符に設定
                        track->onkai = 255;
                        track->leng = *si++;
                        track->keyon_flag++;
                        track->address = si;

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
                fnumsetp(track, oshiftp(track, lfoinitp(track, *si++)));

                track->leng = *si++;
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
                track->address = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;
                track->keyoff_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらkeyoffしない
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
            lfo_change(track);
            if (lfop(track))
            {
                lfo_change(track);
                _DriverState.lfo_switch |= (track->lfoswi & 0x30);
            }
            else
            {
                lfo_change(track);
            }
        }

        if (_DriverState.lfo_switch & 0x19)
        {
            if (_DriverState.lfo_switch & 0x08)
                porta_calc(track);

            // SSG 3ch で休符かつ SSG Drum 発音中は操作しない
            if (!(track == &_SSGTrack[2] && track->onkai == 255 && _State.kshot_dat && !_DriverState._UsePPS))
                OtodasiP(track);
        }
    }

    temp = soft_env(track);

    if (temp || _DriverState.lfo_switch & 0x22 || (_State._FadeOutSpeed != 0))
    {
        // SSG 3ch で休符かつ SSG Drum 発音中は volume set しない
        if (!(track == &_SSGTrack[2] && track->onkai == 255 && _State.kshot_dat && !_DriverState._UsePPS))
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
    if (track->address == nullptr)
        return;

    uint8_t * si = track->address;

    if (--track->leng == 0)
    {
        uint8_t * bx = _State.rhyadr;

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
                    bx = rhythmon(track, bx, al, &result);

                    if (result == 0)
                        continue;
                }
                else
                    _State.kshot_dat = 0;  //rest

                al = *bx++;

                _State.rhyadr = bx;

                track->leng = al;
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
                    track->address = si;

                    bx = _State.rhyadr = &_State._MData[_State._RythmAddressTable[al]];
                    goto rhyms00;
                }

                // al > 0x80
                si = RhythmCommands(track, si - 1);
            }

            track->address = --si;
            track->loopcheck = 3;

            bx = track->partloop;

            if (bx == nullptr)
            {
                _State.rhyadr = (uint8_t *) &_DriverState.rhydmy;
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

// PSG Rhythm ON
uint8_t * PMD::rhythmon(Track * track, uint8_t * bx, int al, int * result)
{
    if (al & 0x40)
    {
        bx = RhythmCommands(track, bx - 1);
        *result = 0;

        return bx;
    }

    *result = 1;

    if (track->Mask)
    {
        _State.kshot_dat = 0;

        return ++bx;
    }

    al = ((al << 8) + *bx++) & 0x3fff;

    _State.kshot_dat = al;

    if (al == 0)
        return bx;

    _State.rhyadr = bx;

    if (_State._UseSSG)
    {
        for (int cl = 0; cl < 11; cl++)
        {
            if (al & (1 << cl))
            {
                _OPNA->SetReg((uint32_t) rhydat[cl][0], (uint32_t) rhydat[cl][1]);

                int dl = rhydat[cl][2] & _State.rhythmmask;

                if (dl)
                {
                    if (dl < 0x80)
                        _OPNA->SetReg(0x10, (uint32_t) dl);
                    else
                    {
                        _OPNA->SetReg(0x10, 0x84);

                        dl = _State.rhythmmask & 0x08;

                        if (dl)
                            _OPNA->SetReg(0x10, (uint32_t) dl);
                    }
                }
            }
        }
    }

    if (_State._FadeOutVolume)
    {
        if (_State._UseSSG)
        {
            int dl = _State.rhyvol;

            if (_State._FadeOutVolume)
                dl = ((256 - _State._FadeOutVolume) * dl) >> 8;

            _OPNA->SetReg(0x11, (uint32_t) dl);
        }

        if (!_DriverState._UsePPS)
        {
            // No sound during fadeout when using PPS.
            bx = _State.rhyadr;

            return bx;
        }
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
    while (_DriverState._UsePPS && bx_);  // If you use PPSDRV, try playing the second or more notes.

    return _State.rhyadr;
}

//  ＰＳＧ　ドラムス＆効果音　ルーチン
//  Ｆｒｏｍ　ＷＴ２９８
//
//  AL に 効果音Ｎｏ．を入れて　ＣＡＬＬする
//  ppsdrvがあるならそっちを鳴らす
void PMD::effgo(Track * qq, int al)
{
    if (_DriverState._UsePPS)
    {    // PPS を鳴らす
        al |= 0x80;
        if (_EffectState.last_shot_data == al)
        {
            _PPS->Stop();
        }
        else
        {
            _EffectState.last_shot_data = al;
        }
    }

    _EffectState.hosei_flag = 3;        //  音程/音量補正あり (K part)
    eff_main(qq, al);
}

void PMD::eff_on2(Track * qq, int al)
{
    _EffectState.hosei_flag = 1;        //  音程のみ補正あり (n command)
    eff_main(qq, al);
}

void PMD::eff_main(Track * qq, int al)
{
    int    ah, bh, bl;

    if (_State.effflag)
        return;    //  効果音を使用しないモード

    if (_DriverState._UsePPS && (al & 0x80))
    {  // PPS を鳴らす
        if (_EffectState.effon >= 2)
            return;  // 通常効果音発音時は発声させない

        _SSGTrack[2].Mask |= 2;

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

            if (_State._FadeOutVolume)
                bl = (bl * (256 - _State._FadeOutVolume)) >> 8;
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
            if (_DriverState._UsePPS)
                _PPS->Stop();

            _SSGTrack[2].Mask |= 2;    // Part Mask

            efffor(efftbl[al].table);    // １発目を発音

            _EffectState.effon = efftbl[al].priority; // Set priority
        }
    }
}

//  こーかおん　えんそう　めいん
//   Ｆｒｏｍ　ＶＲＴＣ
void PMD::effplay()
{
    if (--_EffectState.effcnt)
    {
        effsweep();      // 新しくセットされない
    }
    else
    {
        efffor(_EffectState.effadr);
    }
}

void PMD::efffor(const int * si)
{
    int    al, ch, cl;

    al = *si++;
    if (al == -1)
    {
        effend();
    }
    else
    {
        _EffectState.effcnt = al;    // カウント数
        cl = *si;
        _OPNA->SetReg(4, (uint32_t) (*si++));    // 周波数セット
        ch = *si;
        _OPNA->SetReg(5, (uint32_t) (*si++));    // 周波数セット
        _EffectState.eswthz = (ch << 8) + cl;

        _State.psnoi_last = _EffectState.eswnhz = *si;
        _OPNA->SetReg(6, (uint32_t) *si++);    // ノイズ

        _OPNA->SetReg(7, ((*si++ << 2) & 0x24) | (_OPNA->GetReg(0x07) & 0xdb));

        _OPNA->SetReg(10, (uint32_t) *si++);    // ボリューム
        _OPNA->SetReg(11, (uint32_t) *si++);    // エンベロープ周波数
        _OPNA->SetReg(12, (uint32_t) *si++);    // 
        _OPNA->SetReg(13, (uint32_t) *si++);    // エンベロープPATTERN

        _EffectState.eswtst = *si++;    // スイープ増分 (TONE)
        _EffectState.eswnst = *si++;    // スイープ増分 (NOISE)

        _EffectState.eswnct = _EffectState.eswnst & 15;    // スイープカウント (NOISE)

        _EffectState.effadr = (int *) si;
    }
}

void PMD::effend()
{
    if (_DriverState._UsePPS)
        _PPS->Stop();

    _OPNA->SetReg(0x0a, 0x00);
    _OPNA->SetReg(0x07, ((_OPNA->GetReg(0x07)) & 0xdb) | 0x24);

    _EffectState.effon = 0;
    _EffectState.psgefcnum = -1;
}

// 普段の処理
void PMD::effsweep()
{
    int    dl;

    _EffectState.eswthz += _EffectState.eswtst;
    _OPNA->SetReg(4, (uint32_t) LOBYTE(_EffectState.eswthz));
    _OPNA->SetReg(5, (uint32_t) HIBYTE(_EffectState.eswthz));

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

    _OPNA->SetReg(6, (uint32_t) _EffectState.eswnhz);
    _State.psnoi_last = _EffectState.eswnhz;
}

//  PDRのswitch
uint8_t * PMD::pdrswitch(Track *, uint8_t * si)
{
    if (!_DriverState._UsePPS)
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
    if (track->address == nullptr)
        return;

    uint8_t * si = track->address;

    track->leng--;

    if (track->Mask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((track->keyoff_flag & 3) == 0)
        {    // 既にkeyoffしたか？
            if (track->leng <= track->qdat)
            {
                keyoffm(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->leng == 0)
    {
        track->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = ADPCMCommands(track, si);
            }
            else
            if (*si == 0x80)
            {
                track->address = si;
                track->loopcheck = 3;
                track->onkai = 255;

                if (track->partloop == NULL)
                {
                    if (track->Mask)
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
                si = track->partloop;
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
                if (track->Mask)
                {
                    si++;
                    track->fnum = 0;    //休符に設定
                    track->onkai = 255;
                    //          qq->onkai_def = 255;
                    track->leng = *si++;
                    track->keyon_flag++;
                    track->address = si;

                    if (--_DriverState.volpush_flag)
                        track->volpush = 0;

                    _DriverState.tieflag = 0;
                    _DriverState.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumsetm(track, oshift(track, lfoinitp(track, *si++)));

                track->leng = *si++;
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
                track->address = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;

                if (*si == 0xfb)
                {   // Do not keyoff if '&' immediately follows
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
            lfo_change(track);

            if (lfop(track))
            {
                lfo_change(track);
                _DriverState.lfo_switch |= (track->lfoswi & 0x30);
            }
            else
                lfo_change(track);
        }

        if (_DriverState.lfo_switch & 0x19)
        {
            if (_DriverState.lfo_switch & 0x08)
                porta_calc(track);

            OtodasiM(track);
        }
    }

    int temp = soft_env(track);

    if ((temp != 0) || _DriverState.lfo_switch & 0x22 || (_State._FadeOutSpeed != 0))
        volsetm(track);

    _DriverState.loop_work &= track->loopcheck;
}

/// <summary>
/// Main PCM86 processing
/// </summary>
void PMD::PCM86Main(Track * track)
{
    if (track->address == nullptr)
        return;

    uint8_t * si = track->address;

    int    temp;

    track->leng--;

    if (track->Mask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((track->keyoff_flag & 3) == 0)
        {    // 既にkeyoffしたか？
            if (track->leng <= track->qdat)
            {
                keyoff8(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->leng == 0)
    {
        while (1)
        {
            //      if(*si > 0x80 && *si != 0xda) {
            if (*si > 0x80)
            {
                si = PCM86Commands(track, si);
            }
            else if (*si == 0x80)
            {
                track->address = si;
                track->loopcheck = 3;
                track->onkai = 255;

                if (track->partloop == nullptr)
                {
                    if (track->Mask)
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
                si = track->partloop;
                track->loopcheck = 1;
            }
            else
            {
                if (track->Mask)
                {
                    si++;
                    track->fnum = 0;    //休符に設定
                    track->onkai = 255;
                    //          qq->onkai_def = 255;
                    track->leng = *si++;
                    track->keyon_flag++;
                    track->address = si;

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

                track->leng = *si++;
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
                track->address = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらkeyoffしない
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
            lfo_change(track);
            if (lfo(track))
            {
                lfo_change(track);
                _DriverState.lfo_switch |= (track->lfoswi & 0x20);
            }
            else
            {
                lfo_change(track);
            }
        }

        temp = soft_env(track);
        if (temp || _DriverState.lfo_switch & 0x22 || _State._FadeOutSpeed)
        {
            volset8(track);
        }
    }
    else
    {
        temp = soft_env(track);
        if (temp || _State._FadeOutSpeed)
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
    if (track->address == nullptr)
        return;

    uint8_t * si = track->address;

    int    temp;

    track->leng--;

    if (track->Mask)
    {
        track->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((track->keyoff_flag & 3) == 0)
        {    // 既にkeyoffしたか？
            if (track->leng <= track->qdat)
            {
                keyoffz(track);
                track->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (track->leng == 0)
    {
        track->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = PPZ8Commands(track, si);
            }
            else if (*si == 0x80)
            {
                track->address = si;
                track->loopcheck = 3;
                track->onkai = 255;

                if (track->partloop == nullptr)
                {
                    if (track->Mask)
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
                si = track->partloop;
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
                if (track->Mask)
                {
                    si++;
                    track->fnum = 0;    //休符に設定
                    track->onkai = 255;
                    //          qq->onkai_def = 255;
                    track->leng = *si++;
                    track->keyon_flag++;
                    track->address = si;

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

                track->leng = *si++;
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
                track->address = si;

                _DriverState.tieflag = 0;
                _DriverState.volpush_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらkeyoffしない
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
            lfo_change(track);
            if (lfop(track))
            {
                lfo_change(track);
                _DriverState.lfo_switch |= (track->lfoswi & 0x30);
            }
            else
            {
                lfo_change(track);
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
    if (temp || _DriverState.lfo_switch & 0x22 || _State._FadeOutSpeed)
    {
        volsetz(track);
    }

    _DriverState.loop_work &= track->loopcheck;
}

//  PCM KEYON
void PMD::keyonm(Track * qq)
{
    if (qq->onkai == 255)
        return;

    _OPNA->SetReg(0x101, 0x02);  // PAN=0 / x8 bit mode
    _OPNA->SetReg(0x100, 0x21);  // PCM RESET

    _OPNA->SetReg(0x102, (uint32_t) LOBYTE(_State.pcmstart));
    _OPNA->SetReg(0x103, (uint32_t) HIBYTE(_State.pcmstart));
    _OPNA->SetReg(0x104, (uint32_t) LOBYTE(_State.pcmstop));
    _OPNA->SetReg(0x105, (uint32_t) HIBYTE(_State.pcmstop));

    if ((_DriverState.pcmrepeat1 | _DriverState.pcmrepeat2) == 0)
    {
        _OPNA->SetReg(0x100, 0xa0);  // PCM PLAY(non_repeat)
        _OPNA->SetReg(0x101, (uint32_t) (qq->fmpan | 2));  // PAN SET / x8 bit mode
    }
    else
    {
        _OPNA->SetReg(0x100, 0xb0);  // PCM PLAY(repeat)
        _OPNA->SetReg(0x101, (uint32_t) (qq->fmpan | 2));  // PAN SET / x8 bit mode

        _OPNA->SetReg(0x102, (uint32_t) LOBYTE(_DriverState.pcmrepeat1));
        _OPNA->SetReg(0x103, (uint32_t) HIBYTE(_DriverState.pcmrepeat1));
        _OPNA->SetReg(0x104, (uint32_t) LOBYTE(_DriverState.pcmrepeat2));
        _OPNA->SetReg(0x105, (uint32_t) HIBYTE(_DriverState.pcmrepeat2));
    }
}

//  PCM KEYON (PMD86)
void PMD::keyon8(Track * qq)
{
    if (qq->onkai == 255) return;
    _P86->Play();
}

//  PPZ KEYON
void PMD::keyonz(Track * qq)
{
    if (qq->onkai == 255) return;

    if ((qq->voicenum & 0x80) == 0)
    {
        _PPZ8->Play(_DriverState._CurrentTrack, 0, qq->voicenum, 0, 0);
    }
    else
    {
        _PPZ8->Play(_DriverState._CurrentTrack, 1, qq->voicenum & 0x7f, 0, 0);
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
    _OPNA->SetReg(0x109, (uint32_t) LOBYTE(bx));
    _OPNA->SetReg(0x10a, (uint32_t) HIBYTE(bx));
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

    // TONE SET
    _PPZ8->SetPitchFrequency(_DriverState._CurrentTrack, cx);
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
    if (_State._FadeOutVolume)
        al = (((256 - _State._FadeOutVolume) * (256 - _State._FadeOutVolume) >> 8) * al) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        _OPNA->SetReg(0x10b, 0);
        return;
    }

    if (qq->envf == -1)
    {
        //  拡張版 音量=al*(eenv_vol+1)/16
        if (qq->eenv_volume == 0)
        {
            _OPNA->SetReg(0x10b, 0);
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
                _OPNA->SetReg(0x10b, 0);
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
        _OPNA->SetReg(0x10b, (uint32_t) al);
        return;
    }

    int dx = (qq->lfoswi & 2) ? qq->lfodat : 0;

    if (qq->lfoswi & 0x20)
        dx += qq->_lfodat;

    if (dx >= 0)
    {
        al += dx;

        if (al & 0xff00)
            _OPNA->SetReg(0x10b, 255);
        else
            _OPNA->SetReg(0x10b, (uint32_t) al);
    }
    else
    {
        al += dx;

        if (al < 0)
            _OPNA->SetReg(0x10b, 0);
        else
            _OPNA->SetReg(0x10b, (uint32_t) al);
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
    if (_State._FadeOutVolume != 0)
        al = ((256 - _State._FadeOutVolume) * al) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        _OPNA->SetReg(0x10b, 0);
        return;
    }

    if (qq->envf == -1)
    {
        //  拡張版 音量=al*(eenv_vol+1)/16
        if (qq->eenv_volume == 0)
        {
            _OPNA->SetReg(0x10b, 0);
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
                _OPNA->SetReg(0x10b, 0);
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
    if (_State._FadeOutVolume != 0)
        al = ((256 - _State._FadeOutVolume) * al) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        _PPZ8->SetVolume(_DriverState._CurrentTrack, 0);
        _PPZ8->Stop(_DriverState._CurrentTrack);
        return;
    }

    if (qq->envf == -1)
    {
        //  拡張版 音量=al*(eenv_vol+1)/16
        if (qq->eenv_volume == 0)
        {
            //*@    ppz8->SetVol(pmdwork._CurrentPart, 0);
            _PPZ8->Stop(_DriverState._CurrentTrack);
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
                _PPZ8->Stop(_DriverState._CurrentTrack);
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
        _PPZ8->SetVolume(_DriverState._CurrentTrack, al >> 4);
    else
        _PPZ8->Stop(_DriverState._CurrentTrack);
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

    if (qq->Mask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

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

    qq->leng = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->leng;    // 商
    qq->porta_num3 = ax % qq->leng;    // 余り
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
    qq->address = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {      // '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }

    _DriverState.loop_work &= qq->loopcheck;
    return si;
}

//  ポルタメント(PPZ)
uint8_t * PMD::portaz(Track * qq, uint8_t * si)
{
    int    ax, al_, bx_;

    if (qq->Mask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

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

    qq->leng = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->leng;    // 商
    qq->porta_num3 = ax % qq->leng;    // 余り
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
    qq->address = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {      // '&'が直後にあったらkeyoffしない
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

    if (_DriverState.pcmrelease != 0x8000)
    {
        // PCM RESET
        _OPNA->SetReg(0x100, 0x21);

        _OPNA->SetReg(0x102, (uint32_t) LOBYTE(_DriverState.pcmrelease));
        _OPNA->SetReg(0x103, (uint32_t) HIBYTE(_DriverState.pcmrelease));

        // Stop ADDRESS for Release
        _OPNA->SetReg(0x104, (uint32_t) LOBYTE(_State.pcmstop));
        _OPNA->SetReg(0x105, (uint32_t) HIBYTE(_State.pcmstop));

        // PCM PLAY(non_repeat)
        _OPNA->SetReg(0x100, 0xa0);
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

//  リピート設定
uint8_t * PMD::pcmrepeat_set(Track *, uint8_t * si)
{
    int    ax;

    ax = *(int16_t *) si;
    si += 2;

    if (ax >= 0)
    {
        ax += _State.pcmstart;
    }
    else
    {
        ax += _State.pcmstop;
    }

    _DriverState.pcmrepeat1 = ax;

    ax = *(int16_t *) si;
    si += 2;
    if (ax > 0)
    {
        ax += _State.pcmstart;
    }
    else
    {
        ax += _State.pcmstop;
    }

    _DriverState.pcmrepeat2 = ax;

    ax = *(uint16_t *) si;
    si += 2;
    if (ax < 0x8000)
    {
        ax += _State.pcmstart;
    }
    else if (ax > 0x8000)
    {
        ax += _State.pcmstop;
    }

    _DriverState.pcmrelease = ax;
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

    if ((qq->voicenum & 0x80) == 0)
    {
        LoopStart = *(int16_t *) data;
        data += 2;

        if (LoopStart < 0)
            LoopStart = (int) (_PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].Size - LoopStart);

        LoopEnd = *(int16_t *) data;
        data += 2;

        if (LoopEnd < 0)
            LoopEnd = (int) (_PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].Size - LoopStart);
    }
    else
    {
        LoopStart = *(int16_t *) data;
        data += 2;

        if (LoopStart < 0)
            LoopStart = (int) (_PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].Size - LoopStart);

        LoopEnd = *(int16_t *) data;
        data += 2;

        if (LoopEnd < 0)
            LoopEnd = (int) (_PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].Size - LoopEnd);
    }

    _PPZ8->SetLoop(_DriverState._CurrentTrack, (uint32_t) LoopStart, (uint32_t) LoopEnd);

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
    _PPZ8->SetPan(_DriverState._CurrentTrack, qq->fmpan);
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
    _PPZ8->SetPan(_DriverState._CurrentTrack, qq->fmpan);
    return si;
}

uint8_t * PMD::comatm(Track * qq, uint8_t * si)
{
    qq->voicenum = *si++;
    _State.pcmstart = pcmends.Address[qq->voicenum][0];
    _State.pcmstop = pcmends.Address[qq->voicenum][1];
    _DriverState.pcmrepeat1 = 0;
    _DriverState.pcmrepeat2 = 0;
    _DriverState.pcmrelease = 0x8000;
    return si;
}

uint8_t * PMD::comat8(Track * qq, uint8_t * si)
{
    qq->voicenum = *si++;
    _P86->SetNeiro(qq->voicenum);
    return si;
}

uint8_t * PMD::comatz(Track * qq, uint8_t * si)
{
    qq->voicenum = *si++;

    if ((qq->voicenum & 0x80) == 0)
    {
        _PPZ8->SetLoop(_DriverState._CurrentTrack, _PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].LoopStart, _PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].LoopEnd);
        _PPZ8->SetSourceRate(_DriverState._CurrentTrack, _PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].SampleRate);
    }
    else
    {
        _PPZ8->SetLoop(_DriverState._CurrentTrack, _PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].LoopStart, _PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].LoopEnd);
        _PPZ8->SetSourceRate(_DriverState._CurrentTrack, _PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].SampleRate);
    }
    return si;
}

/// <summary>
/// Decides to stop the SSG drum and reset the SSG.
/// </summary>
bool PMD::ssgdrum_check(Track * track, int al)
{
    // Do not stop the drum during the SSG mask. SSG drums are not playing.
    if ((track->Mask & 1) || ((track->Mask & 2) == 0))
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

    track->Mask &= 0xFD;

    return (track->Mask == 0);
}

uint8_t * PMD::FMCommands(Track * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comat(ps, si); break;
        case 0xfe: ps->qdata = *si++; ps->qdat3 = 0; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: ps->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(ps, si); break;
        case 0xf8: si = comedloop(ps, si); break;
        case 0xf7: si = comexloop(ps, si); break;
        case 0xf6: ps->partloop = si; break;
        case 0xf5: ps->shift = *(int8_t *) si++; break;
        case 0xf4: if ((ps->volume += 4) > 127) ps->volume = 127; break;
        case 0xf3: if (ps->volume < 4) ps->volume = 0; else ps->volume -= 4; break;
        case 0xf2: si = lfoset(ps, si); break;
        case 0xf1: si = lfoswitch(ps, si); ch3_setting(ps); break;
        case 0xf0: si += 4; break;

        case 0xef: _OPNA->SetReg((uint32_t) (_DriverState.fmsel + *si), (uint32_t) (*(si + 1))); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = panset(ps, si); break;        // FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: ps->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: ps->hldelay = *si++; break;
            //追加 for V2.3
        case 0xe3: if ((ps->volume += *si++) > 127) ps->volume = 127; break;
        case 0xe2:
            if (ps->volume < *si) ps->volume = 0; else ps->volume -= *si;
            si++;
            break;
            //
        case 0xe1: si = hlfo_set(ps, si); break;
        case 0xe0: _State.port22h = *si; _OPNA->SetReg(0x22, *si++); break;
            //
        case 0xdf: _State.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_fm(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;
            //
        case 0xda: si = porta(ps, si); break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: ps->mdspd = ps->mdspd2 = *si++; ps->mdepth = *(int8_t *) si++; break;
        case 0xd5: ps->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(ps, si); break;
        case 0xd3: si = fm_efct_set(ps, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State._FadeOutSpeed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
        case 0xcf: si = slotmask_set(ps, si); break;
        case 0xce: si += 6; break;
        case 0xcd: si += 5; break;
        case 0xcc: si++; break;
        case 0xcb: ps->lfo_wave = *si++; break;
        case 0xca:
            ps->extendmode = (ps->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9: si++; break;
        case 0xc8: si = slotdetune_set(ps, si); break;
        case 0xc7: si = slotdetune_set2(ps, si); break;
        case 0xc6: si = fm3_extpartset(ps, si); break;
        case 0xc5: si = volmask_set(ps, si); break;
        case 0xc4: ps->qdatb = *si++; break;
        case 0xc3: si = panset_ex(ps, si); break;
        case 0xc2: ps->delay = ps->delay2 = *si++; lfoinit_main(ps); break;
        case 0xc1: break;
        case 0xc0: si = fm_mml_part_mask(ps, si); break;
        case 0xbf: lfo_change(ps); si = lfoset(ps, si); lfo_change(ps); break;
        case 0xbe: si = _lfoswitch(ps, si); ch3_setting(ps); break;
        case 0xbd:
            lfo_change(ps);
            ps->mdspd = ps->mdspd2 = *si++;
            ps->mdepth = *(int8_t *) si++;
            lfo_change(ps);
            break;

        case 0xbc: lfo_change(ps); ps->lfo_wave = *si++; lfo_change(ps); break;
        case 0xbb:
            lfo_change(ps);
            ps->extendmode = (ps->extendmode & 0xfd) | ((*si++ & 1) << 1);
            lfo_change(ps);
            break;

        case 0xba: si = _volmask_set(ps, si); break;
        case 0xb9:
            lfo_change(ps);
            ps->delay = ps->delay2 = *si++; lfoinit_main(ps);
            lfo_change(ps);
            break;

        case 0xb8: si = tl_set(ps, si); break;
        case 0xb7: si = mdepth_count(ps, si); break;
        case 0xb6: si = fb_set(ps, si); break;
        case 0xb5:
            ps->sdelay_m = (~(*si++) << 4) & 0xf0;
            ps->sdelay_c = ps->sdelay = *si++;
            break;

        case 0xb4: si += 16; break;
        case 0xb3: ps->qdat2 = *si++; break;
        case 0xb2: ps->shift_def = *(int8_t *) si++; break;
        case 0xb1: ps->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::PSGCommands(Track * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si++; break;
        case 0xfe: ps->qdata = *si++; ps->qdat3 = 0; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: ps->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(ps, si); break;
        case 0xf8: si = comedloop(ps, si); break;
        case 0xf7: si = comexloop(ps, si); break;
        case 0xf6: ps->partloop = si; break;
        case 0xf5: ps->shift = *(int8_t *) si++; break;
        case 0xf4: if (ps->volume < 15) ps->volume++; break;
        case 0xf3: if (ps->volume > 0) ps->volume--; break;
        case 0xf2: si = lfoset(ps, si); break;
        case 0xf1: si = lfoswitch(ps, si); break;
        case 0xf0: si = psgenvset(ps, si); break;

        case 0xef: _OPNA->SetReg(*si, *(si + 1)); si += 2; break;
        case 0xee: _State.psnoi = *si++; break;
        case 0xed: ps->psgpat = *si++; break;
            //
        case 0xec: si++; break;
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: ps->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: si++; break;
            //追加 for V2.3
            // saturate
        case 0xe3: ps->volume += *si++; if (ps->volume > 15) ps->volume = 15; break;
        case 0xe2: ps->volume -= *si++; if (ps->volume < 0) ps->volume = 0; break;

            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _State.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_psg(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;
            //
        case 0xda: si = portap(ps, si); break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: ps->mdspd = ps->mdspd2 = *si++; ps->mdepth = *(int8_t *) si++; break;
        case 0xd5: ps->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(ps, si); break;
        case 0xd3: si = fm_efct_set(ps, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State._FadeOutSpeed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si = psgnoise_move(si); break;
            //
        case 0xcf: si++; break;
        case 0xce: si += 6; break;
        case 0xcd: si = extend_psgenvset(ps, si); break;
        case 0xcc:
            ps->extendmode = (ps->extendmode & 0xfe) | (*si++ & 1);
            break;

        case 0xcb: ps->lfo_wave = *si++; break;
        case 0xca:
            ps->extendmode = (ps->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            ps->extendmode = (ps->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: ps->qdatb = *si++; break;
        case 0xc3: si += 2; break;
        case 0xc2: ps->delay = ps->delay2 = *si++; lfoinit_main(ps); break;
        case 0xc1: break;
        case 0xc0: si = ssg_mml_part_mask(ps, si); break;
        case 0xbf: lfo_change(ps); si = lfoset(ps, si); lfo_change(ps); break;
        case 0xbe:
            ps->lfoswi = (ps->lfoswi & 0x8f) | ((*si++ & 7) << 4);

            lfo_change(ps);
            lfoinit_main(ps);
            lfo_change(ps);
            break;

        case 0xbd:
            lfo_change(ps);
            ps->mdspd = ps->mdspd2 = *si++;
            ps->mdepth = *(int8_t *) si++;
            lfo_change(ps);
            break;

        case 0xbc:
            lfo_change(ps);

            ps->lfo_wave = *si++;

            lfo_change(ps);
            break;

        case 0xbb:
            lfo_change(ps);

            ps->extendmode = (ps->extendmode & 0xfd) | ((*si++ & 1) << 1);

            lfo_change(ps);
            break;

        case 0xba: si++; break;
        case 0xb9:
            lfo_change(ps);

            ps->delay = ps->delay2 = *si++;
            lfoinit_main(ps);

// FIXME    break;

            lfo_change(ps);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(ps, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;
        case 0xb3: ps->qdat2 = *si++; break;
        case 0xb2: ps->shift_def = *(int8_t *) si++; break;
        case 0xb1: ps->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::RhythmCommands(Track * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si++; break;
        case 0xfe: si++; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: ps->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(ps, si); break;
        case 0xf8: si = comedloop(ps, si); break;
        case 0xf7: si = comexloop(ps, si); break;
        case 0xf6: ps->partloop = si; break;
        case 0xf5: si++; break;
        case 0xf4: if (ps->volume < 15) ps->volume++; break;
        case 0xf3: if (ps->volume > 0) ps->volume--; break;
        case 0xf2: si += 4; break;
        case 0xf1: si = pdrswitch(ps, si); break;
        case 0xf0: si += 4; break;

        case 0xef: _OPNA->SetReg(*si, *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
            //
        case 0xec: si++; break;
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: si++; break;
            //追加 for V2.3
        case 0xe3: if ((ps->volume + *si) < 16) ps->volume += *si; si++; break;
        case 0xe2: if ((ps->volume - *si) >= 0) ps->volume -= *si; si++; break;
            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _State.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_psg(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;
            //
        case 0xda: si++; break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: si += 2; break;
        case 0xd5: ps->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(ps, si); break;
        case 0xd3: si = fm_efct_set(ps, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State._FadeOutSpeed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
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
        case 0xc0: si = rhythm_mml_part_mask(ps, si); break;
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

uint8_t * PMD::ADPCMCommands(Track * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comatm(ps, si); break;
        case 0xfe: ps->qdata = *si++; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;
        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: ps->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(ps, si); break;
        case 0xf8: si = comedloop(ps, si); break;
        case 0xf7: si = comexloop(ps, si); break;
        case 0xf6: ps->partloop = si; break;
        case 0xf5: ps->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (ps->volume < (255 - 16)) ps->volume += 16;
            else ps->volume = 255;
            break;

        case 0xf3: if (ps->volume < 16) ps->volume = 0; else ps->volume -= 16; break;
        case 0xf2: si = lfoset(ps, si); break;
        case 0xf1: si = lfoswitch(ps, si); break;
        case 0xf0: si = psgenvset(ps, si); break;

        case 0xef: _OPNA->SetReg((uint32_t) (0x100 + *si), (uint32_t) (*(si + 1))); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = pansetm(ps, si); break;        // FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: ps->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: si++; break;
            //追加 for V2.3
        case 0xe3:
            if (ps->volume < (255 - (*si))) ps->volume += (*si);
            else ps->volume = 255;
            si++;
            break;

        case 0xe2:
            if (ps->volume < *si) ps->volume = 0; else ps->volume -= *si;
            si++;
            break;
            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _State.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_pcm(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;
            //
        case 0xda: si = portam(ps, si); break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: ps->mdspd = ps->mdspd2 = *si++; ps->mdepth = *(int8_t *) si++; break;
        case 0xd5: ps->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(ps, si); break;
        case 0xd3: si = fm_efct_set(ps, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State._FadeOutSpeed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
        case 0xcf: si++; break;
        case 0xce: si = pcmrepeat_set(ps, si); break;
        case 0xcd: si = extend_psgenvset(ps, si); break;
        case 0xcc: si++; break;
        case 0xcb: ps->lfo_wave = *si++; break;
        case 0xca:
            ps->extendmode = (ps->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            ps->extendmode = (ps->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: ps->qdatb = *si++; break;
        case 0xc3: si = pansetm_ex(ps, si); break;
        case 0xc2: ps->delay = ps->delay2 = *si++; lfoinit_main(ps); break;
        case 0xc1: break;
        case 0xc0: si = pcm_mml_part_mask(ps, si); break;
        case 0xbf: lfo_change(ps); si = lfoset(ps, si); lfo_change(ps); break;
        case 0xbe: si = _lfoswitch(ps, si); break;
        case 0xbd:
            lfo_change(ps);

            ps->mdspd = ps->mdspd2 = *si++;
            ps->mdepth = *(int8_t *) si++;

            lfo_change(ps);
            break;

        case 0xbc:
            lfo_change(ps);

            ps->lfo_wave = *si++;

            lfo_change(ps);
            break;

        case 0xbb:
            lfo_change(ps);

            ps->extendmode = (ps->extendmode & 0xfd) | ((*si++ & 1) << 1);

            lfo_change(ps);
            break;

        case 0xba: si = _volmask_set(ps, si); break;
        case 0xb9:
            lfo_change(ps);

            ps->delay = ps->delay2 = *si++;
            lfoinit_main(ps);
// FIXME    break;

            lfo_change(ps);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(ps, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si = ppz_extpartset(ps, si); break;
        case 0xb3: ps->qdat2 = *si++; break;
        case 0xb2: ps->shift_def = *(int8_t *) si++; break;
        case 0xb1: ps->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::PCM86Commands(Track * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comat8(ps, si); break;
        case 0xfe: ps->qdata = *si++; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;
        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: ps->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(ps, si); break;
        case 0xf8: si = comedloop(ps, si); break;
        case 0xf7: si = comexloop(ps, si); break;
        case 0xf6: ps->partloop = si; break;
        case 0xf5: ps->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (ps->volume < (255 - 16)) ps->volume += 16;
            else ps->volume = 255;
            break;

        case 0xf3: if (ps->volume < 16) ps->volume = 0; else ps->volume -= 16; break;
        case 0xf2: si = lfoset(ps, si); break;
        case 0xf1: si = lfoswitch(ps, si); break;
        case 0xf0: si = psgenvset(ps, si); break;

        case 0xef: _OPNA->SetReg((uint32_t) (0x100 + *si), (uint32_t) (*(si + 1))); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = panset8(ps, si); break;        // FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: ps->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: si++; break;
            //追加 for V2.3
        case 0xe3:
            if (ps->volume < (255 - (*si))) ps->volume += (*si);
            else ps->volume = 255;
            si++;
            break;

        case 0xe2:
            if (ps->volume < *si) ps->volume = 0; else ps->volume -= *si;
            si++;
            break;
            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _State.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_pcm(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;
            //
        case 0xda: si++; break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: ps->mdspd = ps->mdspd2 = *si++; ps->mdepth = *(int8_t *) si++; break;
        case 0xd5: ps->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(ps, si); break;
        case 0xd3: si = fm_efct_set(ps, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State._FadeOutSpeed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
        case 0xcf: si++; break;
        case 0xce: si = pcmrepeat_set8(ps, si); break;
        case 0xcd: si = extend_psgenvset(ps, si); break;
        case 0xcc: si++; break;
        case 0xcb: ps->lfo_wave = *si++; break;
        case 0xca:
            ps->extendmode = (ps->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            ps->extendmode = (ps->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: ps->qdatb = *si++; break;
        case 0xc3: si = panset8_ex(ps, si); break;
        case 0xc2: ps->delay = ps->delay2 = *si++; lfoinit_main(ps); break;
        case 0xc1: break;
        case 0xc0: si = pcm_mml_part_mask8(ps, si); break;
        case 0xbf: si += 4; break;
        case 0xbe: si++; break;
        case 0xbd: si += 2; break;
        case 0xbc: si++; break;
        case 0xbb: si++; break;
        case 0xba: si++; break;
        case 0xb9: si++; break;
        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(ps, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si = ppz_extpartset(ps, si); break;
        case 0xb3: ps->qdat2 = *si++; break;
        case 0xb2: ps->shift_def = *(int8_t *) si++; break;
        case 0xb1: ps->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::PPZ8Commands(Track * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comatz(ps, si); break;
        case 0xfe: ps->qdata = *si++; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _DriverState.tieflag |= 1; break;
        case 0xfa: ps->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(ps, si); break;
        case 0xf8: si = comedloop(ps, si); break;
        case 0xf7: si = comexloop(ps, si); break;
        case 0xf6: ps->partloop = si; break;
        case 0xf5: ps->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (ps->volume < (255 - 16))
                ps->volume += 16;
            else
                ps->volume = 255;
            break;

        case 0xf3: if (ps->volume < 16) ps->volume = 0; else ps->volume -= 16; break;
        case 0xf2: si = lfoset(ps, si); break;
        case 0xf1: si = lfoswitch(ps, si); break;
        case 0xf0: si = psgenvset(ps, si); break;

        case 0xef: _OPNA->SetReg((uint32_t) (_DriverState.fmsel + *si), (uint32_t) *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = pansetz(ps, si); break;        // FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: ps->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: si++; break;
            //追加 for V2.3
        case 0xe3:
            if (ps->volume < (255 - (*si))) ps->volume += (*si);
            else ps->volume = 255;
            si++;
            break;

        case 0xe2:
            if (ps->volume < *si) ps->volume = 0; else ps->volume -= *si;
            si++;
            break;
            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _State.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_pcm(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _State.status = *si++; break;
        case 0xdb: _State.status += *si++; break;
            //
        case 0xda: si = portaz(ps, si); break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: ps->mdspd = ps->mdspd2 = *si++; ps->mdepth = *(int8_t *) si++; break;
        case 0xd5: ps->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(ps, si); break;
        case 0xd3: si = fm_efct_set(ps, si); break;
        case 0xd2:
            _State.fadeout_flag = 1;
            _State._FadeOutSpeed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
        case 0xcf: si++; break;
        case 0xce: si = ppzrepeat_set(ps, si); break;
        case 0xcd: si = extend_psgenvset(ps, si); break;
        case 0xcc: si++; break;
        case 0xcb: ps->lfo_wave = *si++; break;
        case 0xca:
            ps->extendmode = (ps->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            ps->extendmode = (ps->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: ps->qdatb = *si++; break;
        case 0xc3: si = pansetz_ex(ps, si); break;
        case 0xc2: ps->delay = ps->delay2 = *si++; lfoinit_main(ps); break;
        case 0xc1: break;
        case 0xc0: si = ppz_mml_part_mask(ps, si); break;
        case 0xbf: lfo_change(ps); si = lfoset(ps, si); lfo_change(ps); break;
        case 0xbe: si = _lfoswitch(ps, si); break;
        case 0xbd:
            lfo_change(ps);
            ps->mdspd = ps->mdspd2 = *si++;
            ps->mdepth = *(int8_t *) si++;
            lfo_change(ps);
            break;

        case 0xbc: lfo_change(ps); ps->lfo_wave = *si++; lfo_change(ps); break;
        case 0xbb:
            lfo_change(ps);
            ps->extendmode = (ps->extendmode & 0xfd) | ((*si++ & 1) << 1);
            lfo_change(ps);
            break;

        case 0xba: si = _volmask_set(ps, si); break;
        case 0xb9:
            lfo_change(ps);

            ps->delay = ps->delay2 = *si++;
            lfoinit_main(ps);
// FIXME     break;
            lfo_change(ps);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(ps, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;
        case 0xb3: ps->qdat2 = *si++; break;
        case 0xb2: ps->shift_def = *(int8_t *) si++; break;
        case 0xb1: ps->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

//  COMMAND '@' [PROGRAM CHANGE]
uint8_t * PMD::comat(Track * qq, uint8_t * si)
{
    uint8_t * bx;
    int    al, dl;

    qq->voicenum = al = *si++;
    dl = qq->voicenum;

    if (qq->Mask == 0)
    {  // パートマスクされているか？
        neiroset(qq, dl);
        return si;
    }
    else
    {
        bx = toneadr_calc(qq, dl);
        qq->alg_fb = dl = bx[24];
        bx += 4;

        // tl設定
        qq->slot1 = bx[0];
        qq->slot3 = bx[1];
        qq->slot2 = bx[2];
        qq->slot4 = bx[3];

        //  FM3chで、マスクされていた場合、fm3_alg_fbを設定
        if (_DriverState._CurrentTrack == 3 && qq->neiromask)
        {
            if (_DriverState.fmsel == 0)
            {
                // in. dl = alg/fb
                if ((qq->slotmask & 0x10) == 0)
                {
                    al = _DriverState.fm3_alg_fb & 0x38;    // fbは前の値を使用
                    dl = (dl & 7) | al;
                }

                _DriverState.fm3_alg_fb = dl;
                qq->alg_fb = al;
            }
        }
    }
    return si;
}

//  音色の設定
//    INPUTS  -- [PARTB]
//      -- dl [TONE_NUMBER]
//      -- di [PART_DATA_ADDRESS]
void PMD::neiroset(Track * qq, int dl)
{
    uint8_t * bx;
    int    ah, al, dh;

    bx = toneadr_calc(qq, dl);
    if (MuteFMPart(qq))
    {
        // neiromask=0の時 (TLのworkのみ設定)
        bx += 4;

        // tl設定
        qq->slot1 = bx[0];
        qq->slot3 = bx[1];
        qq->slot2 = bx[2];
        qq->slot4 = bx[3];
        return;
    }

    //=========================================================================
    //  音色設定メイン
    //=========================================================================
    //-------------------------------------------------------------------------
    //  AL/FBを設定
    //-------------------------------------------------------------------------

    dh = 0xb0 - 1 + _DriverState._CurrentTrack;

    if (_DriverState.af_check)
    {    // ALG/FBは設定しないmodeか？
        dl = qq->alg_fb;
    }
    else
    {
        dl = bx[24];
    }

    if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
    {
        if (_DriverState.af_check != 0)
        {  // ALG/FBは設定しないmodeか？
            dl = _DriverState.fm3_alg_fb;
        }
        else
        {
            if ((qq->slotmask & 0x10) == 0)
            {  // slot1を使用しているか？
                dl = (_DriverState.fm3_alg_fb & 0x38) | (dl & 7);
            }
            _DriverState.fm3_alg_fb = dl;
        }
    }

    _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    qq->alg_fb = dl;
    dl &= 7;    // dl = algo

    //-------------------------------------------------------------------------
    //  Carrierの位置を調べる (VolMaskにも設定)
    //-------------------------------------------------------------------------

    if ((qq->volmask & 0x0f) == 0)
    {
        qq->volmask = carrier_table[dl];
    }

    if ((qq->_volmask & 0x0f) == 0)
    {
        qq->_volmask = carrier_table[dl];
    }

    qq->carrier = carrier_table[dl];
    ah = carrier_table[dl + 8];  // slot2/3の逆転データ(not済み)
    al = qq->neiromask;
    ah &= al;        // AH=TL用のmask / AL=その他用のmask

    //-------------------------------------------------------------------------
    //  各音色パラメータを設定 (TLはモジュレータのみ)
    //-------------------------------------------------------------------------

    dh = 0x30 - 1 + _DriverState._CurrentTrack;
    dl = *bx++;        // DT/ML
    if (al & 0x80) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // TL
    if (ah & 0x80) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x40) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh),(uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x20) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x10) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // KS/AR
    if (al & 0x08) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // AM/DR
    if (al & 0x80) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // SR
    if (al & 0x08) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // SL/RR
    if (al & 0x80)
    {
        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    }
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
    dh += 4;

    /*
        dl = *bx++;        // SL/RR
        if(al & 0x80) opna->SetReg((uint32_t) (pmdwork.fmsel + dh), (uint32_t) dl);
        dh+=4;

        dl = *bx++;
        if(al & 0x40) opna->SetReg((uint32_t) (pmdwork.fmsel + dh), (uint32_t) dl);
        dh+=4;

        dl = *bx++;
        if(al & 0x20) opna->SetReg((uint32_t) (pmdwork.fmsel + dh), (uint32_t) dl);
        dh+=4;

        dl = *bx++;
        if(al & 0x10) opna->SetReg((uint32_t) (pmdwork.fmsel + dh), (uint32_t) dl);
        dh+=4;
    */

    //-------------------------------------------------------------------------
    //  SLOT毎のTLをワークに保存
    //-------------------------------------------------------------------------
    bx -= 20;
    qq->slot1 = bx[0];
    qq->slot3 = bx[1];
    qq->slot2 = bx[2];
    qq->slot4 = bx[3];
}

// Completely muting the [PartB] part (TL=127 and RR=15 and KEY-OFF). cy=1 ･･･ All slots are neiromasked
int PMD::MuteFMPart(Track * qq)
{
    if (qq->neiromask == 0)
        return 1;

    int dh = _DriverState._CurrentTrack + 0x40 - 1;

    if (qq->neiromask & 0x80)
    {
        _OPNA->SetReg((uint32_t) ( _DriverState.fmsel         + dh), 127);
        _OPNA->SetReg((uint32_t) ((_DriverState.fmsel + 0x40) + dh), 127);
    }

    dh += 4;

    if (qq->neiromask & 0x40)
    {
        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), 127);
        _OPNA->SetReg((uint32_t) ((_DriverState.fmsel + 0x40) + dh), 127);
    }

    dh += 4;

    if (qq->neiromask & 0x20)
    {
        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), 127);
        _OPNA->SetReg((uint32_t) ((_DriverState.fmsel + 0x40) + dh), 127);
    }

    dh += 4;

    if (qq->neiromask & 0x10)
    {
        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), 127);
        _OPNA->SetReg((uint32_t) ((_DriverState.fmsel + 0x40) + dh), 127);
    }

    kof1(qq);

    return 0;
}

//  TONE DATA START ADDRESS を計算
//    input  dl  tone_number
//    output  bx  address
uint8_t * PMD::toneadr_calc(Track * qq, int dl)
{
    uint8_t * bx;

    if (_State.prg_flg == 0 && qq != &_EffectTrack)
        return _State._VData + ((size_t) dl << 5);

    bx = _State.prgdat_adr;

    while (*bx != dl)
    {
        bx += 26;

        if (bx > _MData + sizeof(_MData) - 26)
            return _State.prgdat_adr + 1; // Set first timbre if not found.
    }

    return bx + 1;
}

// FM tone generator hard LFO setting (V2.4 expansion)
uint8_t * PMD::hlfo_set(Track * qq, uint8_t * si)
{
    qq->fmpan = (qq->fmpan & 0xc0) | *si++;

    if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
    {
        // Part_e is impossible because it is only for 2608
        // For FM3, set all four parts
        _FMTrack[2].fmpan = qq->fmpan;
        _ExtensionTrack[0].fmpan = qq->fmpan;
        _ExtensionTrack[1].fmpan = qq->fmpan;
        _ExtensionTrack[2].fmpan = qq->fmpan;
    }

    if (qq->Mask == 0)
    {    // パートマスクされているか？
        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + _DriverState._CurrentTrack + 0xb4 - 1), calc_panout(qq));
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

    if (track->Mask)
    {
        track->fnum = 0;    //休符に設定
        track->onkai = 255;
        track->leng = *(si + 2);
        track->keyon_flag++;
        track->address = si + 3;

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

    track->leng = *si++;
    si = calc_q(track, si);

    track->porta_num2 = ax / track->leng;  // 商
    track->porta_num3 = ax % track->leng;  // 余り
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
    keyon(track);

    track->keyon_flag++;
    track->address = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;

    if (*si == 0xfb)
    {    // '&'が直後にあったらkeyoffしない
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
uint8_t * PMD::slotmask_set(Track * qq, uint8_t * si)
{
    uint8_t * bx;
    int    ah, al, bl;

    ah = al = *si++;

    if (al &= 0x0f)
    {
        qq->carrier = al << 4;
    }
    else
    {
        if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
        {
            bl = _DriverState.fm3_alg_fb;
        }
        else
        {
            bx = toneadr_calc(qq, qq->voicenum);
            bl = bx[24];
        }
        qq->carrier = carrier_table[bl & 7];
    }

    ah &= 0xf0;

    if (qq->slotmask != ah)
    {
        qq->slotmask = ah;

        if ((ah & 0xf0) == 0)
            qq->Mask |= 0x20;  // Part mask at s0
        else
            qq->Mask &= 0xdf;  // Unmask part when other than s0

        if (ch3_setting(qq))
        {   // Change process of ch3mode only for FM3ch. If it is ch3, keyon processing in the previous FM3 part
            if (qq != &_FMTrack[2])
            {
                if (_FMTrack[2].Mask == 0 && (_FMTrack[2].keyoff_flag & 1) == 0)
                    keyon(&_FMTrack[2]);

                if (qq != &_ExtensionTrack[0])
                {
                    if (_ExtensionTrack[0].Mask == 0 && (_ExtensionTrack[0].keyoff_flag & 1) == 0)
                        keyon(&_ExtensionTrack[0]);

                    if (qq != &_ExtensionTrack[1])
                    {
                        if (_ExtensionTrack[1].Mask == 0 && (_ExtensionTrack[1].keyoff_flag & 1) == 0)
                            keyon(&_ExtensionTrack[1]);
                    }
                }
            }
        }

        ah = 0;

        if (qq->slotmask & 0x80) ah += 0x11;    // slot4
        if (qq->slotmask & 0x40) ah += 0x44;    // slot3
        if (qq->slotmask & 0x20) ah += 0x22;    // slot2
        if (qq->slotmask & 0x10) ah += 0x88;    // slot1

        qq->neiromask = ah;
    }

    return si;
}

//  Slot Detune Set
uint8_t * PMD::slotdetune_set(Track * qq, uint8_t * si)
{
    int    ax, bl;

    if (_DriverState._CurrentTrack != 3 || _DriverState.fmsel)
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

    if (_DriverState._CurrentTrack != 3 || _DriverState.fmsel)
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
    qq->address = ax;
    qq->leng = 1;          // ｱﾄ 1ｶｳﾝﾄ ﾃﾞ ｴﾝｿｳ ｶｲｼ
    qq->keyoff_flag = -1;      // 現在keyoff中
    qq->mdc = -1;          // MDepth Counter (無限)
    qq->mdc2 = -1;          //
    qq->_mdc = -1;          //
    qq->_mdc2 = -1;          //
    qq->onkai = 255;        // rest
    qq->onkai_def = 255;      // rest
    qq->volume = 108;        // FM  VOLUME DEFAULT= 108
    qq->fmpan = _FMTrack[2].fmpan;  // FM PAN = CH3と同じ
    qq->Mask |= 0x20;      // s0用 partmask
}

//  FM3ch 拡張パートセット
uint8_t * PMD::fm3_extpartset(Track *, uint8_t * si)
{
    int16_t ax;

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&_ExtensionTrack[0], &_State._MData[ax]);

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&_ExtensionTrack[1], &_State._MData[ax]);

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&_ExtensionTrack[2], &_State._MData[ax]);
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
            _PPZ8Track[i].address = &_State._MData[ax];
            _PPZ8Track[i].leng = 1;          // ｱﾄ 1ｶｳﾝﾄ ﾃﾞ ｴﾝｿｳ ｶｲｼ
            _PPZ8Track[i].keyoff_flag = -1;      // 現在keyoff中
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
        qq->volmask = al;
    }
    else
    {
        qq->volmask = qq->carrier;
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
        qq->Mask |= 0x40;

        if (qq->Mask == 0x40)
            MuteFMPart(qq);  // 音消去
    }
    else
    {
        if ((qq->Mask &= 0xbf) == 0)
            neiro_reset(qq);    // 音色再設定
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
        qq->Mask |= 0x40;

        if (qq->Mask == 0x40)
        {
            int ah = ((1 << (_DriverState._CurrentTrack - 1)) | (4 << _DriverState._CurrentTrack));
            uint32_t al = _OPNA->GetReg(0x07);

            _OPNA->SetReg(0x07, ah | al);    // PSG keyoff
        }
    }
    else
        qq->Mask &= 0xbf;

    return si;
}

uint8_t * PMD::rhythm_mml_part_mask(Track * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al != 0)
        qq->Mask |= 0x40;
    else
        qq->Mask &= 0xbf;

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
        qq->Mask |= 0x40;

        if (qq->Mask == 0x40)
        {
            _OPNA->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
            _OPNA->SetReg(0x100, 0x01);    // PCM RESET
        }
    }
    else
        qq->Mask &= 0xbf;

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
        qq->Mask |= 0x40;

        if (qq->Mask == 0x40)
            _P86->Stop();
    }
    else
        qq->Mask &= 0xbf;

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
        qq->Mask |= 0x40;

        if (qq->Mask == 0x40)
            _PPZ8->Stop(_DriverState._CurrentTrack);
    }
    else
        qq->Mask &= 0xbf;

    return si;
}

// Reset the tone of the FM sound source
void PMD::neiro_reset(Track * qq)
{
    if (qq->neiromask == 0)
        return;

    int s1 = qq->slot1;
    int s2 = qq->slot2;
    int s3 = qq->slot3;
    int s4 = qq->slot4;

    _DriverState.af_check = 1;
    neiroset(qq, qq->voicenum);    // 音色復帰
    _DriverState.af_check = 0;

    qq->slot1 = s1;
    qq->slot2 = s2;
    qq->slot3 = s3;
    qq->slot4 = s4;

    int dh;

    int al = ((~qq->carrier) & qq->slotmask) & 0xf0;

    // al<- TLを再設定していいslot 4321xxxx
    if (al)
    {
        dh = 0x4c - 1 + _DriverState._CurrentTrack;  // dh=TL FM Port Address

        if (al & 0x80) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) s4);

        dh -= 8;

        if (al & 0x40) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) s3);

        dh += 4;

        if (al & 0x20) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) s2);

        dh -= 8;

        if (al & 0x10) _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) s1);
    }

    dh = _DriverState._CurrentTrack + 0xb4 - 1;

    _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), calc_panout(qq));
}

uint8_t * PMD::_lfoswitch(Track * qq, uint8_t * si)
{
    qq->lfoswi = (qq->lfoswi & 0x8f) | ((*si++ & 7) << 4);

    lfo_change(qq);
    lfoinit_main(qq);
    lfo_change(qq);

    return si;
}

uint8_t * PMD::_volmask_set(Track * qq, uint8_t * si)
{
    int al = *si++ & 0x0f;

    if (al)
    {
        al = (al << 4) | 0x0f;
        qq->_volmask = al;
    }
    else
    {
        qq->_volmask = qq->carrier;
    }

    ch3_setting(qq);

    return si;
}

//  TL変化
uint8_t * PMD::tl_set(Track * qq, uint8_t * si)
{
    int dh = 0x40 - 1 + _DriverState._CurrentTrack;    // dh=TL FM Port Address
    int al = *(int8_t *) si++;
    int ah = al & 0x0f;
    int ch = (qq->slotmask >> 4) | ((qq->slotmask << 4) & 0xf0);

    ah &= ch;              // ah=変化させるslot 00004321

    int dl = *(int8_t *) si++;

    if (al >= 0)
    {
        dl &= 127;

        if (ah & 1)
        {
            qq->slot1 = dl;
            if (qq->Mask == 0)
            {
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
        }

        dh += 8;
        if (ah & 2)
        {
            qq->slot2 = dl;
            if (qq->Mask == 0)
            {
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
        }

        dh -= 4;
        if (ah & 4)
        {
            qq->slot3 = dl;
            if (qq->Mask == 0)
            {
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
            }
        }

        dh += 8;
        if (ah & 8)
        {
            qq->slot4 = dl;
            if (qq->Mask == 0)
            {
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
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
            if (qq->Mask == 0)
            {
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
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
            if (qq->Mask == 0)
            {
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
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
            if (qq->Mask == 0)
            {
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
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
            if (qq->Mask == 0)
            {
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
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

    int dh = _DriverState._CurrentTrack + 0xb0 - 1;  // dh=ALG/FB port address
    int al = *(int8_t *) si++;

    if (al >= 0)
    {
        // in  al 00000xxx 設定するFB
        al = ((al << 3) & 0xff) | (al >> 5);

        // in  al 00xxx000 設定するFB
        if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
        {
            if ((qq->slotmask & 0x10) == 0) return si;
            dl = (_DriverState.fm3_alg_fb & 7) | al;
            _DriverState.fm3_alg_fb = dl;
        }
        else
        {
            dl = (qq->alg_fb & 7) | al;
        }

        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
        qq->alg_fb = dl;

        return si;
    }
    else
    {
        if ((al & 0x40) == 0)
            al &= 7;

        if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
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
                if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
                {
                    if ((qq->slotmask & 0x10) == 0) return si;

                    dl = (_DriverState.fm3_alg_fb & 7) | al;
                    _DriverState.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (qq->alg_fb & 7) | al;
                }
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
                qq->alg_fb = dl;
                return si;
            }
            else
            {
                // in  al 00000xxx 設定するFB
                al = ((al << 3) & 0xff) | (al >> 5);

                // in  al 00xxx000 設定するFB
                if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
                {
                    if ((qq->slotmask & 0x10) == 0) return si;
                    dl = (_DriverState.fm3_alg_fb & 7) | al;
                    _DriverState.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (qq->alg_fb & 7) | al;
                }
                _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
                qq->alg_fb = dl;
                return si;
            }
        }
        else
        {
            al = 0;
            if (_DriverState._CurrentTrack == 3 && _DriverState.fmsel == 0)
            {
                if ((qq->slotmask & 0x10) == 0) return si;

                dl = (_DriverState.fm3_alg_fb & 7) | al;
                _DriverState.fm3_alg_fb = dl;
            }
            else
            {
                dl = (qq->alg_fb & 7) | al;
            }
            _OPNA->SetReg((uint32_t) (_DriverState.fmsel + dh), (uint32_t) dl);
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
    int    al;

    al = *si++;
    if (al < 251)
    {
        _State.tempo_d = al;    // T (FC)
        _State.tempo_d_push = al;
        calc_tb_tempo();

    }
    else if (al == 0xff)
    {
        al = *si++;          // t (FC FF)
        if (al < 18) al = 18;
        _State.tempo_48 = al;
        _State.tempo_48_push = al;
        calc_tempo_tb();

    }
    else if (al == 0xfe)
    {
        al = int8_t(*si++);      // T± (FC FE)
        if (al >= 0)
        {
            al += _State.tempo_d_push;
        }
        else
        {
            al += _State.tempo_d_push;
            if (al < 0)
            {
                al = 0;
            }
        }

        if (al > 250) al = 250;

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
            {
                al = 255;
            }
        }
        else
        {
            al += _State.tempo_48_push;
            if (al < 0) al = 18;
        }

        _State.tempo_48 = al;
        _State.tempo_48_push = al;

        calc_tempo_tb();
    }

    return si;
}

//  COMMAND '[' [ﾙｰﾌﾟ ｽﾀｰﾄ]
uint8_t * PMD::comstloop(Track * qq, uint8_t * si)
{
    uint8_t * ax;

    if (qq == &_EffectTrack)
    {
        ax = _State._EData;
    }
    else
    {
        ax = _State._MData;
    }

    ax[*(uint16_t *) si + 1] = 0;

    si += 2;

    return si;
}

//  COMMAND  ']' [ﾙｰﾌﾟ ｴﾝﾄﾞ]
uint8_t * PMD::comedloop(Track * qq, uint8_t * si)
{
    int    ah, al, ax;
    ah = *si++;

    if (ah)
    {
        (*si)++;
        al = *si++;
        if (ah == al)
        {
            si += 2;
            return si;
        }
    }
    else
    {      // 0 ﾅﾗ ﾑｼﾞｮｳｹﾝ ﾙｰﾌﾟ
        si++;
        qq->loopcheck = 1;
    }

    ax = *(uint16_t *) si + 2;

    if (qq == &_EffectTrack)
    {
        si = _State._EData + ax;
    }
    else
    {
        si = _State._MData + ax;
    }
    return si;
}

//  COMMAND  ':' [ﾙｰﾌﾟ ﾀﾞｯｼｭﾂ]
uint8_t * PMD::comexloop(Track * qq, uint8_t * si)
{
    uint8_t * bx;
    int    dl;


    if (qq == &_EffectTrack)
    {
        bx = _State._EData;
    }
    else
    {
        bx = _State._MData;
    }


    bx += *(uint16_t *) si;
    si += 2;

    dl = *bx++ - 1;
    if (dl != *bx) return si;
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

//  PSG ENVELOPE SET
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
    int dl = *si++ & _State.rhythmmask;

    if (dl == 0)
        return si;

    if (_State._FadeOutVolume != 0)
    {
        int al = ((256 - _State._FadeOutVolume) * _State.rhyvol) >> 8;

        _OPNA->SetReg(0x11, (uint32_t) al);
    }

    if (dl < 0x80)
    {
        if (dl & 0x01) _OPNA->SetReg(0x18, (uint32_t) _State.rdat[0]);
        if (dl & 0x02) _OPNA->SetReg(0x19, (uint32_t) _State.rdat[1]);
        if (dl & 0x04) _OPNA->SetReg(0x1a, (uint32_t) _State.rdat[2]);
        if (dl & 0x08) _OPNA->SetReg(0x1b, (uint32_t) _State.rdat[3]);
        if (dl & 0x10) _OPNA->SetReg(0x1c, (uint32_t) _State.rdat[4]);
        if (dl & 0x20) _OPNA->SetReg(0x1d, (uint32_t) _State.rdat[5]);
    }

    _OPNA->SetReg(0x10, (uint32_t) dl);

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

    _OPNA->SetReg((uint32_t) dh, (uint32_t) dl);

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

    _OPNA->SetReg((uint32_t) dh, (uint32_t) dl);

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
    _OPNA->SetReg((uint32_t) dh, (uint32_t) dl);

    return si;
}

//  "\Vn" COMMAND
uint8_t * PMD::rmsvs(uint8_t * si)
{
    int dl = *si++;

    if (_State.rhythm_voldown != 0)
        dl = ((256 - _State.rhythm_voldown) * dl) >> 8;

    _State.rhyvol = dl;

    if (_State._FadeOutVolume != 0)
        dl = ((256 - _State._FadeOutVolume) * dl) >> 8;

    _OPNA->SetReg(0x11, (uint32_t) dl);

    return si;
}

uint8_t * PMD::rmsvs_sft(uint8_t * si)
{
    int dl = _State.rhyvol + *(int8_t *) si++;

    if (dl >= 64)
    {
        if (dl & 0x80)
            dl = 0;
        else
            dl = 63;
    }

    _State.rhyvol = dl;

    if (_State._FadeOutVolume != 0)
        dl = ((256 - _State._FadeOutVolume) * dl) >> 8;

    _OPNA->SetReg(0x11, (uint32_t) dl);

    return si;
}

// Change only one volume (V2.7 expansion)
uint8_t * PMD::vol_one_up_psg(Track * qq, uint8_t * si)
{
    int    al;

    al = qq->volume + *si++;
    if (al > 15) al = 15;
    qq->volpush = ++al;
    _DriverState.volpush_flag = 1;
    return si;
}

uint8_t * PMD::vol_one_down(Track * qq, uint8_t * si)
{
    int    al;

    al = qq->volume - *si++;
    if (al < 0)
    {
        al = 0;
    }
    else
    {
        if (al >= 255) al = 254;
    }

    qq->volpush = ++al;
    _DriverState.volpush_flag = 1;
    return si;
}

//  ポルタメント(PSG)
uint8_t * PMD::portap(Track * qq, uint8_t * si)
{
    int    ax, al_, bx_;

    if (qq->Mask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

        if (--_DriverState.volpush_flag)
        {
            qq->volpush = 0;
        }

        _DriverState.tieflag = 0;
        _DriverState.volpush_flag = 0;
        _DriverState.loop_work &= qq->loopcheck;
        return si + 3;    // 読み飛ばす  (Mask時)
    }

    fnumsetp(qq, oshiftp(qq, lfoinitp(qq, *si++)));

    bx_ = (int) qq->fnum;
    al_ = qq->onkai;

    fnumsetp(qq, oshiftp(qq, *si++));

    ax = (int) qq->fnum;       // ax = ポルタメント先のpsg_tune値

    qq->onkai = al_;
    qq->fnum = (uint32_t) bx_;      // bx = ポルタメント元のpsg_tune値
    ax -= bx_;

    qq->leng = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->leng;    // 商
    qq->porta_num3 = ax % qq->leng;    // 余り
    qq->lfoswi |= 8;        // Porta ON

    if (qq->volpush && qq->onkai != 255)
    {
        if (--_DriverState.volpush_flag)
        {
            _DriverState.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volsetp(qq);
    OtodasiP(qq);
    keyonp(qq);

    qq->keyon_flag++;
    qq->address = si;

    _DriverState.tieflag = 0;
    _DriverState.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {      // '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }

    _DriverState.loop_work &= qq->loopcheck;
    return si;
}

//  'w' COMMAND [PSG NOISE ﾍｲｷﾝ ｼｭｳﾊｽｳ]
uint8_t * PMD::psgnoise_move(uint8_t * si)
{
    _State.psnoi += *(int8_t *) si++;
    if (_State.psnoi < 0) _State.psnoi = 0;
    if (_State.psnoi > 31) _State.psnoi = 31;
    return si;
}

//  PSG Envelope set (Extend)
uint8_t * PMD::extend_psgenvset(Track * qq, uint8_t * si)
{
    qq->eenv_ar = *si++ & 0x1f;
    qq->eenv_dr = *si++ & 0x1f;
    qq->eenv_sr = *si++ & 0x1f;
    qq->eenv_rr = *si & 0x0f;
    qq->eenv_sl = ((*si++ >> 4) & 0x0f) ^ 0x0f;
    qq->eenv_al = *si++ & 0x0f;

    if (qq->envf != -1)
    {  // ノーマル＞拡張に移行したか？
        qq->envf = -1;
        qq->eenv_count = 4;    // RR
        qq->eenv_volume = 0;  // Volume
    }
    return si;
}

uint8_t * PMD::mdepth_count(Track * qq, uint8_t * si)
{
    int    al;

    al = *si++;

    if (al >= 0x80)
    {
        if ((al &= 0x7f) == 0) al = 255;
        qq->_mdc = al;
        qq->_mdc2 = al;
        return si;
    }

    if (al == 0) al = 255;
    qq->mdc = al;
    qq->mdc2 = al;
    return si;
}

// Initialization of LFO and PSG/PCM software envelopes

//  ＰＳＧ／ＰＣＭ音源用　Entry
int PMD::lfoinitp(Track * qq, int al)
{
    int    ah;

    ah = al & 0x0f;

    if (ah == 0x0c)
    {
        al = qq->onkai_def;
        ah = al & 0x0f;
    }

    qq->onkai_def = al;

    // 4.8r 修正
    if (ah == 0x0f)
    {      // ｷｭｰﾌ ﾉ ﾄｷ ﾊ INIT ｼﾅｲﾖ
// PMD 4.8r 修正
        soft_env(qq);
        lfo_exit(qq);
        return al;
    }

    qq->porta_num = 0;      // ポルタメントは初期化

    if (_DriverState.tieflag & 1)
    {  // ﾏｴ ｶﾞ & ﾉ ﾄｷ ﾓ INIT ｼﾅｲ｡
// PMD 4.8r 修正
        soft_env(qq);      // 前が & の場合 -> 1回 SofeEnv処理
        lfo_exit(qq);
        return al;
    }

    //------------------------------------------------------------------------
    //  ソフトウエアエンベロープ初期化
    //------------------------------------------------------------------------
    if (qq->envf != -1)
    {
        qq->envf = 0;
        qq->eenv_volume = 0;
        qq->eenv_ar = qq->eenv_arc;

        if (qq->eenv_ar == 0)
        {
            qq->envf = 1;  // ATTACK=0 ... ｽｸﾞ Decay ﾆ
            qq->eenv_volume = qq->eenv_dr;
        }

        qq->eenv_sr = qq->eenv_src;
        qq->eenv_rr = qq->eenv_rrc;
        lfin1(qq);

    }
    else
    {
        //  拡張ssg_envelope用

        qq->eenv_arc = qq->eenv_ar - 16;
        if (qq->eenv_dr < 16)
        {
            qq->eenv_drc = (qq->eenv_dr - 16) * 2;
        }
        else
        {
            qq->eenv_drc = qq->eenv_dr - 16;
        }

        if (qq->eenv_sr < 16)
        {
            qq->eenv_src = (qq->eenv_sr - 16) * 2;
        }
        else
        {
            qq->eenv_src = qq->eenv_sr - 16;
        }

        qq->eenv_rrc = (qq->eenv_rr) * 2 - 16;
        qq->eenv_volume = qq->eenv_al;
        qq->eenv_count = 1;
        ext_ssgenv_main(qq);
        lfin1(qq);
    }
    return al;
}

void PMD::lfo_exit(Track * qq)
{
    if ((qq->lfoswi & 3) != 0)
    {    // 前が & の場合 -> 1回 LFO処理
        lfo(qq);
    }

    if ((qq->lfoswi & 0x30) != 0)
    {  // 前が & の場合 -> 1回 LFO処理
        lfo_change(qq);
        lfo(qq);
        lfo_change(qq);
    }
}

//  ＬＦＯ初期化
void PMD::lfin1(Track * qq)
{
    qq->hldelay_c = qq->hldelay;

    if (qq->hldelay)
        _OPNA->SetReg((uint32_t) (_DriverState.fmsel + _DriverState._CurrentTrack + 0xb4 - 1), (uint32_t) (qq->fmpan & 0xc0));

    qq->sdelay_c = qq->sdelay;

    if (qq->lfoswi & 3)
    {  // LFOは未使用
        if ((qq->lfoswi & 4) == 0)
        {  //keyon非同期か?
            lfoinit_main(qq);
        }
        lfo(qq);
    }

    if (qq->lfoswi & 0x30)
    {  // LFOは未使用
        if ((qq->lfoswi & 0x40) == 0)
        {  //keyon非同期か?
            lfo_change(qq);
            lfoinit_main(qq);
            lfo_change(qq);
        }

        lfo_change(qq);
        lfo(qq);
        lfo_change(qq);
    }
}

void PMD::lfoinit_main(Track * qq)
{
    qq->lfodat = 0;
    qq->delay = qq->delay2;
    qq->speed = qq->speed2;
    qq->step = qq->step2;
    qq->time = qq->time2;
    qq->mdc = qq->mdc2;

    if (qq->lfo_wave == 2 || qq->lfo_wave == 3)
    {  // 矩形波 or ランダム波？
        qq->speed = 1;  // delay直後にLFOが掛かるようにする
    }
    else
    {
        qq->speed++;  // それ以外の場合はdelay直後のspeed値を +1
    }
}

//  SHIFT[di] 分移調する
int PMD::oshiftp(Track * qq, int al)
{
    return oshift(qq, al);
}

int PMD::oshift(Track * qq, int al)
{
    int  bl, bh, dl;

    if (al == 0x0f) return al;

    dl = qq->shift + qq->shift_def;
    if (dl == 0) return al;

    bl = (al & 0x0f);    // bl = ONKAI
    bh = (al & 0xf0) >> 4;  // bh = OCT

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
        if (bh < 0) bh = 0;
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

        if (bh > 7) bh = 7;
        return (bh << 4) | bl;
    }
}

//  PSG TUNE SET
void PMD::fnumsetp(Track * qq, int al)
{
    if ((al & 0x0f) == 0x0f)
    {    // ｷｭｳﾌ ﾅﾗ FNUM ﾆ 0 ｦ ｾｯﾄ
        qq->onkai = 255;

        if (qq->lfoswi & 0x11)
            return;

        qq->fnum = 0;  // 音程LFO未使用

        return;
    }

    qq->onkai = al;

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

    qq->fnum = (uint32_t) ax;
}

//  Q値の計算
//    break  dx
uint8_t * PMD::calc_q(Track * qq, uint8_t * si)
{
    if (*si == 0xc1)
    {    // &&
        si++;
        qq->qdat = 0;
        return si;
    }

    int dl = qq->qdata;

    if (qq->qdatb)
        dl += (qq->leng * qq->qdatb) >> 8;

    if (qq->qdat3)
    {    //  Random-Q
        int ax = rnd((qq->qdat3 & 0x7f) + 1);

        if ((qq->qdat3 & 0x80) == 0)
        {
            dl += ax;
        }
        else
        {
            dl -= ax;
            if (dl < 0) dl = 0;
        }
    }

    if (qq->qdat2)
    {
        int dh = qq->leng - qq->qdat2;

        if (dh < 0)
        {
            qq->qdat = 0;
            return si;
        }

        if (dl < dh)
            qq->qdat = dl;
        else
            qq->qdat = dh;
    }
    else
        qq->qdat = dl;

    return si;
}

// Set PSG volume.
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
    dl = ((256 - _State._FadeOutVolume) * dl) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (dl <= 0)
    {
        _OPNA->SetReg((uint32_t) (_DriverState._CurrentTrack + 8 - 1), 0);
        return;
    }

    if (track->envf == -1)
    {
        if (track->eenv_volume == 0)
        {
            _OPNA->SetReg((uint32_t) (_DriverState._CurrentTrack + 8 - 1), 0);
            return;
        }

        dl = ((((dl * (track->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        dl += track->eenv_volume;

        if (dl <= 0)
        {
            _OPNA->SetReg((uint32_t) (_DriverState._CurrentTrack + 8 - 1), 0);
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
        _OPNA->SetReg((uint32_t) (_DriverState._CurrentTrack + 8 - 1), (uint32_t) dl);
        return;
    }

    int ax = (track->lfoswi & 2) ? track->lfodat : 0;

    if (track->lfoswi & 0x20)
        ax += track->_lfodat;

    dl += ax;

    if (dl < 0)
    {
        _OPNA->SetReg((uint32_t) (_DriverState._CurrentTrack + 8 - 1), 0);
        return;
    }

    if (dl > 15)
        dl = 15;

    //------------------------------------------------------------------------
    //  出力
    //------------------------------------------------------------------------
    _OPNA->SetReg((uint32_t) (_DriverState._CurrentTrack + 8 - 1), (uint32_t) dl);
}

// Set PSG pitch.
void PMD::OtodasiP(Track * track)
{
    if (track->fnum == 0)
        return;

    // PSG Portament set
    int ax = (int) (track->fnum + track->porta_num);
    int dx = 0;

    // PSG Detune/LFO set
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

    _OPNA->SetReg((uint32_t) ((_DriverState._CurrentTrack - 1) * 2),     (uint32_t) LOBYTE(ax));
    _OPNA->SetReg((uint32_t) ((_DriverState._CurrentTrack - 1) * 2 + 1), (uint32_t) HIBYTE(ax));
}

//  ＰＳＧ　ＫＥＹＯＮ
void PMD::keyonp(Track * qq)
{
    if (qq->onkai == 255)
        return;    // ｷｭｳﾌ ﾉ ﾄｷ

    int ah = (1 << (_DriverState._CurrentTrack - 1)) | (1 << (_DriverState._CurrentTrack + 2));
    int al = ((int32_t) _OPNA->GetReg(0x07) | ah);

    ah = ~(ah & qq->psgpat);
    al &= ah;

    _OPNA->SetReg(7, (uint32_t) al);

    // PSG ﾉｲｽﾞ ｼｭｳﾊｽｳ ﾉ ｾｯﾄ

    if (_State.psnoi != _State.psnoi_last && _EffectState.effon == 0)
    {
        _OPNA->SetReg(6, (uint32_t) _State.psnoi);
        _State.psnoi_last = _State.psnoi;
    }
}

//  ＬＦＯ処理
//    Don't Break cl
//    output    cy=1  変化があった
int PMD::lfo(Track * qq)
{
    return lfop(qq);
}

int PMD::lfop(Track * qq)
{
    int    ax, ch;

    if (qq->delay)
    {
        qq->delay--;
        return 0;
    }

    if (qq->extendmode & 2)
    {  // TimerAと合わせるか？
        // そうじゃないなら無条件にlfo処理
        ch = _State._TimerATime - _DriverState._OldTimerATime;
        if (ch == 0) return 0;
        ax = qq->lfodat;

        for (; ch > 0; ch--)
        {
            lfo_main(qq);
        }
    }
    else
    {

        ax = qq->lfodat;
        lfo_main(qq);
    }

    if (ax == qq->lfodat)
    {
        return 0;
    }
    return 1;
}

void PMD::lfo_main(Track * qq)
{
    int    al, ax;

    if (qq->speed != 1)
    {
        if (qq->speed != 255) qq->speed--;
        return;
    }

    qq->speed = qq->speed2;

    if (qq->lfo_wave == 0 || qq->lfo_wave == 4 || qq->lfo_wave == 5)
    {
        //  三角波    lfowave = 0,4,5
        if (qq->lfo_wave == 5)
        {
            ax = abs(qq->step) * qq->step;
        }
        else
        {
            ax = qq->step;
        }

        if ((qq->lfodat += ax) == 0)
        {
            md_inc(qq);
        }

        al = qq->time;
        if (al != 255)
        {
            if (--al == 0)
            {
                al = qq->time2;
                if (qq->lfo_wave != 4)
                {
                    al += al;  // lfowave=0,5の場合 timeを反転時２倍にする
                }
                qq->time = al;
                qq->step = -qq->step;
                return;
            }
        }
        qq->time = al;

    }
    else if (qq->lfo_wave == 2)
    {
        //  矩形波    lfowave = 2
        qq->lfodat = (qq->step * qq->time);
        md_inc(qq);
        qq->step = -qq->step;

    }
    else if (qq->lfo_wave == 6)
    {
        //  ワンショット  lfowave = 6
        if (qq->time)
        {
            if (qq->time != 255)
            {
                qq->time--;
            }
            qq->lfodat += qq->step;
        }
    }
    else if (qq->lfo_wave == 1)
    {
        //ノコギリ波  lfowave = 1
        qq->lfodat += qq->step;
        al = qq->time;
        if (al != -1)
        {
            al--;
            if (al == 0)
            {
                qq->lfodat = -qq->lfodat;
                md_inc(qq);
                al = (qq->time2) * 2;
            }
        }
        qq->time = al;

    }
    else
    {
        //  ランダム波  lfowave = 3
        ax = abs(qq->step) * qq->time;
        qq->lfodat = ax - rnd(ax * 2);
        md_inc(qq);
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
void PMD::md_inc(Track * qq)
{
    int    al;

    if (--qq->mdspd) return;

    qq->mdspd = qq->mdspd2;

    if (qq->mdc == 0) return;    // count = 0
    if (qq->mdc <= 127)
    {
        qq->mdc--;
    }

    if (qq->step < 0)
    {
        al = qq->mdepth - qq->step;

        if (al < 128)
        {
            qq->step = -al;
        }
        else
        {
            if (qq->mdepth < 0)
            {
                qq->step = 0;
            }
            else
            {
                qq->step = -127;
            }
        }

    }
    else
    {
        al = qq->step + qq->mdepth;

        if (al < 128)
        {
            qq->step = al;
        }
        else
        {
            if (qq->mdepth < 0)
            {
                qq->step = 0;
            }
            else
            {
                qq->step = 127;
            }
        }
    }
}

void PMD::swap(int * a, int * b)
{
    int    temp;

    temp = *a;
    *a = *b;
    *b = temp;
}

//  LFO1<->LFO2 change
void PMD::lfo_change(Track * qq)
{
    swap(&qq->lfodat, &qq->_lfodat);
    qq->lfoswi = ((qq->lfoswi & 0x0f) << 4) + (qq->lfoswi >> 4);
    qq->extendmode = ((qq->extendmode & 0x0f) << 4) + (qq->extendmode >> 4);

    swap(&qq->delay, &qq->_delay);
    swap(&qq->speed, &qq->_speed);
    swap(&qq->step, &qq->_step);
    swap(&qq->time, &qq->_time);
    swap(&qq->delay2, &qq->_delay2);
    swap(&qq->speed2, &qq->_speed2);
    swap(&qq->step2, &qq->_step2);
    swap(&qq->time2, &qq->_time2);
    swap(&qq->mdepth, &qq->_mdepth);
    swap(&qq->mdspd, &qq->_mdspd);
    swap(&qq->mdspd2, &qq->_mdspd2);
    swap(&qq->lfo_wave, &qq->_lfo_wave);
    swap(&qq->mdc, &qq->_mdc);
    swap(&qq->mdc2, &qq->_mdc2);
}

//  ポルタメント計算なのね
void PMD::porta_calc(Track * qq)
{
    qq->porta_num += qq->porta_num2;
    if (qq->porta_num3 == 0) return;
    if (qq->porta_num3 > 0)
    {
        qq->porta_num3--;
        qq->porta_num++;
    }
    else
    {
        qq->porta_num3++;
        qq->porta_num--;
    }
}

// PSG/PCM Software Envelope
int PMD::soft_env(Track * qq)
{
    if (qq->extendmode & 4)
    {
        if (_State._TimerATime == _DriverState._OldTimerATime) return 0;

        int cl = 0;

        for (int i = 0; i < _State._TimerATime - _DriverState._OldTimerATime; i++)
        {
            if (soft_env_main(qq))
                cl = 1;
        }

        return cl;
    }
    else
        return soft_env_main(qq);
}

int PMD::soft_env_main(Track * qq)
{
    if (qq->envf == -1)
        return ext_ssgenv_main(qq);

    int dl = qq->eenv_volume;

    soft_env_sub(qq);

    if (dl == qq->eenv_volume)
        return 0;

    return -1;
}

int PMD::soft_env_sub(Track * qq)
{
    if (qq->envf == 0)
    {
        // Attack
        if (--qq->eenv_ar != 0)
            return 0;

        qq->envf = 1;
        qq->eenv_volume = qq->eenv_dr;

        return 1;
    }

    if (qq->envf != 2)
    {
        // Decay
        if (qq->eenv_sr == 0) return 0;  // ＤＲ＝０の時は減衰しない
        if (--qq->eenv_sr != 0) return 0;

        qq->eenv_sr = qq->eenv_src;
        qq->eenv_volume--;

        if (qq->eenv_volume >= -15 || qq->eenv_volume < 15)
            return 0;

        qq->eenv_volume = -15;

        return 0;
    }

    // Release
    if (qq->eenv_rr == 0)
    {        // ＲＲ＝０の時はすぐに音消し
        qq->eenv_volume = -15;
        return 0;
    }

    if (--qq->eenv_rr != 0)
        return 0;

    qq->eenv_rr = qq->eenv_rrc;
    qq->eenv_volume--;

    if (qq->eenv_volume >= -15 && qq->eenv_volume < 15)
        return 0;

    qq->eenv_volume = -15;

    return 0;
}

//  拡張版
int PMD::ext_ssgenv_main(Track * qq)
{
    if (qq->eenv_count == 0)
        return 0;

    int dl = qq->eenv_volume;

    esm_sub(qq, qq->eenv_count);

    if (dl == qq->eenv_volume)
        return 0;

    return -1;
}

void PMD::esm_sub(Track * qq, int ah)
{
    if (--ah == 0)
    {
        //  [[[ Attack Rate ]]]
        if (qq->eenv_arc > 0)
        {
            qq->eenv_volume += qq->eenv_arc;
            if (qq->eenv_volume < 15)
            {
                qq->eenv_arc = qq->eenv_ar - 16;
                return;
            }

            qq->eenv_volume = 15;
            qq->eenv_count++;

            if (qq->eenv_sl != 15)
                return;    // SL=0の場合はすぐSRに

            qq->eenv_count++;

            return;
        }
        else
        {
            if (qq->eenv_ar == 0)
                return;

            qq->eenv_arc++;

            return;
        }
    }

    if (--ah == 0)
    {
        //  [[[ Decay Rate ]]]
        if (qq->eenv_drc > 0)
        {  // 0以下の場合はカウントCHECK
            qq->eenv_volume -= qq->eenv_drc;
            if (qq->eenv_volume < 0 || qq->eenv_volume < qq->eenv_sl)
            {
                qq->eenv_volume = qq->eenv_sl;
                qq->eenv_count++;
                return;
            }

            if (qq->eenv_dr < 16)
                qq->eenv_drc = (qq->eenv_dr - 16) * 2;
            else
                qq->eenv_drc = qq->eenv_dr - 16;

            return;
        }

        if (qq->eenv_dr == 0)
            return;

        qq->eenv_drc++;

        return;
    }

    if (--ah == 0)
    {
        //  [[[ Sustain Rate ]]]
        if (qq->eenv_src > 0)
        {  // 0以下の場合はカウントCHECK
            if ((qq->eenv_volume -= qq->eenv_src) < 0)
            {
                qq->eenv_volume = 0;
            }

            if (qq->eenv_sr < 16)
            {
                qq->eenv_src = (qq->eenv_sr - 16) * 2;
            }
            else
            {
                qq->eenv_src = qq->eenv_sr - 16;
            }
            return;
        }

        if (qq->eenv_sr == 0) return;  // SR=0?
        qq->eenv_src++;
        return;
    }

    //  [[[ Release Rate ]]]
    if (qq->eenv_rrc > 0)
    {  // 0以下の場合はカウントCHECK
        if ((qq->eenv_volume -= qq->eenv_rrc) < 0)
            qq->eenv_volume = 0;

        qq->eenv_rrc = (qq->eenv_rr) * 2 - 16;

        return;
    }

    if (qq->eenv_rr == 0)
        return;

    qq->eenv_rrc++;
}

//  テンポ設定
void PMD::settempo_b()
{
    if (_State._TimerBSpeed != _State.tempo_d)
    {
        _State._TimerBSpeed = _State.tempo_d;
        _OPNA->SetReg(0x26, (uint32_t) _State._TimerBSpeed);
    }
}

//  小節のカウント
void PMD::syousetu_count()
{
    if (_State.opncount + 1 == _State.syousetu_lng)
    {
        _State.syousetu++;
        _State.opncount = 0;
    }
    else
    {
        _State.opncount++;
    }
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

    _State.rhythmmask = 255;
    _DriverState.rhydmy = 255;
    InitializeDataArea();
    opn_init();

    _OPNA->SetReg(0x07, 0xbf);
    DriverStop();
    setint();
    _OPNA->SetReg(0x29, 0x83);
}

void PMD::InitializeDataArea()
{
    _State._FadeOutVolume = 0;
    _State._FadeOutSpeed = 0;
    _State.fadeout_flag = 0;
    _State._FadeOutSpeedHQ = 0;

    for (int i = 0; i < 6; i++)
    {
        int partmask = _FMTrack[i].Mask;
        int keyon_flag = _FMTrack[i].keyon_flag;

        ::memset(&_FMTrack[i], 0, sizeof(Track));

        _FMTrack[i].Mask = partmask & 0x0f;
        _FMTrack[i].keyon_flag = keyon_flag;
        _FMTrack[i].onkai = 255;
        _FMTrack[i].onkai_def = 255;
    }

    for (int i = 0; i < 3; i++)
    {
        int partmask = _SSGTrack[i].Mask;
        int keyon_flag = _SSGTrack[i].keyon_flag;

        ::memset(&_SSGTrack[i], 0, sizeof(Track));

        _SSGTrack[i].Mask = partmask & 0x0f;
        _SSGTrack[i].keyon_flag = keyon_flag;
        _SSGTrack[i].onkai = 255;
        _SSGTrack[i].onkai_def = 255;
    }

    {
        int partmask = _ADPCMTrack.Mask;
        int keyon_flag = _ADPCMTrack.keyon_flag;

        ::memset(&_ADPCMTrack, 0, sizeof(Track));

        _ADPCMTrack.Mask = partmask & 0x0f;
        _ADPCMTrack.keyon_flag = keyon_flag;
        _ADPCMTrack.onkai = 255;
        _ADPCMTrack.onkai_def = 255;
    }

    {
        int partmask = _RhythmTrack.Mask;
        int keyon_flag = _RhythmTrack.keyon_flag;

        ::memset(&_RhythmTrack, 0, sizeof(Track));

        _RhythmTrack.Mask = partmask & 0x0f;
        _RhythmTrack.keyon_flag = keyon_flag;
        _RhythmTrack.onkai = 255;
        _RhythmTrack.onkai_def = 255;
    }

    for (int i = 0; i < 3; i++)
    {
        int partmask = _ExtensionTrack[i].Mask;
        int keyon_flag = _ExtensionTrack[i].keyon_flag;

        ::memset(&_ExtensionTrack[i], 0, sizeof(Track));

        _ExtensionTrack[i].Mask = partmask & 0x0f;
        _ExtensionTrack[i].keyon_flag = keyon_flag;
        _ExtensionTrack[i].onkai = 255;
        _ExtensionTrack[i].onkai_def = 255;
    }

    for (int i = 0; i < 8; i++)
    {
        int partmask = _PPZ8Track[i].Mask;
        int keyon_flag = _PPZ8Track[i].keyon_flag;

        ::memset(&_PPZ8Track[i], 0, sizeof(Track));

        _PPZ8Track[i].Mask = partmask & 0x0f;
        _PPZ8Track[i].keyon_flag = keyon_flag;
        _PPZ8Track[i].onkai = 255;
        _PPZ8Track[i].onkai_def = 255;
    }

    _DriverState.tieflag = 0;
    _State.status = 0;
    _State._LoopCount = 0;
    _State.syousetu = 0;
    _State.opncount = 0;
    _State._TimerATime = 0;
    _DriverState._OldTimerATime = 0;

    _DriverState.omote_key[0] = 0;
    _DriverState.omote_key[1] = 0;
    _DriverState.omote_key[2] = 0;
    _DriverState.ura_key[0] = 0;
    _DriverState.ura_key[1] = 0;
    _DriverState.ura_key[2] = 0;

    _DriverState.fm3_alg_fb = 0;
    _DriverState.af_check = 0;

    _State.pcmstart = 0;
    _State.pcmstop = 0;
    _DriverState.pcmrepeat1 = 0;
    _DriverState.pcmrepeat2 = 0;
    _DriverState.pcmrelease = 0x8000;

    _State.kshot_dat = 0;
    _State.rshot_dat = 0;
    _EffectState.last_shot_data = 0;

    _DriverState.slotdetune_flag = 0;
    _State.slot_detune1 = 0;
    _State.slot_detune2 = 0;
    _State.slot_detune3 = 0;
    _State.slot_detune4 = 0;

    _DriverState.slot3_flag = 0;
    _State.ch3mode = 0x3f;

    _DriverState.fmsel = 0;

    _State.syousetu_lng = 96;

    _State.fm_voldown = _State._fm_voldown;
    _State.ssg_voldown = _State._ssg_voldown;
    _State.pcm_voldown = _State._pcm_voldown;
    _State.ppz_voldown = _State._ppz_voldown;
    _State.rhythm_voldown = _State._rhythm_voldown;
    _State.pcm86_vol = _State._pcm86_vol;
}

//  OPN INIT
void PMD::opn_init()
{
    _OPNA->ClearBuffer();
    _OPNA->SetReg(0x29, 0x83);

    _State.psnoi = 0;

    _OPNA->SetReg(0x06, 0x00);
    _State.psnoi_last = 0;

    // SSG-EG RESET (4.8s)
    for (uint32_t i = 0x90; i < 0x9F; i++)
    {
        if (i % 4 != 3)
            _OPNA->SetReg(i, 0x00);
    }

    for (uint32_t i = 0x190; i < 0x19F; i++)
    {
        if (i % 4 != 3)
            _OPNA->SetReg(i, 0x00);
    }

    // PAN/HARDLFO DEFAULT
    _OPNA->SetReg(0x0b4, 0xc0);
    _OPNA->SetReg(0x0b5, 0xc0);
    _OPNA->SetReg(0x0b6, 0xc0);
    _OPNA->SetReg(0x1b4, 0xc0);
    _OPNA->SetReg(0x1b5, 0xc0);
    _OPNA->SetReg(0x1b6, 0xc0);

    _State.port22h = 0x00;
    _OPNA->SetReg(0x22, 0x00);

    //  Rhythm Default = Pan : Mid , Vol : 15
    for (int i = 0; i < 6; i++)
        _State.rdat[i] = 0xcf;

    _OPNA->SetReg(0x10, 0xff);

    // Rhythm total level set
    _State.rhyvol = 48 * 4 * (256 - _State.rhythm_voldown) / 1024;
    _OPNA->SetReg(0x11, (uint32_t) _State.rhyvol);

    // PCM reset & LIMIT SET
    _OPNA->SetReg(0x10c, 0xff);
    _OPNA->SetReg(0x10d, 0xff);

    for (int i = 0; i < PCM_CNL_MAX; i++)
        _PPZ8->SetPan(i, 5);
}

void PMD::Silence()
{
    _OPNA->SetReg(0x80, 0xff); // FM Release = 15
    _OPNA->SetReg(0x81, 0xff);
    _OPNA->SetReg(0x82, 0xff);
    _OPNA->SetReg(0x84, 0xff);
    _OPNA->SetReg(0x85, 0xff);
    _OPNA->SetReg(0x86, 0xff);
    _OPNA->SetReg(0x88, 0xff);
    _OPNA->SetReg(0x89, 0xff);
    _OPNA->SetReg(0x8a, 0xff);
    _OPNA->SetReg(0x8c, 0xff);
    _OPNA->SetReg(0x8d, 0xff);
    _OPNA->SetReg(0x8e, 0xff);

    _OPNA->SetReg(0x180, 0xff);
    _OPNA->SetReg(0x181, 0xff);
    _OPNA->SetReg(0x184, 0xff);
    _OPNA->SetReg(0x185, 0xff);
    _OPNA->SetReg(0x188, 0xff);
    _OPNA->SetReg(0x189, 0xff);
    _OPNA->SetReg(0x18c, 0xff);
    _OPNA->SetReg(0x18d, 0xff);

    _OPNA->SetReg(0x182, 0xff);
    _OPNA->SetReg(0x186, 0xff);
    _OPNA->SetReg(0x18a, 0xff);
    _OPNA->SetReg(0x18e, 0xff);

    _OPNA->SetReg(0x28, 0x00); // FM KEYOFF
    _OPNA->SetReg(0x28, 0x01);
    _OPNA->SetReg(0x28, 0x02);
    _OPNA->SetReg(0x28, 0x04); // FM KEYOFF [URA]
    _OPNA->SetReg(0x28, 0x05);
    _OPNA->SetReg(0x28, 0x06);

    _PPS->Stop();
    _P86->Stop();
    _P86->SetPan(3, 0);

    // 2003.11.30 For small noise measures
//@  if(effwork.effon == 0) {
    _OPNA->SetReg(0x07, 0xBF);
    _OPNA->SetReg(0x08, 0x00);
    _OPNA->SetReg(0x09, 0x00);
    _OPNA->SetReg(0x0a, 0x00);
//@  } else {
//@ opna->SetReg(0x07, (opna->GetReg(0x07) & 0x3f) | 0x9b);
//@  }

    _OPNA->SetReg(0x10, 0xff);   // Rhythm dump

    _OPNA->SetReg(0x101, 0x02);  // PAN=0 / x8 bit mode
    _OPNA->SetReg(0x100, 0x01);  // PCM RESET
    _OPNA->SetReg(0x110, 0x80);  // TA/TB/EOS を RESET
    _OPNA->SetReg(0x110, 0x18);  // Bit change only for TIMERB/A/EOS

    for (int i = 0; i < PCM_CNL_MAX; i++)
        _PPZ8->Stop(i);
}

/// <summary>
/// Starts the driver.
/// </summary>
void PMD::Start()
{
    if (_State._IsTimerABusy || _State._IsTimerBBusy)
    {
        _DriverState.music_flag |= 1; // Not executed during TA/TB processing

        return;
    }

    DriverStart();
}

/// <summary>
/// Stops the driver.
/// </summary>
void PMD::Stop()
{
    if (_State._IsTimerABusy || _State._IsTimerBBusy)
    {
        _DriverState.music_flag |= 2;
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
    // Set TimerB = 0 and Timer Reset (to match the length of the song every time)
    _State.tempo_d = 0;

    settempo_b();

    _OPNA->SetReg(0x27, 0x00); // TIMER RESET (both timer A and B)

    _DriverState.music_flag &= 0xFE;
    DriverStop();

    _SamplePtr = _SampleSrc;
    _SamplesToDo = 0;
    _Position = 0;

    InitializeDataArea();
    InitializeTracks();

    opn_init();

    setint();

    _State._IsPlaying = true;
}

void PMD::DriverStop()
{
    _DriverState.music_flag &= 0xfd;

    _State._IsPlaying = false;
    _State._LoopCount = -1;
    _State._FadeOutSpeed = 0;
    _State._FadeOutVolume = 0xFF;

    Silence();
}

/// <summary>
/// Sets the start address and initial value of each track.
/// </summary>
void PMD::InitializeTracks()
{
    _State.x68_flg = *(_State._MData - 1);

    // 2.6 additional minutes
    if (*_State._MData != 2 * (max_part2 + 1))
    {
        _State.prgdat_adr = _State._MData + *(uint16_t *) (&_State._MData[2 * (max_part2 + 1)]);
        _State.prg_flg = 1;
    }
    else
    {
        _State.prg_flg = 0;
    }

    uint16_t * p = (uint16_t *) _State._MData;

    for (size_t i = 0; i < _countof(_FMTrack); ++i)
    {
        if (_State._MData[*p] == 0x80) // Do not play.
            _FMTrack[i].address = nullptr;
        else
            _FMTrack[i].address = &_State._MData[*p];

        _FMTrack[i].leng = 1;
        _FMTrack[i].keyoff_flag = -1;    // 現在keyoff中
        _FMTrack[i].mdc = -1;        // MDepth Counter (無限)
        _FMTrack[i].mdc2 = -1;      // 同上
        _FMTrack[i]._mdc = -1;      // 同上
        _FMTrack[i]._mdc2 = -1;      // 同上
        _FMTrack[i].onkai = 255;      // rest
        _FMTrack[i].onkai_def = 255;    // rest
        _FMTrack[i].volume = 108;      // FM  VOLUME DEFAULT= 108
        _FMTrack[i].fmpan = 0xc0;      // FM PAN = Middle
        _FMTrack[i].slotmask = 0xf0;    // FM SLOT MASK
        _FMTrack[i].neiromask = 0xff;    // FM Neiro MASK

        p++;
    }

    for (size_t i = 0; i < _countof(_SSGTrack); i++)
    {
        if (_State._MData[*p] == 0x80) // Do not play.
            _SSGTrack[i].address = nullptr;
        else
            _SSGTrack[i].address = &_State._MData[*p];

        _SSGTrack[i].leng = 1;
        _SSGTrack[i].keyoff_flag = -1;  // 現在keyoff中
        _SSGTrack[i].mdc = -1;      // MDepth Counter (無限)
        _SSGTrack[i].mdc2 = -1;      // 同上
        _SSGTrack[i]._mdc = -1;      // 同上
        _SSGTrack[i]._mdc2 = -1;      // 同上
        _SSGTrack[i].onkai = 255;      // rest
        _SSGTrack[i].onkai_def = 255;    // rest
        _SSGTrack[i].volume = 8;      // PSG VOLUME DEFAULT= 8
        _SSGTrack[i].psgpat = 7;      // PSG = TONE
        _SSGTrack[i].envf = 3;      // PSG ENV = NONE/normal

        p++;
    }

    if (_State._MData[*p] == 0x80) // Do not play
        _ADPCMTrack.address = NULL;
    else
        _ADPCMTrack.address = &_State._MData[*p];

    _ADPCMTrack.leng = 1;
    _ADPCMTrack.keyoff_flag = -1;    // 現在keyoff中
    _ADPCMTrack.mdc = -1;        // MDepth Counter (無限)
    _ADPCMTrack.mdc2 = -1;      // 同上
    _ADPCMTrack._mdc = -1;      // 同上
    _ADPCMTrack._mdc2 = -1;      // 同上
    _ADPCMTrack.onkai = 255;      // rest
    _ADPCMTrack.onkai_def = 255;    // rest
    _ADPCMTrack.volume = 128;      // PCM VOLUME DEFAULT= 128
    _ADPCMTrack.fmpan = 0xc0;      // PCM PAN = Middle
    p++;

    if (_State._MData[*p] == 0x80) // Do not play
        _RhythmTrack.address = nullptr;
    else
        _RhythmTrack.address = &_State._MData[*p];

    _RhythmTrack.leng = 1;
    _RhythmTrack.keyoff_flag = -1;  // 現在keyoff中
    _RhythmTrack.mdc = -1;      // MDepth Counter (無限)
    _RhythmTrack.mdc2 = -1;      // 同上
    _RhythmTrack._mdc = -1;      // 同上
    _RhythmTrack._mdc2 = -1;      // 同上
    _RhythmTrack.onkai = 255;      // rest
    _RhythmTrack.onkai_def = 255;    // rest
    _RhythmTrack.volume = 15;      // PPSDRV volume
    p++;

    _State._RythmAddressTable = (uint16_t *) &_State._MData[*p];

    _State.rhyadr = (uint8_t *) &_DriverState.rhydmy;
}

//  インタラプト　設定
//  FM音源専用
void PMD::setint()
{
    // ＯＰＮ割り込み初期設定
    _State.tempo_d = 200;
    _State.tempo_d_push = 200;

    calc_tb_tempo();
    settempo_b();

    _OPNA->SetReg(0x25, 0x00);      // TIMER A SET (9216μs固定)
    _OPNA->SetReg(0x24, 0x00);      // 一番遅くて丁度いい
    _OPNA->SetReg(0x27, 0x3f);      // TIMER ENABLE

    //　小節カウンタリセット
    _State.opncount = 0;
    _State.syousetu = 0;
    _State.syousetu_lng = 96;
}

//  T->t 変換
//    input  [tempo_d]
//    output  [tempo_48]
void PMD::calc_tb_tempo()
{
    //  TEMPO = 0x112C / [ 256 - TB ]  timerB -> tempo
    int temp;

    if (256 - _State.tempo_d == 0)
    {
        temp = 255;
    }
    else
    {
        temp = (0x112c * 2 / (256 - _State.tempo_d) + 1) / 2;

        if (temp > 255)
            temp = 255;
    }

    _State.tempo_48 = temp;
    _State.tempo_48_push = temp;
}

//  t->T 変換
//    input  [tempo_48]
//    output  [tempo_d]
void PMD::calc_tempo_tb()
{
    int    al;

    //  TB = 256 - [ 112CH / TEMPO ]  tempo -> timerB

    if (_State.tempo_48 >= 18)
    {
        al = 256 - 0x112c / _State.tempo_48;
        if (0x112c % _State.tempo_48 >= 128)
        {
            al--;
        }
        //al = 256 - (0x112c * 2 / open_work.tempo_48 + 1) / 2;
    }
    else
    {
        al = 0;
    }
    _State.tempo_d = al;
    _State.tempo_d_push = al;
}

//  ＰＣＭメモリからメインメモリへのデータ取り込み
//
//  INPUTS   .. pcmstart     to Start Address
//      .. pcmstop      to Stop  Address
//      .. buf      to PCMDATA_Buffer
void PMD::pcmread(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf)
{
    _OPNA->SetReg(0x100, 0x01);
    _OPNA->SetReg(0x110, 0x00);
    _OPNA->SetReg(0x110, 0x80);
    _OPNA->SetReg(0x100, 0x20);
    _OPNA->SetReg(0x101, 0x02);    // x8
    _OPNA->SetReg(0x10c, 0xff);
    _OPNA->SetReg(0x10d, 0xff);
    _OPNA->SetReg(0x102, (uint32_t) LOBYTE(pcmstart));
    _OPNA->SetReg(0x103, (uint32_t) HIBYTE(pcmstart));
    _OPNA->SetReg(0x104, 0xff);
    _OPNA->SetReg(0x105, 0xff);

    *buf = (uint8_t) _OPNA->GetReg(0x108);    // 無駄読み
    *buf = (uint8_t) _OPNA->GetReg(0x108);    // 無駄読み

    for (int i = 0; i < (pcmstop - pcmstart) * 32; i++)
    {
        *buf++ = (uint8_t) _OPNA->GetReg(0x108);

        _OPNA->SetReg(0x110, 0x80);
    }
}

//  ＰＣＭメモリへメインメモリからデータを送る (x8,高速版)
//
//  INPUTS   .. pcmstart     to Start Address
//      .. pcmstop      to Stop  Address
//      .. buf      to PCMDATA_Buffer
void PMD::pcmstore(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf)
{
    _OPNA->SetReg(0x100, 0x01);
//  _OPNA->SetReg(0x110, 0x17);  // brdy以外はマスク(=timer割り込みは掛からない)
    _OPNA->SetReg(0x110, 0x80);
    _OPNA->SetReg(0x100, 0x60);
    _OPNA->SetReg(0x101, 0x02);  // x8
    _OPNA->SetReg(0x10c, 0xff);
    _OPNA->SetReg(0x10d, 0xff);
    _OPNA->SetReg(0x102, (uint32_t) LOBYTE(pcmstart));
    _OPNA->SetReg(0x103, (uint32_t) HIBYTE(pcmstart));
    _OPNA->SetReg(0x104, 0xff);
    _OPNA->SetReg(0x105, 0xff);

    for (int i = 0; i < (pcmstop - pcmstart) * 32; i++)
        _OPNA->SetReg(0x108, *buf++);
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

    ::wcscpy(_State._PPCFileName, filePath);

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
        _State._PPCFileName[0] = '\0';

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
        for (i = 0; i < 128; i++)
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
        for (i = 128; i < 256; i++)
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
            _State._PPCFileName[0] = '\0';

            return ERR_UNKNOWN_FORMAT;
        }

        pcmends.Count = *pcmdata2++;

        for (i = 0; i < 256; i++)
        {
            pcmends.Address[i][0] = *pcmdata2++;
            pcmends.Address[i][1] = *pcmdata2++;
        }
    }
    else
    {
        _State._PPCFileName[0] = '\0';

        return ERR_UNKNOWN_FORMAT;
    }

    uint8_t tempbuf[0x26 * 32];

    // Compare PMD work and PCMRAM header
    pcmread(0, 0x25, tempbuf);

    // Skip the "ADPCM?" header
    // Ignore file name (PMDWin specification)
    if (::memcmp(&tempbuf[30], &pcmends, sizeof(pcmends)) == 0)
        return ERR_ALREADY_LOADED;

    uint8_t tempbuf2[30 + 4 * 256 + 128 + 2];

    // Write PMD work to PCMRAM head
    ::memcpy(tempbuf2, PPCHeader, sizeof(PPCHeader) - 1);
    ::memcpy(&tempbuf2[sizeof(PPCHeader) - 1], &pcmends.Count, sizeof(tempbuf2) - (sizeof(PPCHeader) - 1));

    pcmstore(0, 0x25, tempbuf2);

    // Write PCMDATA to PCMRAM
    if (FoundPVI)
    {
        pcmdata2 = (uint16_t *) (pcmdata + 0x10 + sizeof(uint16_t) * 2 * 128);

        if (size < (int) (pcmends.Count - (0x10 + sizeof(uint16_t) * 2 * 128)) * 32)
        {
            _State._PPCFileName[0] = '\0';

            return ERR_UNKNOWN_FORMAT;
        }
    }
    else
    {
        pcmdata2 = (uint16_t *) pcmdata + (30 + 4 * 256 + 2) / 2;

        if (size < (pcmends.Count - ((30 + 4 * 256 + 2) / 2)) * 32)
        {
            _State._PPCFileName[0] = '\0';

            return ERR_UNKNOWN_FORMAT;
        }
    }

    uint16_t pcmstart = 0x26;
    uint16_t pcmstop = pcmends.Count;

    pcmstore(pcmstart, pcmstop, (uint8_t *) pcmdata2);

    return ERR_SUCCESS;
}

/// <summary>
/// Finds a PCM sample in the specified search path.
/// </summary>
WCHAR * PMD::FindFile(WCHAR * filePath, const WCHAR * filename)
{
    WCHAR FilePath[MAX_PATH];

    for (size_t i = 0; i < _State._SearchPath.size(); ++i)
    {
        CombinePath(FilePath, _countof(FilePath), _State._SearchPath[i].c_str(), filename);

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

    if (qq->Mask)
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
    if (_State._FadeOutSpeed == 0)
        return;

    if (_State._FadeOutSpeed > 0)
    {
        if (_State._FadeOutSpeed + _State._FadeOutVolume < 256)
        {
            _State._FadeOutVolume += _State._FadeOutSpeed;
        }
        else
        {
            _State._FadeOutVolume = 255;
            _State._FadeOutSpeed  =   0;

            if (_State.fade_stop_flag == 1)
                _DriverState.music_flag |= 2;
        }
    }
    else
    {   // Fade in
        if (_State._FadeOutSpeed + _State._FadeOutVolume > 255)
        {
            _State._FadeOutVolume += _State._FadeOutSpeed;
        }
        else
        {
            _State._FadeOutVolume = 0;
            _State._FadeOutSpeed = 0;

            _OPNA->SetReg(0x11, (uint32_t) _State.rhyvol);
        }
    }
}
