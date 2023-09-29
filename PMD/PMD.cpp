
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

    _OPNAW = new OPNAW(_File);
    _PPZ = new PPZ8(_File);
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
    delete _PPZ;
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

    InitializeInternal();

    _PPZ->Init(_Work._OPNARate, false);
    _PPS->Init(_Work._OPNARate, false);
    _P86->Init(_Work._OPNARate, false);

    if (_OPNAW->Init(OPNAClock, SOUND_44K, false, DirectoryPath) == false)
        return false;

    // Initialize ADPCM RAM.
    {
        _OPNAW->SetFMWait(0);
        _OPNAW->SetSSGWait(0);
        _OPNAW->SetRhythmWait(0);
        _OPNAW->SetADPCMWait(0);

        uint8_t  Page[0x400]; // 0x400 * 0x100 = 0x40000(256K)

        ::memset(Page, 0x08, sizeof(Page));

        for (int i = 0; i < 0x100; i++)
            pcmstore((uint16_t) i * sizeof(Page) / 32, (uint16_t) (i + 1) * sizeof(Page) / 32, Page);
    }

    _OPNAW->SetFMVolume(0);
    _OPNAW->SetPSGVolume(-18);
    _OPNAW->SetADPCMVolume(0);
    _OPNAW->SetRhythmMasterVolume(0);

    _PPZ->SetVolume(0);
    _PPS->SetVolume(0);
    _P86->SetVolume(0);

    _OPNAW->SetFMWait(DEFAULT_REG_WAIT);
    _OPNAW->SetSSGWait(DEFAULT_REG_WAIT);
    _OPNAW->SetRhythmWait(DEFAULT_REG_WAIT);
    _OPNAW->SetADPCMWait(DEFAULT_REG_WAIT);

    pcmends.pcmends = 0x26;

    for (int i = 0; i < 256; i++)
    {
        pcmends.pcmadrs[i][0] = 0;
        pcmends.pcmadrs[i][1] = 0;
    }

    _Work._PPCFileName[0] = '\0';

    // Initial setting of 088/188/288/388 (same INT number only)
    _OPNAW->SetReg(0x29, 0x00);
    _OPNAW->SetReg(0x24, 0x00);
    _OPNAW->SetReg(0x25, 0x00);
    _OPNAW->SetReg(0x26, 0x00);
    _OPNAW->SetReg(0x27, 0x3f);

    StartOPNInterrupt();

    return true;
}

// Initialization (internal processing)
void PMD::InitializeInternal()
{
    ::memset(&_Work, 0, sizeof(_Work));

    ::memset(_FMChannel, 0, sizeof(_FMChannel));
    ::memset(_SSGChannel, 0, sizeof(_SSGChannel));
    ::memset(&_ADPCMChannel, 0, sizeof(_ADPCMChannel));
    ::memset(&_RhythmChannel, 0, sizeof(_RhythmChannel));
    ::memset(_ExtensionChannel, 0, sizeof(_ExtensionChannel));
    ::memset(&_DummyChannel, 0, sizeof(_DummyChannel));
    ::memset(&_EffectChannel, 0, sizeof(_EffectChannel));
    ::memset(_PPZChannel, 0, sizeof(_PPZChannel));

    ::memset(&_PMDWork, 0, sizeof(PMDWork));
    ::memset(&_EffectState, 0, sizeof(EffectState));
    ::memset(&pcmends, 0, sizeof(pcmends));

    ::memset(wavbuf2, 0, sizeof(wavbuf2));
    ::memset(wavbuf, 0, sizeof(wavbuf2));
    ::memset(wavbuf_conv, 0, sizeof(wavbuf_conv));

    _PCMPtr = (uint8_t *) wavbuf2;
    
    _SamplesToDo = 0;
    _Position = 0;
    _FadeOutPosition = 0;
    _Seed = 0;

    ::memset(_MData, 0, sizeof(_MData));
    ::memset(_VData, 0, sizeof(_VData));
    ::memset(_EData, 0, sizeof(_EData));
    ::memset(&pcmends, 0, sizeof(pcmends));

    // Initialize Work.
    _Work._OPNARate = SOUND_44K;
    _Work._PPZ8Rate = SOUND_44K;
    _Work.rhyvol = 0x3c;
    _Work.fade_stop_flag = 0;
    _Work._IsTimerBBusy = false;

    _Work._IsTimerABusy = false;
    _Work._TimerBSpeed = 0x100;
    _Work.port22h = 0;

    _Work._UseP86 = false;

    _Work.ppz8ip = false;
    _Work.p86ip = false;
    _Work.ppsip = false;

    // Initialize variables.
    _Work.Channel[ 0] = &_FMChannel[0];
    _Work.Channel[ 1] = &_FMChannel[1];
    _Work.Channel[ 2] = &_FMChannel[2];
    _Work.Channel[ 3] = &_FMChannel[3];
    _Work.Channel[ 4] = &_FMChannel[4];
    _Work.Channel[ 5] = &_FMChannel[5];

    _Work.Channel[ 6] = &_SSGChannel[0];
    _Work.Channel[ 7] = &_SSGChannel[1];
    _Work.Channel[ 8] = &_SSGChannel[2];

    _Work.Channel[ 9] = &_ADPCMChannel;

    _Work.Channel[10] = &_RhythmChannel;

    _Work.Channel[11] = &_ExtensionChannel[0];
    _Work.Channel[12] = &_ExtensionChannel[1];
    _Work.Channel[13] = &_ExtensionChannel[2];

    _Work.Channel[14] = &_DummyChannel;
    _Work.Channel[15] = &_EffectChannel;

    _Work.Channel[16] = &_PPZChannel[0];
    _Work.Channel[17] = &_PPZChannel[1];
    _Work.Channel[18] = &_PPZChannel[2];
    _Work.Channel[19] = &_PPZChannel[3];
    _Work.Channel[20] = &_PPZChannel[4];
    _Work.Channel[21] = &_PPZChannel[5];
    _Work.Channel[22] = &_PPZChannel[6];
    _Work.Channel[23] = &_PPZChannel[7];

    _MData[0] = 0;

    for (int i = 0; i < 12; ++i)
    {
        _MData[i * 2 + 1] = 0x18;
        _MData[i * 2 + 2] = 0x00;
    }

    _MData[25] = 0x80;

    _Work.fm_voldown = fmvd_init;   // FM_VOLDOWN
    _Work._fm_voldown = fmvd_init;  // FM_VOLDOWN

    _Work.ssg_voldown = 0;          // SSG_VOLDOWN
    _Work._ssg_voldown = 0;         // SSG_VOLDOWN

    _Work.pcm_voldown = 0;          // PCM_VOLDOWN
    _Work._pcm_voldown = 0;         // PCM_VOLDOWN

    _Work.ppz_voldown = 0;          // PPZ_VOLDOWN
    _Work._ppz_voldown = 0;         // PPZ_VOLDOWN

    _Work.rhythm_voldown = 0;       // RHYTHM_VOLDOWN
    _Work._rhythm_voldown = 0;      // RHYTHM_VOLDOWN

    _Work._UseRhythmSoundSource = false;   // Whether to play the Rhytmn Sound Source with SSGDRUM

    _Work.rshot_bd = 0;             // Rhythm Sound Source shot inc flag (BD)
    _Work.rshot_sd = 0;             // Rhythm Sound Source shot inc flag (SD)
    _Work.rshot_sym = 0;            // Rhythm Sound Source shot inc flag (CYM)
    _Work.rshot_hh = 0;             // Rhythm Sound Source shot inc flag (HH)
    _Work.rshot_tom = 0;            // Rhythm Sound Source shot inc flag (TOM)
    _Work.rshot_rim = 0;            // Rhythm Sound Source shot inc flag (RIM)

    _Work.rdump_bd = 0;             // Rhythm Sound dump inc flag (BD)
    _Work.rdump_sd = 0;             // Rhythm Sound dump inc flag (SD)
    _Work.rdump_sym = 0;            // Rhythm Sound dump inc flag (CYM)
    _Work.rdump_hh = 0;             // Rhythm Sound dump inc flag (HH)
    _Work.rdump_tom = 0;            // Rhythm Sound dump inc flag (TOM)
    _Work.rdump_rim = 0;            // Rhythm Sound dump inc flag (RIM)

    _Work.pcm86_vol = 0;            // PCM volume adjustment
    _Work._pcm86_vol = 0;           // PCM volume adjustment
    _Work.fade_stop_flag = 1;       // MSTOP after FADEOUT FLAG

    _PMDWork._UsePPS = false;
    _PMDWork.music_flag = 0;

    // Set song data and timbre data storage addresses.
    _Work.mmlbuf = &_MData[1];
    _Work.tondat = _VData;
    _Work.efcdat = _EData;

    // Initialize sound effects FMINT/EFCINT.
    _EffectState.effon = 0;
    _EffectState.psgefcnum = 0xff;
}

int PMD::Load(const uint8_t * data, size_t size)
{
    if (size > sizeof(_MData))
        return ERR_UNKNOWN_FORMAT;

    // 020120 Header parsing only for Towns
    if ((data[0] > 0x0F && data[0] != 0xFF) || (data[1] != 0x18 && data[1] != 0x1A) || data[2] != 0x00)
        return ERR_UNKNOWN_FORMAT;

    Stop();

    ::memcpy(_MData, data, size);
    ::memset(_MData + size, 0, sizeof(_MData) - size);

    if (_Work._SearchPath.size() == 0)
        return ERR_SUCCESS;

    int Result = ERR_SUCCESS;

    char FileName[MAX_PATH] = { 0 };
    WCHAR FileNameW[MAX_PATH] = { 0 };

    WCHAR FilePath[MAX_PATH] = { 0 };

    // P86/PPC reading
    {
        GetNoteInternal(data, size, 0, FileName);

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

            if (HasExtension(FileNameW, _countof(FileNameW), L".P86")) // Is it a Professional Music Driver P86 Samples Pack file?
            {
                FindFile(FilePath, FileNameW);

                Result = _P86->Load(FilePath);

                if (Result == P86_SUCCESS || Result == P86_ALREADY_LOADED)
                    _Work._UseP86 = true;
            }
            else
            if (HasExtension(FileNameW, _countof(FileNameW), L".PPC"))
            {
                FindFile(FilePath, FileNameW);

                Result = LoadPPCInternal(FilePath);

                if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
                    _Work._UseP86 = false;
            }
        }
    }

    // PPS import
    {
        GetNoteInternal(data, size, -1, FileName);

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
        GetNoteInternal(data, size, -2, FileName);

        if (*FileName != '\0')
        {
            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, FileName, -1, FileNameW, _countof(FileNameW));

            if (HasExtension(FileNameW, _countof(FileNameW), L".PZI") && (data[0] != 0xff))
            {
                FindFile(FilePath, FileNameW);

                Result = _PPZ->Load(FilePath, 0);
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

                    Result = _PPZ->Load(FilePath, 0);
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

    int FMWait = _OPNAW->GetFMWait();
    int SSGWait = _OPNAW->GetSSGWait();
    int RhythmWait = _OPNAW->GetRhythmWait();
    int ADPCMWait = _OPNAW->GetADPCMWait();

    _OPNAW->SetFMWait(0);
    _OPNAW->SetSSGWait(0);
    _OPNAW->SetRhythmWait(0);
    _OPNAW->SetADPCMWait(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNAW->SetReg(0x27, _Work.ch3mode | 0x30); // Timer Reset (Both timer A and B)

            uint32_t us = _OPNAW->GetNextEvent();

            _OPNAW->Count(us);
            _Position += us;
        }

        if ((_Work._LoopCount == 1) && (*songLength == 0)) // When looping
        {
            *songLength = (int) (_Position / 1000);
        }
        else
        if (_Work._LoopCount == -1) // End without loop
        {
            *songLength = (int) (_Position / 1000);
            *loopLength = 0;

            DriverStop();

            _OPNAW->SetFMWait(FMWait);
            _OPNAW->SetSSGWait(SSGWait);
            _OPNAW->SetRhythmWait(RhythmWait);
            _OPNAW->SetADPCMWait(ADPCMWait);

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
    while (_Work._LoopCount < 2);

    *loopLength = (int) (_Position / 1000) - *songLength;

    DriverStop();

    _OPNAW->SetFMWait(FMWait);
    _OPNAW->SetSSGWait(SSGWait);
    _OPNAW->SetRhythmWait(RhythmWait);
    _OPNAW->SetADPCMWait(ADPCMWait);

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

    int FMWait = _OPNAW->GetFMWait();
    int SSGWait = _OPNAW->GetSSGWait();
    int RhythmWait = _OPNAW->GetRhythmWait();
    int ADPCMWait = _OPNAW->GetADPCMWait();

    _OPNAW->SetFMWait(0);
    _OPNAW->SetSSGWait(0);
    _OPNAW->SetRhythmWait(0);
    _OPNAW->SetADPCMWait(0);

    do
    {
        {
            if (_OPNAW->ReadStatus() & 0x01)
                HandleTimerA();

            if (_OPNAW->ReadStatus() & 0x02)
                HandleTimerB();

            _OPNAW->SetReg(0x27, _Work.ch3mode | 0x30);  // Timer Reset (Both timer A and B)

            uint32_t us = _OPNAW->GetNextEvent();

            _OPNAW->Count(us);
            _Position += us;
        }

        if ((_Work._LoopCount == 1) && (*eventCount == 0)) // When looping
        {
            *eventCount = GetEventNumber();
        }
        else
        if (_Work._LoopCount == -1) // End without loop
        {
            *eventCount = GetEventNumber();
            *loopEventCount = 0;

            DriverStop();

            _OPNAW->SetFMWait(FMWait);
            _OPNAW->SetSSGWait(SSGWait);
            _OPNAW->SetRhythmWait(RhythmWait);
            _OPNAW->SetADPCMWait(ADPCMWait);

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
    while (_Work._LoopCount < 2);

    *loopEventCount = GetEventNumber() - *eventCount;

    DriverStop();

    _OPNAW->SetFMWait(FMWait);
    _OPNAW->SetSSGWait(SSGWait);
    _OPNAW->SetRhythmWait(RhythmWait);
    _OPNAW->SetADPCMWait(ADPCMWait);

    return true;
}

// Gets the current loop number.
uint32_t PMD::GetLoopNumber()
{
    return (uint32_t) _Work._LoopCount;
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

        _PCMPtr = (uint8_t *) wavbuf2;    // Start position of remaining samples in buf

        _SamplesToDo = 0;           // Number of samples remaining in buf
        _Position = 0;              // Time from start of playing (μsec)
    }

    while (_Position < NewPosition)
    {
        if (_OPNAW->ReadStatus() & 0x01)
            HandleTimerA();

        if (_OPNAW->ReadStatus() & 0x02)
            HandleTimerB();

        _OPNAW->SetReg(0x27, _Work.ch3mode | 0x30); // Timer Reset (Both timer A and B)

        uint32_t us = _OPNAW->GetNextEvent();

        _OPNAW->Count(us);
        _Position += us;
    }

    if (_Work._LoopCount == -1)
        Silence();

    _OPNAW->ClearBuffer();
}

// Renders a chunk of PCM data.
void PMD::Render(int16_t * sampleData, int sampleCount)
{
    int  SamplesRendered = 0;

    do
    {
        if (sampleCount - SamplesRendered <= _SamplesToDo)
        {
            ::memcpy(sampleData, _PCMPtr, ((size_t) sampleCount - SamplesRendered) * sizeof(uint16_t) * 2);
            _SamplesToDo -= (sampleCount - SamplesRendered);

            _PCMPtr += ((size_t) sampleCount - SamplesRendered) * sizeof(uint16_t) * 2;
            SamplesRendered = sampleCount;
        }
        else
        {
            {
                ::memcpy(sampleData, _PCMPtr, _SamplesToDo * sizeof(uint16_t) * 2);
                sampleData += (_SamplesToDo * 2);

                _PCMPtr = (uint8_t *) wavbuf2;
                SamplesRendered += _SamplesToDo;
            }

            {
                if (_OPNAW->ReadStatus() & 0x01)
                    HandleTimerA();

                if (_OPNAW->ReadStatus() & 0x02)
                    HandleTimerB();

                _OPNAW->SetReg(0x27, _Work.ch3mode | 0x30); // Timer Reset (Both timer A and B)
            }

            uint32_t us = _OPNAW->GetNextEvent(); // in microseconds

            {
                _SamplesToDo = (int) ((double) us * _Work._OPNARate / 1000000.0);
                _OPNAW->Count(us);

                ::memset(wavbuf, 0x00, _SamplesToDo * sizeof(Sample) * 2);

                if (_Work._OPNARate == _Work._PPZ8Rate)
                    _PPZ->Mix((Sample *) wavbuf, _SamplesToDo);
                else
                {
                    // PCM frequency transform of ppz8 (no interpolation)
                    int ppzsample = (int) (_SamplesToDo * _Work._PPZ8Rate / _Work._OPNARate + 1);
                    int delta     = (int) (8192         * _Work._PPZ8Rate / _Work._OPNARate);

                    ::memset(wavbuf_conv, 0, ppzsample * sizeof(Sample) * 2);

                    _PPZ->Mix((Sample *) wavbuf_conv, ppzsample);

                    int carry = 0;

                    // Frequency transform (1 << 13 = 8192)
                    for (int i = 0; i < _SamplesToDo; i++)
                    {
                        wavbuf[i].left  = wavbuf_conv[(carry >> 13)].left;
                        wavbuf[i].right = wavbuf_conv[(carry >> 13)].right;

                        carry += delta;
                    }
                }
            }

            {
                _OPNAW->Mix((Sample *) wavbuf, _SamplesToDo);

                if (_PMDWork._UsePPS)
                    _PPS->Mix((Sample *) wavbuf, _SamplesToDo);

                if (_Work._UseP86)
                    _P86->Mix((Sample *) wavbuf, _SamplesToDo);
            }

            {
                _Position += us;

                if (_Work._FadeOutSpeedHQ > 0)
                {
                    int  ftemp = (_Work._LoopCount != -1) ? (int) ((1 << 10) * ::pow(512, -(double) (_Position - _FadeOutPosition) / 1000 / _Work._FadeOutSpeedHQ)) : 0;

                    for (int i = 0; i < _SamplesToDo; i++)
                    {
                        wavbuf2[i].left  = (short) Limit(wavbuf[i].left  * ftemp >> 10, 32767, -32768);
                        wavbuf2[i].right = (short) Limit(wavbuf[i].right * ftemp >> 10, 32767, -32768);
                    }

                    // Fadeout end
                    if ((_Position - _FadeOutPosition > (int64_t)_Work._FadeOutSpeedHQ * 1000) && (_Work.fade_stop_flag == 1))
                        _PMDWork.music_flag |= 2;
                }
                else
                {
                    for (int i = 0; i < _SamplesToDo; i++)
                    {
                        wavbuf2[i].left  = (short) Limit(wavbuf[i].left,  32767, -32768);
                        wavbuf2[i].right = (short) Limit(wavbuf[i].right, 32767, -32768);
                    }
                }
            }
        }
    }
    while (SamplesRendered < sampleCount);
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

        _Work._SearchPath.push_back(Path);
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
        _Work._OPNARate =
        _Work._PPZ8Rate = SOUND_44K;
        _Work.fmcalc55k = true;
    }
    else
    {
        _Work._OPNARate =
        _Work._PPZ8Rate = frequency;
        _Work.fmcalc55k = false;
    }

    _OPNAW->SetRate(OPNAClock, _Work._OPNARate, _Work.fmcalc55k);

    _PPZ->SetRate(_Work._PPZ8Rate, _Work.ppz8ip);
    _PPS->SetRate(_Work._OPNARate, _Work.ppsip);
    _P86->SetRate(_Work._OPNARate, _Work.p86ip);
}

/// <summary>
/// Sets the rate at which raw PPZ data is synthesized (in Hz, for example 44100)
/// </summary>
void PMD::SetPPZSynthesisRate(uint32_t frequency)
{
    _Work._PPZ8Rate = frequency;

    _PPZ->SetRate(frequency, _Work.ppz8ip);
}

//Enable 55kHz synthesis in FM primary interpolation.
void PMD::EnableFM55kHzSynthesis(bool flag)
{
    _Work.fmcalc55k = flag;

    _OPNAW->SetRate(OPNAClock, _Work._OPNARate, _Work.fmcalc55k);
}

// Enable PPZ8 primary completion.
void PMD::EnablePPZInterpolation(bool flag)
{
    _Work.ppz8ip = flag;

    _PPZ->SetRate(_Work._PPZ8Rate, flag);
}

// Sets FM Wait after register output.
void PMD::SetFMWait(int nsec)
{
    _OPNAW->SetFMWait(nsec);
}

// Sets SSG Wait after register output.
void PMD::SetSSGWait(int nsec)
{
    _OPNAW->SetSSGWait(nsec);
}

// Sets Rythm Wait after register output.
void PMD::SetRhythmWait(int nsec)
{
    _OPNAW->SetRhythmWait(nsec);
}

// Sets ADPCM Wait after register output.
void PMD::SetADPCMWait(int nsec)
{
    _OPNAW->SetADPCMWait(nsec);
}

// Fade out (PMD compatible)
void PMD::SetFadeOutSpeed(int speed)
{
    _Work._FadeOutSpeed = speed;
}

// Fade out (High quality sound)
void PMD::SetFadeOutDurationHQ(int speed)
{
    if (speed > 0)
    {
        if (_Work._FadeOutSpeedHQ == 0)
            _FadeOutPosition = _Position;

        _Work._FadeOutSpeedHQ = speed;
    }
    else
        _Work._FadeOutSpeedHQ = 0; // Fadeout forced stop
}

// Sets the playback position (in ticks).
void PMD::SetEventNumber(int pos)
{
    if (_Work.syousetu_lng * _Work.syousetu + _Work.opncount > pos)
    {
        DriverStart();

        _PCMPtr = (uint8_t *) wavbuf2; // Start position of remaining samples in buf
        _SamplesToDo = 0; // Number of samples remaining in buf
    }

    while (_Work.syousetu_lng * _Work.syousetu + _Work.opncount < pos)
    {
        if (_OPNAW->ReadStatus() & 0x01)
            HandleTimerA();

        if (_OPNAW->ReadStatus() & 0x02)
            HandleTimerB();

        _OPNAW->SetReg(0x27, _Work.ch3mode | 0x30); // Timer Reset (Both timer A and B)

        uint32_t us = _OPNAW->GetNextEvent();
        _OPNAW->Count(us);
    }

    if (_Work._LoopCount == -1)
        Silence();

    _OPNAW->ClearBuffer();
}

// Gets the playback position (in ticks)
int PMD::GetEventNumber()
{
    return _Work.syousetu_lng * _Work.syousetu + _Work.opncount;
}

// Gets PPC / P86 filename.
WCHAR * PMD::GetPCMFileName(WCHAR * filePath)
{
    if (_Work._UseP86)
        ::wcscpy(filePath, _P86->_FileName);
    else
        ::wcscpy(filePath, _Work._PPCFileName);

    return filePath;
}

// Gets PPZ filename.
WCHAR * PMD::GetPPZFileName(WCHAR * filePath, int index)
{
    ::wcscpy(filePath, _PPZ->PVI_FILE[index]);

    return filePath;
}

/// <summary>
/// Enables or disables the PPS.
/// </summary>
void PMD::UsePPS(bool value) noexcept
{
    _PMDWork._UsePPS = value;
}

/// <summary>
/// Enables playing the OPNA Rhythm with the SSG Sound Source.
/// </summary>
void PMD::UseSSG(bool flag) noexcept
{
    _Work._UseRhythmSoundSource = flag;
}

// Make PMD86 PCM compatible with PMDB2?
void PMD::EnablePMDB2CompatibilityMode(bool value)
{
    if (value)
    {
        _Work.pcm86_vol =
        _Work._pcm86_vol = 1;
    }
    else
    {
        _Work.pcm86_vol =
        _Work._pcm86_vol = 0;
    }
}

// Get whether PMD86's PCM is PMDB2 compatible
bool PMD::GetPMDB2CompatibilityMode()
{
    return _Work.pcm86_vol ? true : false;
}

//  PPS で一次補完するかどうかの設定
void PMD::setppsinterpolation(bool flag)
{
    _Work.ppsip = flag;
    _PPS->SetRate(_Work._OPNARate, flag);
}

//  P86 で一次補完するかどうかの設定
void PMD::setp86interpolation(bool flag)
{
    _Work.p86ip = flag;
    _P86->SetRate(_Work._OPNARate, flag);
}

/// <summary>
/// Disables the specified channel.
/// </summary>
int PMD::DisableChannel(int ch)
{
    if (ch >= sizeof(_Work.Channel) / sizeof(Channel *))
        return ERR_WRONG_PARTNO;

    if (part_table[ch][0] < 0)
    {
        _Work.rhythmmask = 0;  // Rhythm音源をMask
        _OPNAW->SetReg(0x10, 0xff);  // Rhythm音源を全部Dump
    }
    else
    {
        int fmseltmp = _PMDWork.fmsel;

        if ((_Work.Channel[ch]->partmask == 0) && _Work._IsPlaying)
        {
            if (part_table[ch][2] == 0)
            {
                _PMDWork.partb = part_table[ch][1];
                _PMDWork.fmsel = 0;

                MuteFMPart(_Work.Channel[ch]);
            }
            else
            if (part_table[ch][2] == 1)
            {
                _PMDWork.partb = part_table[ch][1];
                _PMDWork.fmsel = 0x100;

                MuteFMPart(_Work.Channel[ch]);
            }
            else
            if (part_table[ch][2] == 2)
            {
                _PMDWork.partb = part_table[ch][1];

                int ah = 1 << (_PMDWork.partb - 1);

                ah |= (ah << 3);

                // PSG keyoff
                _OPNAW->SetReg(0x07, ah | _OPNAW->GetReg(0x07));
            }
            else
            if (part_table[ch][2] == 3)
            {
                _OPNAW->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
                _OPNAW->SetReg(0x100, 0x01);    // PCM RESET
            }
            else
            if (part_table[ch][2] == 4)
            {
                if (_EffectState.psgefcnum < 11)
                    effend();
            }
            else
            if (part_table[ch][2] == 5)
                _PPZ->Stop(part_table[ch][1]);
        }

        _Work.Channel[ch]->partmask |= 1;
        _PMDWork.fmsel = fmseltmp;
    }

    return ERR_SUCCESS;
}

/// <summary>
/// Enables the specified channel.
/// </summary>
int PMD::EnableChannel(int ch)
{
    if (ch >= sizeof(_Work.Channel) / sizeof(Channel *))
        return ERR_WRONG_PARTNO;

    if (part_table[ch][0] < 0)
    {
        _Work.rhythmmask = 0xff;
    }
    else
    {
        if (_Work.Channel[ch]->partmask == 0)
            return ERR_NOT_MASKED;

        // Still masked by sound effects

        if ((_Work.Channel[ch]->partmask &= 0xfe) != 0)
            return ERR_EFFECT_USED;

        // The song has stopped.
        if (!_Work._IsPlaying)
            return ERR_MUSIC_STOPPED;

        int fmseltmp = _PMDWork.fmsel;

        if (_Work.Channel[ch]->address != NULL)
        {
            if (part_table[ch][2] == 0)
            {    // FM音源(表)
                _PMDWork.fmsel = 0;
                _PMDWork.partb = part_table[ch][1];
                neiro_reset(_Work.Channel[ch]);
            }
            else
            if (part_table[ch][2] == 1)
            {  // FM音源(裏)
                _PMDWork.fmsel = 0x100;
                _PMDWork.partb = part_table[ch][1];
                neiro_reset(_Work.Channel[ch]);
            }
        }

        _PMDWork.fmsel = fmseltmp;
    }

    return ERR_SUCCESS;
}

//  FM Volume Down の設定
void PMD::setfmvoldown(int voldown)
{
    _Work.fm_voldown = _Work._fm_voldown = voldown;
}

//  SSG Volume Down の設定
void PMD::setssgvoldown(int voldown)
{
    _Work.ssg_voldown = _Work._ssg_voldown = voldown;
}

//  Rhythm Volume Down の設定
void PMD::setrhythmvoldown(int voldown)
{
    _Work.rhythm_voldown = _Work._rhythm_voldown = voldown;
    _Work.rhyvol         = 48 * 4 * (256 - _Work.rhythm_voldown) / 1024;

    _OPNAW->SetReg(0x11, (uint32_t) _Work.rhyvol);
}

//  ADPCM Volume Down の設定
void PMD::setadpcmvoldown(int voldown)
{
    _Work.pcm_voldown = _Work._pcm_voldown = voldown;
}

//  PPZ8 Volume Down の設定
void PMD::setppzvoldown(int voldown)
{
    _Work.ppz_voldown = _Work._ppz_voldown = voldown;
}

//  FM Volume Down の取得
int PMD::getfmvoldown()
{
    return _Work.fm_voldown;
}

//  FM Volume Down の取得（その２）
int PMD::getfmvoldown2()
{
    return _Work._fm_voldown;
}

//  SSG Volume Down の取得
int PMD::getssgvoldown()
{
    return _Work.ssg_voldown;
}

//  SSG Volume Down の取得（その２）
int PMD::getssgvoldown2()
{
    return _Work._ssg_voldown;
}

//  Rhythm Volume Down の取得
int PMD::getrhythmvoldown()
{
    return _Work.rhythm_voldown;
}

//  Rhythm Volume Down の取得（その２）
int PMD::getrhythmvoldown2()
{
    return _Work._rhythm_voldown;
}

//  ADPCM Volume Down の取得
int PMD::getadpcmvoldown()
{
    return _Work.pcm_voldown;
}

//  ADPCM Volume Down の取得（その２）
int PMD::getadpcmvoldown2()
{
    return _Work._pcm_voldown;
}

//  PPZ8 Volume Down の取得
int PMD::getppzvoldown()
{
    return _Work.ppz_voldown;
}

//  PPZ8 Volume Down の取得（その２）
int PMD::getppzvoldown2()
{
    return _Work._ppz_voldown;
}

// Gets a note.
bool PMD::GetNote(const uint8_t * data, size_t size, int index, char * text, size_t textSize)
{
    if ((text == nullptr) || (textSize < 1))
        return false;

    text[0] = '\0';

    char a[1024 + 64];

    GetNoteInternal(data, size, index, a);

    char b[1024 + 64];

    zen2tohan(b, a);

    RemoveEscapeSequences(text, b);

    return true;
}

// Gets a note
char * PMD::GetNoteInternal(const uint8_t * data, size_t size, int index, char * text)
{
    const uint8_t * Data;
    size_t Size;

    if (data == nullptr || size == 0)
    {
        Data = _Work.mmlbuf;
        Size = sizeof(_MData) - 1;
    }
    else
    {
        Data = &data[1];
        Size = size - 1;
    }

    if (Size < 2)
    {
        *text = '\0'; // Incorrect song data

        return NULL;
    }

    if (Data[0] != 0x1a || Data[1] != 0x00)
    {
        *text = '\0'; // Unable to get address of file=memo without sound

        return text;
    }

    if (Size < (size_t)0x18 + 1)
    {
        *text = '\0'; // Incorrect song data

        return NULL;
    }

    if (Size < (size_t)*(uint16_t *) &Data[0x18] - 4 + 3)
    {
        *text = '\0'; // Incorrect song data

        return NULL;
    }

    const uint8_t * Src = &Data[*(uint16_t *) &Data[0x18] - 4];

    if (*(Src + 2) != 0x40)
    {
        if (*(Src + 3) != 0xfe || *(Src + 2) < 0x41)
        {
            *text = '\0'; // Unable to get address of file=memo without sound

            return text;
        }
    }

    if (*(Src + 2) >= 0x42)
        index++;

    if (*(Src + 2) >= 0x48)
        index++;

    if (index < 0)
    {
        *text = '\0'; // No registration
        return text;
    }

    Src = &Data[*(uint16_t *) Src];

    size_t i;

    uint16_t dx = 0;

    for (i = 0; i <= (size_t) index; i++)
    {
        if (Size < (size_t)(Src - Data + 1))
        {
            *text = '\0';  // Incorrect song data

            return NULL;
        }

        dx = *(uint16_t *) Src;

        if (dx == 0)
        {
            *text = '\0';
            return text;
        }

        if (Size < (size_t) dx)
        {
            *text = '\0'; // Incorrect song data
            return NULL;
        }

        if (Data[dx] == '/')
        {
            *text = '\0';
            return text;
        }

        Src += 2;
    }

    for (i = dx; i < Size; i++)
    {
        if (Data[i] == '\0')
            break;
    }

    // Without the terminating \0
    if (i >= Size)
    {
        ::memcpy(text, &Data[dx], (size_t) Size - dx);
        text[Size - dx - 1] = '\0';
    }
    else
        ::strcpy(text, (char *) &Data[dx]);

    return text;
}

// Load PPC
int PMD::LoadPPC(const WCHAR * filePath)
{
    Stop();

    int Result = LoadPPCInternal(filePath);

    if (Result == ERR_SUCCESS || Result == ERR_ALREADY_LOADED)
        _Work._UseP86 = false;

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
        _Work._UseP86 = true;

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

// Get the Work pointer
Work * PMD::GetOpenWork()
{
    return &_Work;
}

// Get part work pointer
Channel * PMD::GetOpenPartWork(int ch)
{
    if (ch >= sizeof(_Work.Channel) / sizeof(Channel *))
        return NULL;

    return _Work.Channel[ch];
}

void PMD::HandleTimerA()
{
    _Work._IsTimerABusy = true;
    _Work._TimerATime++;

    if ((_Work._TimerATime & 7) == 0)
        Fade();

    if (_EffectState.effon && (!_PMDWork._UsePPS || _EffectState.psgefcnum == 0x80))
        effplay(); // SSG Sound Source effect processing

    _Work._IsTimerABusy = false;
}

void PMD::HandleTimerB()
{
    _Work._IsTimerBBusy = true;

    if (_PMDWork.music_flag)
    {
        if (_PMDWork.music_flag & 1)
            DriverStart();

        if (_PMDWork.music_flag & 2)
            DriverStop();
    }

    if (_Work._IsPlaying)
    {
        DriverMain();
        settempo_b();
        syousetu_count();

        _PMDWork._OldTimerATime = _Work._TimerATime;
    }

    _Work._IsTimerBBusy = false;
}

void PMD::DriverMain()
{
    int i;

    _PMDWork.loop_work = 3;

    if (_Work.x68_flg == 0)
    {
        for (i = 0; i < 3; i++)
        {
            _PMDWork.partb = i + 1;
            PSGMain(&_SSGChannel[i]);
        }
    }

    _PMDWork.fmsel = 0x100;

    for (i = 0; i < 3; i++)
    {
        _PMDWork.partb = i + 1;
        FMMain(&_FMChannel[i + 3]);
    }

    _PMDWork.fmsel = 0;

    for (i = 0; i < 3; i++)
    {
        _PMDWork.partb = i + 1;
        FMMain(&_FMChannel[i]);
    }

    for (i = 0; i < 3; i++)
    {
        _PMDWork.partb = 3;
        FMMain(&_ExtensionChannel[i]);
    }

    if (_Work.x68_flg == 0)
    {
        RhythmMain(&_RhythmChannel);

        if (_Work._UseP86)
            PCM86Main(&_ADPCMChannel);
        else
            ADPCMMain(&_ADPCMChannel);
    }

    if (_Work.x68_flg != 0xff)
    {
        for (i = 0; i < 8; i++)
        {
            _PMDWork.partb = i;
            PPZ8Main(&_PPZChannel[i]);
        }
    }

    if (_PMDWork.loop_work == 0)
        return;

    for (i = 0; i < 6; i++)
    {
        if (_FMChannel[i].loopcheck != 3)
            _FMChannel[i].loopcheck = 0;
    }

    for (i = 0; i < 3; i++)
    {
        if (_SSGChannel[i].loopcheck != 3)
            _SSGChannel[i].loopcheck = 0;

        if (_ExtensionChannel[i].loopcheck != 3)
            _ExtensionChannel[i].loopcheck = 0;
    }

    if (_ADPCMChannel.loopcheck != 3)
        _ADPCMChannel.loopcheck = 0;

    if (_RhythmChannel.loopcheck != 3)
        _RhythmChannel.loopcheck = 0;

    if (_EffectChannel.loopcheck != 3)
        _EffectChannel.loopcheck = 0;

    for (i = 0; i < PCM_CNL_MAX; i++)
    {
        if (_PPZChannel[i].loopcheck != 3)
            _PPZChannel[i].loopcheck = 0;
    }

    if (_PMDWork.loop_work != 3)
    {
        _Work._LoopCount++;

        if (_Work._LoopCount == 255)
            _Work._LoopCount = 1;
    }
    else
        _Work._LoopCount = -1;
}

void PMD::FMMain(Channel * qq)
{
    if (qq->address == NULL)
        return;

    uint8_t * si = qq->address;

    qq->leng--;

    if (qq->partmask)
    {
        qq->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK & Keyoff
        if ((qq->keyoff_flag & 3) == 0)
        {   // Already keyoff?
            if (qq->leng <= qq->qdat)
            {
                keyoff(qq);
                qq->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (qq->leng == 0)
    {
        if (qq->partmask == 0)
            qq->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = FMCommands(qq, si);
            }
            else
            if (*si == 0x80)
            {
                qq->address = si;
                qq->loopcheck = 3;
                qq->onkai = 255;

                if (qq->partloop == NULL)
                {
                    if (qq->partmask)
                    {
                        _PMDWork.tieflag = 0;
                        _PMDWork.volpush_flag = 0;
                        _PMDWork.loop_work &= qq->loopcheck;

                        return;
                    }
                    else
                        break;
                }

                // "L"があった時
                si = qq->partloop;
                qq->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {   // ポルタメント
                    si = porta(qq, ++si);
                    _PMDWork.loop_work &= qq->loopcheck;
                    return;
                }
                else
                if (qq->partmask == 0)
                {
                    // TONE SET
                    fnumset(qq, oshift(qq, lfoinit(qq, *si++)));

                    qq->leng = *si++;
                    si = calc_q(qq, si);

                    if (qq->volpush && qq->onkai != 255)
                    {
                        if (--_PMDWork.volpush_flag)
                        {
                            _PMDWork.volpush_flag = 0;
                            qq->volpush = 0;
                        }
                    }

                    volset(qq);
                    otodasi(qq);
                    keyon(qq);

                    qq->keyon_flag++;
                    qq->address = si;

                    _PMDWork.tieflag = 0;
                    _PMDWork.volpush_flag = 0;

                    if (*si == 0xfb)
                    {   // '&'が直後にあったらkeyoffしない
                        qq->keyoff_flag = 2;
                    }
                    else
                    {
                        qq->keyoff_flag = 0;
                    }
                    _PMDWork.loop_work &= qq->loopcheck;
                    return;
                }
                else
                {
                    si++;

                    qq->fnum = 0;       //休符に設定
                    qq->onkai = 255;
                    qq->onkai_def = 255;
                    qq->leng = *si++;
                    qq->keyon_flag++;
                    qq->address = si;

                    if (--_PMDWork.volpush_flag)
                    {
                        qq->volpush = 0;
                    }

                    _PMDWork.tieflag = 0;
                    _PMDWork.volpush_flag = 0;
                    break;
                }
            }
        }
    }

    if (qq->partmask == 0)
    {
        // Finish with LFO & Portament & Fadeout processing
        if (qq->hldelay_c)
        {
            if (--qq->hldelay_c == 0)
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + (_PMDWork.partb - 1 + 0xb4)), (uint32_t) qq->fmpan);
        }

        if (qq->sdelay_c)
        {
            if (--qq->sdelay_c == 0)
            {
                if ((qq->keyoff_flag & 1) == 0)
                {   // Already keyoffed?
                    keyon(qq);
                }
            }
        }

        if (qq->lfoswi)
        {
            _PMDWork.lfo_switch = qq->lfoswi & 8;

            if (qq->lfoswi & 3)
            {
                if (lfo(qq))
                {
                    _PMDWork.lfo_switch |= (qq->lfoswi & 3);
                }
            }

            if (qq->lfoswi & 0x30)
            {
                lfo_change(qq);

                if (lfo(qq))
                {
                    lfo_change(qq);

                    _PMDWork.lfo_switch |= (qq->lfoswi & 0x30);
                }
                else
                {
                    lfo_change(qq);
                }
            }

            if (_PMDWork.lfo_switch & 0x19)
            {
                if (_PMDWork.lfo_switch & 8)
                {
                    porta_calc(qq);

                }

                otodasi(qq);
            }

            if (_PMDWork.lfo_switch & 0x22)
            {
                volset(qq);
                _PMDWork.loop_work &= qq->loopcheck;

                return;
            }
        }

        if (_Work._FadeOutSpeed != 0)
            volset(qq);
    }

    _PMDWork.loop_work &= qq->loopcheck;
}

//  KEY OFF
void PMD::keyoff(Channel * qq)
{
    if (qq->onkai == 255)
        return;

    kof1(qq);
}

void PMD::kof1(Channel * qq)
{
    if (_PMDWork.fmsel == 0)
    {
        _PMDWork.omote_key[_PMDWork.partb - 1] = (~qq->slotmask) & (_PMDWork.omote_key[_PMDWork.partb - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) ((_PMDWork.partb - 1) | _PMDWork.omote_key[_PMDWork.partb - 1]));
    }
    else
    {
        _PMDWork.ura_key[_PMDWork.partb - 1] = (~qq->slotmask) & (_PMDWork.ura_key[_PMDWork.partb - 1]);
        _OPNAW->SetReg(0x28, (uint32_t) (((_PMDWork.partb - 1) | _PMDWork.ura_key[_PMDWork.partb - 1]) | 4));
    }
}

// FM Key On
void PMD::keyon(Channel * qq)
{
    int  al;

    if (qq->onkai == 255)
        return; // ｷｭｳﾌ ﾉ ﾄｷ

    if (_PMDWork.fmsel == 0)
    {
        al = _PMDWork.omote_key[_PMDWork.partb - 1] | qq->slotmask;

        if (qq->sdelay_c)
            al &= qq->sdelay_m;

        _PMDWork.omote_key[_PMDWork.partb - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) ((_PMDWork.partb - 1) | al));
    }
    else
    {
        al = _PMDWork.ura_key[_PMDWork.partb - 1] | qq->slotmask;

        if (qq->sdelay_c)
            al &= qq->sdelay_m;

        _PMDWork.ura_key[_PMDWork.partb - 1] = al;
        _OPNAW->SetReg(0x28, (uint32_t) (((_PMDWork.partb - 1) | al) | 4));
    }
}

//  Set [ FNUM/BLOCK + DETUNE + LFO ]
void PMD::otodasi(Channel * qq)
{
    if ((qq->fnum == 0) || (qq->slotmask == 0))
        return;

    int cx = (int) (qq->fnum & 0x3800);    // cx=BLOCK
    int ax = (int) (qq->fnum) & 0x7ff;    // ax=FNUM

    // Portament/LFO/Detune SET
    ax += qq->porta_num + qq->detune;

    if ((_PMDWork.partb == 3) && (_PMDWork.fmsel == 0) && (_Work.ch3mode != 0x3f))
        ch3_special(qq, ax, cx);
    else
    {
        if (qq->lfoswi & 1)
            ax += qq->lfodat;

        if (qq->lfoswi & 0x10)
            ax += qq->_lfodat;

        fm_block_calc(&cx, &ax);

        // SET BLOCK/FNUM TO OPN

        ax |= cx;

        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + _PMDWork.partb + 0xa4 - 1), (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + _PMDWork.partb + 0xa4 - 5), (uint32_t) LOBYTE(ax));
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
int PMD::ch3_setting(Channel * qq)
{
    if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
    {
        ch3mode_set(qq);

        return 1;
    }

    return 0;
}

void PMD::cm_clear(int * ah, int * al)
{
    *al ^= 0xff;

    if ((_PMDWork.slot3_flag &= *al) == 0)
    {
        if (_PMDWork.slotdetune_flag != 1)
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
void PMD::ch3mode_set(Channel * qq)
{
    int al;

    if (qq == &_FMChannel[3 - 1])
    {
        al = 1;
    }
    else
    if (qq == &_ExtensionChannel[0])
    {
        al = 2;
    }
    else
    if (qq == &_ExtensionChannel[1])
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
        _PMDWork.slot3_flag |= al;
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
        _PMDWork.slot3_flag |= al;
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
        _PMDWork.slot3_flag |= al;
        ah = 0x7f;
    }
    else
    {
        cm_clear(&ah, &al);
    }

    if ((uint32_t) ah == _Work.ch3mode)
        return;

    _Work.ch3mode = (uint32_t) ah;

    _OPNAW->SetReg(0x27, (uint32_t) (ah & 0xcf)); // Don't reset.

    // When moving to sound effect mode, the pitch is rewritten with the previous FM3 part
    if (ah == 0x3f || qq == &_FMChannel[2])
        return;

    if (_FMChannel[2].partmask == 0)
        otodasi(&_FMChannel[2]);

    if (qq == &_ExtensionChannel[0])
        return;

    if (_ExtensionChannel[0].partmask == 0)
        otodasi(&_ExtensionChannel[0]);

    if (qq == &_ExtensionChannel[1])
        return;

    if (_ExtensionChannel[1].partmask == 0)
        otodasi(&_ExtensionChannel[1]);
}

//  ch3=効果音モード を使用する場合の音程設定
//      input CX:block AX:fnum
void PMD::ch3_special(Channel * qq, int ax, int cx)
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
        ax += _Work.slot_detune4;
        if ((bh & shiftmask) && (qq->lfoswi & 0x01))  ax += qq->lfodat;
        if ((ch & shiftmask) && (qq->lfoswi & 0x10))  ax += qq->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xa6, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa2, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  3
    if (qq->slotmask & 0x40)
    {
        ax_ = ax;
        ax += _Work.slot_detune3;
        if ((bh & shiftmask) && (qq->lfoswi & 0x01))  ax += qq->lfodat;
        if ((ch & shiftmask) && (qq->lfoswi & 0x10))  ax += qq->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xac, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa8, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  2
    if (qq->slotmask & 0x20)
    {
        ax_ = ax;
        ax += _Work.slot_detune2;

        if ((bh & shiftmask) && (qq->lfoswi & 0x01))
            ax += qq->lfodat;

        if ((ch & shiftmask) && (qq->lfoswi & 0x10))
            ax += qq->_lfodat;

        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xae, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xaa, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }

    //  slot  1
    if (qq->slotmask & 0x10)
    {
        ax_ = ax;
        ax += _Work.slot_detune1;

        if ((bh & shiftmask) && (qq->lfoswi & 0x01)) 
            ax += qq->lfodat;

        if ((ch & shiftmask) && (qq->lfoswi & 0x10))
            ax += qq->_lfodat;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNAW->SetReg(0xad, (uint32_t) HIBYTE(ax));
        _OPNAW->SetReg(0xa9, (uint32_t) LOBYTE(ax));

        ax = ax_;
    }
}

//  'p' COMMAND [FM PANNING SET]
uint8_t * PMD::panset(Channel * qq, uint8_t * si)
{
    panset_main(qq, *si++);
    return si;
}

void PMD::panset_main(Channel * qq, int al)
{
    qq->fmpan = (qq->fmpan & 0x3f) | ((al << 6) & 0xc0);

    if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
    {
        //  FM3の場合は 4つのパート総て設定
        _FMChannel[2].fmpan = qq->fmpan;
        _ExtensionChannel[0].fmpan = qq->fmpan;
        _ExtensionChannel[1].fmpan = qq->fmpan;
        _ExtensionChannel[2].fmpan = qq->fmpan;
    }

    if (qq->partmask == 0)
    {    // パートマスクされているか？
// dl = al;
        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + _PMDWork.partb + 0xb4 - 1), calc_panout(qq));
    }
}

//  0b4h?に設定するデータを取得 out.dl
uint8_t PMD::calc_panout(Channel * qq)
{
    int  dl;

    dl = qq->fmpan;

    if (qq->hldelay_c)
        dl &= 0xc0;  // HLFO Delayが残ってる場合はパンのみ設定

    return (uint8_t) dl;
}

//  Pan setting Extend
uint8_t * PMD::panset_ex(Channel * qq, uint8_t * si)
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
uint8_t * PMD::panset8_ex(Channel * qq, uint8_t * si)
{
    int    flag, data;

    qq->fmpan = (int8_t) *si++;
    _Work.revpan = *si++;


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

    if (_Work.revpan != 1)
    {
        flag |= 4;        // 逆相
    }
    _P86->SetPan(flag, data);

    return si;
}

//  ＦＭ音源用　Entry
int PMD::lfoinit(Channel * qq, int al)
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

    if ((_PMDWork.tieflag & 1) == 0)
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
void PMD::fnumset(Channel * qq, int al)
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
void PMD::volset(Channel * qq)
{
    if (qq->slotmask == 0)
        return;

    int cl = (qq->volpush) ? qq->volpush - 1 : qq->volume;

    if (qq != &_EffectChannel)
    {  // 効果音の場合はvoldown/fadeout影響無し
//--------------------------------------------------------------------
//  音量down計算
//--------------------------------------------------------------------
        if (_Work.fm_voldown)
            cl = ((256 - _Work.fm_voldown) * cl) >> 8;

        //--------------------------------------------------------------------
        //  Fadeout計算
        //--------------------------------------------------------------------
        if (_Work._FadeOutVolume >= 2)
            cl = ((256 - (_Work._FadeOutVolume >> 1)) * cl) >> 8;
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

    int dh = 0x4c - 1 + _PMDWork.partb;    // dh=FM Port Address

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

    _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) al);
}

//  音量LFO用サブ
void PMD::fmlfo_sub(Channel *, int al, int bl, uint8_t * vol_tbl)
{
    if (bl & 0x80) vol_tbl[0] = (uint8_t) Limit(vol_tbl[0] - al, 255, 0);
    if (bl & 0x40) vol_tbl[1] = (uint8_t) Limit(vol_tbl[1] - al, 255, 0);
    if (bl & 0x20) vol_tbl[2] = (uint8_t) Limit(vol_tbl[2] - al, 255, 0);
    if (bl & 0x10) vol_tbl[3] = (uint8_t) Limit(vol_tbl[3] - al, 255, 0);
}

// SSG Sound Source Main
void PMD::PSGMain(Channel * qq)
{
    uint8_t * si;
    int    temp;

    if (qq->address == NULL) return;
    si = qq->address;

    qq->leng--;

    // KEYOFF CHECK & Keyoff
    if (qq == &_SSGChannel[2] && _PMDWork._UsePPS && _Work.kshot_dat && qq->leng <= qq->qdat)
    {
        // PPS 使用時 & SSG 3ch & SSG 効果音鳴らしている場合
        keyoffp(qq);
        _OPNAW->SetReg((uint32_t) (_PMDWork.partb + 8 - 1), 0U);    // 強制的に音を止める
        qq->keyoff_flag = -1;
    }

    if (qq->partmask)
    {
        qq->keyoff_flag = -1;
    }
    else
    {
        if ((qq->keyoff_flag & 3) == 0)
        {    // 既にkeyoffしたか？
            if (qq->leng <= qq->qdat)
            {
                keyoffp(qq);
                qq->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (qq->leng == 0)
    {
        qq->lfoswi &= 0xf7;

        // DATA READ
        while (1)
        {
            if (*si == 0xda && ssgdrum_check(qq, *si) < 0)
            {
                si++;
            }
            else if (*si > 0x80 && *si != 0xda)
            {
                si = PSGCommands(qq, si);
            }
            else if (*si == 0x80)
            {
                qq->address = si;
                qq->loopcheck = 3;
                qq->onkai = 255;
                if (qq->partloop == NULL)
                {
                    if (qq->partmask)
                    {
                        _PMDWork.tieflag = 0;
                        _PMDWork.volpush_flag = 0;
                        _PMDWork.loop_work &= qq->loopcheck;
                        return;
                    }
                    else
                    {
                        break;
                    }
                }
                // "L"があった時
                si = qq->partloop;
                qq->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {            // ポルタメント
                    si = portap(qq, ++si);
                    _PMDWork.loop_work &= qq->loopcheck;
                    return;
                }
                else if (qq->partmask)
                {
                    if (ssgdrum_check(qq, *si) == 0)
                    {
                        si++;
                        qq->fnum = 0;    //休符に設定
                        qq->onkai = 255;
                        qq->leng = *si++;
                        qq->keyon_flag++;
                        qq->address = si;

                        if (--_PMDWork.volpush_flag)
                        {
                            qq->volpush = 0;
                        }

                        _PMDWork.tieflag = 0;
                        _PMDWork.volpush_flag = 0;
                        break;
                    }
                }

                //  TONE SET
                fnumsetp(qq, oshiftp(qq, lfoinitp(qq, *si++)));

                qq->leng = *si++;
                si = calc_q(qq, si);

                if (qq->volpush && qq->onkai != 255)
                {
                    if (--_PMDWork.volpush_flag)
                    {
                        _PMDWork.volpush_flag = 0;
                        qq->volpush = 0;
                    }
                }

                volsetp(qq);
                otodasip(qq);
                keyonp(qq);

                qq->keyon_flag++;
                qq->address = si;

                _PMDWork.tieflag = 0;
                _PMDWork.volpush_flag = 0;
                qq->keyoff_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらkeyoffしない
                    qq->keyoff_flag = 2;
                }
                _PMDWork.loop_work &= qq->loopcheck;
                return;
            }
        }
    }

    _PMDWork.lfo_switch = (qq->lfoswi & 8);

    if (qq->lfoswi)
    {
        if (qq->lfoswi & 3)
        {
            if (lfop(qq))
            {
                _PMDWork.lfo_switch |= (qq->lfoswi & 3);
            }
        }

        if (qq->lfoswi & 0x30)
        {
            lfo_change(qq);
            if (lfop(qq))
            {
                lfo_change(qq);
                _PMDWork.lfo_switch |= (qq->lfoswi & 0x30);
            }
            else
            {
                lfo_change(qq);
            }
        }

        if (_PMDWork.lfo_switch & 0x19)
        {
            if (_PMDWork.lfo_switch & 0x08)
                porta_calc(qq);

            // SSG 3ch で休符かつ SSG Drum 発音中は操作しない
            if (!(qq == &_SSGChannel[2] && qq->onkai == 255 && _Work.kshot_dat && !_PMDWork._UsePPS))
                otodasip(qq);
        }
    }

    temp = soft_env(qq);

    if (temp || _PMDWork.lfo_switch & 0x22 || (_Work._FadeOutSpeed != 0))
    {
        // SSG 3ch で休符かつ SSG Drum 発音中は volume set しない
        if (!(qq == &_SSGChannel[2] && qq->onkai == 255 && _Work.kshot_dat && !_PMDWork._UsePPS))
        {
            volsetp(qq);
        }
    }

    _PMDWork.loop_work &= qq->loopcheck;
}

void PMD::keyoffp(Channel * qq)
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

// Rhythm part performance main
void PMD::RhythmMain(Channel * qq)
{
    uint8_t * si, * bx;
    int    al, result = 0;

    if (qq->address == NULL) return;

    si = qq->address;

    if (--qq->leng == 0)
    {
        bx = _Work.rhyadr;

    rhyms00:
        do
        {
            result = 1;
            al = *bx++;
            if (al != 0xff)
            {
                if (al & 0x80)
                {
                    bx = rhythmon(qq, bx, al, &result);
                    if (result == 0) continue;
                }
                else
                {
                    _Work.kshot_dat = 0;  //rest
                }

                al = *bx++;
                _Work.rhyadr = bx;
                qq->leng = al;
                qq->keyon_flag++;
                _PMDWork.tieflag = 0;
                _PMDWork.volpush_flag = 0;
                _PMDWork.loop_work &= qq->loopcheck;
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
                    qq->address = si;

                    //          bx = (uint16_t *)&open_work.radtbl[al];
                    //          bx = open_work.rhyadr = &open_work.mmlbuf[*bx];
                    bx = _Work.rhyadr = &_Work.mmlbuf[_Work.radtbl[al]];
                    goto rhyms00;
                }

                // al > 0x80
                si = RhythmCommands(qq, si - 1);
            }

            qq->address = --si;
            qq->loopcheck = 3;
            bx = qq->partloop;
            if (bx == NULL)
            {
                _Work.rhyadr = (uint8_t *) &_PMDWork.rhydmy;
                _PMDWork.tieflag = 0;
                _PMDWork.volpush_flag = 0;
                _PMDWork.loop_work &= qq->loopcheck;
                return;
            }
            else
            {    // "L"があった時
                si = bx;
                qq->loopcheck = 1;
            }
        }
    }

    _PMDWork.loop_work &= qq->loopcheck;
}

// PSG Rhythm ON
uint8_t * PMD::rhythmon(Channel * qq, uint8_t * bx, int al, int * result)
{
    if (al & 0x40)
    {
        bx = RhythmCommands(qq, bx - 1);
        *result = 0;
        return bx;
    }

    *result = 1;

    if (qq->partmask)
    {    // maskされている場合
        _Work.kshot_dat = 0;
        return ++bx;
    }

    al = ((al << 8) + *bx++) & 0x3fff;

    _Work.kshot_dat = al;

    if (al == 0)
        return bx;

    _Work.rhyadr = bx;

    if (_Work._UseRhythmSoundSource)
    {
        for (int cl = 0; cl < 11; cl++)
        {
            if (al & (1 << cl))
            {
                _OPNAW->SetReg((uint32_t) rhydat[cl][0], (uint32_t) rhydat[cl][1]);

                int dl = rhydat[cl][2] & _Work.rhythmmask;

                if (dl != 0)
                {
                    if (dl < 0x80)
                        _OPNAW->SetReg(0x10, (uint32_t) dl);
                    else
                    {
                        _OPNAW->SetReg(0x10, 0x84);

                        dl = _Work.rhythmmask & 8;

                        if (dl)
                            _OPNAW->SetReg(0x10, (uint32_t) dl);
                    }
                }
            }
        }
    }

    if (_Work._FadeOutVolume)
    {
        if (_Work._UseRhythmSoundSource)
        {
            int dl = _Work.rhyvol;

            if (_Work._FadeOutVolume)
                dl = ((256 - _Work._FadeOutVolume) * dl) >> 8;

            _OPNAW->SetReg(0x11, (uint32_t) dl);
        }

        if (_PMDWork._UsePPS == false)
        {  // fadeout時ppsdrvでなら発音しない
            bx = _Work.rhyadr;
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

        effgo(qq, al);

        bx_ >>= 1;
    }
    while (_PMDWork._UsePPS && bx_);  // PPSDRVなら２音目以上も鳴らしてみる

    return _Work.rhyadr;
}

//  ＰＳＧ　ドラムス＆効果音　ルーチン
//  Ｆｒｏｍ　ＷＴ２９８
//
//  AL に 効果音Ｎｏ．を入れて　ＣＡＬＬする
//  ppsdrvがあるならそっちを鳴らす
void PMD::effgo(Channel * qq, int al)
{
    if (_PMDWork._UsePPS)
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

void PMD::eff_on2(Channel * qq, int al)
{
    _EffectState.hosei_flag = 1;        //  音程のみ補正あり (n command)
    eff_main(qq, al);
}

void PMD::eff_main(Channel * qq, int al)
{
    int    ah, bh, bl;

    if (_Work.effflag) return;    //  効果音を使用しないモード

    if (_PMDWork._UsePPS && (al & 0x80))
    {  // PPS を鳴らす
        if (_EffectState.effon >= 2) return;  // 通常効果音発音時は発声させない

        _SSGChannel[2].partmask |= 2;    // Part Mask
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

            if (_Work._FadeOutVolume)
                bl = (bl * (256 - _Work._FadeOutVolume)) >> 8;
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
            if (_PMDWork._UsePPS)
                _PPS->Stop();

            _SSGChannel[2].partmask |= 2;    // Part Mask
            efffor(efftbl[al].table);    // １発目を発音
            _EffectState.effon = efftbl[al].priority;
            //  優先順位を設定(発音開始)
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
        _OPNAW->SetReg(4, (uint32_t) (*si++));    // 周波数セット
        ch = *si;
        _OPNAW->SetReg(5, (uint32_t) (*si++));    // 周波数セット
        _EffectState.eswthz = (ch << 8) + cl;

        _Work.psnoi_last = _EffectState.eswnhz = *si;
        _OPNAW->SetReg(6, (uint32_t) *si++);    // ノイズ

        _OPNAW->SetReg(7, ((*si++ << 2) & 0x24) | (_OPNAW->GetReg(0x07) & 0xdb));

        _OPNAW->SetReg(10, (uint32_t) *si++);    // ボリューム
        _OPNAW->SetReg(11, (uint32_t) *si++);    // エンベロープ周波数
        _OPNAW->SetReg(12, (uint32_t) *si++);    // 
        _OPNAW->SetReg(13, (uint32_t) *si++);    // エンベロープPATTERN

        _EffectState.eswtst = *si++;    // スイープ増分 (TONE)
        _EffectState.eswnst = *si++;    // スイープ増分 (NOISE)

        _EffectState.eswnct = _EffectState.eswnst & 15;    // スイープカウント (NOISE)

        _EffectState.effadr = (int *) si;
    }
}

void PMD::effend()
{
    if (_PMDWork._UsePPS)
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
    _Work.psnoi_last = _EffectState.eswnhz;
}

//  PDRのswitch
uint8_t * PMD::pdrswitch(Channel *, uint8_t * si)
{
    if (!_PMDWork._UsePPS)
        return si + 1;

//  ppsdrv->SetParam((*si & 1) << 1, *si & 1);    @暫定
    si++;

    return si;
}

// PCM sound source performance main
void PMD::ADPCMMain(Channel * qq)
{
    if (qq->address == NULL)
        return;

    uint8_t * si = qq->address;

    qq->leng--;

    if (qq->partmask)
    {
        qq->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((qq->keyoff_flag & 3) == 0)
        {    // 既にkeyoffしたか？
            if (qq->leng <= qq->qdat)
            {
                keyoffm(qq);
                qq->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (qq->leng == 0)
    {
        qq->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = ADPCMCommands(qq, si);
            }
            else
            if (*si == 0x80)
            {
                qq->address = si;
                qq->loopcheck = 3;
                qq->onkai = 255;

                if (qq->partloop == NULL)
                {
                    if (qq->partmask)
                    {
                        _PMDWork.tieflag = 0;
                        _PMDWork.volpush_flag = 0;
                        _PMDWork.loop_work &= qq->loopcheck;

                        return;
                    }
                    else
                        break;
                }

                // "L"があった時
                si = qq->partloop;
                qq->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {        // ポルタメント
                    si = portam(qq, ++si);

                    _PMDWork.loop_work &= qq->loopcheck;

                    return;
                }
                else
                if (qq->partmask)
                {
                    si++;
                    qq->fnum = 0;    //休符に設定
                    qq->onkai = 255;
                    //          qq->onkai_def = 255;
                    qq->leng = *si++;
                    qq->keyon_flag++;
                    qq->address = si;

                    if (--_PMDWork.volpush_flag)
                        qq->volpush = 0;

                    _PMDWork.tieflag = 0;
                    _PMDWork.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumsetm(qq, oshift(qq, lfoinitp(qq, *si++)));

                qq->leng = *si++;
                si = calc_q(qq, si);

                if (qq->volpush && qq->onkai != 255)
                {
                    if (--_PMDWork.volpush_flag)
                    {
                        _PMDWork.volpush_flag = 0;
                        qq->volpush = 0;
                    }
                }

                volsetm(qq);
                otodasim(qq);

                if (qq->keyoff_flag & 1)
                    keyonm(qq);

                qq->keyon_flag++;
                qq->address = si;

                _PMDWork.tieflag = 0;
                _PMDWork.volpush_flag = 0;

                if (*si == 0xfb)
                {   // Do not keyoff if '&' immediately follows
                    qq->keyoff_flag = 2;
                }
                else
                {
                    qq->keyoff_flag = 0;
                }

                _PMDWork.loop_work &= qq->loopcheck;

                return;
            }
        }
    }

    _PMDWork.lfo_switch = (qq->lfoswi & 8);

    if (qq->lfoswi)
    {
        if (qq->lfoswi & 3)
        {
            if (lfo(qq))
                _PMDWork.lfo_switch |= (qq->lfoswi & 3);
        }

        if (qq->lfoswi & 0x30)
        {
            lfo_change(qq);

            if (lfop(qq))
            {
                lfo_change(qq);
                _PMDWork.lfo_switch |= (qq->lfoswi & 0x30);
            }
            else
                lfo_change(qq);
        }

        if (_PMDWork.lfo_switch & 0x19)
        {
            if (_PMDWork.lfo_switch & 0x08)
                porta_calc(qq);

            otodasim(qq);
        }
    }

    int temp = soft_env(qq);

    if ((temp != 0) || _PMDWork.lfo_switch & 0x22 || (_Work._FadeOutSpeed != 0))
        volsetm(qq);

    _PMDWork.loop_work &= qq->loopcheck;
}

// PCM sound source performance main (PMD86)
void PMD::PCM86Main(Channel * qq)
{
    uint8_t * si;
    int    temp;

    if (qq->address == NULL) return;
    si = qq->address;

    qq->leng--;
    if (qq->partmask)
    {
        qq->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((qq->keyoff_flag & 3) == 0)
        {    // 既にkeyoffしたか？
            if (qq->leng <= qq->qdat)
            {
                keyoff8(qq);
                qq->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (qq->leng == 0)
    {
        while (1)
        {
            //      if(*si > 0x80 && *si != 0xda) {
            if (*si > 0x80)
            {
                si = PCM86Commands(qq, si);
            }
            else if (*si == 0x80)
            {
                qq->address = si;
                qq->loopcheck = 3;
                qq->onkai = 255;
                if (qq->partloop == NULL)
                {
                    if (qq->partmask)
                    {
                        _PMDWork.tieflag = 0;
                        _PMDWork.volpush_flag = 0;
                        _PMDWork.loop_work &= qq->loopcheck;
                        return;
                    }
                    else
                    {
                        break;
                    }
                }
                // "L"があった時
                si = qq->partloop;
                qq->loopcheck = 1;
            }
            else
            {
                if (qq->partmask)
                {
                    si++;
                    qq->fnum = 0;    //休符に設定
                    qq->onkai = 255;
                    //          qq->onkai_def = 255;
                    qq->leng = *si++;
                    qq->keyon_flag++;
                    qq->address = si;

                    if (--_PMDWork.volpush_flag)
                    {
                        qq->volpush = 0;
                    }

                    _PMDWork.tieflag = 0;
                    _PMDWork.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumset8(qq, oshift(qq, lfoinitp(qq, *si++)));

                qq->leng = *si++;
                si = calc_q(qq, si);

                if (qq->volpush && qq->onkai != 255)
                {
                    if (--_PMDWork.volpush_flag)
                    {
                        _PMDWork.volpush_flag = 0;
                        qq->volpush = 0;
                    }
                }

                volset8(qq);
                otodasi8(qq);
                if (qq->keyoff_flag & 1)
                {
                    keyon8(qq);
                }

                qq->keyon_flag++;
                qq->address = si;

                _PMDWork.tieflag = 0;
                _PMDWork.volpush_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらkeyoffしない
                    qq->keyoff_flag = 2;
                }
                else
                {
                    qq->keyoff_flag = 0;
                }
                _PMDWork.loop_work &= qq->loopcheck;
                return;

            }
        }
    }

    if (qq->lfoswi & 0x22)
    {
        _PMDWork.lfo_switch = 0;
        if (qq->lfoswi & 2)
        {
            lfo(qq);
            _PMDWork.lfo_switch |= (qq->lfoswi & 2);
        }

        if (qq->lfoswi & 0x20)
        {
            lfo_change(qq);
            if (lfo(qq))
            {
                lfo_change(qq);
                _PMDWork.lfo_switch |= (qq->lfoswi & 0x20);
            }
            else
            {
                lfo_change(qq);
            }
        }

        temp = soft_env(qq);
        if (temp || _PMDWork.lfo_switch & 0x22 || _Work._FadeOutSpeed)
        {
            volset8(qq);
        }
    }
    else
    {
        temp = soft_env(qq);
        if (temp || _Work._FadeOutSpeed)
        {
            volset8(qq);
        }
    }

    _PMDWork.loop_work &= qq->loopcheck;
}

// PCM sound source performance main (PPZ8)
void PMD::PPZ8Main(Channel * qq)
{
    uint8_t * si;
    int    temp;

    if (qq->address == NULL) return;
    si = qq->address;

    qq->leng--;
    if (qq->partmask)
    {
        qq->keyoff_flag = -1;
    }
    else
    {
        // KEYOFF CHECK
        if ((qq->keyoff_flag & 3) == 0)
        {    // 既にkeyoffしたか？
            if (qq->leng <= qq->qdat)
            {
                keyoffz(qq);
                qq->keyoff_flag = -1;
            }
        }
    }

    // LENGTH CHECK
    if (qq->leng == 0)
    {
        qq->lfoswi &= 0xf7;

        while (1)
        {
            if (*si > 0x80 && *si != 0xda)
            {
                si = PPZ8Commands(qq, si);
            }
            else if (*si == 0x80)
            {
                qq->address = si;
                qq->loopcheck = 3;
                qq->onkai = 255;
                if (qq->partloop == NULL)
                {
                    if (qq->partmask)
                    {
                        _PMDWork.tieflag = 0;
                        _PMDWork.volpush_flag = 0;
                        _PMDWork.loop_work &= qq->loopcheck;
                        return;
                    }
                    else
                    {
                        break;
                    }
                }
                // "L"があった時
                si = qq->partloop;
                qq->loopcheck = 1;
            }
            else
            {
                if (*si == 0xda)
                {        // ポルタメント
                    si = portaz(qq, ++si);
                    _PMDWork.loop_work &= qq->loopcheck;
                    return;
                }
                else if (qq->partmask)
                {
                    si++;
                    qq->fnum = 0;    //休符に設定
                    qq->onkai = 255;
                    //          qq->onkai_def = 255;
                    qq->leng = *si++;
                    qq->keyon_flag++;
                    qq->address = si;

                    if (--_PMDWork.volpush_flag)
                    {
                        qq->volpush = 0;
                    }

                    _PMDWork.tieflag = 0;
                    _PMDWork.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumsetz(qq, oshift(qq, lfoinitp(qq, *si++)));

                qq->leng = *si++;
                si = calc_q(qq, si);

                if (qq->volpush && qq->onkai != 255)
                {
                    if (--_PMDWork.volpush_flag)
                    {
                        _PMDWork.volpush_flag = 0;
                        qq->volpush = 0;
                    }
                }

                volsetz(qq);
                otodasiz(qq);
                if (qq->keyoff_flag & 1)
                {
                    keyonz(qq);
                }

                qq->keyon_flag++;
                qq->address = si;

                _PMDWork.tieflag = 0;
                _PMDWork.volpush_flag = 0;

                if (*si == 0xfb)
                {    // '&'が直後にあったらkeyoffしない
                    qq->keyoff_flag = 2;
                }
                else
                {
                    qq->keyoff_flag = 0;
                }
                _PMDWork.loop_work &= qq->loopcheck;
                return;

            }
        }
    }

    _PMDWork.lfo_switch = (qq->lfoswi & 8);
    if (qq->lfoswi)
    {
        if (qq->lfoswi & 3)
        {
            if (lfo(qq))
            {
                _PMDWork.lfo_switch |= (qq->lfoswi & 3);
            }
        }

        if (qq->lfoswi & 0x30)
        {
            lfo_change(qq);
            if (lfop(qq))
            {
                lfo_change(qq);
                _PMDWork.lfo_switch |= (qq->lfoswi & 0x30);
            }
            else
            {
                lfo_change(qq);
            }
        }

        if (_PMDWork.lfo_switch & 0x19)
        {
            if (_PMDWork.lfo_switch & 0x08)
            {
                porta_calc(qq);
            }
            otodasiz(qq);
        }
    }

    temp = soft_env(qq);
    if (temp || _PMDWork.lfo_switch & 0x22 || _Work._FadeOutSpeed)
    {
        volsetz(qq);
    }

    _PMDWork.loop_work &= qq->loopcheck;
}

//  PCM KEYON
void PMD::keyonm(Channel * qq)
{
    if (qq->onkai == 255)
        return;

    _OPNAW->SetReg(0x101, 0x02);  // PAN=0 / x8 bit mode
    _OPNAW->SetReg(0x100, 0x21);  // PCM RESET

    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_Work.pcmstart));
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_Work.pcmstart));
    _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_Work.pcmstop));
    _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_Work.pcmstop));

    if ((_PMDWork.pcmrepeat1 | _PMDWork.pcmrepeat2) == 0)
    {
        _OPNAW->SetReg(0x100, 0xa0);  // PCM PLAY(non_repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (qq->fmpan | 2));  // PAN SET / x8 bit mode
    }
    else
    {
        _OPNAW->SetReg(0x100, 0xb0);  // PCM PLAY(repeat)
        _OPNAW->SetReg(0x101, (uint32_t) (qq->fmpan | 2));  // PAN SET / x8 bit mode

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_PMDWork.pcmrepeat1));
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_PMDWork.pcmrepeat1));
        _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_PMDWork.pcmrepeat2));
        _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_PMDWork.pcmrepeat2));
    }
}

//  PCM KEYON(PMD86)
void PMD::keyon8(Channel * qq)
{
    if (qq->onkai == 255) return;
    _P86->Play();
}

//  PPZ KEYON
void PMD::keyonz(Channel * qq)
{
    if (qq->onkai == 255) return;

    if ((qq->voicenum & 0x80) == 0)
    {
        _PPZ->Play(_PMDWork.partb, 0, qq->voicenum, 0, 0);
    }
    else
    {
        _PPZ->Play(_PMDWork.partb, 1, qq->voicenum & 0x7f, 0, 0);
    }
}

//  PCM OTODASI
void PMD::otodasim(Channel * qq)
{
    if (qq->fnum == 0)
        return;

    // Portament/LFO/Detune SET
    int bx = (int) (qq->fnum + qq->porta_num);
    int dx = (int) (((qq->lfoswi & 0x11) && (qq->lfoswi & 1)) ? dx = qq->lfodat : 0);

    if (qq->lfoswi & 0x10)
        dx += qq->_lfodat;

    dx *= 4;  // PCM ﾊ LFO ｶﾞ ｶｶﾘﾆｸｲ ﾉﾃﾞ depth ｦ 4ﾊﾞｲ ｽﾙ

    dx += qq->detune;

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

//  PCM OTODASI(PMD86)
void PMD::otodasi8(Channel * qq)
{
    if (qq->fnum == 0)
        return;

    int bl = (int) ((qq->fnum & 0x0e00000) >> (16 + 5));    // 設定周波数
    int cx = (int) (qq->fnum & 0x01fffff);          // fnum

    if (_Work.pcm86_vol == 0 && qq->detune)
        cx = Limit((cx >> 5) + qq->detune, 65535, 1) << 5;

    _P86->SetOntei(bl, (uint32_t) cx);
}

//  PPZ OTODASI
void PMD::otodasiz(Channel * qq)
{
    uint32_t  cx;
    int64_t  cx2;
    int    ax;

    if ((cx = qq->fnum) == 0) return;
    cx += qq->porta_num * 16;

    if (qq->lfoswi & 1)
    {
        ax = qq->lfodat;
    }
    else
    {
        ax = 0;
    }

    if (qq->lfoswi & 0x10)
    {
        ax += qq->_lfodat;
    }

    ax += qq->detune;

    cx2 = cx + ((int64_t) cx) / 256 * ax;
    if (cx2 > 0xffffffff) cx = 0xffffffff;
    else if (cx2 < 0) cx = 0;
    else cx = (uint32_t) cx2;

    // TONE SET
    _PPZ->SetPitchFrequency(_PMDWork.partb, cx);
}

//  PCM VOLUME SET
void PMD::volsetm(Channel * qq)
{
    int al = qq->volpush ? qq->volpush : qq->volume;

    //------------------------------------------------------------------------
    //  音量down計算
    //------------------------------------------------------------------------
    al = ((256 - _Work.pcm_voldown) * al) >> 8;

    //------------------------------------------------------------------------
    //  Fadeout計算
    //------------------------------------------------------------------------
    if (_Work._FadeOutVolume)
        al = (((256 - _Work._FadeOutVolume) * (256 - _Work._FadeOutVolume) >> 8) * al) >> 8;

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
void PMD::volset8(Channel * qq)
{
    int al = qq->volpush ? qq->volpush : qq->volume;

    //------------------------------------------------------------------------
    //  音量down計算
    //------------------------------------------------------------------------
    al = ((256 - _Work.pcm_voldown) * al) >> 8;

    //------------------------------------------------------------------------
    //  Fadeout計算
    //------------------------------------------------------------------------
    if (_Work._FadeOutVolume != 0)
        al = ((256 - _Work._FadeOutVolume) * al) >> 8;

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

    if (_Work.pcm86_vol)
        al = (int) ::sqrt(al); //  SPBと同様の音量設定
    else
        al >>= 4;

    _P86->SetVol(al);
}

//  PPZ VOLUME SET
void PMD::volsetz(Channel * qq)
{
    int al = qq->volpush ? qq->volpush : qq->volume;

    //------------------------------------------------------------------------
    //  音量down計算
    //------------------------------------------------------------------------
    al = ((256 - _Work.ppz_voldown) * al) >> 8;

    //------------------------------------------------------------------------
    //  Fadeout計算
    //------------------------------------------------------------------------
    if (_Work._FadeOutVolume != 0)
        al = ((256 - _Work._FadeOutVolume) * al) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        _PPZ->SetVolume(_PMDWork.partb, 0);
        _PPZ->Stop(_PMDWork.partb);
        return;
    }

    if (qq->envf == -1)
    {
        //  拡張版 音量=al*(eenv_vol+1)/16
        if (qq->eenv_volume == 0)
        {
            //*@    ppz8->SetVol(pmdwork.partb, 0);
            _PPZ->Stop(_PMDWork.partb);
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
                //*@      ppz8->SetVol(pmdwork.partb, 0);
                _PPZ->Stop(_PMDWork.partb);
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
        _PPZ->SetVolume(_PMDWork.partb, al >> 4);
    else
        _PPZ->Stop(_PMDWork.partb);
}

//  ADPCM FNUM SET
void PMD::fnumsetm(Channel * qq, int al)
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
void PMD::fnumset8(Channel * qq, int al)
{
    int    ah, bl;

    ah = al & 0x0f;
    if (ah != 0x0f)
    {      // 音符の場合
        if (_Work.pcm86_vol && al >= 0x65)
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
void PMD::fnumsetz(Channel * qq, int al)
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
uint8_t * PMD::portam(Channel * qq, uint8_t * si)
{
    int    ax, al_, bx_;

    if (qq->partmask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

        if (--_PMDWork.volpush_flag)
        {
            qq->volpush = 0;
        }

        _PMDWork.tieflag = 0;
        _PMDWork.volpush_flag = 0;
        _PMDWork.loop_work &= qq->loopcheck;
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
        if (--_PMDWork.volpush_flag)
        {
            _PMDWork.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volsetm(qq);
    otodasim(qq);
    if (qq->keyoff_flag & 1)
    {
        keyonm(qq);
    }

    qq->keyon_flag++;
    qq->address = si;

    _PMDWork.tieflag = 0;
    _PMDWork.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {      // '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }

    _PMDWork.loop_work &= qq->loopcheck;
    return si;
}

//  ポルタメント(PPZ)
uint8_t * PMD::portaz(Channel * qq, uint8_t * si)
{
    int    ax, al_, bx_;

    if (qq->partmask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

        if (--_PMDWork.volpush_flag)
        {
            qq->volpush = 0;
        }

        _PMDWork.tieflag = 0;
        _PMDWork.volpush_flag = 0;
        _PMDWork.loop_work &= qq->loopcheck;
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
        if (--_PMDWork.volpush_flag)
        {
            _PMDWork.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volsetz(qq);
    otodasiz(qq);
    if (qq->keyoff_flag & 1)
    {
        keyonz(qq);
    }

    qq->keyon_flag++;
    qq->address = si;

    _PMDWork.tieflag = 0;
    _PMDWork.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {      // '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }

    _PMDWork.loop_work &= qq->loopcheck;
    return si;
}

void PMD::keyoffm(Channel * qq)
{
    if (qq->envf != -1)
    {
        if (qq->envf == 2) return;
    }
    else
    {
        if (qq->eenv_count == 4) return;
    }

    if (_PMDWork.pcmrelease != 0x8000)
    {
        // PCM RESET
        _OPNAW->SetReg(0x100, 0x21);

        _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(_PMDWork.pcmrelease));
        _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(_PMDWork.pcmrelease));

        // Stop ADDRESS for Release
        _OPNAW->SetReg(0x104, (uint32_t) LOBYTE(_Work.pcmstop));
        _OPNAW->SetReg(0x105, (uint32_t) HIBYTE(_Work.pcmstop));

        // PCM PLAY(non_repeat)
        _OPNAW->SetReg(0x100, 0xa0);
    }

    keyoffp(qq);
    return;
}

void PMD::keyoff8(Channel * qq)
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
void PMD::keyoffz(Channel * qq)
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
uint8_t * PMD::pansetm_ex(Channel * qq, uint8_t * si)
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
uint8_t * PMD::pcmrepeat_set(Channel *, uint8_t * si)
{
    int    ax;

    ax = *(int16_t *) si;
    si += 2;

    if (ax >= 0)
    {
        ax += _Work.pcmstart;
    }
    else
    {
        ax += _Work.pcmstop;
    }

    _PMDWork.pcmrepeat1 = ax;

    ax = *(int16_t *) si;
    si += 2;
    if (ax > 0)
    {
        ax += _Work.pcmstart;
    }
    else
    {
        ax += _Work.pcmstop;
    }

    _PMDWork.pcmrepeat2 = ax;

    ax = *(uint16_t *) si;
    si += 2;
    if (ax < 0x8000)
    {
        ax += _Work.pcmstart;
    }
    else if (ax > 0x8000)
    {
        ax += _Work.pcmstop;
    }

    _PMDWork.pcmrelease = ax;
    return si;
}

//  リピート設定(PMD86)
uint8_t * PMD::pcmrepeat_set8(Channel *, uint8_t * si)
{
    int16_t loop_start, loop_end, release_start;

    loop_start = *(int16_t *) si;
    si += 2;
    loop_end = *(int16_t *) si;
    si += 2;
    release_start = *(int16_t *) si;

    if (_Work.pcm86_vol)
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
uint8_t * PMD::ppzrepeat_set(Channel * qq, uint8_t * data)
{
    int LoopStart, LoopEnd;

    if ((qq->voicenum & 0x80) == 0)
    {
        LoopStart = *(int16_t *) data;
        data += 2;

        if (LoopStart < 0)
            LoopStart = (int) (_PPZ->PCME_WORK[0].pcmnum[qq->voicenum].Size - LoopStart);

        LoopEnd = *(int16_t *) data;
        data += 2;

        if (LoopEnd < 0)
            LoopEnd = (int) (_PPZ->PCME_WORK[0].pcmnum[qq->voicenum].Size - LoopStart);
    }
    else
    {
        LoopStart = *(int16_t *) data;
        data += 2;

        if (LoopStart < 0)
            LoopStart = (int) (_PPZ->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].Size - LoopStart);

        LoopEnd = *(int16_t *) data;
        data += 2;

        if (LoopEnd < 0)
            LoopEnd = (int) (_PPZ->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].Size - LoopEnd);
    }

    _PPZ->SetLoop(_PMDWork.partb, (uint32_t) LoopStart, (uint32_t) LoopEnd);

    return data + 2;
}

uint8_t * PMD::vol_one_up_pcm(Channel * qq, uint8_t * si)
{
    int    al;

    al = (int) *si++ + qq->volume;
    if (al > 254) al = 254;
    al++;
    qq->volpush = al;
    _PMDWork.volpush_flag = 1;
    return si;
}

//  COMMAND 'p' [Panning Set]
uint8_t * PMD::pansetm(Channel * qq, uint8_t * si)
{
    qq->fmpan = (*si << 6) & 0xc0;
    return si + 1;
}

//  COMMAND 'p' [Panning Set]
//  p0    逆相
//  p1    右
//  p2    左
//  p3    中
uint8_t * PMD::panset8(Channel *, uint8_t * si)
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
uint8_t * PMD::pansetz(Channel * qq, uint8_t * si)
{
    qq->fmpan = ppzpandata[*si++];
    _PPZ->SetPan(_PMDWork.partb, qq->fmpan);
    return si;
}

//  Pan setting Extend
//    px -4?+4
uint8_t * PMD::pansetz_ex(Channel * qq, uint8_t * si)
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
    _PPZ->SetPan(_PMDWork.partb, qq->fmpan);
    return si;
}

uint8_t * PMD::comatm(Channel * qq, uint8_t * si)
{
    qq->voicenum = *si++;
    _Work.pcmstart = pcmends.pcmadrs[qq->voicenum][0];
    _Work.pcmstop = pcmends.pcmadrs[qq->voicenum][1];
    _PMDWork.pcmrepeat1 = 0;
    _PMDWork.pcmrepeat2 = 0;
    _PMDWork.pcmrelease = 0x8000;
    return si;
}

uint8_t * PMD::comat8(Channel * qq, uint8_t * si)
{
    qq->voicenum = *si++;
    _P86->SetNeiro(qq->voicenum);
    return si;
}

uint8_t * PMD::comatz(Channel * qq, uint8_t * si)
{
    qq->voicenum = *si++;

    if ((qq->voicenum & 0x80) == 0)
    {
        _PPZ->SetLoop(_PMDWork.partb, _PPZ->PCME_WORK[0].pcmnum[qq->voicenum].LoopStart, _PPZ->PCME_WORK[0].pcmnum[qq->voicenum].LoopEnd);
        _PPZ->SetSourceRate(_PMDWork.partb, _PPZ->PCME_WORK[0].pcmnum[qq->voicenum].SampleRate);
    }
    else
    {
        _PPZ->SetLoop(_PMDWork.partb, _PPZ->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].LoopStart, _PPZ->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].LoopEnd);
        _PPZ->SetSourceRate(_PMDWork.partb, _PPZ->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].SampleRate);
    }
    return si;
}

//  SSGドラムを消してSSGを復活させるかどうかcheck
//    input  AL <- Command
//    output  cy=1 : 復活させる
int PMD::ssgdrum_check(Channel * qq, int al)
{
    // SSGマスク中はドラムを止めない
    // SSGドラムは鳴ってない
    if ((qq->partmask & 1) || ((qq->partmask & 2) == 0)) return 0;

    // 普通の効果音は消さない
    if (_EffectState.effon >= 2) return 0;

    al = (al & 0x0f);

    // 休符の時はドラムは止めない
    if (al == 0x0f) return 0;

    // SSGドラムはまだ再生中か？
    if (_EffectState.effon == 1)
    {
        effend();      // SSGドラムを消す
    }

    if ((qq->partmask &= 0xfd) == 0) return -1;
    return 0;
}

uint8_t * PMD::FMCommands(Channel * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comat(ps, si); break;
        case 0xfe: ps->qdata = *si++; ps->qdat3 = 0; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _PMDWork.tieflag |= 1; break;
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

        case 0xef: _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + *si), (uint32_t) (*(si + 1))); si += 2; break;
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
        case 0xe0: _Work.port22h = *si; _OPNAW->SetReg(0x22, *si++); break;
            //
        case 0xdf: _Work.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_fm(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _Work.status = *si++; break;
        case 0xdb: _Work.status += *si++; break;
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
            _Work.fadeout_flag = 1;
            _Work._FadeOutSpeed = *si++;
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

uint8_t * PMD::PSGCommands(Channel * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si++; break;
        case 0xfe: ps->qdata = *si++; ps->qdat3 = 0; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _PMDWork.tieflag |= 1; break;
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

        case 0xef: _OPNAW->SetReg(*si, *(si + 1)); si += 2; break;
        case 0xee: _Work.psnoi = *si++; break;
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
        case 0xdf: _Work.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_psg(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _Work.status = *si++; break;
        case 0xdb: _Work.status += *si++; break;
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
            _Work.fadeout_flag = 1;
            _Work._FadeOutSpeed = *si++;
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

uint8_t * PMD::RhythmCommands(Channel * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si++; break;
        case 0xfe: si++; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _PMDWork.tieflag |= 1; break;
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

        case 0xef: _OPNAW->SetReg(*si, *(si + 1)); si += 2; break;
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
        case 0xdf: _Work.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_psg(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _Work.status = *si++; break;
        case 0xdb: _Work.status += *si++; break;
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
            _Work.fadeout_flag = 1;
            _Work._FadeOutSpeed = *si++;
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

uint8_t * PMD::ADPCMCommands(Channel * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comatm(ps, si); break;
        case 0xfe: ps->qdata = *si++; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;
        case 0xfb: _PMDWork.tieflag |= 1; break;
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

        case 0xef: _OPNAW->SetReg((uint32_t) (0x100 + *si), (uint32_t) (*(si + 1))); si += 2; break;
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
        case 0xdf: _Work.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_pcm(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _Work.status = *si++; break;
        case 0xdb: _Work.status += *si++; break;
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
            _Work.fadeout_flag = 1;
            _Work._FadeOutSpeed = *si++;
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

uint8_t * PMD::PCM86Commands(Channel * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comat8(ps, si); break;
        case 0xfe: ps->qdata = *si++; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;
        case 0xfb: _PMDWork.tieflag |= 1; break;
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

        case 0xef: _OPNAW->SetReg((uint32_t) (0x100 + *si), (uint32_t) (*(si + 1))); si += 2; break;
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
        case 0xdf: _Work.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_pcm(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _Work.status = *si++; break;
        case 0xdb: _Work.status += *si++; break;
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
            _Work.fadeout_flag = 1;
            _Work._FadeOutSpeed = *si++;
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

uint8_t * PMD::PPZ8Commands(Channel * ps, uint8_t * si)
{
    int al = *si++;

    switch (al)
    {
        case 0xff: si = comatz(ps, si); break;
        case 0xfe: ps->qdata = *si++; break;
        case 0xfd: ps->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: _PMDWork.tieflag |= 1; break;
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

        case 0xef: _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + *si), (uint32_t) *(si + 1)); si += 2; break;
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
        case 0xdf: _Work.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_pcm(ps, si); break;
        case 0xdd: si = vol_one_down(ps, si); break;
            //
        case 0xdc: _Work.status = *si++; break;
        case 0xdb: _Work.status += *si++; break;
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
            _Work.fadeout_flag = 1;
            _Work._FadeOutSpeed = *si++;
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
uint8_t * PMD::comat(Channel * qq, uint8_t * si)
{
    uint8_t * bx;
    int    al, dl;

    qq->voicenum = al = *si++;
    dl = qq->voicenum;

    if (qq->partmask == 0)
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
        if (_PMDWork.partb == 3 && qq->neiromask)
        {
            if (_PMDWork.fmsel == 0)
            {
                // in. dl = alg/fb
                if ((qq->slotmask & 0x10) == 0)
                {
                    al = _PMDWork.fm3_alg_fb & 0x38;    // fbは前の値を使用
                    dl = (dl & 7) | al;
                }

                _PMDWork.fm3_alg_fb = dl;
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
void PMD::neiroset(Channel * qq, int dl)
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

    dh = 0xb0 - 1 + _PMDWork.partb;

    if (_PMDWork.af_check)
    {    // ALG/FBは設定しないmodeか？
        dl = qq->alg_fb;
    }
    else
    {
        dl = bx[24];
    }

    if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
    {
        if (_PMDWork.af_check != 0)
        {  // ALG/FBは設定しないmodeか？
            dl = _PMDWork.fm3_alg_fb;
        }
        else
        {
            if ((qq->slotmask & 0x10) == 0)
            {  // slot1を使用しているか？
                dl = (_PMDWork.fm3_alg_fb & 0x38) | (dl & 7);
            }
            _PMDWork.fm3_alg_fb = dl;
        }
    }

    _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
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

    dh = 0x30 - 1 + _PMDWork.partb;
    dl = *bx++;        // DT/ML
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // TL
    if (ah & 0x80) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x40) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh),(uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x20) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x10) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // KS/AR
    if (al & 0x08) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // AM/DR
    if (al & 0x80) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // SR
    if (al & 0x08) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;        // SL/RR
    if (al & 0x80)
    {
        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    }
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
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
int PMD::MuteFMPart(Channel * qq)
{
    if (qq->neiromask == 0)
        return 1;

    int dh = _PMDWork.partb + 0x40 - 1;

    if (qq->neiromask & 0x80)
    {
        _OPNAW->SetReg((uint32_t) ( _PMDWork.fmsel         + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_PMDWork.fmsel + 0x40) + dh), 127);
    }

    dh += 4;

    if (qq->neiromask & 0x40)
    {
        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_PMDWork.fmsel + 0x40) + dh), 127);
    }

    dh += 4;

    if (qq->neiromask & 0x20)
    {
        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_PMDWork.fmsel + 0x40) + dh), 127);
    }

    dh += 4;

    if (qq->neiromask & 0x10)
    {
        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), 127);
        _OPNAW->SetReg((uint32_t) ((_PMDWork.fmsel + 0x40) + dh), 127);
    }

    kof1(qq);

    return 0;
}

//  TONE DATA START ADDRESS を計算
//    input  dl  tone_number
//    output  bx  address
uint8_t * PMD::toneadr_calc(Channel * qq, int dl)
{
    uint8_t * bx;

    if (_Work.prg_flg == 0 && qq != &_EffectChannel)
        return _Work.tondat + ((size_t) dl << 5);

    bx = _Work.prgdat_adr;

    while (*bx != dl)
    {
        bx += 26;

        if (bx > _MData + sizeof(_MData) - 26)
            return _Work.prgdat_adr + 1; // Set first timbre if not found.
    }

    return bx + 1;
}

// FM tone generator hard LFO setting (V2.4 expansion)
uint8_t * PMD::hlfo_set(Channel * qq, uint8_t * si)
{
    qq->fmpan = (qq->fmpan & 0xc0) | *si++;

    if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
    {
        // Part_e is impossible because it is only for 2608
        // For FM3, set all four parts
        _FMChannel[2].fmpan = qq->fmpan;
        _ExtensionChannel[0].fmpan = qq->fmpan;
        _ExtensionChannel[1].fmpan = qq->fmpan;
        _ExtensionChannel[2].fmpan = qq->fmpan;
    }

    if (qq->partmask == 0)
    {    // パートマスクされているか？
        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + _PMDWork.partb + 0xb4 - 1), calc_panout(qq));
    }
    return si;
}

// Change only one volume (V2.7 expansion)
uint8_t * PMD::vol_one_up_fm(Channel * qq, uint8_t * si)
{
    int    al;

    al = (int) qq->volume + 1 + *si++;
    if (al > 128) al = 128;

    qq->volpush = al;
    _PMDWork.volpush_flag = 1;
    return si;
}

// Portamento (FM)
uint8_t * PMD::porta(Channel * qq, uint8_t * si)
{
    int ax;

    if (qq->partmask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

        if (--_PMDWork.volpush_flag)
        {
            qq->volpush = 0;
        }

        _PMDWork.tieflag = 0;
        _PMDWork.volpush_flag = 0;
        _PMDWork.loop_work &= qq->loopcheck;

        return si + 3;    // 読み飛ばす  (Mask時)
    }

    fnumset(qq, oshift(qq, lfoinit(qq, *si++)));

    int cx = (int) qq->fnum;
    int cl = qq->onkai;

    fnumset(qq, oshift(qq, *si++));

    int bx = (int) qq->fnum;      // bx=ポルタメント先のfnum値

    qq->onkai = cl;
    qq->fnum = (uint32_t) cx;      // cx=ポルタメント元のfnum値

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

    qq->leng = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->leng;  // 商
    qq->porta_num3 = ax % qq->leng;  // 余り
    qq->lfoswi |= 8;        // Porta ON

    if (qq->volpush && qq->onkai != 255)
    {
        if (--_PMDWork.volpush_flag)
        {
            _PMDWork.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volset(qq);
    otodasi(qq);
    keyon(qq);

    qq->keyon_flag++;
    qq->address = si;

    _PMDWork.tieflag = 0;
    _PMDWork.volpush_flag = 0;

    if (*si == 0xfb)
    {    // '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }
    else
    {
        qq->keyoff_flag = 0;
    }
    _PMDWork.loop_work &= qq->loopcheck;
    return si;
}

//  FM slotmask set
uint8_t * PMD::slotmask_set(Channel * qq, uint8_t * si)
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
        if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
        {
            bl = _PMDWork.fm3_alg_fb;
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
            qq->partmask |= 0x20;  // Part mask at s0
        else
            qq->partmask &= 0xdf;  // Unmask part when other than s0

        if (ch3_setting(qq))
        {   // Change process of ch3mode only for FM3ch. If it is ch3, keyon processing in the previous FM3 part
            if (qq != &_FMChannel[2])
            {
                if (_FMChannel[2].partmask == 0 && (_FMChannel[2].keyoff_flag & 1) == 0)
                    keyon(&_FMChannel[2]);

                if (qq != &_ExtensionChannel[0])
                {
                    if (_ExtensionChannel[0].partmask == 0 && (_ExtensionChannel[0].keyoff_flag & 1) == 0)
                        keyon(&_ExtensionChannel[0]);

                    if (qq != &_ExtensionChannel[1])
                    {
                        if (_ExtensionChannel[1].partmask == 0 && (_ExtensionChannel[1].keyoff_flag & 1) == 0)
                            keyon(&_ExtensionChannel[1]);
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
uint8_t * PMD::slotdetune_set(Channel * qq, uint8_t * si)
{
    int    ax, bl;

    if (_PMDWork.partb != 3 || _PMDWork.fmsel)
    {
        return si + 3;
    }

    bl = *si++;
    ax = *(int16_t *) si;
    si += 2;

    if (bl & 1)
    {
        _Work.slot_detune1 = ax;
    }

    if (bl & 2)
    {
        _Work.slot_detune2 = ax;
    }

    if (bl & 4)
    {
        _Work.slot_detune3 = ax;
    }

    if (bl & 8)
    {
        _Work.slot_detune4 = ax;
    }

    if (_Work.slot_detune1 || _Work.slot_detune2 ||
        _Work.slot_detune3 || _Work.slot_detune4)
    {
        _PMDWork.slotdetune_flag = 1;
    }
    else
    {
        _PMDWork.slotdetune_flag = 0;
        _Work.slot_detune1 = 0;
    }
    ch3mode_set(qq);
    return si;
}

//  Slot Detune Set(相対)
uint8_t * PMD::slotdetune_set2(Channel * qq, uint8_t * si)
{
    int    ax, bl;

    if (_PMDWork.partb != 3 || _PMDWork.fmsel)
    {
        return si + 3;
    }

    bl = *si++;
    ax = *(int16_t *) si;
    si += 2;

    if (bl & 1)
    {
        _Work.slot_detune1 += ax;
    }

    if (bl & 2)
    {
        _Work.slot_detune2 += ax;
    }

    if (bl & 4)
    {
        _Work.slot_detune3 += ax;
    }

    if (bl & 8)
    {
        _Work.slot_detune4 += ax;
    }

    if (_Work.slot_detune1 || _Work.slot_detune2 ||
        _Work.slot_detune3 || _Work.slot_detune4)
    {
        _PMDWork.slotdetune_flag = 1;
    }
    else
    {
        _PMDWork.slotdetune_flag = 0;
        _Work.slot_detune1 = 0;
    }
    ch3mode_set(qq);
    return si;
}

void PMD::fm3_partinit(Channel * qq, uint8_t * ax)
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
    qq->fmpan = _FMChannel[2].fmpan;  // FM PAN = CH3と同じ
    qq->partmask |= 0x20;      // s0用 partmask
}

//  FM3ch 拡張パートセット
uint8_t * PMD::fm3_extpartset(Channel *, uint8_t * si)
{
    int16_t ax;

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&_ExtensionChannel[0], &_Work.mmlbuf[ax]);

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&_ExtensionChannel[1], &_Work.mmlbuf[ax]);

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&_ExtensionChannel[2], &_Work.mmlbuf[ax]);
    return si;
}

//  ppz 拡張パートセット
uint8_t * PMD::ppz_extpartset(Channel *, uint8_t * si)
{
    int16_t  ax;
    int    i;

    for (i = 0; i < 8; i++)
    {
        ax = *(int16_t *) si;
        si += 2;
        if (ax)
        {
            _PPZChannel[i].address = &_Work.mmlbuf[ax];
            _PPZChannel[i].leng = 1;          // ｱﾄ 1ｶｳﾝﾄ ﾃﾞ ｴﾝｿｳ ｶｲｼ
            _PPZChannel[i].keyoff_flag = -1;      // 現在keyoff中
            _PPZChannel[i].mdc = -1;          // MDepth Counter (無限)
            _PPZChannel[i].mdc2 = -1;          //
            _PPZChannel[i]._mdc = -1;          //
            _PPZChannel[i]._mdc2 = -1;          //
            _PPZChannel[i].onkai = 255;        // rest
            _PPZChannel[i].onkai_def = 255;      // rest
            _PPZChannel[i].volume = 128;        // PCM VOLUME DEFAULT= 128
            _PPZChannel[i].fmpan = 5;          // PAN=Middle
        }
    }
    return si;
}

//  音量マスクslotの設定
uint8_t * PMD::volmask_set(Channel * qq, uint8_t * si)
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
uint8_t * PMD::special_0c0h(Channel * qq, uint8_t * si, uint8_t al)
{
    switch (al)
    {
        case 0xff: _Work.fm_voldown = *si++; break;
        case 0xfe: si = _vd_fm(qq, si); break;
        case 0xfd: _Work.ssg_voldown = *si++; break;
        case 0xfc: si = _vd_ssg(qq, si); break;
        case 0xfb: _Work.pcm_voldown = *si++; break;
        case 0xfa: si = _vd_pcm(qq, si); break;
        case 0xf9: _Work.rhythm_voldown = *si++; break;
        case 0xf8: si = _vd_rhythm(qq, si); break;
        case 0xf7: _Work.pcm86_vol = (*si++ & 1); break;
        case 0xf6: _Work.ppz_voldown = *si++; break;
        case 0xf5: si = _vd_ppz(qq, si); break;
        default:
            si--;
            *si = 0x80;
    }
    return si;
}

uint8_t * PMD::_vd_fm(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _Work.fm_voldown = Limit(al + _Work.fm_voldown, 255, 0);
    else
        _Work.fm_voldown = _Work._fm_voldown;

    return si;
}

uint8_t * PMD::_vd_ssg(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _Work.ssg_voldown = Limit(al + _Work.ssg_voldown, 255, 0);
    else
        _Work.ssg_voldown = _Work._ssg_voldown;

    return si;
}

uint8_t * PMD::_vd_pcm(Channel *, uint8_t * si)
{
    int  al = *(int8_t *) si++;

    if (al)
        _Work.pcm_voldown = Limit(al + _Work.pcm_voldown, 255, 0);
    else
        _Work.pcm_voldown = _Work._pcm_voldown;

    return si;
}

uint8_t * PMD::_vd_rhythm(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _Work.rhythm_voldown = Limit(al + _Work.rhythm_voldown, 255, 0);
    else
        _Work.rhythm_voldown = _Work._rhythm_voldown;

    return si;
}

uint8_t * PMD::_vd_ppz(Channel *, uint8_t * si)
{
    int al = *(int8_t *) si++;

    if (al)
        _Work.ppz_voldown = Limit(al + _Work.ppz_voldown, 255, 0);
    else
        _Work.ppz_voldown = _Work._ppz_voldown;

    return si;
}

// Mask on/off for playing parts
uint8_t * PMD::fm_mml_part_mask(Channel * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al != 0)
    {
        qq->partmask |= 0x40;

        if (qq->partmask == 0x40)
            MuteFMPart(qq);  // 音消去
    }
    else
    {
        if ((qq->partmask &= 0xbf) == 0)
            neiro_reset(qq);    // 音色再設定
    }

    return si;
}

uint8_t * PMD::ssg_mml_part_mask(Channel * qq, uint8_t * si)
{
    uint8_t b = *si++;

    if (b >= 2)
        si = special_0c0h(qq, si, b);
    else
    if (b != 0)
    {
        qq->partmask |= 0x40;

        if (qq->partmask == 0x40)
        {
            int ah = ((1 << (_PMDWork.partb - 1)) | (4 << _PMDWork.partb));
            uint32_t al = _OPNAW->GetReg(0x07);

            _OPNAW->SetReg(0x07, ah | al);    // PSG keyoff
        }
    }
    else
        qq->partmask &= 0xbf;

    return si;
}

uint8_t * PMD::rhythm_mml_part_mask(Channel * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al != 0)
        qq->partmask |= 0x40;
    else
        qq->partmask &= 0xbf;

    return si;
}

uint8_t * PMD::pcm_mml_part_mask(Channel * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al)
    {
        qq->partmask |= 0x40;

        if (qq->partmask == 0x40)
        {
            _OPNAW->SetReg(0x101, 0x02);    // PAN=0 / x8 bit mode
            _OPNAW->SetReg(0x100, 0x01);    // PCM RESET
        }
    }
    else
        qq->partmask &= 0xbf;

    return si;
}

uint8_t * PMD::pcm_mml_part_mask8(Channel * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al)
    {
        qq->partmask |= 0x40;

        if (qq->partmask == 0x40)
            _P86->Stop();
    }
    else
        qq->partmask &= 0xbf;

    return si;
}

uint8_t * PMD::ppz_mml_part_mask(Channel * qq, uint8_t * si)
{
    uint8_t al = *si++;

    if (al >= 2)
        si = special_0c0h(qq, si, al);
    else
    if (al != 0)
    {
        qq->partmask |= 0x40;

        if (qq->partmask == 0x40)
            _PPZ->Stop(_PMDWork.partb);
    }
    else
        qq->partmask &= 0xbf;

    return si;
}

// Reset the tone of the FM sound source
void PMD::neiro_reset(Channel * qq)
{
    if (qq->neiromask == 0)
        return;

    int s1 = qq->slot1;
    int s2 = qq->slot2;
    int s3 = qq->slot3;
    int s4 = qq->slot4;

    _PMDWork.af_check = 1;
    neiroset(qq, qq->voicenum);    // 音色復帰
    _PMDWork.af_check = 0;

    qq->slot1 = s1;
    qq->slot2 = s2;
    qq->slot3 = s3;
    qq->slot4 = s4;

    int dh;

    int al = ((~qq->carrier) & qq->slotmask) & 0xf0;

    // al<- TLを再設定していいslot 4321xxxx
    if (al)
    {
        dh = 0x4c - 1 + _PMDWork.partb;  // dh=TL FM Port Address

        if (al & 0x80) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) s4);

        dh -= 8;

        if (al & 0x40) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) s3);

        dh += 4;

        if (al & 0x20) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) s2);

        dh -= 8;

        if (al & 0x10) _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) s1);
    }

    dh = _PMDWork.partb + 0xb4 - 1;

    _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), calc_panout(qq));
}

uint8_t * PMD::_lfoswitch(Channel * qq, uint8_t * si)
{
    qq->lfoswi = (qq->lfoswi & 0x8f) | ((*si++ & 7) << 4);

    lfo_change(qq);
    lfoinit_main(qq);
    lfo_change(qq);

    return si;
}

uint8_t * PMD::_volmask_set(Channel * qq, uint8_t * si)
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
uint8_t * PMD::tl_set(Channel * qq, uint8_t * si)
{
    int dh = 0x40 - 1 + _PMDWork.partb;    // dh=TL FM Port Address
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
            if (qq->partmask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
            }
        }

        dh += 8;
        if (ah & 2)
        {
            qq->slot2 = dl;
            if (qq->partmask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
            }
        }

        dh -= 4;
        if (ah & 4)
        {
            qq->slot3 = dl;
            if (qq->partmask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
            }
        }

        dh += 8;
        if (ah & 8)
        {
            qq->slot4 = dl;
            if (qq->partmask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
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
            if (qq->partmask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
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
            if (qq->partmask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
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
            if (qq->partmask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
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
            if (qq->partmask == 0)
            {
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
            }
            qq->slot4 = dl;
        }
    }
    return si;
}

//  FB変化
uint8_t * PMD::fb_set(Channel * qq, uint8_t * si)
{
    int dl;

    int dh = _PMDWork.partb + 0xb0 - 1;  // dh=ALG/FB port address
    int al = *(int8_t *) si++;

    if (al >= 0)
    {
        // in  al 00000xxx 設定するFB
        al = ((al << 3) & 0xff) | (al >> 5);

        // in  al 00xxx000 設定するFB
        if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
        {
            if ((qq->slotmask & 0x10) == 0) return si;
            dl = (_PMDWork.fm3_alg_fb & 7) | al;
            _PMDWork.fm3_alg_fb = dl;
        }
        else
        {
            dl = (qq->alg_fb & 7) | al;
        }

        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
        qq->alg_fb = dl;

        return si;
    }
    else
    {
        if ((al & 0x40) == 0)
            al &= 7;

        if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
        {
            dl = _PMDWork.fm3_alg_fb;
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
                if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
                {
                    if ((qq->slotmask & 0x10) == 0) return si;

                    dl = (_PMDWork.fm3_alg_fb & 7) | al;
                    _PMDWork.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (qq->alg_fb & 7) | al;
                }
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
                qq->alg_fb = dl;
                return si;
            }
            else
            {
                // in  al 00000xxx 設定するFB
                al = ((al << 3) & 0xff) | (al >> 5);

                // in  al 00xxx000 設定するFB
                if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
                {
                    if ((qq->slotmask & 0x10) == 0) return si;
                    dl = (_PMDWork.fm3_alg_fb & 7) | al;
                    _PMDWork.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (qq->alg_fb & 7) | al;
                }
                _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
                qq->alg_fb = dl;
                return si;
            }
        }
        else
        {
            al = 0;
            if (_PMDWork.partb == 3 && _PMDWork.fmsel == 0)
            {
                if ((qq->slotmask & 0x10) == 0) return si;

                dl = (_PMDWork.fm3_alg_fb & 7) | al;
                _PMDWork.fm3_alg_fb = dl;
            }
            else
            {
                dl = (qq->alg_fb & 7) | al;
            }
            _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + dh), (uint32_t) dl);
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
        _Work.tempo_d = al;    // T (FC)
        _Work.tempo_d_push = al;
        calc_tb_tempo();

    }
    else if (al == 0xff)
    {
        al = *si++;          // t (FC FF)
        if (al < 18) al = 18;
        _Work.tempo_48 = al;
        _Work.tempo_48_push = al;
        calc_tempo_tb();

    }
    else if (al == 0xfe)
    {
        al = int8_t(*si++);      // T± (FC FE)
        if (al >= 0)
        {
            al += _Work.tempo_d_push;
        }
        else
        {
            al += _Work.tempo_d_push;
            if (al < 0)
            {
                al = 0;
            }
        }

        if (al > 250) al = 250;

        _Work.tempo_d = al;
        _Work.tempo_d_push = al;
        calc_tb_tempo();

    }
    else
    {
        al = int8_t(*si++);      // t± (FC FD)
        if (al >= 0)
        {
            al += _Work.tempo_48_push;
            if (al > 255)
            {
                al = 255;
            }
        }
        else
        {
            al += _Work.tempo_48_push;
            if (al < 0) al = 18;
        }

        _Work.tempo_48 = al;
        _Work.tempo_48_push = al;

        calc_tempo_tb();
    }

    return si;
}

//  COMMAND '[' [ﾙｰﾌﾟ ｽﾀｰﾄ]
uint8_t * PMD::comstloop(Channel * qq, uint8_t * si)
{
    uint8_t * ax;

    if (qq == &_EffectChannel)
    {
        ax = _Work.efcdat;
    }
    else
    {
        ax = _Work.mmlbuf;
    }

    ax[*(uint16_t *) si + 1] = 0;

    si += 2;

    return si;
}

//  COMMAND  ']' [ﾙｰﾌﾟ ｴﾝﾄﾞ]
uint8_t * PMD::comedloop(Channel * qq, uint8_t * si)
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

    if (qq == &_EffectChannel)
    {
        si = _Work.efcdat + ax;
    }
    else
    {
        si = _Work.mmlbuf + ax;
    }
    return si;
}

//  COMMAND  ':' [ﾙｰﾌﾟ ﾀﾞｯｼｭﾂ]
uint8_t * PMD::comexloop(Channel * qq, uint8_t * si)
{
    uint8_t * bx;
    int    dl;


    if (qq == &_EffectChannel)
    {
        bx = _Work.efcdat;
    }
    else
    {
        bx = _Work.mmlbuf;
    }


    bx += *(uint16_t *) si;
    si += 2;

    dl = *bx++ - 1;
    if (dl != *bx) return si;
    si = bx + 3;
    return si;
}

//  LFO ﾊﾟﾗﾒｰﾀ ｾｯﾄ
uint8_t * PMD::lfoset(Channel * qq, uint8_t * si)
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
uint8_t * PMD::lfoswitch(Channel * qq, uint8_t * si)
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

//  "\?" COMMAND [ OPNA Rhythm Keyon/Dump ]
uint8_t * PMD::rhykey(uint8_t * si)
{
    int dl = *si++ & _Work.rhythmmask;

    if (dl == 0)
        return si;

    if (_Work._FadeOutVolume != 0)
    {
        int al = ((256 - _Work._FadeOutVolume) * _Work.rhyvol) >> 8;

        _OPNAW->SetReg(0x11, (uint32_t) al);
    }

    if (dl < 0x80)
    {
        if (dl & 0x01) _OPNAW->SetReg(0x18, (uint32_t) _Work.rdat[0]);
        if (dl & 0x02) _OPNAW->SetReg(0x19, (uint32_t) _Work.rdat[1]);
        if (dl & 0x04) _OPNAW->SetReg(0x1a, (uint32_t) _Work.rdat[2]);
        if (dl & 0x08) _OPNAW->SetReg(0x1b, (uint32_t) _Work.rdat[3]);
        if (dl & 0x10) _OPNAW->SetReg(0x1c, (uint32_t) _Work.rdat[4]);
        if (dl & 0x20) _OPNAW->SetReg(0x1d, (uint32_t) _Work.rdat[5]);
    }

    _OPNAW->SetReg(0x10, (uint32_t) dl);

    if (dl >= 0x80)
    {
        if (dl & 0x01) _Work.rdump_bd++;
        if (dl & 0x02) _Work.rdump_sd++;
        if (dl & 0x04) _Work.rdump_sym++;
        if (dl & 0x08) _Work.rdump_hh++;
        if (dl & 0x10) _Work.rdump_tom++;
        if (dl & 0x20) _Work.rdump_rim++;

        _Work.rshot_dat &= (~dl);
    }
    else
    {
        if (dl & 0x01) _Work.rshot_bd++;
        if (dl & 0x02) _Work.rshot_sd++;
        if (dl & 0x04) _Work.rshot_sym++;
        if (dl & 0x08) _Work.rshot_hh++;
        if (dl & 0x10) _Work.rshot_tom++;
        if (dl & 0x20) _Work.rshot_rim++;

        _Work.rshot_dat |= dl;
    }

    return si;
}

//  "\v?n" COMMAND
uint8_t * PMD::rhyvs(uint8_t * si)
{
    int dl = *si & 0x1f;
    int dh = *si++ >> 5;
    int * bx = &_Work.rdat[dh - 1];

    dh = 0x18 - 1 + dh;
    dl |= (*bx & 0xc0);
    *bx = dl;

    _OPNAW->SetReg((uint32_t) dh, (uint32_t) dl);

    return si;
}

uint8_t * PMD::rhyvs_sft(uint8_t * si)
{
    int * bx = &_Work.rdat[*si - 1];
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
    bx = &_Work.rdat[dh - 1];

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

    if (_Work.rhythm_voldown != 0)
        dl = ((256 - _Work.rhythm_voldown) * dl) >> 8;

    _Work.rhyvol = dl;

    if (_Work._FadeOutVolume != 0)
        dl = ((256 - _Work._FadeOutVolume) * dl) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) dl);

    return si;
}

uint8_t * PMD::rmsvs_sft(uint8_t * si)
{
    int dl = _Work.rhyvol + *(int8_t *) si++;

    if (dl >= 64)
    {
        if (dl & 0x80)
            dl = 0;
        else
            dl = 63;
    }

    _Work.rhyvol = dl;

    if (_Work._FadeOutVolume != 0)
        dl = ((256 - _Work._FadeOutVolume) * dl) >> 8;

    _OPNAW->SetReg(0x11, (uint32_t) dl);

    return si;
}

// Change only one volume (V2.7 expansion)
uint8_t * PMD::vol_one_up_psg(Channel * qq, uint8_t * si)
{
    int    al;

    al = qq->volume + *si++;
    if (al > 15) al = 15;
    qq->volpush = ++al;
    _PMDWork.volpush_flag = 1;
    return si;
}

uint8_t * PMD::vol_one_down(Channel * qq, uint8_t * si)
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
    _PMDWork.volpush_flag = 1;
    return si;
}

//  ポルタメント(PSG)
uint8_t * PMD::portap(Channel * qq, uint8_t * si)
{
    int    ax, al_, bx_;

    if (qq->partmask)
    {
        qq->fnum = 0;    //休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

        if (--_PMDWork.volpush_flag)
        {
            qq->volpush = 0;
        }

        _PMDWork.tieflag = 0;
        _PMDWork.volpush_flag = 0;
        _PMDWork.loop_work &= qq->loopcheck;
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
        if (--_PMDWork.volpush_flag)
        {
            _PMDWork.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volsetp(qq);
    otodasip(qq);
    keyonp(qq);

    qq->keyon_flag++;
    qq->address = si;

    _PMDWork.tieflag = 0;
    _PMDWork.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {      // '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }

    _PMDWork.loop_work &= qq->loopcheck;
    return si;
}

//  'w' COMMAND [PSG NOISE ﾍｲｷﾝ ｼｭｳﾊｽｳ]
uint8_t * PMD::psgnoise_move(uint8_t * si)
{
    _Work.psnoi += *(int8_t *) si++;
    if (_Work.psnoi < 0) _Work.psnoi = 0;
    if (_Work.psnoi > 31) _Work.psnoi = 31;
    return si;
}

//  PSG Envelope set (Extend)
uint8_t * PMD::extend_psgenvset(Channel * qq, uint8_t * si)
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

uint8_t * PMD::mdepth_count(Channel * qq, uint8_t * si)
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
int PMD::lfoinitp(Channel * qq, int al)
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

    if (_PMDWork.tieflag & 1)
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

void PMD::lfo_exit(Channel * qq)
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
void PMD::lfin1(Channel * qq)
{
    qq->hldelay_c = qq->hldelay;

    if (qq->hldelay)
        _OPNAW->SetReg((uint32_t) (_PMDWork.fmsel + _PMDWork.partb + 0xb4 - 1), (uint32_t) (qq->fmpan & 0xc0));

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

void PMD::lfoinit_main(Channel * qq)
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
int PMD::oshiftp(Channel * qq, int al)
{
    return oshift(qq, al);
}

int PMD::oshift(Channel * qq, int al)
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
void PMD::fnumsetp(Channel * qq, int al)
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
uint8_t * PMD::calc_q(Channel * qq, uint8_t * si)
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

//  ＰＳＧ　ＶＯＬＵＭＥ　ＳＥＴ
void PMD::volsetp(Channel * qq)
{
    if (qq->envf == 3 || (qq->envf == -1 && qq->eenv_count == 0))
        return;

    int dl = (qq->volpush) ? qq->volpush - 1 : qq->volume;

    //------------------------------------------------------------------------
    //  音量down計算
    //------------------------------------------------------------------------
    dl = ((256 - _Work.ssg_voldown) * dl) >> 8;

    //------------------------------------------------------------------------
    //  Fadeout計算
    //------------------------------------------------------------------------
    dl = ((256 - _Work._FadeOutVolume) * dl) >> 8;

    //------------------------------------------------------------------------
    //  ENVELOPE 計算
    //------------------------------------------------------------------------
    if (dl <= 0)
    {
        _OPNAW->SetReg((uint32_t) (_PMDWork.partb + 8 - 1), 0);
        return;
    }

    if (qq->envf == -1)
    {
        if (qq->eenv_volume == 0)
        {
            _OPNAW->SetReg((uint32_t) (_PMDWork.partb + 8 - 1), 0);
            return;
        }

        dl = ((((dl * (qq->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        dl += qq->eenv_volume;

        if (dl <= 0)
        {
            _OPNAW->SetReg((uint32_t) (_PMDWork.partb + 8 - 1), 0);
            return;
        }

        if (dl > 15)
            dl = 15;
    }

    //--------------------------------------------------------------------
    //  音量LFO計算
    //--------------------------------------------------------------------
    if ((qq->lfoswi & 0x22) == 0)
    {
        _OPNAW->SetReg((uint32_t) (_PMDWork.partb + 8 - 1), (uint32_t) dl);
        return;
    }

    int ax = (qq->lfoswi & 2) ? qq->lfodat : 0;

    if (qq->lfoswi & 0x20)
        ax += qq->_lfodat;

    dl += ax;

    if (dl < 0)
    {
        _OPNAW->SetReg((uint32_t) (_PMDWork.partb + 8 - 1), 0);
        return;
    }

    if (dl > 15)
        dl = 15;

    //------------------------------------------------------------------------
    //  出力
    //------------------------------------------------------------------------
    _OPNAW->SetReg((uint32_t) (_PMDWork.partb + 8 - 1), (uint32_t) dl);
}

//  ＰＳＧ　音程設定
void PMD::otodasip(Channel * qq)
{
    if (qq->fnum == 0)
        return;

    // PSG Portament set
    int ax = (int) (qq->fnum + qq->porta_num);
    int dx = 0;

    // PSG Detune/LFO set
    if ((qq->extendmode & 1) == 0)
    {
        ax -= qq->detune;
        if (qq->lfoswi & 1)
        {
            ax -= qq->lfodat;
        }

        if (qq->lfoswi & 0x10)
        {
            ax -= qq->_lfodat;
        }
    }
    else
    {
        // 拡張DETUNE(DETUNE)の計算
        if (qq->detune)
        {
            dx = (ax * qq->detune) >> 12;    // dx:ax=ax * qq->detune
            if (dx >= 0)
            {
                dx++;
            }
            else
            {
                dx--;
            }
            ax -= dx;
        }
        // 拡張DETUNE(LFO)の計算
        if (qq->lfoswi & 0x11)
        {
            if (qq->lfoswi & 1)
            {
                dx = qq->lfodat;
            }
            else
            {
                dx = 0;
            }

            if (qq->lfoswi & 0x10)
            {
                dx += qq->_lfodat;
            }

            if (dx)
            {
                dx = (ax * dx) >> 12;
                if (dx >= 0)
                {
                    dx++;
                }
                else
                {
                    dx--;
                }
            }
            ax -= dx;
        }
    }

    // TONE SET

    if (ax >= 0x1000)
    {
        if (ax >= 0)
        {
            ax = 0xfff;
        }
        else
        {
            ax = 0;
        }
    }

    _OPNAW->SetReg((uint32_t) ((_PMDWork.partb - 1) * 2),     (uint32_t) LOBYTE(ax));
    _OPNAW->SetReg((uint32_t) ((_PMDWork.partb - 1) * 2 + 1), (uint32_t) HIBYTE(ax));
}

//  ＰＳＧ　ＫＥＹＯＮ
void PMD::keyonp(Channel * qq)
{
    if (qq->onkai == 255)
        return;    // ｷｭｳﾌ ﾉ ﾄｷ

    int ah = (1 << (_PMDWork.partb - 1)) | (1 << (_PMDWork.partb + 2));
    int al = ((int32_t) _OPNAW->GetReg(0x07) | ah);

    ah = ~(ah & qq->psgpat);
    al &= ah;

    _OPNAW->SetReg(7, (uint32_t) al);

    // PSG ﾉｲｽﾞ ｼｭｳﾊｽｳ ﾉ ｾｯﾄ

    if (_Work.psnoi != _Work.psnoi_last && _EffectState.effon == 0)
    {
        _OPNAW->SetReg(6, (uint32_t) _Work.psnoi);
        _Work.psnoi_last = _Work.psnoi;
    }
}

//  ＬＦＯ処理
//    Don't Break cl
//    output    cy=1  変化があった
int PMD::lfo(Channel * qq)
{
    return lfop(qq);
}

int PMD::lfop(Channel * qq)
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
        ch = _Work._TimerATime - _PMDWork._OldTimerATime;
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

void PMD::lfo_main(Channel * qq)
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
void PMD::md_inc(Channel * qq)
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
void PMD::lfo_change(Channel * qq)
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
void PMD::porta_calc(Channel * qq)
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
int PMD::soft_env(Channel * qq)
{
    if (qq->extendmode & 4)
    {
        if (_Work._TimerATime == _PMDWork._OldTimerATime) return 0;

        int cl = 0;

        for (int i = 0; i < _Work._TimerATime - _PMDWork._OldTimerATime; i++)
        {
            if (soft_env_main(qq))
                cl = 1;
        }

        return cl;
    }
    else
        return soft_env_main(qq);
}

int PMD::soft_env_main(Channel * qq)
{
    if (qq->envf == -1)
        return ext_ssgenv_main(qq);

    int dl = qq->eenv_volume;

    soft_env_sub(qq);

    if (dl == qq->eenv_volume)
        return 0;

    return -1;
}

int PMD::soft_env_sub(Channel * qq)
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
int PMD::ext_ssgenv_main(Channel * qq)
{
    if (qq->eenv_count == 0)
        return 0;

    int dl = qq->eenv_volume;

    esm_sub(qq, qq->eenv_count);

    if (dl == qq->eenv_volume)
        return 0;

    return -1;
}

void PMD::esm_sub(Channel * qq, int ah)
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
    if (_Work._TimerBSpeed != _Work.tempo_d)
    {
        _Work._TimerBSpeed = _Work.tempo_d;
        _OPNAW->SetReg(0x26, (uint32_t) _Work._TimerBSpeed);
    }
}

//  小節のカウント
void PMD::syousetu_count()
{
    if (_Work.opncount + 1 == _Work.syousetu_lng)
    {
        _Work.syousetu++;
        _Work.opncount = 0;
    }
    else
    {
        _Work.opncount++;
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
    ::memset(_ExtensionChannel, 0, sizeof(_ExtensionChannel));
    ::memset(&_EffectChannel, 0, sizeof(_EffectChannel));
    ::memset(_PPZChannel, 0, sizeof(_PPZChannel));

    _Work.rhythmmask = 255;
    _PMDWork.rhydmy = 255;
    InitializeDataArea();
    opn_init();

    _OPNAW->SetReg(0x07, 0xbf);
    DriverStop();
    setint();
    _OPNAW->SetReg(0x29, 0x83);
}

void PMD::InitializeDataArea()
{
    _Work._FadeOutVolume = 0;
    _Work._FadeOutSpeed = 0;
    _Work.fadeout_flag = 0;
    _Work._FadeOutSpeedHQ = 0;

    for (int i = 0; i < 6; i++)
    {
        int partmask = _FMChannel[i].partmask;
        int keyon_flag = _FMChannel[i].keyon_flag;

        ::memset(&_FMChannel[i], 0, sizeof(Channel));

        _FMChannel[i].partmask = partmask & 0x0f;
        _FMChannel[i].keyon_flag = keyon_flag;
        _FMChannel[i].onkai = 255;
        _FMChannel[i].onkai_def = 255;
    }

    for (int i = 0; i < 3; i++)
    {
        int partmask = _SSGChannel[i].partmask;
        int keyon_flag = _SSGChannel[i].keyon_flag;

        ::memset(&_SSGChannel[i], 0, sizeof(Channel));

        _SSGChannel[i].partmask = partmask & 0x0f;
        _SSGChannel[i].keyon_flag = keyon_flag;
        _SSGChannel[i].onkai = 255;
        _SSGChannel[i].onkai_def = 255;
    }

    {
        int partmask = _ADPCMChannel.partmask;
        int keyon_flag = _ADPCMChannel.keyon_flag;

        ::memset(&_ADPCMChannel, 0, sizeof(Channel));

        _ADPCMChannel.partmask = partmask & 0x0f;
        _ADPCMChannel.keyon_flag = keyon_flag;
        _ADPCMChannel.onkai = 255;
        _ADPCMChannel.onkai_def = 255;
    }

    {
        int partmask = _RhythmChannel.partmask;
        int keyon_flag = _RhythmChannel.keyon_flag;

        ::memset(&_RhythmChannel, 0, sizeof(Channel));

        _RhythmChannel.partmask = partmask & 0x0f;
        _RhythmChannel.keyon_flag = keyon_flag;
        _RhythmChannel.onkai = 255;
        _RhythmChannel.onkai_def = 255;
    }

    for (int i = 0; i < 3; i++)
    {
        int partmask = _ExtensionChannel[i].partmask;
        int keyon_flag = _ExtensionChannel[i].keyon_flag;

        ::memset(&_ExtensionChannel[i], 0, sizeof(Channel));

        _ExtensionChannel[i].partmask = partmask & 0x0f;
        _ExtensionChannel[i].keyon_flag = keyon_flag;
        _ExtensionChannel[i].onkai = 255;
        _ExtensionChannel[i].onkai_def = 255;
    }

    for (int i = 0; i < 8; i++)
    {
        int partmask = _PPZChannel[i].partmask;
        int keyon_flag = _PPZChannel[i].keyon_flag;

        ::memset(&_PPZChannel[i], 0, sizeof(Channel));

        _PPZChannel[i].partmask = partmask & 0x0f;
        _PPZChannel[i].keyon_flag = keyon_flag;
        _PPZChannel[i].onkai = 255;
        _PPZChannel[i].onkai_def = 255;
    }

    _PMDWork.tieflag = 0;
    _Work.status = 0;
    _Work._LoopCount = 0;
    _Work.syousetu = 0;
    _Work.opncount = 0;
    _Work._TimerATime = 0;
    _PMDWork._OldTimerATime = 0;

    _PMDWork.omote_key[0] = 0;
    _PMDWork.omote_key[1] = 0;
    _PMDWork.omote_key[2] = 0;
    _PMDWork.ura_key[0] = 0;
    _PMDWork.ura_key[1] = 0;
    _PMDWork.ura_key[2] = 0;

    _PMDWork.fm3_alg_fb = 0;
    _PMDWork.af_check = 0;

    _Work.pcmstart = 0;
    _Work.pcmstop = 0;
    _PMDWork.pcmrepeat1 = 0;
    _PMDWork.pcmrepeat2 = 0;
    _PMDWork.pcmrelease = 0x8000;

    _Work.kshot_dat = 0;
    _Work.rshot_dat = 0;
    _EffectState.last_shot_data = 0;

    _PMDWork.slotdetune_flag = 0;
    _Work.slot_detune1 = 0;
    _Work.slot_detune2 = 0;
    _Work.slot_detune3 = 0;
    _Work.slot_detune4 = 0;

    _PMDWork.slot3_flag = 0;
    _Work.ch3mode = 0x3f;

    _PMDWork.fmsel = 0;

    _Work.syousetu_lng = 96;

    _Work.fm_voldown = _Work._fm_voldown;
    _Work.ssg_voldown = _Work._ssg_voldown;
    _Work.pcm_voldown = _Work._pcm_voldown;
    _Work.ppz_voldown = _Work._ppz_voldown;
    _Work.rhythm_voldown = _Work._rhythm_voldown;
    _Work.pcm86_vol = _Work._pcm86_vol;
}

//  OPN INIT
void PMD::opn_init()
{
    _OPNAW->ClearBuffer();
    _OPNAW->SetReg(0x29, 0x83);

    _Work.psnoi = 0;

    _OPNAW->SetReg(0x06, 0x00);
    _Work.psnoi_last = 0;

    // SSG-EG RESET (4.8s)
    for (uint32_t i = 0x90; i < 0x9F; i++)
    {
        if (i % 4 != 3)
            _OPNAW->SetReg(i, 0x00);
    }

    for (uint32_t i = 0x190; i < 0x19F; i++)
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

    _Work.port22h = 0x00;
    _OPNAW->SetReg(0x22, 0x00);

    //  Rhythm Default = Pan : Mid , Vol : 15
    for (int i = 0; i < 6; i++)
        _Work.rdat[i] = 0xcf;

    _OPNAW->SetReg(0x10, 0xff);

    // Rhythm total level set
    _Work.rhyvol = 48 * 4 * (256 - _Work.rhythm_voldown) / 1024;
    _OPNAW->SetReg(0x11, (uint32_t) _Work.rhyvol);

    // PCM reset & LIMIT SET
    _OPNAW->SetReg(0x10c, 0xff);
    _OPNAW->SetReg(0x10d, 0xff);

    for (int i = 0; i < PCM_CNL_MAX; i++)
        _PPZ->SetPan(i, 5);
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

    for (int i = 0; i < PCM_CNL_MAX; i++)
        _PPZ->Stop(i);
}

/// <summary>
/// Starts the driver.
/// </summary>
void PMD::Start()
{
    if (_Work._IsTimerABusy || _Work._IsTimerBBusy)
    {
        _PMDWork.music_flag |= 1; // Not executed during TA/TB processing

        return;
    }

    DriverStart();
}

/// <summary>
/// Stops the driver.
/// </summary>
void PMD::Stop()
{
    if (_Work._IsTimerABusy || _Work._IsTimerBBusy)
    {
        _PMDWork.music_flag |= 2;
    }
    else
    {
        _Work.fadeout_flag = 0;
        DriverStop();
    }

    ::memset(wavbuf2, 0, sizeof(wavbuf2));
    _Position = 0;
}

void PMD::DriverStart()
{
    // Set TimerB = 0 and Timer Reset (to match the length of the song every time)
    _Work.tempo_d = 0;

    settempo_b();

    _OPNAW->SetReg(0x27, 0x00); // TIMER RESET (both timer A and B)

    //  演奏停止
    _PMDWork.music_flag &= 0xfe;
    DriverStop();

    //  バッファ初期化
    _PCMPtr = (uint8_t *) wavbuf2;    // Start position of remaining samples in buf
    _SamplesToDo = 0;           // Number of samples remaining in buf
    _Position = 0;                   // Time from start of playing (μs)

    //  演奏準備
    InitializeDataArea();
    play_init();

    //  OPN初期化
    opn_init();

    //  音楽の演奏を開始
    setint();

    _Work._IsPlaying = true;
}

void PMD::DriverStop()
{
    _PMDWork.music_flag &= 0xfd;

    _Work._IsPlaying = false;
    _Work._LoopCount = -1;
    _Work._FadeOutSpeed = 0;
    _Work._FadeOutVolume = 0xFF;

    Silence();
}

// Set the start address and initial value of each part
void PMD::play_init()
{
    _Work.x68_flg = *(_Work.mmlbuf - 1);

    //２．６追加分
    if (*_Work.mmlbuf != 2 * (max_part2 + 1))
    {
        _Work.prgdat_adr = _Work.mmlbuf + *(uint16_t *) (&_Work.mmlbuf[2 * (max_part2 + 1)]);
        _Work.prg_flg = 1;
    }
    else
    {
        _Work.prg_flg = 0;
    }

    uint16_t * p = (uint16_t *) _Work.mmlbuf;

    //  Part 0,1,2,3,4,5(FM1?6)の時
    for (int i = 0; i < 6; i++)
    {
        if (_Work.mmlbuf[*p] == 0x80) // Do not play if the beginning is 80h
            _FMChannel[i].address = NULL;
        else
            _FMChannel[i].address = &_Work.mmlbuf[*p];

        _FMChannel[i].leng = 1;
        _FMChannel[i].keyoff_flag = -1;    // 現在keyoff中
        _FMChannel[i].mdc = -1;        // MDepth Counter (無限)
        _FMChannel[i].mdc2 = -1;      // 同上
        _FMChannel[i]._mdc = -1;      // 同上
        _FMChannel[i]._mdc2 = -1;      // 同上
        _FMChannel[i].onkai = 255;      // rest
        _FMChannel[i].onkai_def = 255;    // rest
        _FMChannel[i].volume = 108;      // FM  VOLUME DEFAULT= 108
        _FMChannel[i].fmpan = 0xc0;      // FM PAN = Middle
        _FMChannel[i].slotmask = 0xf0;    // FM SLOT MASK
        _FMChannel[i].neiromask = 0xff;    // FM Neiro MASK
        p++;
    }

    //  Part 6,7,8(PSG1?3)の時
    for (int i = 0; i < 3; i++)
    {
        if (_Work.mmlbuf[*p] == 0x80) // Do not play if the beginning is 80h
            _SSGChannel[i].address = NULL;
        else
            _SSGChannel[i].address = &_Work.mmlbuf[*p];

        _SSGChannel[i].leng = 1;
        _SSGChannel[i].keyoff_flag = -1;  // 現在keyoff中
        _SSGChannel[i].mdc = -1;      // MDepth Counter (無限)
        _SSGChannel[i].mdc2 = -1;      // 同上
        _SSGChannel[i]._mdc = -1;      // 同上
        _SSGChannel[i]._mdc2 = -1;      // 同上
        _SSGChannel[i].onkai = 255;      // rest
        _SSGChannel[i].onkai_def = 255;    // rest
        _SSGChannel[i].volume = 8;      // PSG VOLUME DEFAULT= 8
        _SSGChannel[i].psgpat = 7;      // PSG = TONE
        _SSGChannel[i].envf = 3;      // PSG ENV = NONE/normal
        p++;

    }

    //  Part 9(OPNA/ADPCM)の時
    if (_Work.mmlbuf[*p] == 0x80) // Do not play if the beginning is 80h
        _ADPCMChannel.address = NULL;
    else
        _ADPCMChannel.address = &_Work.mmlbuf[*p];

    _ADPCMChannel.leng = 1;
    _ADPCMChannel.keyoff_flag = -1;    // 現在keyoff中
    _ADPCMChannel.mdc = -1;        // MDepth Counter (無限)
    _ADPCMChannel.mdc2 = -1;      // 同上
    _ADPCMChannel._mdc = -1;      // 同上
    _ADPCMChannel._mdc2 = -1;      // 同上
    _ADPCMChannel.onkai = 255;      // rest
    _ADPCMChannel.onkai_def = 255;    // rest
    _ADPCMChannel.volume = 128;      // PCM VOLUME DEFAULT= 128
    _ADPCMChannel.fmpan = 0xc0;      // PCM PAN = Middle
    p++;

    //  Part 10(Rhythm)の時
    if (_Work.mmlbuf[*p] == 0x80) // Do not play if the beginning is 80h
        _RhythmChannel.address = NULL;
    else
        _RhythmChannel.address = &_Work.mmlbuf[*p];

    _RhythmChannel.leng = 1;
    _RhythmChannel.keyoff_flag = -1;  // 現在keyoff中
    _RhythmChannel.mdc = -1;      // MDepth Counter (無限)
    _RhythmChannel.mdc2 = -1;      // 同上
    _RhythmChannel._mdc = -1;      // 同上
    _RhythmChannel._mdc2 = -1;      // 同上
    _RhythmChannel.onkai = 255;      // rest
    _RhythmChannel.onkai_def = 255;    // rest
    _RhythmChannel.volume = 15;      // PPSDRV volume
    p++;

    // Set rhythm address table.
    _Work.radtbl = (uint16_t *) &_Work.mmlbuf[*p];
    _Work.rhyadr = (uint8_t *) &_PMDWork.rhydmy;
}

//  インタラプト　設定
//  FM音源専用
void PMD::setint()
{
    // ＯＰＮ割り込み初期設定
    _Work.tempo_d = 200;
    _Work.tempo_d_push = 200;

    calc_tb_tempo();
    settempo_b();

    _OPNAW->SetReg(0x25, 0x00);      // TIMER A SET (9216μs固定)
    _OPNAW->SetReg(0x24, 0x00);      // 一番遅くて丁度いい
    _OPNAW->SetReg(0x27, 0x3f);      // TIMER ENABLE

    //　小節カウンタリセット
    _Work.opncount = 0;
    _Work.syousetu = 0;
    _Work.syousetu_lng = 96;
}

//  T->t 変換
//    input  [tempo_d]
//    output  [tempo_48]
void PMD::calc_tb_tempo()
{
    //  TEMPO = 0x112C / [ 256 - TB ]  timerB -> tempo
    int temp;

    if (256 - _Work.tempo_d == 0)
    {
        temp = 255;
    }
    else
    {
        temp = (0x112c * 2 / (256 - _Work.tempo_d) + 1) / 2;

        if (temp > 255)
            temp = 255;
    }

    _Work.tempo_48 = temp;
    _Work.tempo_48_push = temp;
}

//  t->T 変換
//    input  [tempo_48]
//    output  [tempo_d]
void PMD::calc_tempo_tb()
{
    int    al;

    //  TB = 256 - [ 112CH / TEMPO ]  tempo -> timerB

    if (_Work.tempo_48 >= 18)
    {
        al = 256 - 0x112c / _Work.tempo_48;
        if (0x112c % _Work.tempo_48 >= 128)
        {
            al--;
        }
        //al = 256 - (0x112c * 2 / open_work.tempo_48 + 1) / 2;
    }
    else
    {
        al = 0;
    }
    _Work.tempo_d = al;
    _Work.tempo_d_push = al;
}

//  ＰＣＭメモリからメインメモリへのデータ取り込み
//
//  INPUTS   .. pcmstart     to Start Address
//      .. pcmstop      to Stop  Address
//      .. buf      to PCMDATA_Buffer
void PMD::pcmread(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf)
{
    _OPNAW->SetReg(0x100, 0x01);
    _OPNAW->SetReg(0x110, 0x00);
    _OPNAW->SetReg(0x110, 0x80);
    _OPNAW->SetReg(0x100, 0x20);
    _OPNAW->SetReg(0x101, 0x02);    // x8
    _OPNAW->SetReg(0x10c, 0xff);
    _OPNAW->SetReg(0x10d, 0xff);
    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(pcmstart));
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(pcmstart));
    _OPNAW->SetReg(0x104, 0xff);
    _OPNAW->SetReg(0x105, 0xff);

    *buf = (uint8_t) _OPNAW->GetReg(0x108);    // 無駄読み
    *buf = (uint8_t) _OPNAW->GetReg(0x108);    // 無駄読み

    for (int i = 0; i < (pcmstop - pcmstart) * 32; i++)
    {
        *buf++ = (uint8_t) _OPNAW->GetReg(0x108);

        _OPNAW->SetReg(0x110, 0x80);
    }
}

//  ＰＣＭメモリへメインメモリからデータを送る (x8,高速版)
//
//  INPUTS   .. pcmstart     to Start Address
//      .. pcmstop      to Stop  Address
//      .. buf      to PCMDATA_Buffer
void PMD::pcmstore(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf)
{
    _OPNAW->SetReg(0x100, 0x01);
//  _OPNA->SetReg(0x110, 0x17);  // brdy以外はマスク(=timer割り込みは掛からない)
    _OPNAW->SetReg(0x110, 0x80);
    _OPNAW->SetReg(0x100, 0x60);
    _OPNAW->SetReg(0x101, 0x02);  // x8
    _OPNAW->SetReg(0x10c, 0xff);
    _OPNAW->SetReg(0x10d, 0xff);
    _OPNAW->SetReg(0x102, (uint32_t) LOBYTE(pcmstart));
    _OPNAW->SetReg(0x103, (uint32_t) HIBYTE(pcmstart));
    _OPNAW->SetReg(0x104, 0xff);
    _OPNAW->SetReg(0x105, 0xff);

    for (int i = 0; i < (pcmstop - pcmstart) * 32; i++)
        _OPNAW->SetReg(0x108, *buf++);
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

    ::wcscpy(_Work._PPCFileName, filePath);

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
        _Work._PPCFileName[0] = '\0';

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
                pcmends.pcmadrs[i][0] = pcmdata[16 + i * 4];
                pcmends.pcmadrs[i][1] = 0;
            }
            else
            {
                pcmends.pcmadrs[i][0] = (uint16_t) (*(uint16_t *) &pcmdata[16 + i * 4] + 0x26);
                pcmends.pcmadrs[i][1] = (uint16_t) (*(uint16_t *) &pcmdata[18 + i * 4] + 0x26);
            }

            if (bx < pcmends.pcmadrs[i][1])
                bx = pcmends.pcmadrs[i][1] + 1;
        }

        // The remaining 128 are undefined
        for (i = 128; i < 256; i++)
        { 
            pcmends.pcmadrs[i][0] = 0;
            pcmends.pcmadrs[i][1] = 0;
        }

        pcmends.pcmends = (uint16_t) bx;
    }
    else
    if (::strncmp((char *) pcmdata, PPCHeader, sizeof(PPCHeader) - 1) == 0)
    {   // PPC
        FoundPVI = false;

        pcmdata2 = (uint16_t *) pcmdata + 30 / 2;

        if (size < 30 + 4 * 256 + 2)
        {
            _Work._PPCFileName[0] = '\0';

            return ERR_UNKNOWN_FORMAT;
        }

        pcmends.pcmends = *pcmdata2++;

        for (i = 0; i < 256; i++)
        {
            pcmends.pcmadrs[i][0] = *pcmdata2++;
            pcmends.pcmadrs[i][1] = *pcmdata2++;
        }
    }
    else
    {
        _Work._PPCFileName[0] = '\0';

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
    ::memcpy(&tempbuf2[sizeof(PPCHeader) - 1], &pcmends.pcmends, sizeof(tempbuf2) - (sizeof(PPCHeader) - 1));

    pcmstore(0, 0x25, tempbuf2);

    // Write PCMDATA to PCMRAM
    if (FoundPVI)
    {
        pcmdata2 = (uint16_t *) (pcmdata + 0x10 + sizeof(uint16_t) * 2 * 128);

        if (size < (int) (pcmends.pcmends - (0x10 + sizeof(uint16_t) * 2 * 128)) * 32)
        {
            _Work._PPCFileName[0] = '\0';

            return ERR_UNKNOWN_FORMAT;
        }
    }
    else
    {
        pcmdata2 = (uint16_t *) pcmdata + (30 + 4 * 256 + 2) / 2;

        if (size < (pcmends.pcmends - ((30 + 4 * 256 + 2) / 2)) * 32)
        {
            _Work._PPCFileName[0] = '\0';

            return ERR_UNKNOWN_FORMAT;
        }
    }

    uint16_t pcmstart = 0x26;
    uint16_t pcmstop = pcmends.pcmends;

    pcmstore(pcmstart, pcmstop, (uint8_t *) pcmdata2);

    return ERR_SUCCESS;
}

/// <summary>
/// Finds a PCM sample in the specified search path.
/// </summary>
WCHAR * PMD::FindFile(WCHAR * filePath, const WCHAR * filename)
{
    WCHAR FilePath[MAX_PATH];

    for (size_t i = 0; i < _Work._SearchPath.size(); ++i)
    {
        CombinePath(FilePath, _countof(FilePath), _Work._SearchPath[i].c_str(), filename);

        if (_File->GetFileSize(FilePath) > 0)
        {
            ::wcscpy(filePath, FilePath);

            return filePath;
        }
    }

    return nullptr;
}

//  fm effect
uint8_t * PMD::fm_efct_set(Channel *, uint8_t * si)
{
    return si + 1;
}

uint8_t * PMD::ssg_efct_set(Channel * qq, uint8_t * si)
{
    int al = *si++;

    if (qq->partmask)
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
    if (_Work._FadeOutSpeed == 0)
        return;

    if (_Work._FadeOutSpeed > 0)
    {
        if (_Work._FadeOutSpeed + _Work._FadeOutVolume < 256)
        {
            _Work._FadeOutVolume += _Work._FadeOutSpeed;
        }
        else
        {
            _Work._FadeOutVolume = 255;
            _Work._FadeOutSpeed  =   0;

            if (_Work.fade_stop_flag == 1)
                _PMDWork.music_flag |= 2;
        }
    }
    else
    {   // Fade in
        if (_Work._FadeOutSpeed + _Work._FadeOutVolume > 255)
        {
            _Work._FadeOutVolume += _Work._FadeOutSpeed;
        }
        else
        {
            _Work._FadeOutVolume = 0;
            _Work._FadeOutSpeed = 0;

            _OPNAW->SetReg(0x11, (uint32_t) _Work.rhyvol);
        }
    }
}
