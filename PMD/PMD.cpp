
/** $VER: PMD.cpp (2023.07.15) P. Stuer **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4710 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "PMD.h"

#include "util.h"
#include "table.h"
#include "opnaw.h"
#include "ppz8l.h"
#include "ppsdrv.h"
#include "p86drv.h"

PMD::PMD()
{
    fileio = new FileIO();

    pfileio = fileio;

    _OPNA = new FM::OPNAW(pfileio);
    _PPZ8 = new PPZ8(pfileio);
    _PPS  = new PPSDRV(pfileio);
    _P86  = new P86DRV(pfileio);

    InitializeInternal();
}

PMD::~PMD()
{
    delete _P86;
    delete _PPS;
    delete _PPZ8;
    delete _OPNA;
}

#pragma region(IPCMMusicDriver)
/// <summary>
/// Initializes the driver.
/// </summary>
bool PMD::Initialize(const WCHAR * directoryPath)
{
    WCHAR Path[MAX_PATH] = { 0 };

    if (directoryPath != nullptr)
    {
        _FilePath.Strcpy(Path, directoryPath);
        _FilePath.AddDelimiter(Path);
    }

    InitializeInternal();

    _PPZ8->Init(_OpenWork._OPNARate, false);
    _PPS->Init(_OpenWork._OPNARate, false);
    _P86->Init(_OpenWork._OPNARate, false);

    ::memset(_OpenWork.pcmdir, 0, sizeof(_OpenWork.pcmdir));

    if (_OPNA->Init(OPNAClock, SOUND_44K, false, Path) == false)
        return false;

    // Initialize ADPCM RAM.
    {
        _OPNA->SetFMWait(0);
        _OPNA->SetSSGWait(0);
        _OPNA->SetRhythmWait(0);
        _OPNA->SetADPCMWait(0);

        uint8_t	Page[0x400]; // 0x400 * 0x100 = 0x40000(256K)

        ::memset(Page, 0x08, sizeof(Page));

        for (int i = 0; i < 0x100; i++)
            pcmstore((uint16_t) i * sizeof(Page) / 32, (uint16_t) (i + 1) * sizeof(Page) / 32, Page);
    }
/*
    opna->SetVolumeFM(10);
    opna->SetVolumePSG(0);
    opna->SetVolumeADPCM(10);
    opna->SetVolumeRhythmTotal(10);
    ppz8->SetVolume(10);
    ppsdrv->SetVolume(10);
    p86drv->SetVolume(10);
*/
    _OPNA->SetVolumeFM(0);
    _OPNA->SetVolumePSG(-18);
    _OPNA->SetVolumeADPCM(0);
    _OPNA->SetVolumeRhythmTotal(0);

    _PPZ8->SetVolume(0);

    _PPS->SetVolume(0);
    _P86->SetVolume(0);

    _OPNA->SetFMWait(DEFAULT_REG_WAIT);
    _OPNA->SetSSGWait(DEFAULT_REG_WAIT);
    _OPNA->SetRhythmWait(DEFAULT_REG_WAIT);
    _OPNA->SetADPCMWait(DEFAULT_REG_WAIT);

    pcmends.pcmends = 0x26;

    for (int i = 0; i < 256; i++)
    {
        pcmends.pcmadrs[i][0] = 0;
        pcmends.pcmadrs[i][1] = 0;
    }

    _OpenWork.ppcfilename[0] = '\0';

    for (int i = 0; i < MAX_PCMDIR + 1; ++i)
        _OpenWork.pcmdir[i][0] = '\0';

    // Initial setting of 088/188/288/388 (same INT number only)
    _OPNA->SetReg(0x29, 0x00);
    _OPNA->SetReg(0x24, 0x00);
    _OPNA->SetReg(0x25, 0x00);
    _OPNA->SetReg(0x26, 0x00);
    _OPNA->SetReg(0x27, 0x3f);

    // Start the OPN interrupt.
    opnint_start();

    return true;
}

// Initialization (internal processing)
void PMD::InitializeInternal(void)
{
    memset(&_OpenWork, 0, sizeof(_OpenWork));

    memset(FMPart, 0, sizeof(FMPart));
    memset(SSGPart, 0, sizeof(SSGPart));
    memset(&ADPCMPart, 0, sizeof(ADPCMPart));
    memset(&RhythmPart, 0, sizeof(RhythmPart));
    memset(ExtPart, 0, sizeof(ExtPart));
    memset(&DummyPart, 0, sizeof(DummyPart));
    memset(&EffPart, 0, sizeof(EffPart));
    memset(PPZ8Part, 0, sizeof(PPZ8Part));

    memset(&pmdwork, 0, sizeof(PMDWORK));
    memset(&effwork, 0, sizeof(EffectState));
    memset(&pcmends, 0, sizeof(pcmends));

    memset(wavbuf2, 0, sizeof(wavbuf2));
    memset(wavbuf, 0, sizeof(wavbuf2));
    memset(wavbuf_conv, 0, sizeof(wavbuf_conv));

    _PCMPtr = (char *) wavbuf2;
    
    _SamplesToDo = 0;
    _Position = 0;
    _FadeOutPosition = 0;
    seed = 0;

    memset(_MData, 0x00, sizeof(_MData));
    memset(_VData, 0x00, sizeof(_VData));
    memset(_EData, 0x00, sizeof(_EData));
    memset(&pcmends, 0x00, sizeof(pcmends));

    // Initialize OPEN_WORK.
    _OpenWork._OPNARate = SOUND_44K;
    _OpenWork._PPZ8Rate = SOUND_44K;
    _OpenWork.rhyvol = 0x3c;
    _OpenWork.fade_stop_flag = 0;
    _OpenWork.TimerBflag = 0;
    _OpenWork.TimerAflag = 0;
    _OpenWork.TimerB_speed = 0x100;
    _OpenWork.port22h = 0;

    _OpenWork._UseP86 = false;

    _OpenWork.ppz8ip = false;
    _OpenWork.p86ip = false;
    _OpenWork.ppsip = false;

    // Initialize variables.
    _OpenWork.MusPart[ 0] = &FMPart[0];
    _OpenWork.MusPart[ 1] = &FMPart[1];
    _OpenWork.MusPart[ 2] = &FMPart[2];
    _OpenWork.MusPart[ 3] = &FMPart[3];
    _OpenWork.MusPart[ 4] = &FMPart[4];
    _OpenWork.MusPart[ 5] = &FMPart[5];

    _OpenWork.MusPart[ 6] = &SSGPart[0];
    _OpenWork.MusPart[ 7] = &SSGPart[1];
    _OpenWork.MusPart[ 8] = &SSGPart[2];

    _OpenWork.MusPart[ 9] = &ADPCMPart;

    _OpenWork.MusPart[10] = &RhythmPart;

    _OpenWork.MusPart[11] = &ExtPart[0];
    _OpenWork.MusPart[12] = &ExtPart[1];
    _OpenWork.MusPart[13] = &ExtPart[2];

    _OpenWork.MusPart[14] = &DummyPart;
    _OpenWork.MusPart[15] = &EffPart;

    _OpenWork.MusPart[16] = &PPZ8Part[0];
    _OpenWork.MusPart[17] = &PPZ8Part[1];
    _OpenWork.MusPart[18] = &PPZ8Part[2];
    _OpenWork.MusPart[19] = &PPZ8Part[3];
    _OpenWork.MusPart[20] = &PPZ8Part[4];
    _OpenWork.MusPart[21] = &PPZ8Part[5];
    _OpenWork.MusPart[22] = &PPZ8Part[6];
    _OpenWork.MusPart[23] = &PPZ8Part[7];

    _MData[0] = 0;

    for (int i = 0; i < 12; ++i)
    {
        _MData[i * 2 + 1] = 0x18;
        _MData[i * 2 + 2] = 0x00;
    }

    _MData[25] = 0x80;

    _OpenWork.fm_voldown = fmvd_init;   // FM_VOLDOWN
    _OpenWork._fm_voldown = fmvd_init;  // FM_VOLDOWN

    _OpenWork.ssg_voldown = 0;          // SSG_VOLDOWN
    _OpenWork._ssg_voldown = 0;         // SSG_VOLDOWN

    _OpenWork.pcm_voldown = 0;          // PCM_VOLDOWN
    _OpenWork._pcm_voldown = 0;         // PCM_VOLDOWN

    _OpenWork.ppz_voldown = 0;          // PPZ_VOLDOWN
    _OpenWork._ppz_voldown = 0;         // PPZ_VOLDOWN

    _OpenWork.rhythm_voldown = 0;       // RHYTHM_VOLDOWN
    _OpenWork._rhythm_voldown = 0;      // RHYTHM_VOLDOWN

    _OpenWork.kp_rhythm_flag = false;   // Whether to play the Rhytmn Sound Source with SSGDRUM

    _OpenWork.rshot_bd = 0;             // Rhythm Sound Source shot inc flag (BD)
    _OpenWork.rshot_sd = 0;             // Rhythm Sound Source shot inc flag (SD)
    _OpenWork.rshot_sym = 0;            // Rhythm Sound Source shot inc flag (CYM)
    _OpenWork.rshot_hh = 0;             // Rhythm Sound Source shot inc flag (HH)
    _OpenWork.rshot_tom = 0;            // Rhythm Sound Source shot inc flag (TOM)
    _OpenWork.rshot_rim = 0;            // Rhythm Sound Source shot inc flag (RIM)

    _OpenWork.rdump_bd = 0;             // Rhythm Sound dump inc flag (BD)
    _OpenWork.rdump_sd = 0;             // Rhythm Sound dump inc flag (SD)
    _OpenWork.rdump_sym = 0;            // Rhythm Sound dump inc flag (CYM)
    _OpenWork.rdump_hh = 0;             // Rhythm Sound dump inc flag (HH)
    _OpenWork.rdump_tom = 0;            // Rhythm Sound dump inc flag (TOM)
    _OpenWork.rdump_rim = 0;            // Rhythm Sound dump inc flag (RIM)

    _OpenWork.pcm86_vol = 0;            // PCM volume adjustment
    _OpenWork._pcm86_vol = 0;           // PCM volume adjustment
    _OpenWork.fade_stop_flag = 1;       // MSTOP after FADEOUT FLAG

    pmdwork._UsePPS = false;        // PPSDRV FLAG
    pmdwork.music_flag = 0;

    // Set song data and timbre data storage addresses.
    _OpenWork.mmlbuf = &_MData[1];
    _OpenWork.tondat = _VData;
    _OpenWork.efcdat = _EData;

    // Initialize sound effects FMINT/EFCINT.
    effwork.effon = 0;
    effwork.psgefcnum = 0xff;
}

int PMD::MusicLoad(const uint8_t * musData, size_t musSize, WCHAR * currentDirectoryPath)
{
    int resultp1, resultp2, resultpps, resultz1, resultz2;
    char tempfilename[_MAX_PATH];
    WCHAR pcmfilename[_MAX_PATH];
    WCHAR ppsfilename[_MAX_PATH];
    WCHAR ppzfilename1[_MAX_PATH];
    WCHAR ppzfilename2[_MAX_PATH];
    WCHAR fullfilename[_MAX_PATH];
    bool ppsext;

    memset(tempfilename, 0, sizeof(tempfilename));
    memset(pcmfilename, 0, sizeof(pcmfilename));
    memset(ppsfilename, 0, sizeof(ppsfilename));
    memset(ppzfilename1, 0, sizeof(ppzfilename1));
    memset(ppzfilename2, 0, sizeof(ppzfilename2));
    memset(fullfilename, 0, sizeof(fullfilename));

    if (musSize > sizeof(_MData))
        return ERR_WRONG_MUSIC_FILE;

    // 020120 Header parsing only for Towns
    if ((musData[0] > 0x0F && musData[0] != 0xFF) || (musData[1] != 0x18 && musData[1] != 0x1A) || musData[2] != 0x00)
        return ERR_WRONG_MUSIC_FILE; //not PMD data

    Stop();

    ::memset(_MData, 0x00, sizeof(_MData));
    ::memcpy(_MData, musData, musSize);

    resultp1 = PMDWIN_OK;
    resultp2 = PMDWIN_OK;
    resultpps = PMDWIN_OK;
    resultz1 = PMDWIN_OK;
    resultz2 = PMDWIN_OK;

    // PPC/P86 reading
    GetNoteInternal(musData, musSize, 0, tempfilename);
    _FilePath.CharToTCHARn(pcmfilename, tempfilename, sizeof(tempfilename));

    if (*pcmfilename != '\0')
    {
        if (_FilePath.Comparepath(pcmfilename, _T(".P86"), FilePath::extractpath_ext) == 0)
        {
            // P86 -> PPC
            FindPCMSamples(fullfilename, pcmfilename, currentDirectoryPath);
            resultp1 = _P86->Load(fullfilename);

            if (resultp1 == _P86DRV_OK || resultp1 == _WARNING_P86_ALREADY_LOAD)
            {
                _OpenWork._UseP86 = true;
            }
            else
            {
                _FilePath.ExchangeExt(pcmfilename, pcmfilename, _T(".PPC"));

                FindPCMSamples(fullfilename, pcmfilename, currentDirectoryPath);
                resultp2 = LoadPPCInternal(fullfilename);

                if (resultp2 == PMDWIN_OK || resultp2 == WARNING_PPC_ALREADY_LOAD)
                {
                    _OpenWork._UseP86 = false;
                }
            }
        }
        else
        {
            // PPC -> P86
            FindPCMSamples(fullfilename, pcmfilename, currentDirectoryPath);

            resultp1 = LoadPPCInternal(fullfilename);

            if (resultp1 == PMDWIN_OK || resultp1 == WARNING_PPC_ALREADY_LOAD)
            {
                _OpenWork._UseP86 = false;
            }
            else
            {
                _FilePath.ExchangeExt(pcmfilename, pcmfilename, _T(".P86"));

                FindPCMSamples(fullfilename, pcmfilename, currentDirectoryPath);

                resultp2 = _P86->Load(fullfilename);

                if (resultp2 == _P86DRV_OK || resultp2 == _WARNING_P86_ALREADY_LOAD)
                {
                    _OpenWork._UseP86 = true;
                }
            }
        }
    }

    // PPS 読み込み
    ppsext = false;

    GetNoteInternal(musData, musSize, -1, tempfilename);
    _FilePath.CharToTCHARn(pcmfilename, tempfilename, sizeof(tempfilename));

    if (*pcmfilename != '\0')
    {
        // 拡張子を .PPS に変換する
        if (_FilePath.Comparepath(pcmfilename, _T(".PPS"), FilePath::extractpath_ext) == 0)
        {
            ppsext = true;
        }
        else
        {
            _FilePath.ExchangeExt(pcmfilename, pcmfilename, _T(".PPS"));
        }

        FindPCMSamples(fullfilename, pcmfilename, currentDirectoryPath);

        resultpps = _PPS->Load(fullfilename);

        if (resultpps == _ERR_OPEN_PPS_FILE && ppsext == false)
        {
            // ファイルが読めなかった場合、拡張子を元に戻して再オープンする
            FindPCMSamples(fullfilename, ppsfilename, currentDirectoryPath);
            resultpps = _PPS->Load(fullfilename);
        }
    }

    // 20010120 Ignore if TOWNS
    GetNoteInternal(musData, musSize, -2, tempfilename);
    _FilePath.CharToTCHARn(ppzfilename1, tempfilename, sizeof(tempfilename));

    if (*ppzfilename1 != '\0')
    {
        if (_FilePath.Comparepath(ppzfilename1, _T(".PMB"), FilePath::extractpath_ext) != 0 && musData[0] != 0xff)
        {
            // PPZ 読み込み
            if (*ppzfilename1 != '\0')
            {
                TCHAR * p = _FilePath.Strchr(ppzfilename1, ',');

                if (p == NULL)
                {	// １つのみ
                    if ((p = _FilePath.Strchr(ppzfilename1, '.')) == NULL)
                    {
                        _FilePath.ExchangeExt(ppzfilename1, ppzfilename1, _T(".PZI"));	// とりあえず pzi にする
                    }

                    FindPCMSamples(fullfilename, ppzfilename1, currentDirectoryPath);
                    resultz1 = _PPZ8->Load(fullfilename, 0);

                }
                else
                {
                    *p = '\0';

                    _FilePath.Strcpy(ppzfilename2, p + 1);

                    if ((p = _FilePath.Strchr(ppzfilename1, '.')) == NULL)
                        _FilePath.ExchangeExt(ppzfilename1, ppzfilename1, _T(".PZI"));	// とりあえず pzi にする

                    if ((p = _FilePath.Strchr(ppzfilename2, '.')) == NULL)
                        _FilePath.ExchangeExt(ppzfilename2, ppzfilename2, _T(".PZI"));	// とりあえず pzi にする

                    FindPCMSamples(fullfilename, ppzfilename1, currentDirectoryPath);
                    resultz1 = _PPZ8->Load(fullfilename, 0);

                    FindPCMSamples(fullfilename, ppzfilename2, currentDirectoryPath);
                    resultz2 = _PPZ8->Load(fullfilename, 1);
                }
            }
        }
    }

    switch (resultp1)
    {
        case PMDWIN_OK:
        case WARNING_PPC_ALREADY_LOAD:
        case _WARNING_P86_ALREADY_LOAD:

            switch (resultpps)
            {
                case _PPSDRV_OK:
                case _WARNING_PPS_ALREADY_LOAD:

                    switch (resultz1)
                    {
                        case PMDWIN_OK:
                        case _WARNING_PPZ_ALREADY_LOAD:

                            switch (resultz2)
                            {
                                case _PPZ8_OK:					return PMDWIN_OK;
                                case _WARNING_PPZ_ALREADY_LOAD:	return PMDWIN_OK;

                                case _ERR_OPEN_PPZ_FILE:		return ERR_OPEN_PPZ2_FILE;
                                case _ERR_WRONG_PPZ_FILE:		return ERR_WRONG_PPZ2_FILE;
                                case _ERR_OUT_OF_MEMORY:		return ERR_OUT_OF_MEMORY;
                                default:						return resultz2;
                            }

                        case _ERR_OPEN_PPZ_FILE:			return ERR_OPEN_PPZ1_FILE;
                        case _ERR_WRONG_PPZ_FILE:			return ERR_WRONG_PPZ1_FILE;
                        case _ERR_OUT_OF_MEMORY:			return ERR_OUT_OF_MEMORY;
                        default:							return resultz1;
                    }


                case _ERR_OPEN_PPS_FILE:					return ERR_OPEN_PPS_FILE;
                case _ERR_OUT_OF_MEMORY:					return ERR_OUT_OF_MEMORY;
                default:									return resultpps;
            }

        case ERR_OPEN_PPC_FILE:
        case ERR_WRONG_PPC_FILE:
        case ERR_OUT_OF_MEMORY:

        case _ERR_OPEN_P86_FILE:
        case _ERR_WRONG_P86_FILE:

            switch (resultp2)
            {
                case PMDWIN_OK:
                case WARNING_PPC_ALREADY_LOAD:
                case _WARNING_P86_ALREADY_LOAD:

                    switch (resultpps)
                    {
                        case _PPSDRV_OK:
                        case _WARNING_PPS_ALREADY_LOAD:

                            switch (resultz1)
                            {
                                case PMDWIN_OK:
                                case _WARNING_PPZ_ALREADY_LOAD:

                                    switch (resultz2)
                                    {
                                        case _PPZ8_OK:					return PMDWIN_OK;
                                        case _WARNING_PPZ_ALREADY_LOAD:	return PMDWIN_OK;

                                        case _ERR_OPEN_PPZ_FILE:		return ERR_OPEN_PPZ2_FILE;
                                        case _ERR_WRONG_PPZ_FILE:		return ERR_WRONG_PPZ2_FILE;
                                        case _ERR_OUT_OF_MEMORY:		return ERR_OUT_OF_MEMORY;
                                        default:						return resultz2;
                                    }

                                case _ERR_OPEN_PPZ_FILE:			return ERR_OPEN_PPZ1_FILE;
                                case _ERR_WRONG_PPZ_FILE:			return ERR_WRONG_PPZ1_FILE;
                                case _ERR_OUT_OF_MEMORY:			return ERR_OUT_OF_MEMORY;
                                default:							return resultz1;
                            }


                        case _ERR_OPEN_PPS_FILE:					return ERR_OPEN_PPS_FILE;
                        case _ERR_OUT_OF_MEMORY:					return ERR_OUT_OF_MEMORY;
                        default:									return resultpps;
                    }

                default:
                    switch (resultp1)
                    {
                        case _ERR_OPEN_P86_FILE:					return ERR_OPEN_P86_FILE;
                        case _ERR_WRONG_P86_FILE:					return ERR_WRONG_P86_FILE;
                        default:									return resultp1;
                    }
            }
    }

    return PMDWIN_OK;
}

bool PMD::GetLength(int * songLength, int * loopLength)
{
    mstart();

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
                TimerA_main();

            if (_OPNA->ReadStatus() & 0x02)
                TimerB_main();

            _OPNA->SetReg(0x27, _OpenWork.ch3mode | 0x30); // Timer Reset (Both timer A and B)

            int us = _OPNA->GetNextEvent();

            _OPNA->Count(us);
            _Position += us;
        }

        if ((_OpenWork._LoopCount == 1) && (*songLength == 0)) // When looping
        {
            *songLength = (int) (_Position / 1000);
        }
        else
        if (_OpenWork._LoopCount == -1) // End without loop
        {
            *songLength = (int) (_Position / 1000);
            *loopLength = 0;

            mstop();

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
    while (_OpenWork._LoopCount < 2);

    *loopLength = (int) (_Position / 1000) - *songLength;

    mstop();

    _OPNA->SetFMWait(FMWait);
    _OPNA->SetSSGWait(SSGWait);
    _OPNA->SetRhythmWait(RhythmWait);
    _OPNA->SetADPCMWait(ADPCMWait);

    return true;
}

// Gets song length (in events)
bool PMD::GetLengthInEvents(int * eventCount, int * loopEventCount)
{
    mstart();

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
                TimerA_main();

            if (_OPNA->ReadStatus() & 0x02)
                TimerB_main();

            _OPNA->SetReg(0x27, _OpenWork.ch3mode | 0x30);  // Timer Reset (Both timer A and B)

            int us = _OPNA->GetNextEvent();

            _OPNA->Count(us);
            _Position += us;
        }

        if ((_OpenWork._LoopCount == 1) && (*eventCount == 0)) // When looping
        {
            *eventCount = GetEventNumber();
        }
        else
        if (_OpenWork._LoopCount == -1) // End without loop
        {
            *eventCount = GetEventNumber();
            *loopEventCount = 0;

            mstop();

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
    while (_OpenWork._LoopCount < 2);

    *loopEventCount = GetEventNumber() - *eventCount;

    mstop();

    _OPNA->SetFMWait(FMWait);
    _OPNA->SetSSGWait(SSGWait);
    _OPNA->SetRhythmWait(RhythmWait);
    _OPNA->SetADPCMWait(ADPCMWait);

    return true;
}

// Gets the current loop number.
uint32_t PMD::GetLoopNumber()
{
    return _OpenWork._LoopCount;
}

// Gets the playback position (in ms)
uint32_t PMD::GetPosition()
{
    return (uint32_t) (_Position / 1000);
}

// Sets the playback position (in ms)
void PMD::SetPosition(int position)
{
    int64_t NewPosition = (int64_t) position * 1000; // (ms -> conversion to usec)

    if (_Position > NewPosition)
    {
        mstart();

        _PCMPtr = (char *) wavbuf2;    // Start position of remaining samples in buf

        _SamplesToDo = 0;           // Number of samples remaining in buf
        _Position = 0;              // Time from start of playing (μsec)
    }

    while (_Position < NewPosition)
    {
        if (_OPNA->ReadStatus() & 0x01)
            TimerA_main();

        if (_OPNA->ReadStatus() & 0x02)
            TimerB_main();

        _OPNA->SetReg(0x27, _OpenWork.ch3mode | 0x30); // Timer Reset (Both timer A and B)

        int us = _OPNA->GetNextEvent();

        _OPNA->Count(us);
        _Position += us;
    }

    if (_OpenWork._LoopCount == -1)
        silence();

    _OPNA->ClearBuffer();
}

// Renders a chunk of PCM data.
void PMD::MusicRender(int16_t * sampleData, int sampleCount)
{
    int	SamplesRendered = 0;

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

                _PCMPtr = (char *) wavbuf2;
                SamplesRendered += _SamplesToDo;
            }

            {
                if (_OPNA->ReadStatus() & 0x01)
                    TimerA_main();

                if (_OPNA->ReadStatus() & 0x02)
                    TimerB_main();

                _OPNA->SetReg(0x27, _OpenWork.ch3mode | 0x30); // Timer Reset (Both timer A and B)
            }

            int us = _OPNA->GetNextEvent(); // in microseconds

            {
                _SamplesToDo = (int) ((double) us * _OpenWork._OPNARate / 1000000.0);
                _OPNA->Count(us);

                ::memset(wavbuf, 0x00, _SamplesToDo * sizeof(Sample) * 2);

                if (_OpenWork._OPNARate == _OpenWork._PPZ8Rate)
                    _PPZ8->Mix((Sample *) wavbuf, _SamplesToDo);
                else
                {
                    // PCM frequency transform of ppz8 (no interpolation)
                    int ppzsample = _SamplesToDo * _OpenWork._PPZ8Rate / _OpenWork._OPNARate + 1;
                    int delta     = 8192         * _OpenWork._PPZ8Rate / _OpenWork._OPNARate;

                    ::memset(wavbuf_conv, 0, ppzsample * sizeof(Sample) * 2);

                    _PPZ8->Mix((Sample *) wavbuf_conv, ppzsample);

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
                _OPNA->Mix((Sample *) wavbuf, _SamplesToDo);

                if (pmdwork._UsePPS)
                    _PPS->Mix((Sample *) wavbuf, _SamplesToDo);

                if (_OpenWork._UseP86)
                    _P86->Mix((Sample *) wavbuf, _SamplesToDo);
            }

            {
                _Position += us;

                if (_OpenWork._FadeOutSpeedHQ > 0)
                {
                    int	ftemp = (_OpenWork._LoopCount != -1) ? (int) ((1 << 10) * ::pow(512, -(double) (_Position - _FadeOutPosition) / 1000 / _OpenWork._FadeOutSpeedHQ)) : 0;

                    for (int i = 0; i < _SamplesToDo; i++)
                    {
                        wavbuf2[i].left  = Limit(wavbuf[i].left  * ftemp >> 10, 32767, -32768);
                        wavbuf2[i].right = Limit(wavbuf[i].right * ftemp >> 10, 32767, -32768);
                    }

                    // Fadeout end
                    if ((_Position - _FadeOutPosition > (int64_t)_OpenWork._FadeOutSpeedHQ * 1000) && (_OpenWork.fade_stop_flag == 1))
                        pmdwork.music_flag |= 2;
                }
                else
                {
                    for (int i = 0; i < _SamplesToDo; i++)
                    {
                        wavbuf2[i].left  = Limit(wavbuf[i].left,  32767, -32768);
                        wavbuf2[i].right = Limit(wavbuf[i].right, 32767, -32768);
                    }
                }
            }
        }
    }
    while (SamplesRendered < sampleCount);
}
#pragma endregion

#pragma region(IFMPMD)
// Reload rhythm sound
bool PMD::LoadRythmSample(TCHAR * path)
{
    TCHAR path2[_MAX_PATH];

    _FilePath.Strcpy(path2, path);
    _FilePath.AddDelimiter(path2);

    Stop();

    return _OPNA->LoadRhythmSample(path2);
}

// Sets the PCM search directory
bool PMD::SetPaths(TCHAR ** pathList)
{
    int i = 0;

    while ((pathList[i] != nullptr) && (i < MAX_PCMDIR))
    {
        if (*pathList[i] == '\0')
            break;

        _FilePath.Strcpy(_OpenWork.pcmdir[i], pathList[i]);
        _FilePath.AddDelimiter(_OpenWork.pcmdir[i]);

        if (++i == MAX_PCMDIR)
        {
            _OpenWork.pcmdir[i][0] = '\0';
    
            return false;
        }
    }

    _OpenWork.pcmdir[i][0] = '\0';

    return true;
}

// Sets the synthesis frequency at which raw PCM data is generated (in Hz, for example 44100)
void PMD::SetSynthesisFrequency(int frequency)
{
    if (frequency == SOUND_55K || frequency == SOUND_55K_2)
    {
        _OpenWork._OPNARate      =
        _OpenWork._PPZ8Rate   = SOUND_44K;
        _OpenWork.fmcalc55k = true;
    }
    else
    {
        _OpenWork._OPNARate      =
        _OpenWork._PPZ8Rate   = frequency;
        _OpenWork.fmcalc55k = false;
    }

    _OPNA->SetRate(OPNAClock, _OpenWork._OPNARate, _OpenWork.fmcalc55k);
    _PPZ8->SetRate(_OpenWork._PPZ8Rate, _OpenWork.ppz8ip);
    _PPS->SetRate(_OpenWork._OPNARate, _OpenWork.ppsip);
    _P86->SetRate(_OpenWork._OPNARate, _OpenWork.p86ip);
}

// Sets the PPZ synthesis frequency.
void PMD::SetPPZSynthesisFrequency(int frequency)
{
    _OpenWork._PPZ8Rate = frequency;

    _PPZ8->SetRate(frequency, _OpenWork.ppz8ip);
}

//Enable 55kHz synthesis in FM primary interpolation.
void PMD::EnableFM55kHzSynthesis(bool flag)
{
    _OpenWork.fmcalc55k = flag;

    _OPNA->SetRate(OPNAClock, _OpenWork._OPNARate, _OpenWork.fmcalc55k);
}

// Enable PPZ8 primary completion.
void PMD::EnablePPZInterpolation(bool flag)
{
    _OpenWork.ppz8ip = flag;

    _PPZ8->SetRate(_OpenWork._PPZ8Rate, flag);
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
    _OpenWork.fadeout_speed = speed;
}

// Fade out (High quality sound)
void PMD::SetFadeOutDurationHQ(int speed)
{
    if (speed > 0)
    {
        if (_OpenWork._FadeOutSpeedHQ == 0)
            _FadeOutPosition = _Position;

        _OpenWork._FadeOutSpeedHQ = speed;
    }
    else
        _OpenWork._FadeOutSpeedHQ = 0; // Fadeout forced stop
}

// Sets the playback position (in ticks).
void PMD::SetEventNumber(int pos)
{
    if (_OpenWork.syousetu_lng * _OpenWork.syousetu + _OpenWork.opncount > pos)
    {
        mstart();

        _PCMPtr = (char *) wavbuf2; // Start position of remaining samples in buf
        _SamplesToDo = 0; // Number of samples remaining in buf
    }

    while (_OpenWork.syousetu_lng * _OpenWork.syousetu + _OpenWork.opncount < pos)
    {
        if (_OPNA->ReadStatus() & 0x01)
            TimerA_main();

        if (_OPNA->ReadStatus() & 0x02)
            TimerB_main();

        _OPNA->SetReg(0x27, _OpenWork.ch3mode | 0x30); // Timer Reset (Both timer A and B)

        int us = _OPNA->GetNextEvent();
        _OPNA->Count(us);
    }

    if (_OpenWork._LoopCount == -1)
        silence();

    _OPNA->ClearBuffer();
}

// Gets the playback position (in ticks)
int PMD::GetEventNumber(void)
{
    return _OpenWork.syousetu_lng * _OpenWork.syousetu + _OpenWork.opncount;
}

// Gets PPC / P86 filename.
TCHAR * PMD::GetPCMFileName(TCHAR * filePath)
{
    if (_OpenWork._UseP86)
        _FilePath.Strcpy(filePath, _P86->p86_file);
    else
        _FilePath.Strcpy(filePath, _OpenWork.ppcfilename);

    return filePath;
}

// Gets PPZ filename.
TCHAR * PMD::GetPPZFileName(TCHAR * filePath, int index)
{
    _FilePath.Strcpy(filePath, _PPZ8->PVI_FILE[index]);

    return filePath;
}
#pragma endregion

#pragma region(IPMDWin)
// Ring PPS?
void PMD::EnablePPS(bool flag)
{
    pmdwork._UsePPS = flag;
}

// Do you want to play OPNA Rhythm with SSG sound effects?
void PMD::EnablePlayRythmWithSSG(bool flag)
{
    _OpenWork.kp_rhythm_flag = flag;
}

// Make PMD86 PCM compatible with PMDB2?
void PMD::EnablePMDB2CompatibilityMode(bool value)
{
    if (value)
    {
        _OpenWork.pcm86_vol =
        _OpenWork._pcm86_vol = 1;
    }
    else
    {
        _OpenWork.pcm86_vol =
        _OpenWork._pcm86_vol = 0;
    }
}

// Get whether PMD86's PCM is PMDB2 compatible
bool PMD::GetPMDB2CompatibilityMode(void)
{
    return _OpenWork.pcm86_vol ? true : false;
}

//	PPS で一次補完するかどうかの設定
void PMD::setppsinterpolation(bool flag)
{
    _OpenWork.ppsip = flag;
    _PPS->SetRate(_OpenWork._OPNARate, flag);
}

//	P86 で一次補完するかどうかの設定
void PMD::setp86interpolation(bool flag)
{
    _OpenWork.p86ip = flag;
    _P86->SetRate(_OpenWork._OPNARate, flag);
}

//	パートのマスク
int PMD::maskon(int ch)
{
    int ah, fmseltmp;

    if (ch >= sizeof(_OpenWork.MusPart) / sizeof(PartState *))
        return ERR_WRONG_PARTNO;		// part number error

    if (part_table[ch][0] < 0)
    {
        _OpenWork.rhythmmask = 0;	// Rhythm音源をMask
        _OPNA->SetReg(0x10, 0xff);	// Rhythm音源を全部Dump
    }
    else
    {
        fmseltmp = pmdwork.fmsel;

        if ((_OpenWork.MusPart[ch]->partmask == 0) && (_OpenWork._IsPlaying != 0))
        {
            if (part_table[ch][2] == 0)
            {
                pmdwork.partb = part_table[ch][1];
                pmdwork.fmsel = 0;
                silence_fmpart(_OpenWork.MusPart[ch]);	// 音を完璧に消す
            }
            else
            if (part_table[ch][2] == 1)
            {
                pmdwork.partb = part_table[ch][1];
                pmdwork.fmsel = 0x100;
                silence_fmpart(_OpenWork.MusPart[ch]);	// 音を完璧に消す
            }
            else
            if (part_table[ch][2] == 2)
            {
                pmdwork.partb = part_table[ch][1];
                ah = 1 << (pmdwork.partb - 1);
                ah |= (ah << 3);
                // PSG keyoff
                _OPNA->SetReg(0x07, ah | _OPNA->GetReg(0x07));
            }
            else
            if (part_table[ch][2] == 3)
            {
                _OPNA->SetReg(0x101, 0x02);		// PAN=0 / x8 bit mode
                _OPNA->SetReg(0x100, 0x01);		// PCM RESET
            }
            else
            if (part_table[ch][2] == 4)
            {
                if (effwork.psgefcnum < 11)
                {
                    effend();
                }
            }
            else
            if (part_table[ch][2] == 5)
            {
                _PPZ8->Stop(part_table[ch][1]);
            }
        }

        _OpenWork.MusPart[ch]->partmask |= 1;
        pmdwork.fmsel = fmseltmp;
    }

    return PMDWIN_OK;
}

//	パートのマスク解除
int PMD::maskoff(int ch)
{
    int fmseltmp;

    if (ch >= sizeof(_OpenWork.MusPart) / sizeof(PartState *))
    {
        return ERR_WRONG_PARTNO;		// part number error
    }

    if (part_table[ch][0] < 0)
    {
        _OpenWork.rhythmmask = 0xff;
    }
    else
    {
        if (_OpenWork.MusPart[ch]->partmask == 0)
            return ERR_NOT_MASKED;	// マスクされていない
        // 効果音でまだマスクされている

        if ((_OpenWork.MusPart[ch]->partmask &= 0xfe) != 0)
            return ERR_EFFECT_USED;

        // The song has stopped.
        if (!_OpenWork._IsPlaying)
            return ERR_MUSIC_STOPPED;

        fmseltmp = pmdwork.fmsel;
        if (_OpenWork.MusPart[ch]->address != NULL)
        {
            if (part_table[ch][2] == 0)
            {		// FM音源(表)
                pmdwork.fmsel = 0;
                pmdwork.partb = part_table[ch][1];
                neiro_reset(_OpenWork.MusPart[ch]);
            }
            else if (part_table[ch][2] == 1)
            {	// FM音源(裏)
                pmdwork.fmsel = 0x100;
                pmdwork.partb = part_table[ch][1];
                neiro_reset(_OpenWork.MusPart[ch]);
            }
        }
        pmdwork.fmsel = fmseltmp;

    }
    return PMDWIN_OK;
}

//	FM Volume Down の設定
void PMD::setfmvoldown(int voldown)
{
    _OpenWork.fm_voldown = _OpenWork._fm_voldown = voldown;
}

//	SSG Volume Down の設定
void PMD::setssgvoldown(int voldown)
{
    _OpenWork.ssg_voldown = _OpenWork._ssg_voldown = voldown;
}

//	Rhythm Volume Down の設定
void PMD::setrhythmvoldown(int voldown)
{
    _OpenWork.rhythm_voldown = _OpenWork._rhythm_voldown = voldown;
    _OpenWork.rhyvol = 48 * 4 * (256 - _OpenWork.rhythm_voldown) / 1024;
    _OPNA->SetReg(0x11, _OpenWork.rhyvol);
}

//	ADPCM Volume Down の設定
void PMD::setadpcmvoldown(int voldown)
{
    _OpenWork.pcm_voldown = _OpenWork._pcm_voldown = voldown;
}

//	PPZ8 Volume Down の設定
void PMD::setppzvoldown(int voldown)
{
    _OpenWork.ppz_voldown = _OpenWork._ppz_voldown = voldown;
}

//	FM Volume Down の取得
int PMD::getfmvoldown(void)
{
    return _OpenWork.fm_voldown;
}

//	FM Volume Down の取得（その２）
int PMD::getfmvoldown2(void)
{
    return _OpenWork._fm_voldown;
}

//	SSG Volume Down の取得
int PMD::getssgvoldown(void)
{
    return _OpenWork.ssg_voldown;
}

//	SSG Volume Down の取得（その２）
int PMD::getssgvoldown2(void)
{
    return _OpenWork._ssg_voldown;
}

//	Rhythm Volume Down の取得
int PMD::getrhythmvoldown(void)
{
    return _OpenWork.rhythm_voldown;
}

//	Rhythm Volume Down の取得（その２）
int PMD::getrhythmvoldown2(void)
{
    return _OpenWork._rhythm_voldown;
}

//	ADPCM Volume Down の取得
int PMD::getadpcmvoldown(void)
{
    return _OpenWork.pcm_voldown;
}

//	ADPCM Volume Down の取得（その２）
int PMD::getadpcmvoldown2(void)
{
    return _OpenWork._pcm_voldown;
}

//	PPZ8 Volume Down の取得
int PMD::getppzvoldown(void)
{
    return _OpenWork.ppz_voldown;
}

//	PPZ8 Volume Down の取得（その２）
int PMD::getppzvoldown2(void)
{
    return _OpenWork._ppz_voldown;
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
        Data = _OpenWork.mmlbuf;
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

    int i, dx;

    for (i = 0; i <= index; i++)
    {
        if (Size < Src - Data + 1)
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

        if (Size < dx)
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

//	PPC filename の取得
TCHAR * PMD::getppcfilename(TCHAR * dest)
{
    _FilePath.Strcpy(dest, _OpenWork.ppcfilename);
    return dest;
}

//	PPS filename の取得
TCHAR * PMD::getppsfilename(TCHAR * dest)
{
    _FilePath.Strcpy(dest, _PPS->pps_file);
    return dest;
}

//	P86 filename の取得
TCHAR * PMD::getp86filename(TCHAR * dest)
{
    _FilePath.Strcpy(dest, _P86->p86_file);

    return dest;
}

// Load PPC
int PMD::LoadPPC(TCHAR * filename)
{
    Stop();

    int Result = LoadPPCInternal(filename);

    if (Result == PMDWIN_OK || Result == WARNING_PPC_ALREADY_LOAD)
        _OpenWork._UseP86 = false;

    return Result;
}

// Load PPS
int PMD::LoadPPS(TCHAR * filename)
{
    Stop();

    int Result = _PPS->Load(filename);

    switch (Result)
    {
        case _PPSDRV_OK:                return PMDWIN_OK;
        case _ERR_OPEN_PPS_FILE:        return ERR_OPEN_PPS_FILE;
        case _WARNING_PPS_ALREADY_LOAD: return WARNING_PPS_ALREADY_LOAD;
        case _ERR_OUT_OF_MEMORY:        return ERR_OUT_OF_MEMORY;
        default:                        return ERR_OTHER;
    }
}

// Load P86
int PMD::LoadP86(TCHAR * filename)
{
    Stop();

    int Result = _P86->Load(filename);

    if (Result == _P86DRV_OK || Result == _WARNING_P86_ALREADY_LOAD)
        _OpenWork._UseP86 = true;

    switch (Result)
    {
        case _P86DRV_OK:                return PMDWIN_OK;
        case _ERR_OPEN_P86_FILE:        return ERR_OPEN_P86_FILE;
        case _ERR_WRONG_P86_FILE:       return ERR_WRONG_P86_FILE;
        case _WARNING_P86_ALREADY_LOAD: return WARNING_P86_ALREADY_LOAD;
        case _ERR_OUT_OF_MEMORY:        return ERR_OUT_OF_MEMORY;
        default:                        return ERR_OTHER;
    }
}

// Load .PZI, .PVI
int PMD::LoadPPZ(TCHAR * filename, int bufnum)
{
    Stop();

    int Result = _PPZ8->Load(filename, bufnum);

    switch (Result)
    {
        case _PPZ8_OK:                  return PMDWIN_OK;
        case _ERR_OPEN_PPZ_FILE:        return bufnum ? ERR_OPEN_PPZ2_FILE : ERR_OPEN_PPZ1_FILE;
        case _ERR_WRONG_PPZ_FILE:       return bufnum ? ERR_WRONG_PPZ2_FILE : ERR_WRONG_PPZ1_FILE;
        case _WARNING_PPZ_ALREADY_LOAD: return bufnum ? WARNING_PPZ2_ALREADY_LOAD : WARNING_PPZ1_ALREADY_LOAD;
        case _ERR_OUT_OF_MEMORY:        return ERR_OUT_OF_MEMORY;
        default:                        return ERR_OTHER;
    }
}

// Get the OPEN_WORK pointer
OPEN_WORK * PMD::GetOpenWork(void)
{
    return &_OpenWork;
}

// Get part work pointer
PartState * PMD::GetOpenPartWork(int ch)
{
    if (ch >= sizeof(_OpenWork.MusPart) / sizeof(PartState *))
        return NULL;

    return _OpenWork.MusPart[ch];
}

//	File Stream 設定
void PMD::setfileio(IFILEIO * pfileio)
{
    if (pfileio == NULL)
        pfileio = fileio;

    this->pfileio = pfileio;

    _OPNA->setfileio(this->pfileio);
    _PPZ8->setfileio(this->pfileio);
    _PPS->setfileio(this->pfileio);
    _P86->setfileio(this->pfileio);
}
#pragma endregion

// Timer A processing (main)
void PMD::TimerA_main(void)
{
    _OpenWork.TimerAflag = 1;
    _OpenWork.TimerAtime++;

    if ((_OpenWork.TimerAtime & 7) == 0)
        fout();

    if (effwork.effon && (pmdwork._UsePPS == false || effwork.psgefcnum == 0x80))
        effplay(); // SSG sound effect processing

    _OpenWork.TimerAflag = 0;
}

// Timer B processing (main)
void PMD::TimerB_main(void)
{
    _OpenWork.TimerBflag = 1;

    if (pmdwork.music_flag)
    {
        if (pmdwork.music_flag & 1)
            mstart();

        if (pmdwork.music_flag & 2)
            mstop();
    }

    if (_OpenWork._IsPlaying)
    {
        mmain();
        settempo_b();
        syousetu_count();

        pmdwork.lastTimerAtime = _OpenWork.TimerAtime;
    }

    _OpenWork.TimerBflag = 0;
}

// MUSIC PLAYER MAIN [FROM TIMER-B]
void PMD::mmain(void)
{
    int i;

    pmdwork.loop_work = 3;

    if (_OpenWork.x68_flg == 0)
    {
        for (i = 0; i < 3; i++)
        {
            pmdwork.partb = i + 1;
            psgmain(&SSGPart[i]);
        }
    }

    pmdwork.fmsel = 0x100;

    for (i = 0; i < 3; i++)
    {
        pmdwork.partb = i + 1;
        fmmain(&FMPart[i + 3]);
    }

    pmdwork.fmsel = 0;

    for (i = 0; i < 3; i++)
    {
        pmdwork.partb = i + 1;
        fmmain(&FMPart[i]);
    }

    for (i = 0; i < 3; i++)
    {
        pmdwork.partb = 3;
        fmmain(&ExtPart[i]);
    }

    if (_OpenWork.x68_flg == 0)
    {
        rhythmmain(&RhythmPart);

        if (_OpenWork._UseP86)
            pcm86main(&ADPCMPart);
        else
            adpcmmain(&ADPCMPart);
    }

    if (_OpenWork.x68_flg != 0xff)
    {
        for (i = 0; i < 8; i++)
        {
            pmdwork.partb = i;
            ppz8main(&PPZ8Part[i]);
        }
    }

    if (pmdwork.loop_work == 0) return;

    for (i = 0; i < 6; i++)
    {
        if (FMPart[i].loopcheck != 3)
            FMPart[i].loopcheck = 0;
    }

    for (i = 0; i < 3; i++)
    {
        if (SSGPart[i].loopcheck != 3)
            SSGPart[i].loopcheck = 0;

        if (ExtPart[i].loopcheck != 3)
            ExtPart[i].loopcheck = 0;
    }

    if (ADPCMPart.loopcheck != 3)
        ADPCMPart.loopcheck = 0;

    if (RhythmPart.loopcheck != 3)
        RhythmPart.loopcheck = 0;

    if (EffPart.loopcheck != 3)
        EffPart.loopcheck = 0;

    for (i = 0; i < PCM_CNL_MAX; i++)
    {
        if (PPZ8Part[i].loopcheck != 3)
            PPZ8Part[i].loopcheck = 0;
    }

    if (pmdwork.loop_work != 3)
    {
        _OpenWork._LoopCount++;

        if (_OpenWork._LoopCount == 255)
            _OpenWork._LoopCount = 1;
    }
    else
        _OpenWork._LoopCount = -1;
}

// FM sound source performance main
void PMD::fmmain(PartState * qq)
{
    uint8_t * si;

    if (qq->address == NULL)
        return;

    si = qq->address;

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
                si = commands(qq, si);
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
                        pmdwork.tieflag = 0;
                        pmdwork.volpush_flag = 0;
                        pmdwork.loop_work &= qq->loopcheck;

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
                    pmdwork.loop_work &= qq->loopcheck;
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
                        if (--pmdwork.volpush_flag)
                        {
                            pmdwork.volpush_flag = 0;
                            qq->volpush = 0;
                        }
                    }

                    volset(qq);
                    otodasi(qq);
                    keyon(qq);

                    qq->keyon_flag++;
                    qq->address = si;

                    pmdwork.tieflag = 0;
                    pmdwork.volpush_flag = 0;

                    if (*si == 0xfb)
                    {   // '&'が直後にあったらkeyoffしない
                        qq->keyoff_flag = 2;
                    }
                    else
                    {
                        qq->keyoff_flag = 0;
                    }
                    pmdwork.loop_work &= qq->loopcheck;
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

                    if (--pmdwork.volpush_flag)
                    {
                        qq->volpush = 0;
                    }

                    pmdwork.tieflag = 0;
                    pmdwork.volpush_flag = 0;
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
                _OPNA->SetReg(pmdwork.fmsel + (pmdwork.partb - 1 + 0xb4), qq->fmpan);
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
            pmdwork.lfo_switch = qq->lfoswi & 8;

            if (qq->lfoswi & 3)
            {
                if (lfo(qq))
                {
                    pmdwork.lfo_switch |= (qq->lfoswi & 3);
                }
            }

            if (qq->lfoswi & 0x30)
            {
                lfo_change(qq);

                if (lfo(qq))
                {
                    lfo_change(qq);

                    pmdwork.lfo_switch |= (qq->lfoswi & 0x30);
                }
                else
                {
                    lfo_change(qq);
                }
            }

            if (pmdwork.lfo_switch & 0x19)
            {
                if (pmdwork.lfo_switch & 8)
                {
                    porta_calc(qq);

                }

                otodasi(qq);
            }

            if (pmdwork.lfo_switch & 0x22)
            {
                volset(qq);
                pmdwork.loop_work &= qq->loopcheck;

                return;
            }
        }

        if (_OpenWork.fadeout_speed != 0)
            volset(qq);
    }

    pmdwork.loop_work &= qq->loopcheck;
}

//	KEY OFF
void PMD::keyoff(PartState * qq)
{
    if (qq->onkai == 255)
        return;

    kof1(qq);
}

void PMD::kof1(PartState * qq)
{
    if (pmdwork.fmsel == 0)
    {
        pmdwork.omote_key[pmdwork.partb - 1] = (~qq->slotmask) & (pmdwork.omote_key[pmdwork.partb - 1]);
        _OPNA->SetReg(0x28, (pmdwork.partb - 1) | pmdwork.omote_key[pmdwork.partb - 1]);
    }
    else
    {
        pmdwork.ura_key[pmdwork.partb - 1] = (~qq->slotmask) & (pmdwork.ura_key[pmdwork.partb - 1]);
        _OPNA->SetReg(0x28, ((pmdwork.partb - 1) | pmdwork.ura_key[pmdwork.partb - 1]) | 4);
    }
}

// FM Key On
void PMD::keyon(PartState * qq)
{
    int	al;

    if (qq->onkai == 255)
        return; // ｷｭｳﾌ ﾉ ﾄｷ

    if (pmdwork.fmsel == 0)
    {
        al = pmdwork.omote_key[pmdwork.partb - 1] | qq->slotmask;

        if (qq->sdelay_c)
            al &= qq->sdelay_m;

        pmdwork.omote_key[pmdwork.partb - 1] = al;
        _OPNA->SetReg(0x28, (pmdwork.partb - 1) | al);
    }
    else
    {
        al = pmdwork.ura_key[pmdwork.partb - 1] | qq->slotmask;

        if (qq->sdelay_c)
            al &= qq->sdelay_m;

        pmdwork.ura_key[pmdwork.partb - 1] = al;
        _OPNA->SetReg(0x28, ((pmdwork.partb - 1) | al) | 4);
    }
}

//	Set [ FNUM/BLOCK + DETUNE + LFO ]
void PMD::otodasi(PartState * qq)
{
    int		ax, cx;

    if (qq->fnum == 0) return;
    if (qq->slotmask == 0) return;

    cx = (qq->fnum & 0x3800);		// cx=BLOCK
    ax = (qq->fnum) & 0x7ff;		// ax=FNUM

    // Portament/LFO/Detune SET
    ax += qq->porta_num + qq->detune;

    if (pmdwork.partb == 3 && pmdwork.fmsel == 0 && _OpenWork.ch3mode != 0x3f)
    {
        ch3_special(qq, ax, cx);
    }
    else
    {
        if (qq->lfoswi & 1)
        {
            ax += qq->lfodat;
        }

        if (qq->lfoswi & 0x10)
        {
            ax += qq->_lfodat;
        }

        fm_block_calc(&cx, &ax);

        // SET BLOCK/FNUM TO OPN

        ax |= cx;

        _OPNA->SetReg(pmdwork.fmsel + pmdwork.partb + 0xa4 - 1, ax >> 8);
        _OPNA->SetReg(pmdwork.fmsel + pmdwork.partb + 0xa4 - 5, ax & 0xff);
    }
}

//	FM音源のdetuneでオクターブが変わる時の修正
//		input	CX:block / AX:fnum+detune
//		output	CX:block / AX:fnum
void PMD::fm_block_calc(int * cx, int * ax)
{
    while (*ax >= 0x26a)
    {
        if (*ax < (0x26a * 2)) return;

        *cx += 0x800;			// oct.up
        if (*cx != 0x4000)
        {
            *ax -= 0x26a;		// 4d2h-26ah
        }
        else
        {				// ﾓｳ ｺﾚｲｼﾞｮｳ ｱｶﾞﾝﾅｲﾖﾝ
            *cx = 0x3800;
            if (*ax >= 0x800)
                *ax = 0x7ff;	// 4d2h
            return;
        }
    }

    while (*ax < 0x26a)
    {
        *cx -= 0x800;			// oct.down
        if (*cx >= 0)
        {
            *ax += 0x26a;		// 4d2h-26ah
        }
        else
        {				// ﾓｳ ｺﾚｲｼﾞｮｳ ｻｶﾞﾝﾅｲﾖﾝ
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
int PMD::ch3_setting(PartState * qq)
{
    if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
    {
        ch3mode_set(qq);

        return 1;
    }

    return 0;
}

void PMD::cm_clear(int * ah, int * al)
{
    *al ^= 0xff;

    if ((pmdwork.slot3_flag &= *al) == 0)
    {
        if (pmdwork.slotdetune_flag != 1)
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

//	FM3のmodeを設定する
void PMD::ch3mode_set(PartState * qq)
{
    int		ah, al;

    if (qq == &FMPart[3 - 1])
    {
        al = 1;
    }
    else if (qq == &ExtPart[0])
    {
        al = 2;
    }
    else if (qq == &ExtPart[1])
    {
        al = 4;
    }
    else
    {
        al = 8;
    }

    if ((qq->slotmask & 0xf0) == 0)
    {	// s0
        cm_clear(&ah, &al);
    }
    else if (qq->slotmask != 0xf0)
    {
        pmdwork.slot3_flag |= al;
        ah = 0x7f;
    }
    else if ((qq->volmask & 0x0f) == 0)
    {
        cm_clear(&ah, &al);
    }
    else if ((qq->lfoswi & 1) != 0)
    {
        pmdwork.slot3_flag |= al;
        ah = 0x7f;
    }
    else if ((qq->_volmask & 0x0f) == 0)
    {
        cm_clear(&ah, &al);
    }
    else if (qq->lfoswi & 0x10)
    {
        pmdwork.slot3_flag |= al;
        ah = 0x7f;
    }
    else
    {
        cm_clear(&ah, &al);
    }

    if (ah == _OpenWork.ch3mode) return;		// 以前と変更無しなら何もしない
    _OpenWork.ch3mode = ah;
    _OPNA->SetReg(0x27, ah & 0xcf);			// Resetはしない

    //	効果音モードに移った場合はそれ以前のFM3パートで音程書き換え
    if (ah == 0x3f || qq == &FMPart[2]) return;

    if (FMPart[2].partmask == 0) otodasi(&FMPart[2]);
    if (qq == &ExtPart[0]) return;
    if (ExtPart[0].partmask == 0) otodasi(&ExtPart[0]);
    if (qq == &ExtPart[1]) return;
    if (ExtPart[1].partmask == 0) otodasi(&ExtPart[1]);
}

//	ch3=効果音モード を使用する場合の音程設定
//			input CX:block AX:fnum
void PMD::ch3_special(PartState * qq, int ax, int cx)
{
    int		ax_, bh, ch, si;
    int		shiftmask = 0x80;

    si = cx;

    if ((qq->volmask & 0x0f) == 0)
    {
        bh = 0xf0;			// all
    }
    else
    {
        bh = qq->volmask;	// bh=lfo1 mask 4321xxxx
    }

    if ((qq->_volmask & 0x0f) == 0)
    {
        ch = 0xf0;			// all
    }
    else
    {
        ch = qq->_volmask;	// ch=lfo2 mask 4321xxxx
    }

    //	slot	4
    if (qq->slotmask & 0x80)
    {
        ax_ = ax;
        ax += _OpenWork.slot_detune4;
        if ((bh & shiftmask) && (qq->lfoswi & 0x01))	ax += qq->lfodat;
        if ((ch & shiftmask) && (qq->lfoswi & 0x10))	ax += qq->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNA->SetReg(0xa6, ax >> 8);
        _OPNA->SetReg(0xa2, ax & 0xff);

        ax = ax_;
    }

    //	slot	3
    if (qq->slotmask & 0x40)
    {
        ax_ = ax;
        ax += _OpenWork.slot_detune3;
        if ((bh & shiftmask) && (qq->lfoswi & 0x01))	ax += qq->lfodat;
        if ((ch & shiftmask) && (qq->lfoswi & 0x10))	ax += qq->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNA->SetReg(0xac, ax >> 8);
        _OPNA->SetReg(0xa8, ax & 0xff);

        ax = ax_;
    }

    //	slot	2
    if (qq->slotmask & 0x20)
    {
        ax_ = ax;
        ax += _OpenWork.slot_detune2;
        if ((bh & shiftmask) && (qq->lfoswi & 0x01))	ax += qq->lfodat;
        if ((ch & shiftmask) && (qq->lfoswi & 0x10))	ax += qq->_lfodat;
        shiftmask >>= 1;

        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNA->SetReg(0xae, ax >> 8);
        _OPNA->SetReg(0xaa, ax & 0xff);

        ax = ax_;
    }

    //	slot	1
    if (qq->slotmask & 0x10)
    {
        ax_ = ax;
        ax += _OpenWork.slot_detune1;
        if ((bh & shiftmask) && (qq->lfoswi & 0x01))	ax += qq->lfodat;
        if ((ch & shiftmask) && (qq->lfoswi & 0x10))	ax += qq->_lfodat;
        cx = si;
        fm_block_calc(&cx, &ax);
        ax |= cx;

        _OPNA->SetReg(0xad, ax >> 8);
        _OPNA->SetReg(0xa9, ax & 0xff);

        ax = ax_;
    }
}

//	'p' COMMAND [FM PANNING SET]
uint8_t * PMD::panset(PartState * qq, uint8_t * si)
{
    panset_main(qq, *si++);
    return si;
}

void PMD::panset_main(PartState * qq, int al)
{
    qq->fmpan = (qq->fmpan & 0x3f) | ((al << 6) & 0xc0);

    if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
    {
        //	FM3の場合は 4つのパート総て設定
        FMPart[2].fmpan = qq->fmpan;
        ExtPart[0].fmpan = qq->fmpan;
        ExtPart[1].fmpan = qq->fmpan;
        ExtPart[2].fmpan = qq->fmpan;
    }

    if (qq->partmask == 0)
    {		// パートマスクされているか？
// dl = al;
        _OPNA->SetReg(pmdwork.fmsel + pmdwork.partb + 0xb4 - 1,
            calc_panout(qq));
    }
}

//	0b4h?に設定するデータを取得 out.dl
uint8_t PMD::calc_panout(PartState * qq)
{
    int	dl;

    dl = qq->fmpan;
    if (qq->hldelay_c) dl &= 0xc0;	// HLFO Delayが残ってる場合はパンのみ設定
    return dl;
}

//	Pan setting Extend
uint8_t * PMD::panset_ex(PartState * qq, uint8_t * si)
{
    int		al;

    al = *(int8_t *) si++;
    si++;		// 逆走flagは読み飛ばす

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

//	Pan setting Extend
uint8_t * PMD::panset8_ex(PartState * qq, uint8_t * si)
{
    int		flag, data;

    qq->fmpan = (int8_t) *si++;
    _OpenWork.revpan = *si++;


    if (qq->fmpan == 0)
    {
        flag = 3;				// Center
        data = 0;
    }
    else if (qq->fmpan > 0)
    {
        flag = 2;				// Right
        data = 128 - qq->fmpan;
    }
    else
    {
        flag = 1;				// Left
        data = 128 + qq->fmpan;
    }

    if (_OpenWork.revpan != 1)
    {
        flag |= 4;				// 逆相
    }
    _P86->SetPan(flag, data);

    return si;
}

//	ＦＭ音源用　Entry
int PMD::lfoinit(PartState * qq, int al)
{
    int		ah;

    ah = al & 0x0f;
    if (ah == 0x0c)
    {
        al = qq->onkai_def;
        ah = al & 0x0f;
    }

    qq->onkai_def = al;

    if (ah == 0x0f)
    {				// ｷｭｰﾌ ﾉ ﾄｷ ﾊ INIT ｼﾅｲﾖ
        lfo_exit(qq);
        return al;
    }

    qq->porta_num = 0;				// ポルタメントは初期化

    if ((pmdwork.tieflag & 1) == 0)
    {
        lfin1(qq);
    }
    else
    {
        lfo_exit(qq);
    }
    return al;
}

//	ＦＭ　BLOCK,F-NUMBER SET
//		INPUTS	-- AL [KEY#,0-7F]
void PMD::fnumset(PartState * qq, int al)
{
    int		ax, bx;

    if ((al & 0x0f) != 0x0f)
    {		// 音符の場合
        qq->onkai = al;

        // BLOCK/FNUM CALICULATE
        bx = al & 0x0f;		// bx=onkai
        ax = fnum_data[bx];

        // BLOCK SET
        ax |= (((al >> 1) & 0x38) << 8);
        qq->fnum = ax;
    }
    else
    {						// 休符の場合
        qq->onkai = 255;
        if ((qq->lfoswi & 0x11) == 0)
        {
            qq->fnum = 0;			// 音程LFO未使用
        }
    }
}

//	ＦＭ音量設定メイン
void PMD::volset(PartState * qq)
{
    int bh, bl, cl, dh;
    uint8_t vol_tbl[4] = { 0, 0, 0, 0 };

    if (qq->slotmask == 0) return;
    if (qq->volpush)
    {
        cl = qq->volpush - 1;
    }
    else
    {
        cl = qq->volume;
    }

    if (qq != &EffPart)
    {	// 効果音の場合はvoldown/fadeout影響無し
//--------------------------------------------------------------------
//	音量down計算
//--------------------------------------------------------------------
        if (_OpenWork.fm_voldown)
        {
            cl = ((256 - _OpenWork.fm_voldown) * cl) >> 8;
        }

        //--------------------------------------------------------------------
        //	Fadeout計算
        //--------------------------------------------------------------------
        if (_OpenWork.fadeout_volume >= 2)
        {
            cl = ((256 - (_OpenWork.fadeout_volume >> 1)) * cl) >> 8;
        }
    }

    //------------------------------------------------------------------------
    //	音量をcarrierに設定 & 音量LFO処理
    //		input	cl to Volume[0-127]
    //			bl to SlotMask
    //------------------------------------------------------------------------

    bh = 0;					// Vol Slot Mask
    bl = qq->slotmask;		// ch=SlotMask Push

    vol_tbl[0] = 0x80;
    vol_tbl[1] = 0x80;
    vol_tbl[2] = 0x80;
    vol_tbl[3] = 0x80;

    cl = 255 - cl;			// cl=carrierに設定する音量+80H(add)
    bl &= qq->carrier;		// bl=音量を設定するSLOT xxxx0000b
    bh |= bl;

    if (bl & 0x80) vol_tbl[0] = cl;
    if (bl & 0x40) vol_tbl[1] = cl;
    if (bl & 0x20) vol_tbl[2] = cl;
    if (bl & 0x10) vol_tbl[3] = cl;

    if (cl != 255)
    {
        if (qq->lfoswi & 2)
        {
            bl = qq->volmask;
            bl &= qq->slotmask;		// bl=音量LFOを設定するSLOT xxxx0000b
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

    dh = 0x4c - 1 + pmdwork.partb;		// dh=FM Port Address

    if (bh & 0x80) volset_slot(dh, qq->slot4, vol_tbl[0]);
    if (bh & 0x40) volset_slot(dh - 8, qq->slot3, vol_tbl[1]);
    if (bh & 0x20) volset_slot(dh - 4, qq->slot2, vol_tbl[2]);
    if (bh & 0x10) volset_slot(dh - 12, qq->slot1, vol_tbl[3]);
}

//-----------------------------------------------------------------------------
//	スロット毎の計算 & 出力 マクロ
//			in.	dl	元のTL値
//				dh	Outするレジスタ
//				al	音量変動値 中心=80h
//-----------------------------------------------------------------------------
void PMD::volset_slot(int dh, int dl, int al)
{
    if ((al += dl) > 255) al = 255;
    if ((al -= 0x80) < 0) al = 0;
    _OPNA->SetReg(pmdwork.fmsel + dh, al);
}

//-----------------------------------------------------------------------------
//	音量LFO用サブ
//-----------------------------------------------------------------------------
void PMD::fmlfo_sub(PartState * qq, int al, int bl, uint8_t * vol_tbl)
{
    if (bl & 0x80) vol_tbl[0] = Limit(vol_tbl[0] - al, 255, 0);
    if (bl & 0x40) vol_tbl[1] = Limit(vol_tbl[1] - al, 255, 0);
    if (bl & 0x20) vol_tbl[2] = Limit(vol_tbl[2] - al, 255, 0);
    if (bl & 0x10) vol_tbl[3] = Limit(vol_tbl[3] - al, 255, 0);
}

// SSG sound source performance main
void PMD::psgmain(PartState * qq)
{
    uint8_t * si;
    int		temp;

    if (qq->address == NULL) return;
    si = qq->address;

    qq->leng--;

    // KEYOFF CHECK & Keyoff
    if (qq == &SSGPart[2] && pmdwork._UsePPS &&
        _OpenWork.kshot_dat && qq->leng <= qq->qdat)
    {
        // PPS 使用時 & SSG 3ch & SSG 効果音鳴らしている場合
        keyoffp(qq);
        _OPNA->SetReg(pmdwork.partb + 8 - 1, 0);		// 強制的に音を止める
        qq->keyoff_flag = -1;
    }

    if (qq->partmask)
    {
        qq->keyoff_flag = -1;
    }
    else
    {
        if ((qq->keyoff_flag & 3) == 0)
        {		// 既にkeyoffしたか？
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
                si = commandsp(qq, si);
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
                        pmdwork.tieflag = 0;
                        pmdwork.volpush_flag = 0;
                        pmdwork.loop_work &= qq->loopcheck;
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
                {						// ポルタメント
                    si = portap(qq, ++si);
                    pmdwork.loop_work &= qq->loopcheck;
                    return;
                }
                else if (qq->partmask)
                {
                    if (ssgdrum_check(qq, *si) == 0)
                    {
                        si++;
                        qq->fnum = 0;		//休符に設定
                        qq->onkai = 255;
                        qq->leng = *si++;
                        qq->keyon_flag++;
                        qq->address = si;

                        if (--pmdwork.volpush_flag)
                        {
                            qq->volpush = 0;
                        }

                        pmdwork.tieflag = 0;
                        pmdwork.volpush_flag = 0;
                        break;
                    }
                }

                //  TONE SET
                fnumsetp(qq, oshiftp(qq, lfoinitp(qq, *si++)));

                qq->leng = *si++;
                si = calc_q(qq, si);

                if (qq->volpush && qq->onkai != 255)
                {
                    if (--pmdwork.volpush_flag)
                    {
                        pmdwork.volpush_flag = 0;
                        qq->volpush = 0;
                    }
                }

                volsetp(qq);
                otodasip(qq);
                keyonp(qq);

                qq->keyon_flag++;
                qq->address = si;

                pmdwork.tieflag = 0;
                pmdwork.volpush_flag = 0;
                qq->keyoff_flag = 0;

                if (*si == 0xfb)
                {		// '&'が直後にあったらkeyoffしない
                    qq->keyoff_flag = 2;
                }
                pmdwork.loop_work &= qq->loopcheck;
                return;
            }
        }
    }

    pmdwork.lfo_switch = (qq->lfoswi & 8);

    if (qq->lfoswi)
    {
        if (qq->lfoswi & 3)
        {
            if (lfop(qq))
            {
                pmdwork.lfo_switch |= (qq->lfoswi & 3);
            }
        }

        if (qq->lfoswi & 0x30)
        {
            lfo_change(qq);
            if (lfop(qq))
            {
                lfo_change(qq);
                pmdwork.lfo_switch |= (qq->lfoswi & 0x30);
            }
            else
            {
                lfo_change(qq);
            }
        }

        if (pmdwork.lfo_switch & 0x19)
        {
            if (pmdwork.lfo_switch & 0x08)
            {
                porta_calc(qq);
            }

            // SSG 3ch で休符かつ SSG Drum 発音中は操作しない
            if (!(qq == &SSGPart[2] && qq->onkai == 255 && _OpenWork.kshot_dat && !pmdwork._UsePPS))
            {
                otodasip(qq);
            }
        }
    }

    temp = soft_env(qq);
    if (temp || pmdwork.lfo_switch & 0x22 || (_OpenWork.fadeout_speed != 0))
    {
        // SSG 3ch で休符かつ SSG Drum 発音中は volume set しない
        if (!(qq == &SSGPart[2] && qq->onkai == 255 && _OpenWork.kshot_dat && !pmdwork._UsePPS))
        {
            volsetp(qq);
        }
    }

    pmdwork.loop_work &= qq->loopcheck;
}

void PMD::keyoffp(PartState * qq)
{
    if (qq->onkai == 255) return;		// ｷｭｳﾌ ﾉ ﾄｷ
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
void PMD::rhythmmain(PartState * qq)
{
    uint8_t * si, * bx;
    int		al, result = 0;

    if (qq->address == NULL) return;

    si = qq->address;

    if (--qq->leng == 0)
    {
        bx = _OpenWork.rhyadr;

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
                    _OpenWork.kshot_dat = 0;	//rest
                }

                al = *bx++;
                _OpenWork.rhyadr = bx;
                qq->leng = al;
                qq->keyon_flag++;
                pmdwork.tieflag = 0;
                pmdwork.volpush_flag = 0;
                pmdwork.loop_work &= qq->loopcheck;
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

                    //					bx = (uint16_t *)&open_work.radtbl[al];
                    //					bx = open_work.rhyadr = &open_work.mmlbuf[*bx];
                    bx = _OpenWork.rhyadr = &_OpenWork.mmlbuf[_OpenWork.radtbl[al]];
                    goto rhyms00;
                }

                // al > 0x80
                si = commandsr(qq, si - 1);
            }

            qq->address = --si;
            qq->loopcheck = 3;
            bx = qq->partloop;
            if (bx == NULL)
            {
                _OpenWork.rhyadr = (uint8_t *) &pmdwork.rhydmy;
                pmdwork.tieflag = 0;
                pmdwork.volpush_flag = 0;
                pmdwork.loop_work &= qq->loopcheck;
                return;
            }
            else
            {		// "L"があった時
                si = bx;
                qq->loopcheck = 1;
            }
        }
    }

    pmdwork.loop_work &= qq->loopcheck;
}

// PSG Rhythm ON
uint8_t * PMD::rhythmon(PartState * qq, uint8_t * bx, int al, int * result)
{
    int		cl, dl, bx_;

    if (al & 0x40)
    {
        bx = commandsr(qq, bx - 1);
        *result = 0;
        return bx;
    }

    *result = 1;

    if (qq->partmask)
    {		// maskされている場合
        _OpenWork.kshot_dat = 0;
        return ++bx;
    }

    al = ((al << 8) + *bx++) & 0x3fff;
    _OpenWork.kshot_dat = al;
    if (al == 0) return bx;
    _OpenWork.rhyadr = bx;

    if (_OpenWork.kp_rhythm_flag)
    {
        for (cl = 0; cl < 11; cl++)
        {
            if (al & (1 << cl))
            {
                _OPNA->SetReg(rhydat[cl][0], rhydat[cl][1]);
                if ((dl = (rhydat[cl][2] & _OpenWork.rhythmmask)))
                {
                    if (dl < 0x80)
                    {
                        _OPNA->SetReg(0x10, dl);
                    }
                    else
                    {
                        _OPNA->SetReg(0x10, 0x84);
                        dl = _OpenWork.rhythmmask & 8;
                        if (dl)
                        {
                            _OPNA->SetReg(0x10, dl);
                        }
                    }
                }
            }
        }
    }

    if (_OpenWork.fadeout_volume)
    {
        if (_OpenWork.kp_rhythm_flag)
        {
            dl = _OpenWork.rhyvol;
            if (_OpenWork.fadeout_volume)
            {
                dl = ((256 - _OpenWork.fadeout_volume) * dl) >> 8;
            }
            _OPNA->SetReg(0x11, dl);
        }
        if (pmdwork._UsePPS == false)
        {	// fadeout時ppsdrvでなら発音しない
            bx = _OpenWork.rhyadr;
            return bx;
        }
    }

    bx_ = al;
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
    while (pmdwork._UsePPS && bx_);	// PPSDRVなら２音目以上も鳴らしてみる

    return _OpenWork.rhyadr;
}

//	ＰＳＧ　ドラムス＆効果音　ルーチン
//	Ｆｒｏｍ　ＷＴ２９８
//
//	AL に 効果音Ｎｏ．を入れて　ＣＡＬＬする
//	ppsdrvがあるならそっちを鳴らす
void PMD::effgo(PartState * qq, int al)
{
    if (pmdwork._UsePPS)
    {		// PPS を鳴らす
        al |= 0x80;
        if (effwork.last_shot_data == al)
        {
            _PPS->Stop();
        }
        else
        {
            effwork.last_shot_data = al;
        }
    }

    effwork.hosei_flag = 3;				//	音程/音量補正あり (K part)
    eff_main(qq, al);
}

void PMD::eff_on2(PartState * qq, int al)
{
    effwork.hosei_flag = 1;				//	音程のみ補正あり (n command)
    eff_main(qq, al);
}

void PMD::eff_main(PartState * qq, int al)
{
    int		ah, bh, bl;

    if (_OpenWork.effflag) return;		//	効果音を使用しないモード

    if (pmdwork._UsePPS && (al & 0x80))
    {	// PPS を鳴らす
        if (effwork.effon >= 2) return;	// 通常効果音発音時は発声させない

        SSGPart[2].partmask |= 2;		// Part Mask
        effwork.effon = 1;				// 優先度１(ppsdrv)
        effwork.psgefcnum = al;			// 音色番号設定 (80H?)

        bh = 0;
        bl = 15;
        ah = effwork.hosei_flag;
        if (ah & 1)
        {
            bh = qq->detune % 256;		// BH = Detuneの下位 8bit
        }

        if (ah & 2)
        {
            if (qq->volume < 15)
            {
                bl = qq->volume;		// BL = volume値 (0?15)
            }

            if (_OpenWork.fadeout_volume)
            {
                bl = (bl * (256 - _OpenWork.fadeout_volume)) >> 8;
            }
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
        effwork.psgefcnum = al;

        if (effwork.effon <= efftbl[al].priority)
        {
            if (pmdwork._UsePPS)
            {
                _PPS->Stop();
            }

            SSGPart[2].partmask |= 2;		// Part Mask
            efffor(efftbl[al].table);		// １発目を発音
            effwork.effon = efftbl[al].priority;
            //	優先順位を設定(発音開始)
        }
    }
}

//	こーかおん　えんそう　めいん
// 	Ｆｒｏｍ　ＶＲＴＣ
void PMD::effplay(void)
{
    if (--effwork.effcnt)
    {
        effsweep();			// 新しくセットされない
    }
    else
    {
        efffor(effwork.effadr);
    }
}

void PMD::efffor(const int * si)
{
    int		al, ch, cl;

    al = *si++;
    if (al == -1)
    {
        effend();
    }
    else
    {
        effwork.effcnt = al;		// カウント数
        cl = *si;
        _OPNA->SetReg(4, *si++);		// 周波数セット
        ch = *si;
        _OPNA->SetReg(5, *si++);		// 周波数セット
        effwork.eswthz = (ch << 8) + cl;

        _OpenWork.psnoi_last = effwork.eswnhz = *si;
        _OPNA->SetReg(6, *si++);		// ノイズ

        _OPNA->SetReg(7, ((*si++ << 2) & 0x24) | (_OPNA->GetReg(0x07) & 0xdb));

        _OPNA->SetReg(10, *si++);		// ボリューム
        _OPNA->SetReg(11, *si++);		// エンベロープ周波数
        _OPNA->SetReg(12, *si++);		// 
        _OPNA->SetReg(13, *si++);		// エンベロープPATTERN

        effwork.eswtst = *si++;		// スイープ増分 (TONE)
        effwork.eswnst = *si++;		// スイープ増分 (NOISE)

        effwork.eswnct = effwork.eswnst & 15;		// スイープカウント (NOISE)

        effwork.effadr = (int *) si;
    }
}

void PMD::effend(void)
{
    if (pmdwork._UsePPS)
    {
        _PPS->Stop();
    }
    _OPNA->SetReg(0x0a, 0x00);
    _OPNA->SetReg(0x07, ((_OPNA->GetReg(0x07)) & 0xdb) | 0x24);
    effwork.effon = 0;
    effwork.psgefcnum = -1;
}

// 普段の処理
void PMD::effsweep(void)
{
    int		dl;

    effwork.eswthz += effwork.eswtst;
    _OPNA->SetReg(4, effwork.eswthz & 0xff);
    _OPNA->SetReg(5, effwork.eswthz >> 8);

    if (effwork.eswnst == 0) return;		// ノイズスイープ無し
    if (--effwork.eswnct) return;

    dl = effwork.eswnst;
    effwork.eswnct = dl & 15;

    // used to be "dl / 16"
    // with negative value division is different from shifting right
    // division: usually truncated towards zero (mandatory since c99)
    //   same as x86 idiv
    // shift: usually arithmetic shift
    //   same as x86 sar

    effwork.eswnhz += dl >> 4;

    _OPNA->SetReg(6, effwork.eswnhz);
    _OpenWork.psnoi_last = effwork.eswnhz;
}

//	PDRのswitch
uint8_t * PMD::pdrswitch(PartState * qq, uint8_t * si)
{
    if (pmdwork._UsePPS == false)
        return si + 1;

//  ppsdrv->SetParam((*si & 1) << 1, *si & 1);		@暫定
    si++;

    return si;
}

// PCM sound source performance main
void PMD::adpcmmain(PartState * qq)
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
        {		// 既にkeyoffしたか？
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
                si = commandsm(qq, si);
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
                        pmdwork.tieflag = 0;
                        pmdwork.volpush_flag = 0;
                        pmdwork.loop_work &= qq->loopcheck;

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
                {				// ポルタメント
                    si = portam(qq, ++si);

                    pmdwork.loop_work &= qq->loopcheck;

                    return;
                }
                else
                if (qq->partmask)
                {
                    si++;
                    qq->fnum = 0;		//休符に設定
                    qq->onkai = 255;
                    //					qq->onkai_def = 255;
                    qq->leng = *si++;
                    qq->keyon_flag++;
                    qq->address = si;

                    if (--pmdwork.volpush_flag)
                        qq->volpush = 0;

                    pmdwork.tieflag = 0;
                    pmdwork.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumsetm(qq, oshift(qq, lfoinitp(qq, *si++)));

                qq->leng = *si++;
                si = calc_q(qq, si);

                if (qq->volpush && qq->onkai != 255)
                {
                    if (--pmdwork.volpush_flag)
                    {
                        pmdwork.volpush_flag = 0;
                        qq->volpush = 0;
                    }
                }

                volsetm(qq);
                otodasim(qq);

                if (qq->keyoff_flag & 1)
                    keyonm(qq);

                qq->keyon_flag++;
                qq->address = si;

                pmdwork.tieflag = 0;
                pmdwork.volpush_flag = 0;

                if (*si == 0xfb)
                {   // Do not keyoff if '&' immediately follows
                    qq->keyoff_flag = 2;
                }
                else
                {
                    qq->keyoff_flag = 0;
                }

                pmdwork.loop_work &= qq->loopcheck;

                return;
            }
        }
    }

    pmdwork.lfo_switch = (qq->lfoswi & 8);

    if (qq->lfoswi)
    {
        if (qq->lfoswi & 3)
        {
            if (lfo(qq))
                pmdwork.lfo_switch |= (qq->lfoswi & 3);
        }

        if (qq->lfoswi & 0x30)
        {
            lfo_change(qq);

            if (lfop(qq))
            {
                lfo_change(qq);
                pmdwork.lfo_switch |= (qq->lfoswi & 0x30);
            }
            else
                lfo_change(qq);
        }

        if (pmdwork.lfo_switch & 0x19)
        {
            if (pmdwork.lfo_switch & 0x08)
                porta_calc(qq);

            otodasim(qq);
        }
    }

    int temp = soft_env(qq);

    if ((temp != 0) || pmdwork.lfo_switch & 0x22 || (_OpenWork.fadeout_speed != 0))
        volsetm(qq);

    pmdwork.loop_work &= qq->loopcheck;
}

// PCM sound source performance main (PMD86)
void PMD::pcm86main(PartState * qq)
{
    uint8_t * si;
    int		temp;

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
        {		// 既にkeyoffしたか？
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
            //			if(*si > 0x80 && *si != 0xda) {
            if (*si > 0x80)
            {
                si = commands8(qq, si);
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
                        pmdwork.tieflag = 0;
                        pmdwork.volpush_flag = 0;
                        pmdwork.loop_work &= qq->loopcheck;
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
                    qq->fnum = 0;		//休符に設定
                    qq->onkai = 255;
                    //					qq->onkai_def = 255;
                    qq->leng = *si++;
                    qq->keyon_flag++;
                    qq->address = si;

                    if (--pmdwork.volpush_flag)
                    {
                        qq->volpush = 0;
                    }

                    pmdwork.tieflag = 0;
                    pmdwork.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumset8(qq, oshift(qq, lfoinitp(qq, *si++)));

                qq->leng = *si++;
                si = calc_q(qq, si);

                if (qq->volpush && qq->onkai != 255)
                {
                    if (--pmdwork.volpush_flag)
                    {
                        pmdwork.volpush_flag = 0;
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

                pmdwork.tieflag = 0;
                pmdwork.volpush_flag = 0;

                if (*si == 0xfb)
                {		// '&'が直後にあったらkeyoffしない
                    qq->keyoff_flag = 2;
                }
                else
                {
                    qq->keyoff_flag = 0;
                }
                pmdwork.loop_work &= qq->loopcheck;
                return;

            }
        }
    }

    if (qq->lfoswi & 0x22)
    {
        pmdwork.lfo_switch = 0;
        if (qq->lfoswi & 2)
        {
            lfo(qq);
            pmdwork.lfo_switch |= (qq->lfoswi & 2);
        }

        if (qq->lfoswi & 0x20)
        {
            lfo_change(qq);
            if (lfo(qq))
            {
                lfo_change(qq);
                pmdwork.lfo_switch |= (qq->lfoswi & 0x20);
            }
            else
            {
                lfo_change(qq);
            }
        }

        temp = soft_env(qq);
        if (temp || pmdwork.lfo_switch & 0x22 || _OpenWork.fadeout_speed)
        {
            volset8(qq);
        }
    }
    else
    {
        temp = soft_env(qq);
        if (temp || _OpenWork.fadeout_speed)
        {
            volset8(qq);
        }
    }

    pmdwork.loop_work &= qq->loopcheck;
}

// PCM sound source performance main (PPZ8)
void PMD::ppz8main(PartState * qq)
{
    uint8_t * si;
    int		temp;

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
        {		// 既にkeyoffしたか？
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
                si = commandsz(qq, si);
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
                        pmdwork.tieflag = 0;
                        pmdwork.volpush_flag = 0;
                        pmdwork.loop_work &= qq->loopcheck;
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
                {				// ポルタメント
                    si = portaz(qq, ++si);
                    pmdwork.loop_work &= qq->loopcheck;
                    return;
                }
                else if (qq->partmask)
                {
                    si++;
                    qq->fnum = 0;		//休符に設定
                    qq->onkai = 255;
                    //					qq->onkai_def = 255;
                    qq->leng = *si++;
                    qq->keyon_flag++;
                    qq->address = si;

                    if (--pmdwork.volpush_flag)
                    {
                        qq->volpush = 0;
                    }

                    pmdwork.tieflag = 0;
                    pmdwork.volpush_flag = 0;
                    break;
                }

                //  TONE SET
                fnumsetz(qq, oshift(qq, lfoinitp(qq, *si++)));

                qq->leng = *si++;
                si = calc_q(qq, si);

                if (qq->volpush && qq->onkai != 255)
                {
                    if (--pmdwork.volpush_flag)
                    {
                        pmdwork.volpush_flag = 0;
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

                pmdwork.tieflag = 0;
                pmdwork.volpush_flag = 0;

                if (*si == 0xfb)
                {		// '&'が直後にあったらkeyoffしない
                    qq->keyoff_flag = 2;
                }
                else
                {
                    qq->keyoff_flag = 0;
                }
                pmdwork.loop_work &= qq->loopcheck;
                return;

            }
        }
    }

    pmdwork.lfo_switch = (qq->lfoswi & 8);
    if (qq->lfoswi)
    {
        if (qq->lfoswi & 3)
        {
            if (lfo(qq))
            {
                pmdwork.lfo_switch |= (qq->lfoswi & 3);
            }
        }

        if (qq->lfoswi & 0x30)
        {
            lfo_change(qq);
            if (lfop(qq))
            {
                lfo_change(qq);
                pmdwork.lfo_switch |= (qq->lfoswi & 0x30);
            }
            else
            {
                lfo_change(qq);
            }
        }

        if (pmdwork.lfo_switch & 0x19)
        {
            if (pmdwork.lfo_switch & 0x08)
            {
                porta_calc(qq);
            }
            otodasiz(qq);
        }
    }

    temp = soft_env(qq);
    if (temp || pmdwork.lfo_switch & 0x22 || _OpenWork.fadeout_speed)
    {
        volsetz(qq);
    }

    pmdwork.loop_work &= qq->loopcheck;
}

//	PCM KEYON
void PMD::keyonm(PartState * qq)
{
    if (qq->onkai == 255) return;

    _OPNA->SetReg(0x101, 0x02);	// PAN=0 / x8 bit mode
    _OPNA->SetReg(0x100, 0x21);	// PCM RESET

    _OPNA->SetReg(0x102, _OpenWork.pcmstart & 0xff);
    _OPNA->SetReg(0x103, _OpenWork.pcmstart >> 8);
    _OPNA->SetReg(0x104, _OpenWork.pcmstop & 0xff);
    _OPNA->SetReg(0x105, _OpenWork.pcmstop >> 8);

    if ((pmdwork.pcmrepeat1 | pmdwork.pcmrepeat2) == 0)
    {
        _OPNA->SetReg(0x100, 0xa0);	// PCM PLAY(non_repeat)
        _OPNA->SetReg(0x101, qq->fmpan | 2);	// PAN SET / x8 bit mode
    }
    else
    {
        _OPNA->SetReg(0x100, 0xb0);	// PCM PLAY(repeat)
        _OPNA->SetReg(0x101, qq->fmpan | 2);	// PAN SET / x8 bit mode
        _OPNA->SetReg(0x102, pmdwork.pcmrepeat1 & 0xff);
        _OPNA->SetReg(0x103, pmdwork.pcmrepeat1 >> 8);
        _OPNA->SetReg(0x104, pmdwork.pcmrepeat2 & 0xff);
        _OPNA->SetReg(0x105, pmdwork.pcmrepeat2 >> 8);
    }
}

//	PCM KEYON(PMD86)
void PMD::keyon8(PartState * qq)
{
    if (qq->onkai == 255) return;
    _P86->Play();
}

//	PPZ KEYON
void PMD::keyonz(PartState * qq)
{
    if (qq->onkai == 255) return;

    if ((qq->voicenum & 0x80) == 0)
    {
        _PPZ8->Play(pmdwork.partb, 0, qq->voicenum, 0, 0);
    }
    else
    {
        _PPZ8->Play(pmdwork.partb, 1, qq->voicenum & 0x7f, 0, 0);
    }
}

//	PCM OTODASI
void PMD::otodasim(PartState * qq)
{
    int		bx, dx;

    if ((bx = qq->fnum) == 0) return;

    // Portament/LFO/Detune SET

    bx += qq->porta_num;

    if ((qq->lfoswi & 0x11) && (qq->lfoswi & 1))
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
    dx *= 4;	// PCM ﾊ LFO ｶﾞ ｶｶﾘﾆｸｲ ﾉﾃﾞ depth ｦ 4ﾊﾞｲ ｽﾙ

    dx += qq->detune;
    if (dx >= 0)
    {
        bx += dx;
        if (bx > 0xffff) bx = 0xffff;
    }
    else
    {
        bx += dx;
        if (bx < 0) bx = 0;
    }

    // TONE SET

    _OPNA->SetReg(0x109, bx & 0xff);
    _OPNA->SetReg(0x10a, bx >> 8);
}

//	PCM OTODASI(PMD86)
void PMD::otodasi8(PartState * qq)
{
    int		bl, cx;

    if (qq->fnum == 0) return;

    bl = (qq->fnum & 0x0e00000) >> (16 + 5);		// 設定周波数
    cx = qq->fnum & 0x01fffff;					// fnum

    if (_OpenWork.pcm86_vol == 0 && qq->detune)
    {
        cx = Limit((cx >> 5) + qq->detune, 65535, 1) << 5;
    }

    _P86->SetOntei(bl, cx);
}

//	PPZ OTODASI
void PMD::otodasiz(PartState * qq)
{
    uint	cx;
    int64_t	cx2;
    int		ax;

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
    else cx = (uint) cx2;

    // TONE SET
    _PPZ8->SetOntei(pmdwork.partb, cx);
}

//	PCM VOLUME SET
void PMD::volsetm(PartState * qq)
{
    int		ah, al, dx;

    if (qq->volpush)
    {
        al = qq->volpush;
    }
    else
    {
        al = qq->volume;
    }

    //------------------------------------------------------------------------
    //	音量down計算
    //------------------------------------------------------------------------
    al = ((256 - _OpenWork.pcm_voldown) * al) >> 8;

    //------------------------------------------------------------------------
    //	Fadeout計算
    //------------------------------------------------------------------------
    if (_OpenWork.fadeout_volume)
    {
        al = (((256 - _OpenWork.fadeout_volume) * (256 - _OpenWork.fadeout_volume) >> 8) * al) >> 8;
    }

    //------------------------------------------------------------------------
    //	ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        _OPNA->SetReg(0x10b, 0);
        return;
    }

    if (qq->envf == -1)
    {
        //	拡張版 音量=al*(eenv_vol+1)/16
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
            ah = -qq->eenv_volume * 16;
            if (al < ah)
            {
                _OPNA->SetReg(0x10b, 0);
                return;
            }
            else
            {
                al -= ah;
            }
        }
        else
        {
            ah = qq->eenv_volume * 16;
            if (al + ah > 255)
            {
                al = 255;
            }
            else
            {
                al += ah;
            }
        }
    }

    //--------------------------------------------------------------------
    //	音量LFO計算
    //--------------------------------------------------------------------

    if ((qq->lfoswi & 0x22) == 0)
    {
        _OPNA->SetReg(0x10b, al);
        return;
    }

    if (qq->lfoswi & 2)
    {
        dx = qq->lfodat;
    }
    else
    {
        dx = 0;
    }

    if (qq->lfoswi & 0x20)
    {
        dx += qq->_lfodat;
    }

    if (dx >= 0)
    {
        al += dx;
        if (al & 0xff00)
        {
            _OPNA->SetReg(0x10b, 255);
        }
        else
        {
            _OPNA->SetReg(0x10b, al);
        }
    }
    else
    {
        al += dx;
        if (al < 0)
        {
            _OPNA->SetReg(0x10b, 0);
        }
        else
        {
            _OPNA->SetReg(0x10b, al);
        }
    }
}

//	PCM VOLUME SET(PMD86)
void PMD::volset8(PartState * qq)
{
    int		ah, al, dx;

    if (qq->volpush)
    {
        al = qq->volpush;
    }
    else
    {
        al = qq->volume;
    }

    //------------------------------------------------------------------------
    //	音量down計算
    //------------------------------------------------------------------------
    al = ((256 - _OpenWork.pcm_voldown) * al) >> 8;

    //------------------------------------------------------------------------
    //	Fadeout計算
    //------------------------------------------------------------------------
    if (_OpenWork.fadeout_volume != 0)
    {
        al = ((256 - _OpenWork.fadeout_volume) * al) >> 8;
    }

    //------------------------------------------------------------------------
    //	ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        _OPNA->SetReg(0x10b, 0);
        return;
    }

    if (qq->envf == -1)
    {
        //	拡張版 音量=al*(eenv_vol+1)/16
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
            ah = -qq->eenv_volume * 16;
            if (al < ah)
            {
                _OPNA->SetReg(0x10b, 0);
                return;
            }
            else
            {
                al -= ah;
            }
        }
        else
        {
            ah = qq->eenv_volume * 16;
            if (al + ah > 255)
            {
                al = 255;
            }
            else
            {
                al += ah;
            }
        }
    }

    //--------------------------------------------------------------------
    //	音量LFO計算
    //--------------------------------------------------------------------

    if (qq->lfoswi & 2)
    {
        dx = qq->lfodat;
    }
    else
    {
        dx = 0;
    }

    if (qq->lfoswi & 0x20)
    {
        dx += qq->_lfodat;
    }

    if (dx >= 0)
    {
        if ((al += dx) > 255) al = 255;
    }
    else
    {
        if ((al += dx) < 0) al = 0;
    }

    if (_OpenWork.pcm86_vol)
    {
        //	SPBと同様の音量設定
        al = (int) sqrt(al);
    }
    else
    {
        al >>= 4;
    }

    _P86->SetVol(al);
}

//	PPZ VOLUME SET
void PMD::volsetz(PartState * qq)
{
    int		ah, al, dx;

    if (qq->volpush)
    {
        al = qq->volpush;
    }
    else
    {
        al = qq->volume;
    }

    //------------------------------------------------------------------------
    //	音量down計算
    //------------------------------------------------------------------------
    al = ((256 - _OpenWork.ppz_voldown) * al) >> 8;

    //------------------------------------------------------------------------
    //	Fadeout計算
    //------------------------------------------------------------------------
    if (_OpenWork.fadeout_volume != 0)
    {
        al = ((256 - _OpenWork.fadeout_volume) * al) >> 8;
    }

    //------------------------------------------------------------------------
    //	ENVELOPE 計算
    //------------------------------------------------------------------------
    if (al == 0)
    {
        //*@
        _PPZ8->SetVol(pmdwork.partb, 0);
        _PPZ8->Stop(pmdwork.partb);
        return;
    }

    if (qq->envf == -1)
    {
        //	拡張版 音量=al*(eenv_vol+1)/16
        if (qq->eenv_volume == 0)
        {
            //*@		ppz8->SetVol(pmdwork.partb, 0);
            _PPZ8->Stop(pmdwork.partb);
            return;
        }

        al = ((((al * (qq->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        if (qq->eenv_volume < 0)
        {
            ah = -qq->eenv_volume * 16;
            if (al < ah)
            {
                //*@			ppz8->SetVol(pmdwork.partb, 0);
                _PPZ8->Stop(pmdwork.partb);
                return;
            }
            else
            {
                al -= ah;
            }
        }
        else
        {
            ah = qq->eenv_volume * 16;
            if (al + ah > 255)
            {
                al = 255;
            }
            else
            {
                al += ah;
            }
        }
    }

    //--------------------------------------------------------------------
    //	音量LFO計算
    //--------------------------------------------------------------------

    if ((qq->lfoswi & 0x22))
    {
        if (qq->lfoswi & 2)
        {
            dx = qq->lfodat;
        }
        else
        {
            dx = 0;
        }

        if (qq->lfoswi & 0x20)
        {
            dx += qq->_lfodat;
        }

        al += dx;
        if (dx >= 0)
        {
            if (al & 0xff00)
            {
                al = 255;
            }
        }
        else
        {
            if (al < 0)
            {
                al = 0;
            }
        }
    }

    if (al)
    {
        _PPZ8->SetVol(pmdwork.partb, al >> 4);
    }
    else
    {
        _PPZ8->Stop(pmdwork.partb);
    }
    //*@
    /*
        ppz8->SetVol(pmdwork.partb, al >> 4);

        if(al <= 0) {
            ppz8->Stop(pmdwork.partb);
        }
    */
}

//	ADPCM FNUM SET
void PMD::fnumsetm(PartState * qq, int al)
{
    int		ax, bx, ch, cl;

    if ((al & 0x0f) != 0x0f)
    {			// 音符の場合
        qq->onkai = al;

        bx = al & 0x0f;					// bx=onkai
        ch = cl = (al >> 4) & 0x0f;		// cl = octarb

        if (cl > 5)
        {
            cl = 0;
        }
        else
        {
            cl = 5 - cl;				// cl=5-octarb
        }

        ax = pcm_tune_data[bx];
        if (ch >= 6)
        {					// o7以上?
            ch = 0x50;
            if (ax < 0x8000)
            {
                ax *= 2;				// o7以上で2倍できる場合は2倍
                ch = 0x60;
            }
            qq->onkai = (qq->onkai & 0x0f) | ch;	// onkai値修正
        }
        else
        {
            ax >>= cl;					// ax=ax/[2^OCTARB]
        }
        qq->fnum = ax;
    }
    else
    {						// 休符の場合
        qq->onkai = 255;
        if ((qq->lfoswi & 0x11) == 0)
        {
            qq->fnum = 0;			// 音程LFO未使用
        }
    }
}

//	PCM FNUM SET(PMD86)
void PMD::fnumset8(PartState * qq, int al)
{
    int		ah, bl;

    ah = al & 0x0f;
    if (ah != 0x0f)
    {			// 音符の場合
        if (_OpenWork.pcm86_vol && al >= 0x65)
        {		// o7e?
            if (ah < 5)
            {
                al = 0x60;		// o7
            }
            else
            {
                al = 0x50;		// o6
            }
            al |= ah;
        }

        qq->onkai = al;
        bl = ((al & 0xf0) >> 4) * 12 + ah;
        qq->fnum = p86_tune_data[bl];
    }
    else
    {						// 休符の場合
        qq->onkai = 255;
        if ((qq->lfoswi & 0x11) == 0)
        {
            qq->fnum = 0;			// 音程LFO未使用
        }
    }
}

//	PPZ FNUM SET
void PMD::fnumsetz(PartState * qq, int al)
{
    uint	ax;
    int		bx, cl;


    if ((al & 0x0f) != 0x0f)
    {			// 音符の場合
        qq->onkai = al;

        bx = al & 0x0f;					// bx=onkai
        cl = (al >> 4) & 0x0f;		// cl = octarb

        ax = ppz_tune_data[bx];

        if ((cl -= 4) < 0)
        {
            cl = -cl;
            ax >>= cl;
        }
        else
        {
            ax <<= cl;
        }
        qq->fnum = ax;
    }
    else
    {						// 休符の場合
        qq->onkai = 255;
        if ((qq->lfoswi & 0x11) == 0)
        {
            qq->fnum = 0;			// 音程LFO未使用
        }
    }
}

//	ポルタメント(PCM)
uint8_t * PMD::portam(PartState * qq, uint8_t * si)
{
    int		ax, al_, bx_;

    if (qq->partmask)
    {
        qq->fnum = 0;		//休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

        if (--pmdwork.volpush_flag)
        {
            qq->volpush = 0;
        }

        pmdwork.tieflag = 0;
        pmdwork.volpush_flag = 0;
        pmdwork.loop_work &= qq->loopcheck;
        return si + 3;		// 読み飛ばす	(Mask時)
    }

    fnumsetm(qq, oshift(qq, lfoinitp(qq, *si++)));

    bx_ = qq->fnum;
    al_ = qq->onkai;
    fnumsetm(qq, oshift(qq, *si++));
    ax = qq->fnum; 			// ax = ポルタメント先のdelta_n値

    qq->onkai = al_;
    qq->fnum = bx_;			// bx = ポルタメント元のdekta_n値
    ax -= bx_;				// ax = delta_n差

    qq->leng = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->leng;		// 商
    qq->porta_num3 = ax % qq->leng;		// 余り
    qq->lfoswi |= 8;				// Porta ON

    if (qq->volpush && qq->onkai != 255)
    {
        if (--pmdwork.volpush_flag)
        {
            pmdwork.volpush_flag = 0;
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

    pmdwork.tieflag = 0;
    pmdwork.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {			// '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }

    pmdwork.loop_work &= qq->loopcheck;
    return si;
}

//	ポルタメント(PPZ)
uint8_t * PMD::portaz(PartState * qq, uint8_t * si)
{
    int		ax, al_, bx_;

    if (qq->partmask)
    {
        qq->fnum = 0;		//休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

        if (--pmdwork.volpush_flag)
        {
            qq->volpush = 0;
        }

        pmdwork.tieflag = 0;
        pmdwork.volpush_flag = 0;
        pmdwork.loop_work &= qq->loopcheck;
        return si + 3;		// 読み飛ばす	(Mask時)
    }

    fnumsetz(qq, oshift(qq, lfoinitp(qq, *si++)));

    bx_ = qq->fnum;
    al_ = qq->onkai;
    fnumsetz(qq, oshift(qq, *si++));
    ax = qq->fnum; 			// ax = ポルタメント先のdelta_n値

    qq->onkai = al_;
    qq->fnum = bx_;			// bx = ポルタメント元のdekta_n値
    ax -= bx_;				// ax = delta_n差
    ax /= 16;

    qq->leng = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->leng;		// 商
    qq->porta_num3 = ax % qq->leng;		// 余り
    qq->lfoswi |= 8;				// Porta ON

    if (qq->volpush && qq->onkai != 255)
    {
        if (--pmdwork.volpush_flag)
        {
            pmdwork.volpush_flag = 0;
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

    pmdwork.tieflag = 0;
    pmdwork.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {			// '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }

    pmdwork.loop_work &= qq->loopcheck;
    return si;
}

void PMD::keyoffm(PartState * qq)
{
    if (qq->envf != -1)
    {
        if (qq->envf == 2) return;
    }
    else
    {
        if (qq->eenv_count == 4) return;
    }

    if (pmdwork.pcmrelease != 0x8000)
    {
        // PCM RESET
        _OPNA->SetReg(0x100, 0x21);

        _OPNA->SetReg(0x102, pmdwork.pcmrelease & 0xff);
        _OPNA->SetReg(0x103, pmdwork.pcmrelease >> 8);

        // Stop ADDRESS for Release
        _OPNA->SetReg(0x104, _OpenWork.pcmstop & 0xff);
        _OPNA->SetReg(0x105, _OpenWork.pcmstop >> 8);

        // PCM PLAY(non_repeat)
        _OPNA->SetReg(0x100, 0xa0);
    }

    keyoffp(qq);
    return;
}

void PMD::keyoff8(PartState * qq)
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

//	ppz KEYOFF
void PMD::keyoffz(PartState * qq)
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

//	Pan setting Extend
uint8_t * PMD::pansetm_ex(PartState * qq, uint8_t * si)
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

    return si + 2;	// 逆走flagは読み飛ばす
}

//	リピート設定
uint8_t * PMD::pcmrepeat_set(PartState * qq, uint8_t * si)
{
    int		ax;

    ax = *(int16_t *) si;
    si += 2;

    if (ax >= 0)
    {
        ax += _OpenWork.pcmstart;
    }
    else
    {
        ax += _OpenWork.pcmstop;
    }

    pmdwork.pcmrepeat1 = ax;

    ax = *(int16_t *) si;
    si += 2;
    if (ax > 0)
    {
        ax += _OpenWork.pcmstart;
    }
    else
    {
        ax += _OpenWork.pcmstop;
    }

    pmdwork.pcmrepeat2 = ax;

    ax = *(uint16_t *) si;
    si += 2;
    if (ax < 0x8000)
    {
        ax += _OpenWork.pcmstart;
    }
    else if (ax > 0x8000)
    {
        ax += _OpenWork.pcmstop;
    }

    pmdwork.pcmrelease = ax;
    return si;
}

//	リピート設定(PMD86)
uint8_t * PMD::pcmrepeat_set8(PartState * qq, uint8_t * si)
{
    int16_t loop_start, loop_end, release_start;

    loop_start = *(int16_t *) si;
    si += 2;
    loop_end = *(int16_t *) si;
    si += 2;
    release_start = *(int16_t *) si;

    if (_OpenWork.pcm86_vol)
    {
        _P86->SetLoop(loop_start, loop_end, release_start, true);
    }
    else
    {
        _P86->SetLoop(loop_start, loop_end, release_start, false);
    }

    return si + 2;
}

//	リピート設定
uint8_t * PMD::ppzrepeat_set(PartState * qq, uint8_t * si)
{
    int		ax, ax2;

    if ((qq->voicenum & 0x80) == 0)
    {
        ax = *(int16_t *) si;
        si += 2;
        if (ax < 0)
        {
            ax = _PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].size - ax;
        }

        ax2 = *(int16_t *) si;
        si += 2;
        if (ax2 < 0)
        {
            ax2 = _PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].size - ax;
        }
    }
    else
    {
        ax = *(int16_t *) si;
        si += 2;
        if (ax < 0)
        {
            ax = _PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].size - ax;
        }

        ax2 = *(int16_t *) si;
        si += 2;
        if (ax2 < 0)
        {
            ax2 = _PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].size - ax2;
        }
    }

    _PPZ8->SetLoop(pmdwork.partb, ax, ax2);
    return si + 2;
}

uint8_t * PMD::vol_one_up_pcm(PartState * qq, uint8_t * si)
{
    int		al;

    al = (int) *si++ + qq->volume;
    if (al > 254) al = 254;
    al++;
    qq->volpush = al;
    pmdwork.volpush_flag = 1;
    return si;
}

//	COMMAND 'p' [Panning Set]
uint8_t * PMD::pansetm(PartState * qq, uint8_t * si)
{
    qq->fmpan = (*si << 6) & 0xc0;
    return si + 1;
}

//	COMMAND 'p' [Panning Set]
//	p0		逆相
//	p1		右
//	p2		左
//	p3		中
uint8_t * PMD::panset8(PartState * qq, uint8_t * si)
{
    int		flag, data;

    data = 0;

    switch (*si++)
    {
        case 1:					// Right
            flag = 2;
            data = 1;
            break;

        case 2:					// Left
            flag = 1;
            data = 0;
            break;

        case 3:					// Center
            flag = 3;
            data = 0;
            break;

        default:					// 逆相
            flag = 3 | 4;
            data = 0;

    }
    _P86->SetPan(flag, data);
    return si;
}

//	COMMAND 'p' [Panning Set]
//		0=0	無音
//		1=9	右
//		2=1	左
//		3=5	中央
uint8_t * PMD::pansetz(PartState * qq, uint8_t * si)
{
    qq->fmpan = ppzpandata[*si++];
    _PPZ8->SetPan(pmdwork.partb, qq->fmpan);
    return si;
}

//	Pan setting Extend
//		px -4?+4
uint8_t * PMD::pansetz_ex(PartState * qq, uint8_t * si)
{
    int		al;

    al = *(int8_t *) si++;
    si++;		// 逆相flagは読み飛ばす

    if (al >= 5)
    {
        al = 4;
    }
    else if (al < -4)
    {
        al = -4;
    }

    qq->fmpan = al + 5;
    _PPZ8->SetPan(pmdwork.partb, qq->fmpan);
    return si;
}

uint8_t * PMD::comatm(PartState * qq, uint8_t * si)
{
    qq->voicenum = *si++;
    _OpenWork.pcmstart = pcmends.pcmadrs[qq->voicenum][0];
    _OpenWork.pcmstop = pcmends.pcmadrs[qq->voicenum][1];
    pmdwork.pcmrepeat1 = 0;
    pmdwork.pcmrepeat2 = 0;
    pmdwork.pcmrelease = 0x8000;
    return si;
}

uint8_t * PMD::comat8(PartState * qq, uint8_t * si)
{
    qq->voicenum = *si++;
    _P86->SetNeiro(qq->voicenum);
    return si;
}

uint8_t * PMD::comatz(PartState * qq, uint8_t * si)
{
    qq->voicenum = *si++;

    if ((qq->voicenum & 0x80) == 0)
    {
        _PPZ8->SetLoop(pmdwork.partb,
            _PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].loop_start,
            _PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].loop_end);
        _PPZ8->SetSourceRate(pmdwork.partb,
            _PPZ8->PCME_WORK[0].pcmnum[qq->voicenum].rate);
    }
    else
    {
        _PPZ8->SetLoop(pmdwork.partb,
            _PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].loop_start,
            _PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].loop_end);
        _PPZ8->SetSourceRate(pmdwork.partb,
            _PPZ8->PCME_WORK[1].pcmnum[qq->voicenum & 0x7f].rate);
    }
    return si;
}

//	SSGドラムを消してSSGを復活させるかどうかcheck
//		input	AL <- Command
//		output	cy=1 : 復活させる
int PMD::ssgdrum_check(PartState * qq, int al)
{
    // SSGマスク中はドラムを止めない
    // SSGドラムは鳴ってない
    if ((qq->partmask & 1) || ((qq->partmask & 2) == 0)) return 0;

    // 普通の効果音は消さない
    if (effwork.effon >= 2) return 0;

    al = (al & 0x0f);

    // 休符の時はドラムは止めない
    if (al == 0x0f) return 0;

    // SSGドラムはまだ再生中か？
    if (effwork.effon == 1)
    {
        effend();			// SSGドラムを消す
    }

    if ((qq->partmask &= 0xfd) == 0) return -1;
    return 0;
}

// Various special command processing
uint8_t * PMD::commands(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    switch (al)
    {
        case 0xff: si = comat(qq, si); break;
        case 0xfe: qq->qdata = *si++; qq->qdat3 = 0; break;
        case 0xfd: qq->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: pmdwork.tieflag |= 1; break;
        case 0xfa: qq->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(qq, si); break;
        case 0xf8: si = comedloop(qq, si); break;
        case 0xf7: si = comexloop(qq, si); break;
        case 0xf6: qq->partloop = si; break;
        case 0xf5: qq->shift = *(int8_t *) si++; break;
        case 0xf4: if ((qq->volume += 4) > 127) qq->volume = 127; break;
        case 0xf3: if (qq->volume < 4) qq->volume = 0; else qq->volume -= 4; break;
        case 0xf2: si = lfoset(qq, si); break;
        case 0xf1: si = lfoswitch(qq, si); ch3_setting(qq); break;
        case 0xf0: si += 4; break;

        case 0xef: _OPNA->SetReg(pmdwork.fmsel + *si, *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = panset(qq, si); break;				// FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: qq->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: qq->hldelay = *si++; break;
            //追加 for V2.3
        case 0xe3: if ((qq->volume += *si++) > 127) qq->volume = 127; break;
        case 0xe2:
            if (qq->volume < *si) qq->volume = 0; else qq->volume -= *si;
            si++;
            break;
            //
        case 0xe1: si = hlfo_set(qq, si); break;
        case 0xe0: _OpenWork.port22h = *si; _OPNA->SetReg(0x22, *si++); break;
            //
        case 0xdf: _OpenWork.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_fm(qq, si); break;
        case 0xdd: si = vol_one_down(qq, si); break;
            //
        case 0xdc: _OpenWork.status = *si++; break;
        case 0xdb: _OpenWork.status += *si++; break;
            //
        case 0xda: si = porta(qq, si); break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: qq->mdspd = qq->mdspd2 = *si++; qq->mdepth = *(int8_t *) si++; break;
        case 0xd5: qq->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(qq, si); break;
        case 0xd3: si = fm_efct_set(qq, si); break;
        case 0xd2:
            _OpenWork.fadeout_flag = 1;
            _OpenWork.fadeout_speed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
        case 0xcf: si = slotmask_set(qq, si); break;
        case 0xce: si += 6; break;
        case 0xcd: si += 5; break;
        case 0xcc: si++; break;
        case 0xcb: qq->lfo_wave = *si++; break;
        case 0xca:
            qq->extendmode = (qq->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9: si++; break;
        case 0xc8: si = slotdetune_set(qq, si); break;
        case 0xc7: si = slotdetune_set2(qq, si); break;
        case 0xc6: si = fm3_extpartset(qq, si); break;
        case 0xc5: si = volmask_set(qq, si); break;
        case 0xc4: qq->qdatb = *si++; break;
        case 0xc3: si = panset_ex(qq, si); break;
        case 0xc2: qq->delay = qq->delay2 = *si++; lfoinit_main(qq); break;
        case 0xc1: break;
        case 0xc0: si = fm_mml_part_mask(qq, si); break;
        case 0xbf: lfo_change(qq); si = lfoset(qq, si); lfo_change(qq); break;
        case 0xbe: si = _lfoswitch(qq, si); ch3_setting(qq); break;
        case 0xbd:
            lfo_change(qq);
            qq->mdspd = qq->mdspd2 = *si++;
            qq->mdepth = *(int8_t *) si++;
            lfo_change(qq);
            break;

        case 0xbc: lfo_change(qq); qq->lfo_wave = *si++; lfo_change(qq); break;
        case 0xbb:
            lfo_change(qq);
            qq->extendmode = (qq->extendmode & 0xfd) | ((*si++ & 1) << 1);
            lfo_change(qq);
            break;

        case 0xba: si = _volmask_set(qq, si); break;
        case 0xb9:
            lfo_change(qq);
            qq->delay = qq->delay2 = *si++; lfoinit_main(qq);
            lfo_change(qq);
            break;

        case 0xb8: si = tl_set(qq, si); break;
        case 0xb7: si = mdepth_count(qq, si); break;
        case 0xb6: si = fb_set(qq, si); break;
        case 0xb5:
            qq->sdelay_m = (~(*si++) << 4) & 0xf0;
            qq->sdelay_c = qq->sdelay = *si++;
            break;

        case 0xb4: si += 16; break;
        case 0xb3: qq->qdat2 = *si++; break;
        case 0xb2: qq->shift_def = *(int8_t *) si++; break;
        case 0xb1: qq->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::commandsp(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    switch (al)
    {
        case 0xff: si++; break;
        case 0xfe: qq->qdata = *si++; qq->qdat3 = 0; break;
        case 0xfd: qq->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: pmdwork.tieflag |= 1; break;
        case 0xfa: qq->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(qq, si); break;
        case 0xf8: si = comedloop(qq, si); break;
        case 0xf7: si = comexloop(qq, si); break;
        case 0xf6: qq->partloop = si; break;
        case 0xf5: qq->shift = *(int8_t *) si++; break;
        case 0xf4: if (qq->volume < 15) qq->volume++; break;
        case 0xf3: if (qq->volume > 0) qq->volume--; break;
        case 0xf2: si = lfoset(qq, si); break;
        case 0xf1: si = lfoswitch(qq, si); break;
        case 0xf0: si = psgenvset(qq, si); break;

        case 0xef: _OPNA->SetReg(*si, *(si + 1)); si += 2; break;
        case 0xee: _OpenWork.psnoi = *si++; break;
        case 0xed: qq->psgpat = *si++; break;
            //
        case 0xec: si++; break;
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: qq->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: si++; break;
            //追加 for V2.3
            // saturate
        case 0xe3: qq->volume += *si++; if (qq->volume > 15) qq->volume = 15; break;
        case 0xe2: qq->volume -= *si++; if (qq->volume < 0) qq->volume = 0; break;

            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _OpenWork.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_psg(qq, si); break;
        case 0xdd: si = vol_one_down(qq, si); break;
            //
        case 0xdc: _OpenWork.status = *si++; break;
        case 0xdb: _OpenWork.status += *si++; break;
            //
        case 0xda: si = portap(qq, si); break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: qq->mdspd = qq->mdspd2 = *si++; qq->mdepth = *(int8_t *) si++; break;
        case 0xd5: qq->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(qq, si); break;
        case 0xd3: si = fm_efct_set(qq, si); break;
        case 0xd2:
            _OpenWork.fadeout_flag = 1;
            _OpenWork.fadeout_speed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si = psgnoise_move(si); break;
            //
        case 0xcf: si++; break;
        case 0xce: si += 6; break;
        case 0xcd: si = extend_psgenvset(qq, si); break;
        case 0xcc:
            qq->extendmode = (qq->extendmode & 0xfe) | (*si++ & 1);
            break;

        case 0xcb: qq->lfo_wave = *si++; break;
        case 0xca:
            qq->extendmode = (qq->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            qq->extendmode = (qq->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: qq->qdatb = *si++; break;
        case 0xc3: si += 2; break;
        case 0xc2: qq->delay = qq->delay2 = *si++; lfoinit_main(qq); break;
        case 0xc1: break;
        case 0xc0: si = ssg_mml_part_mask(qq, si); break;
        case 0xbf: lfo_change(qq); si = lfoset(qq, si); lfo_change(qq); break;
        case 0xbe:
            qq->lfoswi = (qq->lfoswi & 0x8f) | ((*si++ & 7) << 4);
            lfo_change(qq); lfoinit_main(qq); lfo_change(qq);
            break;

        case 0xbd:
            lfo_change(qq);
            qq->mdspd = qq->mdspd2 = *si++;
            qq->mdepth = *(int8_t *) si++;
            lfo_change(qq);
            break;

        case 0xbc: lfo_change(qq); qq->lfo_wave = *si++; lfo_change(qq); break;
        case 0xbb:
            lfo_change(qq);
            qq->extendmode = (qq->extendmode & 0xfd) | ((*si++ & 1) << 1);
            lfo_change(qq);
            break;

        case 0xba: si++; break;
        case 0xb9:
            lfo_change(qq);
            qq->delay = qq->delay2 = *si++; lfoinit_main(qq); break;
            lfo_change(qq);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(qq, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;
        case 0xb3: qq->qdat2 = *si++; break;
        case 0xb2: qq->shift_def = *(int8_t *) si++; break;
        case 0xb1: qq->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::commandsr(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    switch (al)
    {
        case 0xff: si++; break;
        case 0xfe: si++; break;
        case 0xfd: qq->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: pmdwork.tieflag |= 1; break;
        case 0xfa: qq->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(qq, si); break;
        case 0xf8: si = comedloop(qq, si); break;
        case 0xf7: si = comexloop(qq, si); break;
        case 0xf6: qq->partloop = si; break;
        case 0xf5: si++; break;
        case 0xf4: if (qq->volume < 15) qq->volume++; break;
        case 0xf3: if (qq->volume > 0) qq->volume--; break;
        case 0xf2: si += 4; break;
        case 0xf1: si = pdrswitch(qq, si); break;
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
        case 0xe3: if ((qq->volume + *si) < 16) qq->volume += *si; si++; break;
        case 0xe2: if ((qq->volume - *si) >= 0) qq->volume -= *si; si++; break;
            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _OpenWork.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_psg(qq, si); break;
        case 0xdd: si = vol_one_down(qq, si); break;
            //
        case 0xdc: _OpenWork.status = *si++; break;
        case 0xdb: _OpenWork.status += *si++; break;
            //
        case 0xda: si++; break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: si += 2; break;
        case 0xd5: qq->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(qq, si); break;
        case 0xd3: si = fm_efct_set(qq, si); break;
        case 0xd2:
            _OpenWork.fadeout_flag = 1;
            _OpenWork.fadeout_speed = *si++;
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
        case 0xc0: si = rhythm_mml_part_mask(qq, si); break;
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

uint8_t * PMD::commandsm(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    switch (al)
    {
        case 0xff: si = comatm(qq, si); break;
        case 0xfe: qq->qdata = *si++; break;
        case 0xfd: qq->volume = *si++; break;
        case 0xfc: si = comt(si); break;
        case 0xfb: pmdwork.tieflag |= 1; break;
        case 0xfa: qq->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(qq, si); break;
        case 0xf8: si = comedloop(qq, si); break;
        case 0xf7: si = comexloop(qq, si); break;
        case 0xf6: qq->partloop = si; break;
        case 0xf5: qq->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (qq->volume < (255 - 16)) qq->volume += 16;
            else qq->volume = 255;
            break;

        case 0xf3: if (qq->volume < 16) qq->volume = 0; else qq->volume -= 16; break;
        case 0xf2: si = lfoset(qq, si); break;
        case 0xf1: si = lfoswitch(qq, si); break;
        case 0xf0: si = psgenvset(qq, si); break;

        case 0xef: _OPNA->SetReg(0x100 + *si, *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = pansetm(qq, si); break;				// FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: qq->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: si++; break;
            //追加 for V2.3
        case 0xe3:
            if (qq->volume < (255 - (*si))) qq->volume += (*si);
            else qq->volume = 255;
            si++;
            break;

        case 0xe2:
            if (qq->volume < *si) qq->volume = 0; else qq->volume -= *si;
            si++;
            break;
            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _OpenWork.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_pcm(qq, si); break;
        case 0xdd: si = vol_one_down(qq, si); break;
            //
        case 0xdc: _OpenWork.status = *si++; break;
        case 0xdb: _OpenWork.status += *si++; break;
            //
        case 0xda: si = portam(qq, si); break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: qq->mdspd = qq->mdspd2 = *si++; qq->mdepth = *(int8_t *) si++; break;
        case 0xd5: qq->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(qq, si); break;
        case 0xd3: si = fm_efct_set(qq, si); break;
        case 0xd2:
            _OpenWork.fadeout_flag = 1;
            _OpenWork.fadeout_speed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
        case 0xcf: si++; break;
        case 0xce: si = pcmrepeat_set(qq, si); break;
        case 0xcd: si = extend_psgenvset(qq, si); break;
        case 0xcc: si++; break;
        case 0xcb: qq->lfo_wave = *si++; break;
        case 0xca:
            qq->extendmode = (qq->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            qq->extendmode = (qq->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: qq->qdatb = *si++; break;
        case 0xc3: si = pansetm_ex(qq, si); break;
        case 0xc2: qq->delay = qq->delay2 = *si++; lfoinit_main(qq); break;
        case 0xc1: break;
        case 0xc0: si = pcm_mml_part_mask(qq, si); break;
        case 0xbf: lfo_change(qq); si = lfoset(qq, si); lfo_change(qq); break;
        case 0xbe: si = _lfoswitch(qq, si); break;
        case 0xbd:
            lfo_change(qq);
            qq->mdspd = qq->mdspd2 = *si++;
            qq->mdepth = *(int8_t *) si++;
            lfo_change(qq);
            break;

        case 0xbc: lfo_change(qq); qq->lfo_wave = *si++; lfo_change(qq); break;
        case 0xbb:
            lfo_change(qq);
            qq->extendmode = (qq->extendmode & 0xfd) | ((*si++ & 1) << 1);
            lfo_change(qq);
            break;

        case 0xba: si = _volmask_set(qq, si); break;
        case 0xb9:
            lfo_change(qq);
            qq->delay = qq->delay2 = *si++; lfoinit_main(qq); break;
            lfo_change(qq);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(qq, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si = ppz_extpartset(qq, si); break;
        case 0xb3: qq->qdat2 = *si++; break;
        case 0xb2: qq->shift_def = *(int8_t *) si++; break;
        case 0xb1: qq->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::commands8(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    switch (al)
    {
        case 0xff: si = comat8(qq, si); break;
        case 0xfe: qq->qdata = *si++; break;
        case 0xfd: qq->volume = *si++; break;
        case 0xfc: si = comt(si); break;
        case 0xfb: pmdwork.tieflag |= 1; break;
        case 0xfa: qq->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(qq, si); break;
        case 0xf8: si = comedloop(qq, si); break;
        case 0xf7: si = comexloop(qq, si); break;
        case 0xf6: qq->partloop = si; break;
        case 0xf5: qq->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (qq->volume < (255 - 16)) qq->volume += 16;
            else qq->volume = 255;
            break;

        case 0xf3: if (qq->volume < 16) qq->volume = 0; else qq->volume -= 16; break;
        case 0xf2: si = lfoset(qq, si); break;
        case 0xf1: si = lfoswitch(qq, si); break;
        case 0xf0: si = psgenvset(qq, si); break;

        case 0xef: _OPNA->SetReg(0x100 + *si, *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = panset8(qq, si); break;				// FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: qq->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: si++; break;
            //追加 for V2.3
        case 0xe3:
            if (qq->volume < (255 - (*si))) qq->volume += (*si);
            else qq->volume = 255;
            si++;
            break;

        case 0xe2:
            if (qq->volume < *si) qq->volume = 0; else qq->volume -= *si;
            si++;
            break;
            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _OpenWork.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_pcm(qq, si); break;
        case 0xdd: si = vol_one_down(qq, si); break;
            //
        case 0xdc: _OpenWork.status = *si++; break;
        case 0xdb: _OpenWork.status += *si++; break;
            //
        case 0xda: si++; break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: qq->mdspd = qq->mdspd2 = *si++; qq->mdepth = *(int8_t *) si++; break;
        case 0xd5: qq->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(qq, si); break;
        case 0xd3: si = fm_efct_set(qq, si); break;
        case 0xd2:
            _OpenWork.fadeout_flag = 1;
            _OpenWork.fadeout_speed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
        case 0xcf: si++; break;
        case 0xce: si = pcmrepeat_set8(qq, si); break;
        case 0xcd: si = extend_psgenvset(qq, si); break;
        case 0xcc: si++; break;
        case 0xcb: qq->lfo_wave = *si++; break;
        case 0xca:
            qq->extendmode = (qq->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            qq->extendmode = (qq->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: qq->qdatb = *si++; break;
        case 0xc3: si = panset8_ex(qq, si); break;
        case 0xc2: qq->delay = qq->delay2 = *si++; lfoinit_main(qq); break;
        case 0xc1: break;
        case 0xc0: si = pcm_mml_part_mask8(qq, si); break;
        case 0xbf: si += 4; break;
        case 0xbe: si++; break;
        case 0xbd: si += 2; break;
        case 0xbc: si++; break;
        case 0xbb: si++; break;
        case 0xba: si++; break;
        case 0xb9: si++; break;
        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(qq, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si = ppz_extpartset(qq, si); break;
        case 0xb3: qq->qdat2 = *si++; break;
        case 0xb2: qq->shift_def = *(int8_t *) si++; break;
        case 0xb1: qq->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

uint8_t * PMD::commandsz(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    switch (al)
    {
        case 0xff: si = comatz(qq, si); break;
        case 0xfe: qq->qdata = *si++; break;
        case 0xfd: qq->volume = *si++; break;
        case 0xfc: si = comt(si); break;

        case 0xfb: pmdwork.tieflag |= 1; break;
        case 0xfa: qq->detune = *(int16_t *) si; si += 2; break;
        case 0xf9: si = comstloop(qq, si); break;
        case 0xf8: si = comedloop(qq, si); break;
        case 0xf7: si = comexloop(qq, si); break;
        case 0xf6: qq->partloop = si; break;
        case 0xf5: qq->shift = *(int8_t *) si++; break;
        case 0xf4:
            if (qq->volume < (255 - 16)) qq->volume += 16;
            else qq->volume = 255;
            break;

        case 0xf3: if (qq->volume < 16) qq->volume = 0; else qq->volume -= 16; break;
        case 0xf2: si = lfoset(qq, si); break;
        case 0xf1: si = lfoswitch(qq, si); break;
        case 0xf0: si = psgenvset(qq, si); break;

        case 0xef: _OPNA->SetReg(pmdwork.fmsel + *si, *(si + 1)); si += 2; break;
        case 0xee: si++; break;
        case 0xed: si++; break;
        case 0xec: si = pansetz(qq, si); break;				// FOR SB2
        case 0xeb: si = rhykey(si); break;
        case 0xea: si = rhyvs(si); break;
        case 0xe9: si = rpnset(si); break;
        case 0xe8: si = rmsvs(si); break;
            //
        case 0xe7: qq->shift += *(int8_t *) si++; break;
        case 0xe6: si = rmsvs_sft(si); break;
        case 0xe5: si = rhyvs_sft(si); break;
            //
        case 0xe4: si++; break;
            //追加 for V2.3
        case 0xe3:
            if (qq->volume < (255 - (*si))) qq->volume += (*si);
            else qq->volume = 255;
            si++;
            break;

        case 0xe2:
            if (qq->volume < *si) qq->volume = 0; else qq->volume -= *si;
            si++;
            break;
            //
        case 0xe1: si++; break;
        case 0xe0: si++; break;
            //
        case 0xdf: _OpenWork.syousetu_lng = *si++; break;
            //
        case 0xde: si = vol_one_up_pcm(qq, si); break;
        case 0xdd: si = vol_one_down(qq, si); break;
            //
        case 0xdc: _OpenWork.status = *si++; break;
        case 0xdb: _OpenWork.status += *si++; break;
            //
        case 0xda: si = portaz(qq, si); break;
            //
        case 0xd9: si++; break;
        case 0xd8: si++; break;
        case 0xd7: si++; break;
            //
        case 0xd6: qq->mdspd = qq->mdspd2 = *si++; qq->mdepth = *(int8_t *) si++; break;
        case 0xd5: qq->detune += *(int16_t *) si; si += 2; break;
            //
        case 0xd4: si = ssg_efct_set(qq, si); break;
        case 0xd3: si = fm_efct_set(qq, si); break;
        case 0xd2:
            _OpenWork.fadeout_flag = 1;
            _OpenWork.fadeout_speed = *si++;
            break;
            //
        case 0xd1: si++; break;
        case 0xd0: si++; break;
            //
        case 0xcf: si++; break;
        case 0xce: si = ppzrepeat_set(qq, si); break;
        case 0xcd: si = extend_psgenvset(qq, si); break;
        case 0xcc: si++; break;
        case 0xcb: qq->lfo_wave = *si++; break;
        case 0xca:
            qq->extendmode = (qq->extendmode & 0xfd) | ((*si++ & 1) << 1);
            break;

        case 0xc9:
            qq->extendmode = (qq->extendmode & 0xfb) | ((*si++ & 1) << 2);
            break;

        case 0xc8: si += 3; break;
        case 0xc7: si += 3; break;
        case 0xc6: si += 6; break;
        case 0xc5: si++; break;
        case 0xc4: qq->qdatb = *si++; break;
        case 0xc3: si = pansetz_ex(qq, si); break;
        case 0xc2: qq->delay = qq->delay2 = *si++; lfoinit_main(qq); break;
        case 0xc1: break;
        case 0xc0: si = ppz_mml_part_mask(qq, si); break;
        case 0xbf: lfo_change(qq); si = lfoset(qq, si); lfo_change(qq); break;
        case 0xbe: si = _lfoswitch(qq, si); break;
        case 0xbd:
            lfo_change(qq);
            qq->mdspd = qq->mdspd2 = *si++;
            qq->mdepth = *(int8_t *) si++;
            lfo_change(qq);
            break;

        case 0xbc: lfo_change(qq); qq->lfo_wave = *si++; lfo_change(qq); break;
        case 0xbb:
            lfo_change(qq);
            qq->extendmode = (qq->extendmode & 0xfd) | ((*si++ & 1) << 1);
            lfo_change(qq);
            break;

        case 0xba: si = _volmask_set(qq, si); break;
        case 0xb9:
            lfo_change(qq);
            qq->delay = qq->delay2 = *si++; lfoinit_main(qq); break;
            lfo_change(qq);
            break;

        case 0xb8: si += 2; break;
        case 0xb7: si = mdepth_count(qq, si); break;
        case 0xb6: si++; break;
        case 0xb5: si += 2; break;
        case 0xb4: si += 16; break;
        case 0xb3: qq->qdat2 = *si++; break;
        case 0xb2: qq->shift_def = *(int8_t *) si++; break;
        case 0xb1: qq->qdat3 = *si++; break;

        default:
            si--;
            *si = 0x80;
    }

    return si;
}

//	COMMAND '@' [PROGRAM CHANGE]
uint8_t * PMD::comat(PartState * qq, uint8_t * si)
{
    uint8_t * bx;
    int		al, dl;

    qq->voicenum = al = *si++;
    dl = qq->voicenum;

    if (qq->partmask == 0)
    {	// パートマスクされているか？
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

        //	FM3chで、マスクされていた場合、fm3_alg_fbを設定
        if (pmdwork.partb == 3 && qq->neiromask)
        {
            if (pmdwork.fmsel == 0)
            {
                // in. dl = alg/fb
                if ((qq->slotmask & 0x10) == 0)
                {
                    al = pmdwork.fm3_alg_fb & 0x38;		// fbは前の値を使用
                    dl = (dl & 7) | al;
                }

                pmdwork.fm3_alg_fb = dl;
                qq->alg_fb = al;
            }
        }
    }
    return si;
}

//	音色の設定
//		INPUTS	-- [PARTB]
//			-- dl [TONE_NUMBER]
//			-- di [PART_DATA_ADDRESS]
void PMD::neiroset(PartState * qq, int dl)
{
    uint8_t * bx;
    int		ah, al, dh;

    bx = toneadr_calc(qq, dl);
    if (silence_fmpart(qq))
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
    //	音色設定メイン
    //=========================================================================
    //-------------------------------------------------------------------------
    //	AL/FBを設定
    //-------------------------------------------------------------------------

    dh = 0xb0 - 1 + pmdwork.partb;

    if (pmdwork.af_check)
    {		// ALG/FBは設定しないmodeか？
        dl = qq->alg_fb;
    }
    else
    {
        dl = bx[24];
    }

    if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
    {
        if (pmdwork.af_check != 0)
        {	// ALG/FBは設定しないmodeか？
            dl = pmdwork.fm3_alg_fb;
        }
        else
        {
            if ((qq->slotmask & 0x10) == 0)
            {	// slot1を使用しているか？
                dl = (pmdwork.fm3_alg_fb & 0x38) | (dl & 7);
            }
            pmdwork.fm3_alg_fb = dl;
        }
    }

    _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    qq->alg_fb = dl;
    dl &= 7;		// dl = algo

    //-------------------------------------------------------------------------
    //	Carrierの位置を調べる (VolMaskにも設定)
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
    ah = carrier_table[dl + 8];	// slot2/3の逆転データ(not済み)
    al = qq->neiromask;
    ah &= al;				// AH=TL用のmask / AL=その他用のmask

    //-------------------------------------------------------------------------
    //	各音色パラメータを設定 (TLはモジュレータのみ)
    //-------------------------------------------------------------------------

    dh = 0x30 - 1 + pmdwork.partb;
    dl = *bx++;				// DT/ML
    if (al & 0x80) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;				// TL
    if (ah & 0x80) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x40) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x20) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (ah & 0x10) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;				// KS/AR
    if (al & 0x08) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;				// AM/DR
    if (al & 0x80) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;				// SR
    if (al & 0x08) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x04) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x02) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x01) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;				// SL/RR
    if (al & 0x80)
    {
        _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    }
    dh += 4;

    dl = *bx++;
    if (al & 0x40) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x20) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    dl = *bx++;
    if (al & 0x10) _OPNA->SetReg(pmdwork.fmsel + dh, dl);
    dh += 4;

    /*
        dl = *bx++;				// SL/RR
        if(al & 0x80) opna->SetReg(pmdwork.fmsel + dh, dl);
        dh+=4;

        dl = *bx++;
        if(al & 0x40) opna->SetReg(pmdwork.fmsel + dh, dl);
        dh+=4;

        dl = *bx++;
        if(al & 0x20) opna->SetReg(pmdwork.fmsel + dh, dl);
        dh+=4;

        dl = *bx++;
        if(al & 0x10) opna->SetReg(pmdwork.fmsel + dh, dl);
        dh+=4;
    */

    //-------------------------------------------------------------------------
    //	SLOT毎のTLをワークに保存
    //-------------------------------------------------------------------------
    bx -= 20;
    qq->slot1 = bx[0];
    qq->slot3 = bx[1];
    qq->slot2 = bx[2];
    qq->slot4 = bx[3];
}

//	[PartB]のパートの音を完璧に消す (TL=127 and RR=15 and KEY-OFF)
//		cy=1 ･･･ 全スロットneiromaskされている
int PMD::silence_fmpart(PartState * qq)
{
    int		dh;

    if (qq->neiromask == 0)
    {
        return 1;
    }

    dh = pmdwork.partb + 0x40 - 1;

    if (qq->neiromask & 0x80)
    {
        _OPNA->SetReg(pmdwork.fmsel + dh, 127);
        _OPNA->SetReg((pmdwork.fmsel + 0x40) + dh, 127);
    }
    dh += 4;

    if (qq->neiromask & 0x40)
    {
        _OPNA->SetReg(pmdwork.fmsel + dh, 127);
        _OPNA->SetReg(pmdwork.fmsel + 0x40 + dh, 127);
    }
    dh += 4;

    if (qq->neiromask & 0x20)
    {
        _OPNA->SetReg(pmdwork.fmsel + dh, 127);
        _OPNA->SetReg(pmdwork.fmsel + 0x40 + dh, 127);
    }
    dh += 4;

    if (qq->neiromask & 0x10)
    {
        _OPNA->SetReg(pmdwork.fmsel + dh, 127);
        _OPNA->SetReg(pmdwork.fmsel + 0x40 + dh, 127);
    }

    kof1(qq);
    return 0;
}

//	TONE DATA START ADDRESS を計算
//		input	dl	tone_number
//		output	bx	address
uint8_t * PMD::toneadr_calc(PartState * qq, int dl)
{
    uint8_t * bx;

    if (_OpenWork.prg_flg == 0 && qq != &EffPart)
    {
        return _OpenWork.tondat + (dl << 5);
    }
    else
    {
        bx = _OpenWork.prgdat_adr;

        while (*bx != dl)
        {
            bx += 26;
            if (bx > _MData + sizeof(_MData) - 26)
            {
                return _OpenWork.prgdat_adr + 1;	// 見つからないときは最初の音色を設定
            }
        }

        return bx + 1;
    }
}

// FM tone generator hard LFO setting (V2.4 expansion)
uint8_t * PMD::hlfo_set(PartState * qq, uint8_t * si)
{
    qq->fmpan = (qq->fmpan & 0xc0) | *si++;

    if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
    {
        // 2608の時のみなので part_eはありえない
        //	FM3の場合は 4つのパート総て設定
        FMPart[2].fmpan = qq->fmpan;
        ExtPart[0].fmpan = qq->fmpan;
        ExtPart[1].fmpan = qq->fmpan;
        ExtPart[2].fmpan = qq->fmpan;
    }

    if (qq->partmask == 0)
    {		// パートマスクされているか？
        _OPNA->SetReg(pmdwork.fmsel + pmdwork.partb + 0xb4 - 1,
            calc_panout(qq));
    }
    return si;
}

// Change only one volume (V2.7 expansion)
uint8_t * PMD::vol_one_up_fm(PartState * qq, uint8_t * si)
{
    int		al;

    al = (int) qq->volume + 1 + *si++;
    if (al > 128) al = 128;

    qq->volpush = al;
    pmdwork.volpush_flag = 1;
    return si;
}

// Portamento (FM)
uint8_t * PMD::porta(PartState * qq, uint8_t * si)
{
    int		ax, cx, cl, bx, bh;

    if (qq->partmask)
    {
        qq->fnum = 0;		//休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

        if (--pmdwork.volpush_flag)
        {
            qq->volpush = 0;
        }

        pmdwork.tieflag = 0;
        pmdwork.volpush_flag = 0;
        pmdwork.loop_work &= qq->loopcheck;

        return si + 3;		// 読み飛ばす	(Mask時)
    }

    fnumset(qq, oshift(qq, lfoinit(qq, *si++)));

    cx = qq->fnum;
    cl = qq->onkai;
    fnumset(qq, oshift(qq, *si++));
    bx = qq->fnum;			// bx=ポルタメント先のfnum値

    qq->onkai = cl;
    qq->fnum = cx;			// cx=ポルタメント元のfnum値

    bh = (int) ((bx / 256) & 0x38) - ((cx / 256) & 0x38);	// 先のoctarb - 元のoctarb
    if (bh)
    {
        bh /= 8;
        ax = bh * 0x26a;			// ax = 26ah * octarb差
    }
    else
    {
        ax = 0;
    }

    bx = (bx & 0x7ff) - (cx & 0x7ff);
    ax += bx;				// ax=26ah*octarb差 + 音程差

    qq->leng = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->leng;	// 商
    qq->porta_num3 = ax % qq->leng;	// 余り
    qq->lfoswi |= 8;				// Porta ON

    if (qq->volpush && qq->onkai != 255)
    {
        if (--pmdwork.volpush_flag)
        {
            pmdwork.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volset(qq);
    otodasi(qq);
    keyon(qq);

    qq->keyon_flag++;
    qq->address = si;

    pmdwork.tieflag = 0;
    pmdwork.volpush_flag = 0;

    if (*si == 0xfb)
    {		// '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }
    else
    {
        qq->keyoff_flag = 0;
    }
    pmdwork.loop_work &= qq->loopcheck;
    return si;
}

//	FM slotmask set
uint8_t * PMD::slotmask_set(PartState * qq, uint8_t * si)
{
    uint8_t * bx;
    int		ah, al, bl;

    ah = al = *si++;

    if (al &= 0x0f)
    {
        qq->carrier = al << 4;
    }
    else
    {
        if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
        {
            bl = pmdwork.fm3_alg_fb;
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
        {
            qq->partmask |= 0x20;	// s0の時パートマスク
        }
        else
        {
            qq->partmask &= 0xdf;	// s0以外の時パートマスク解除
        }

        if (ch3_setting(qq))
        {		// FM3chの場合のみ ch3modeの変更処理
// ch3なら、それ以前のFM3パートでkeyon処理
            if (qq != &FMPart[2])
            {
                if (FMPart[2].partmask == 0 &&
                    (FMPart[2].keyoff_flag & 1) == 0)
                {
                    keyon(&FMPart[2]);
                }

                if (qq != &ExtPart[0])
                {
                    if (ExtPart[0].partmask == 0 &&
                        (ExtPart[0].keyoff_flag & 1) == 0)
                    {
                        keyon(&ExtPart[0]);
                    }

                    if (qq != &ExtPart[1])
                    {
                        if (ExtPart[1].partmask == 0 &&
                            (ExtPart[1].keyoff_flag & 1) == 0)
                        {
                            keyon(&ExtPart[1]);
                        }
                    }
                }
            }
        }

        ah = 0;
        if (qq->slotmask & 0x80) ah += 0x11;		// slot4
        if (qq->slotmask & 0x40) ah += 0x44;		// slot3
        if (qq->slotmask & 0x20) ah += 0x22;		// slot2
        if (qq->slotmask & 0x10) ah += 0x88;		// slot1
        qq->neiromask = ah;
    }
    return si;
}

//	Slot Detune Set
uint8_t * PMD::slotdetune_set(PartState * qq, uint8_t * si)
{
    int		ax, bl;

    if (pmdwork.partb != 3 || pmdwork.fmsel)
    {
        return si + 3;
    }

    bl = *si++;
    ax = *(int16_t *) si;
    si += 2;

    if (bl & 1)
    {
        _OpenWork.slot_detune1 = ax;
    }

    if (bl & 2)
    {
        _OpenWork.slot_detune2 = ax;
    }

    if (bl & 4)
    {
        _OpenWork.slot_detune3 = ax;
    }

    if (bl & 8)
    {
        _OpenWork.slot_detune4 = ax;
    }

    if (_OpenWork.slot_detune1 || _OpenWork.slot_detune2 ||
        _OpenWork.slot_detune3 || _OpenWork.slot_detune4)
    {
        pmdwork.slotdetune_flag = 1;
    }
    else
    {
        pmdwork.slotdetune_flag = 0;
        _OpenWork.slot_detune1 = 0;
    }
    ch3mode_set(qq);
    return si;
}

//	Slot Detune Set(相対)
uint8_t * PMD::slotdetune_set2(PartState * qq, uint8_t * si)
{
    int		ax, bl;

    if (pmdwork.partb != 3 || pmdwork.fmsel)
    {
        return si + 3;
    }

    bl = *si++;
    ax = *(int16_t *) si;
    si += 2;

    if (bl & 1)
    {
        _OpenWork.slot_detune1 += ax;
    }

    if (bl & 2)
    {
        _OpenWork.slot_detune2 += ax;
    }

    if (bl & 4)
    {
        _OpenWork.slot_detune3 += ax;
    }

    if (bl & 8)
    {
        _OpenWork.slot_detune4 += ax;
    }

    if (_OpenWork.slot_detune1 || _OpenWork.slot_detune2 ||
        _OpenWork.slot_detune3 || _OpenWork.slot_detune4)
    {
        pmdwork.slotdetune_flag = 1;
    }
    else
    {
        pmdwork.slotdetune_flag = 0;
        _OpenWork.slot_detune1 = 0;
    }
    ch3mode_set(qq);
    return si;
}

void PMD::fm3_partinit(PartState * qq, uint8_t * ax)
{
    qq->address = ax;
    qq->leng = 1;					// ｱﾄ 1ｶｳﾝﾄ ﾃﾞ ｴﾝｿｳ ｶｲｼ
    qq->keyoff_flag = -1;			// 現在keyoff中
    qq->mdc = -1;					// MDepth Counter (無限)
    qq->mdc2 = -1;					//
    qq->_mdc = -1;					//
    qq->_mdc2 = -1;					//
    qq->onkai = 255;				// rest
    qq->onkai_def = 255;			// rest
    qq->volume = 108;				// FM  VOLUME DEFAULT= 108
    qq->fmpan = FMPart[2].fmpan;	// FM PAN = CH3と同じ
    qq->partmask |= 0x20;			// s0用 partmask
}

//	FM3ch 拡張パートセット
uint8_t * PMD::fm3_extpartset(PartState * qq, uint8_t * si)
{
    int16_t ax;

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&ExtPart[0], &_OpenWork.mmlbuf[ax]);

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&ExtPart[1], &_OpenWork.mmlbuf[ax]);

    ax = *(int16_t *) si;
    si += 2;
    if (ax) fm3_partinit(&ExtPart[2], &_OpenWork.mmlbuf[ax]);
    return si;
}

//	ppz 拡張パートセット
uint8_t * PMD::ppz_extpartset(PartState * qq, uint8_t * si)
{
    int16_t	ax;
    int		i;

    for (i = 0; i < 8; i++)
    {
        ax = *(int16_t *) si;
        si += 2;
        if (ax)
        {
            PPZ8Part[i].address = &_OpenWork.mmlbuf[ax];
            PPZ8Part[i].leng = 1;					// ｱﾄ 1ｶｳﾝﾄ ﾃﾞ ｴﾝｿｳ ｶｲｼ
            PPZ8Part[i].keyoff_flag = -1;			// 現在keyoff中
            PPZ8Part[i].mdc = -1;					// MDepth Counter (無限)
            PPZ8Part[i].mdc2 = -1;					//
            PPZ8Part[i]._mdc = -1;					//
            PPZ8Part[i]._mdc2 = -1;					//
            PPZ8Part[i].onkai = 255;				// rest
            PPZ8Part[i].onkai_def = 255;			// rest
            PPZ8Part[i].volume = 128;				// PCM VOLUME DEFAULT= 128
            PPZ8Part[i].fmpan = 5;					// PAN=Middle
        }
    }
    return si;
}

//	音量マスクslotの設定
uint8_t * PMD::volmask_set(PartState * qq, uint8_t * si)
{
    int		al;

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

//	0c0hの追加special命令
uint8_t * PMD::special_0c0h(PartState * qq, uint8_t * si, uint8_t al)
{
    switch (al)
    {
        case 0xff: _OpenWork.fm_voldown = *si++; break;
        case 0xfe: si = _vd_fm(qq, si); break;
        case 0xfd: _OpenWork.ssg_voldown = *si++; break;
        case 0xfc: si = _vd_ssg(qq, si); break;
        case 0xfb: _OpenWork.pcm_voldown = *si++; break;
        case 0xfa: si = _vd_pcm(qq, si); break;
        case 0xf9: _OpenWork.rhythm_voldown = *si++; break;
        case 0xf8: si = _vd_rhythm(qq, si); break;
        case 0xf7: _OpenWork.pcm86_vol = (*si++ & 1); break;
        case 0xf6: _OpenWork.ppz_voldown = *si++; break;
        case 0xf5: si = _vd_ppz(qq, si); break;
        default:
            si--;
            *si = 0x80;
    }
    return si;
}

uint8_t * PMD::_vd_fm(PartState * qq, uint8_t * si)
{
    int		al;

    al = *(int8_t *) si++;
    if (al)
    {
        _OpenWork.fm_voldown = Limit(al + _OpenWork.fm_voldown, 255, 0);
    }
    else
    {
        _OpenWork.fm_voldown = _OpenWork._fm_voldown;
    }
    return si;
}

uint8_t * PMD::_vd_ssg(PartState * qq, uint8_t * si)
{
    int		al;

    al = *(int8_t *) si++;
    if (al)
    {
        _OpenWork.ssg_voldown = Limit(al + _OpenWork.ssg_voldown, 255, 0);
    }
    else
    {
        _OpenWork.ssg_voldown = _OpenWork._ssg_voldown;
    }
    return si;
}

uint8_t * PMD::_vd_pcm(PartState * qq, uint8_t * si)
{
    int		al;

    al = *(int8_t *) si++;
    if (al)
    {
        _OpenWork.pcm_voldown = Limit(al + _OpenWork.pcm_voldown, 255, 0);
    }
    else
    {
        _OpenWork.pcm_voldown = _OpenWork._pcm_voldown;
    }
    return si;
}

uint8_t * PMD::_vd_rhythm(PartState * qq, uint8_t * si)
{
    int		al;

    al = *(int8_t *) si++;
    if (al)
    {
        _OpenWork.rhythm_voldown = Limit(al + _OpenWork.rhythm_voldown, 255, 0);
    }
    else
    {
        _OpenWork.rhythm_voldown = _OpenWork._rhythm_voldown;
    }
    return si;
}

uint8_t * PMD::_vd_ppz(PartState * qq, uint8_t * si)
{
    int		al;

    al = *(int8_t *) si++;
    if (al)
    {
        _OpenWork.ppz_voldown = Limit(al + _OpenWork.ppz_voldown, 255, 0);
    }
    else
    {
        _OpenWork.ppz_voldown = _OpenWork._ppz_voldown;
    }
    return si;
}

// Mask on/off for playing parts
uint8_t * PMD::fm_mml_part_mask(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    if (al >= 2)
    {
        si = special_0c0h(qq, si, al);
    }
    else if (al)
    {
        qq->partmask |= 0x40;
        if (qq->partmask == 0x40)
        {
            silence_fmpart(qq);	// 音消去
        }
    }
    else
    {
        if ((qq->partmask &= 0xbf) == 0)
        {
            neiro_reset(qq);		// 音色再設定
        }
    }
    return si;
}

uint8_t * PMD::ssg_mml_part_mask(PartState * qq, uint8_t * si)
{
    int		ah, al;

    al = *si++;
    if (al >= 2)
    {
        si = special_0c0h(qq, si, al);
    }
    else if (al)
    {
        qq->partmask |= 0x40;
        if (qq->partmask == 0x40)
        {
            ah = ((1 << (pmdwork.partb - 1)) | (4 << pmdwork.partb));
            al = _OPNA->GetReg(0x07);
            _OPNA->SetReg(0x07, ah | al);		// PSG keyoff
        }
    }
    else
    {
        qq->partmask &= 0xbf;
    }
    return si;
}

uint8_t * PMD::rhythm_mml_part_mask(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    if (al >= 2)
    {
        si = special_0c0h(qq, si, al);
    }
    else if (al)
    {
        qq->partmask |= 0x40;
    }
    else
    {
        qq->partmask &= 0xbf;
    }
    return si;
}

uint8_t * PMD::pcm_mml_part_mask(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    if (al >= 2)
    {
        si = special_0c0h(qq, si, al);
    }
    else if (al)
    {
        qq->partmask |= 0x40;
        if (qq->partmask == 0x40)
        {
            _OPNA->SetReg(0x101, 0x02);		// PAN=0 / x8 bit mode
            _OPNA->SetReg(0x100, 0x01);		// PCM RESET
        }
    }
    else
    {
        qq->partmask &= 0xbf;
    }
    return si;
}

uint8_t * PMD::pcm_mml_part_mask8(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    if (al >= 2)
    {
        si = special_0c0h(qq, si, al);
    }
    else if (al)
    {
        qq->partmask |= 0x40;
        if (qq->partmask == 0x40)
        {
            _P86->Stop();
        }
    }
    else
    {
        qq->partmask &= 0xbf;
    }
    return si;
}

uint8_t * PMD::ppz_mml_part_mask(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;
    if (al >= 2)
    {
        si = special_0c0h(qq, si, al);
    }
    else if (al)
    {
        qq->partmask |= 0x40;
        if (qq->partmask == 0x40)
        {
            _PPZ8->Stop(pmdwork.partb);
        }
    }
    else
    {
        qq->partmask &= 0xbf;
    }
    return si;
}

// Reset the tone of the FM sound source
void PMD::neiro_reset(PartState * qq)
{
    int		dh, al, s1, s2, s3, s4;

    if (qq->neiromask == 0) return;

    s1 = qq->slot1;
    s2 = qq->slot2;
    s3 = qq->slot3;
    s4 = qq->slot4;
    pmdwork.af_check = 1;
    neiroset(qq, qq->voicenum);		// 音色復帰
    pmdwork.af_check = 0;
    qq->slot1 = s1;
    qq->slot2 = s2;
    qq->slot3 = s3;
    qq->slot4 = s4;

    al = ((~qq->carrier) & qq->slotmask) & 0xf0;
    // al<- TLを再設定していいslot 4321xxxx
    if (al)
    {
        dh = 0x4c - 1 + pmdwork.partb;	// dh=TL FM Port Address
        if (al & 0x80) _OPNA->SetReg(pmdwork.fmsel + dh, s4);

        dh -= 8;
        if (al & 0x40) _OPNA->SetReg(pmdwork.fmsel + dh, s3);

        dh += 4;
        if (al & 0x20) _OPNA->SetReg(pmdwork.fmsel + dh, s2);

        dh -= 8;
        if (al & 0x10) _OPNA->SetReg(pmdwork.fmsel + dh, s1);
    }

    dh = pmdwork.partb + 0xb4 - 1;
    _OPNA->SetReg(pmdwork.fmsel + dh, calc_panout(qq));
}

uint8_t * PMD::_lfoswitch(PartState * qq, uint8_t * si)
{
    qq->lfoswi = (qq->lfoswi & 0x8f) | ((*si++ & 7) << 4);
    lfo_change(qq);
    lfoinit_main(qq);
    lfo_change(qq);
    return si;
}

uint8_t * PMD::_volmask_set(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++ & 0x0f;
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

//	TL変化
uint8_t * PMD::tl_set(PartState * qq, uint8_t * si)
{
    int		ah, al, ch, dh, dl;

    dh = 0x40 - 1 + pmdwork.partb;		// dh=TL FM Port Address
    al = *(int8_t *) si++;
    ah = al & 0x0f;
    ch = (qq->slotmask >> 4) | ((qq->slotmask << 4) & 0xf0);
    ah &= ch;							// ah=変化させるslot 00004321
    dl = *(int8_t *) si++;

    if (al >= 0)
    {
        dl &= 127;
        if (ah & 1)
        {
            qq->slot1 = dl;
            if (qq->partmask == 0)
            {
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
            }
        }

        dh += 8;
        if (ah & 2)
        {
            qq->slot2 = dl;
            if (qq->partmask == 0)
            {
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
            }
        }

        dh -= 4;
        if (ah & 4)
        {
            qq->slot3 = dl;
            if (qq->partmask == 0)
            {
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
            }
        }

        dh += 8;
        if (ah & 8)
        {
            qq->slot4 = dl;
            if (qq->partmask == 0)
            {
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
            }
        }
    }
    else
    {
        //	相対変化
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
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
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
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
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
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
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
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
            }
            qq->slot4 = dl;
        }
    }
    return si;
}

//	FB変化
uint8_t * PMD::fb_set(PartState * qq, uint8_t * si)
{
    int		al, dh, dl;


    dh = pmdwork.partb + 0xb0 - 1;	// dh=ALG/FB port address
    al = *(int8_t *) si++;
    if (al >= 0)
    {
        // in	al 00000xxx 設定するFB
        al = ((al << 3) & 0xff) | (al >> 5);

        // in	al 00xxx000 設定するFB
        if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
        {
            if ((qq->slotmask & 0x10) == 0) return si;
            dl = (pmdwork.fm3_alg_fb & 7) | al;
            pmdwork.fm3_alg_fb = dl;
        }
        else
        {
            dl = (qq->alg_fb & 7) | al;
        }
        _OPNA->SetReg(pmdwork.fmsel + dh, dl);
        qq->alg_fb = dl;
        return si;
    }
    else
    {
        if ((al & 0x40) == 0) al &= 7;
        if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
        {
            dl = pmdwork.fm3_alg_fb;
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
                if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
                {
                    if ((qq->slotmask & 0x10) == 0) return si;

                    dl = (pmdwork.fm3_alg_fb & 7) | al;
                    pmdwork.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (qq->alg_fb & 7) | al;
                }
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
                qq->alg_fb = dl;
                return si;
            }
            else
            {
                // in	al 00000xxx 設定するFB
                al = ((al << 3) & 0xff) | (al >> 5);

                // in	al 00xxx000 設定するFB
                if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
                {
                    if ((qq->slotmask & 0x10) == 0) return si;
                    dl = (pmdwork.fm3_alg_fb & 7) | al;
                    pmdwork.fm3_alg_fb = dl;
                }
                else
                {
                    dl = (qq->alg_fb & 7) | al;
                }
                _OPNA->SetReg(pmdwork.fmsel + dh, dl);
                qq->alg_fb = dl;
                return si;
            }
        }
        else
        {
            al = 0;
            if (pmdwork.partb == 3 && pmdwork.fmsel == 0)
            {
                if ((qq->slotmask & 0x10) == 0) return si;

                dl = (pmdwork.fm3_alg_fb & 7) | al;
                pmdwork.fm3_alg_fb = dl;
            }
            else
            {
                dl = (qq->alg_fb & 7) | al;
            }
            _OPNA->SetReg(pmdwork.fmsel + dh, dl);
            qq->alg_fb = dl;
            return si;
        }
    }
}

//	COMMAND 't' [TEMPO CHANGE1]
//	COMMAND 'T' [TEMPO CHANGE2]
//	COMMAND 't±' [TEMPO CHANGE 相対1]
//	COMMAND 'T±' [TEMPO CHANGE 相対2]
uint8_t * PMD::comt(uint8_t * si)
{
    int		al;

    al = *si++;
    if (al < 251)
    {
        _OpenWork.tempo_d = al;		// T (FC)
        _OpenWork.tempo_d_push = al;
        calc_tb_tempo();

    }
    else if (al == 0xff)
    {
        al = *si++;					// t (FC FF)
        if (al < 18) al = 18;
        _OpenWork.tempo_48 = al;
        _OpenWork.tempo_48_push = al;
        calc_tempo_tb();

    }
    else if (al == 0xfe)
    {
        al = int8_t(*si++);			// T± (FC FE)
        if (al >= 0)
        {
            al += _OpenWork.tempo_d_push;
        }
        else
        {
            al += _OpenWork.tempo_d_push;
            if (al < 0)
            {
                al = 0;
            }
        }

        if (al > 250) al = 250;

        _OpenWork.tempo_d = al;
        _OpenWork.tempo_d_push = al;
        calc_tb_tempo();

    }
    else
    {
        al = int8_t(*si++);			// t± (FC FD)
        if (al >= 0)
        {
            al += _OpenWork.tempo_48_push;
            if (al > 255)
            {
                al = 255;
            }
        }
        else
        {
            al += _OpenWork.tempo_48_push;
            if (al < 0) al = 18;
        }
        _OpenWork.tempo_48 = al;
        _OpenWork.tempo_48_push = al;
        calc_tempo_tb();
    }
    return si;
}

//	COMMAND '[' [ﾙｰﾌﾟ ｽﾀｰﾄ]
uint8_t * PMD::comstloop(PartState * qq, uint8_t * si)
{
    uint8_t * ax;

    if (qq == &EffPart)
    {
        ax = _OpenWork.efcdat;
    }
    else
    {
        ax = _OpenWork.mmlbuf;
    }
    ax[*(uint16_t *) si + 1] = 0;
    si += 2;
    return si;
}

//	COMMAND	']' [ﾙｰﾌﾟ ｴﾝﾄﾞ]
uint8_t * PMD::comedloop(PartState * qq, uint8_t * si)
{
    int		ah, al, ax;
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
    {			// 0 ﾅﾗ ﾑｼﾞｮｳｹﾝ ﾙｰﾌﾟ
        si++;
        qq->loopcheck = 1;
    }

    ax = *(uint16_t *) si + 2;

    if (qq == &EffPart)
    {
        si = _OpenWork.efcdat + ax;
    }
    else
    {
        si = _OpenWork.mmlbuf + ax;
    }
    return si;
}

//	COMMAND	':' [ﾙｰﾌﾟ ﾀﾞｯｼｭﾂ]
uint8_t * PMD::comexloop(PartState * qq, uint8_t * si)
{
    uint8_t * bx;
    int		dl;


    if (qq == &EffPart)
    {
        bx = _OpenWork.efcdat;
    }
    else
    {
        bx = _OpenWork.mmlbuf;
    }


    bx += *(uint16_t *) si;
    si += 2;

    dl = *bx++ - 1;
    if (dl != *bx) return si;
    si = bx + 3;
    return si;
}

//	LFO ﾊﾟﾗﾒｰﾀ ｾｯﾄ
uint8_t * PMD::lfoset(PartState * qq, uint8_t * si)
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

//	LFO SWITCH
uint8_t * PMD::lfoswitch(PartState * qq, uint8_t * si)
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

//	PSG ENVELOPE SET
uint8_t * PMD::psgenvset(PartState * qq, uint8_t * si)
{
    qq->eenv_ar = *si; qq->eenv_arc = *si++;
    qq->eenv_dr = *(int8_t *) si++;
    qq->eenv_sr = *si; qq->eenv_src = *si++;
    qq->eenv_rr = *si; qq->eenv_rrc = *si++;
    if (qq->envf == -1)
    {
        qq->envf = 2;		// RR
        qq->eenv_volume = -15;		// volume
    }
    return si;
}

//	"\?" COMMAND [ OPNA Rhythm Keyon/Dump ]
uint8_t * PMD::rhykey(uint8_t * si)
{
    int		al, dl;

    if ((dl = (*si++ & _OpenWork.rhythmmask)) == 0)
    {
        return si;
    }

    if (_OpenWork.fadeout_volume != 0)
    {
        al = ((256 - _OpenWork.fadeout_volume) * _OpenWork.rhyvol) >> 8;
        _OPNA->SetReg(0x11, al);
    }

    if (dl < 0x80)
    {
        if (dl & 0x01) _OPNA->SetReg(0x18, _OpenWork.rdat[0]);
        if (dl & 0x02) _OPNA->SetReg(0x19, _OpenWork.rdat[1]);
        if (dl & 0x04) _OPNA->SetReg(0x1a, _OpenWork.rdat[2]);
        if (dl & 0x08) _OPNA->SetReg(0x1b, _OpenWork.rdat[3]);
        if (dl & 0x10) _OPNA->SetReg(0x1c, _OpenWork.rdat[4]);
        if (dl & 0x20) _OPNA->SetReg(0x1d, _OpenWork.rdat[5]);
    }

    _OPNA->SetReg(0x10, dl);
    if (dl >= 0x80)
    {
        if (dl & 0x01) _OpenWork.rdump_bd++;
        if (dl & 0x02) _OpenWork.rdump_sd++;
        if (dl & 0x04) _OpenWork.rdump_sym++;
        if (dl & 0x08) _OpenWork.rdump_hh++;
        if (dl & 0x10) _OpenWork.rdump_tom++;
        if (dl & 0x20) _OpenWork.rdump_rim++;
        _OpenWork.rshot_dat &= (~dl);
    }
    else
    {
        if (dl & 0x01) _OpenWork.rshot_bd++;
        if (dl & 0x02) _OpenWork.rshot_sd++;
        if (dl & 0x04) _OpenWork.rshot_sym++;
        if (dl & 0x08) _OpenWork.rshot_hh++;
        if (dl & 0x10) _OpenWork.rshot_tom++;
        if (dl & 0x20) _OpenWork.rshot_rim++;
        _OpenWork.rshot_dat |= dl;
    }

    return si;
}

//	"\v?n" COMMAND
uint8_t * PMD::rhyvs(uint8_t * si)
{
    int * bx;
    int		dh, dl;

    dl = *si & 0x1f;
    dh = *si++ >> 5;
    bx = &_OpenWork.rdat[dh - 1];
    dh = 0x18 - 1 + dh;
    dl |= (*bx & 0xc0);
    *bx = dl;

    _OPNA->SetReg(dh, dl);

    return si;
}

uint8_t * PMD::rhyvs_sft(uint8_t * si)
{
    int * bx;
    int		al, dh, dl;

    bx = &_OpenWork.rdat[*si - 1];
    dh = *si++ + 0x18 - 1;
    dl = *bx & 0x1f;

    al = (*(int8_t *) si++ + dl);
    if (al >= 32)
    {
        al = 31;
    }
    else if (al < 0)
    {
        al = 0;
    }

    dl = (al &= 0x1f);
    dl = *bx = ((*bx & 0xe0) | dl);
    _OPNA->SetReg(dh, dl);

    return si;
}

//	"\p?" COMMAND
uint8_t * PMD::rpnset(uint8_t * si)
{
    int * bx;
    int		dh, dl;

    dl = (*si & 3) << 6;

    dh = (*si++ >> 5) & 0x07;
    bx = &_OpenWork.rdat[dh - 1];

    dh += 0x18 - 1;
    dl |= (*bx & 0x1f);
    *bx = dl;
    _OPNA->SetReg(dh, dl);

    return si;
}

//	"\Vn" COMMAND
uint8_t * PMD::rmsvs(uint8_t * si)
{
    int dl = *si++;

    if (_OpenWork.rhythm_voldown != 0)
        dl = ((256 - _OpenWork.rhythm_voldown) * dl) >> 8;

    _OpenWork.rhyvol = dl;

    if (_OpenWork.fadeout_volume != 0)
        dl = ((256 - _OpenWork.fadeout_volume) * dl) >> 8;

    _OPNA->SetReg(0x11, dl);

    return si;
}

uint8_t * PMD::rmsvs_sft(uint8_t * si)
{
    int dl = _OpenWork.rhyvol + *(int8_t *) si++;

    if (dl >= 64)
    {
        if (dl & 0x80)
            dl = 0;
        else
            dl = 63;
    }

    _OpenWork.rhyvol = dl;

    if (_OpenWork.fadeout_volume != 0)
        dl = ((256 - _OpenWork.fadeout_volume) * dl) >> 8;

    _OPNA->SetReg(0x11, dl);

    return si;
}

// Change only one volume (V2.7 expansion)
uint8_t * PMD::vol_one_up_psg(PartState * qq, uint8_t * si)
{
    int		al;

    al = qq->volume + *si++;
    if (al > 15) al = 15;
    qq->volpush = ++al;
    pmdwork.volpush_flag = 1;
    return si;
}

uint8_t * PMD::vol_one_down(PartState * qq, uint8_t * si)
{
    int		al;

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
    pmdwork.volpush_flag = 1;
    return si;
}

//	ポルタメント(PSG)
uint8_t * PMD::portap(PartState * qq, uint8_t * si)
{
    int		ax, al_, bx_;

    if (qq->partmask)
    {
        qq->fnum = 0;		//休符に設定
        qq->onkai = 255;
        qq->leng = *(si + 2);
        qq->keyon_flag++;
        qq->address = si + 3;

        if (--pmdwork.volpush_flag)
        {
            qq->volpush = 0;
        }

        pmdwork.tieflag = 0;
        pmdwork.volpush_flag = 0;
        pmdwork.loop_work &= qq->loopcheck;
        return si + 3;		// 読み飛ばす	(Mask時)
    }

    fnumsetp(qq, oshiftp(qq, lfoinitp(qq, *si++)));

    bx_ = qq->fnum;
    al_ = qq->onkai;
    fnumsetp(qq, oshiftp(qq, *si++));
    ax = qq->fnum; 			// ax = ポルタメント先のpsg_tune値

    qq->onkai = al_;
    qq->fnum = bx_;			// bx = ポルタメント元のpsg_tune値
    ax -= bx_;

    qq->leng = *si++;
    si = calc_q(qq, si);

    qq->porta_num2 = ax / qq->leng;		// 商
    qq->porta_num3 = ax % qq->leng;		// 余り
    qq->lfoswi |= 8;				// Porta ON

    if (qq->volpush && qq->onkai != 255)
    {
        if (--pmdwork.volpush_flag)
        {
            pmdwork.volpush_flag = 0;
            qq->volpush = 0;
        }
    }

    volsetp(qq);
    otodasip(qq);
    keyonp(qq);

    qq->keyon_flag++;
    qq->address = si;

    pmdwork.tieflag = 0;
    pmdwork.volpush_flag = 0;
    qq->keyoff_flag = 0;

    if (*si == 0xfb)
    {			// '&'が直後にあったらkeyoffしない
        qq->keyoff_flag = 2;
    }

    pmdwork.loop_work &= qq->loopcheck;
    return si;
}

//	'w' COMMAND [PSG NOISE ﾍｲｷﾝ ｼｭｳﾊｽｳ]
uint8_t * PMD::psgnoise_move(uint8_t * si)
{
    _OpenWork.psnoi += *(int8_t *) si++;
    if (_OpenWork.psnoi < 0) _OpenWork.psnoi = 0;
    if (_OpenWork.psnoi > 31) _OpenWork.psnoi = 31;
    return si;
}

//	PSG Envelope set (Extend)
uint8_t * PMD::extend_psgenvset(PartState * qq, uint8_t * si)
{
    qq->eenv_ar = *si++ & 0x1f;
    qq->eenv_dr = *si++ & 0x1f;
    qq->eenv_sr = *si++ & 0x1f;
    qq->eenv_rr = *si & 0x0f;
    qq->eenv_sl = ((*si++ >> 4) & 0x0f) ^ 0x0f;
    qq->eenv_al = *si++ & 0x0f;

    if (qq->envf != -1)
    {	// ノーマル＞拡張に移行したか？
        qq->envf = -1;
        qq->eenv_count = 4;		// RR
        qq->eenv_volume = 0;	// Volume
    }
    return si;
}

uint8_t * PMD::mdepth_count(PartState * qq, uint8_t * si)
{
    int		al;

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

//	ＰＳＧ／ＰＣＭ音源用　Entry
int PMD::lfoinitp(PartState * qq, int al)
{
    int		ah;

    ah = al & 0x0f;

    if (ah == 0x0c)
    {
        al = qq->onkai_def;
        ah = al & 0x0f;
    }

    qq->onkai_def = al;

    // 4.8r 修正
    if (ah == 0x0f)
    {			// ｷｭｰﾌ ﾉ ﾄｷ ﾊ INIT ｼﾅｲﾖ
// PMD 4.8r 修正
        soft_env(qq);
        lfo_exit(qq);
        return al;
    }

    qq->porta_num = 0;			// ポルタメントは初期化

    if (pmdwork.tieflag & 1)
    {	// ﾏｴ ｶﾞ & ﾉ ﾄｷ ﾓ INIT ｼﾅｲ｡
// PMD 4.8r 修正
        soft_env(qq);			// 前が & の場合 -> 1回 SofeEnv処理
        lfo_exit(qq);
        return al;
    }

    //------------------------------------------------------------------------
    //	ソフトウエアエンベロープ初期化
    //------------------------------------------------------------------------
    if (qq->envf != -1)
    {
        qq->envf = 0;
        qq->eenv_volume = 0;
        qq->eenv_ar = qq->eenv_arc;

        if (qq->eenv_ar == 0)
        {
            qq->envf = 1;	// ATTACK=0 ... ｽｸﾞ Decay ﾆ
            qq->eenv_volume = qq->eenv_dr;
        }

        qq->eenv_sr = qq->eenv_src;
        qq->eenv_rr = qq->eenv_rrc;
        lfin1(qq);

    }
    else
    {
        //	拡張ssg_envelope用

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

void PMD::lfo_exit(PartState * qq)
{
    if ((qq->lfoswi & 3) != 0)
    {		// 前が & の場合 -> 1回 LFO処理
        lfo(qq);
    }

    if ((qq->lfoswi & 0x30) != 0)
    {	// 前が & の場合 -> 1回 LFO処理
        lfo_change(qq);
        lfo(qq);
        lfo_change(qq);
    }
}

//	ＬＦＯ初期化
void PMD::lfin1(PartState * qq)
{
    qq->hldelay_c = qq->hldelay;

    if (qq->hldelay)
    {
        _OPNA->SetReg(pmdwork.fmsel + pmdwork.partb + 0xb4 - 1, (qq->fmpan) & 0xc0);
    }

    qq->sdelay_c = qq->sdelay;

    if (qq->lfoswi & 3)
    {	// LFOは未使用
        if ((qq->lfoswi & 4) == 0)
        {	//keyon非同期か?
            lfoinit_main(qq);
        }
        lfo(qq);
    }

    if (qq->lfoswi & 0x30)
    {	// LFOは未使用
        if ((qq->lfoswi & 0x40) == 0)
        {	//keyon非同期か?
            lfo_change(qq);
            lfoinit_main(qq);
            lfo_change(qq);
        }

        lfo_change(qq);
        lfo(qq);
        lfo_change(qq);
    }
}

void PMD::lfoinit_main(PartState * qq)
{
    qq->lfodat = 0;
    qq->delay = qq->delay2;
    qq->speed = qq->speed2;
    qq->step = qq->step2;
    qq->time = qq->time2;
    qq->mdc = qq->mdc2;

    if (qq->lfo_wave == 2 || qq->lfo_wave == 3)
    {	// 矩形波 or ランダム波？
        qq->speed = 1;	// delay直後にLFOが掛かるようにする
    }
    else
    {
        qq->speed++;	// それ以外の場合はdelay直後のspeed値を +1
    }
}

//	SHIFT[di] 分移調する
int PMD::oshiftp(PartState * qq, int al)
{
    return oshift(qq, al);
}

int PMD::oshift(PartState * qq, int al)
{
    int	bl, bh, dl;

    if (al == 0x0f) return al;

    dl = qq->shift + qq->shift_def;
    if (dl == 0) return al;

    bl = (al & 0x0f);		// bl = ONKAI
    bh = (al & 0xf0) >> 4;	// bh = OCT

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

//	PSG TUNE SET
void PMD::fnumsetp(PartState * qq, int al)
{
    int	ax, bx, cl;

    if ((al & 0x0f) == 0x0f)
    {		// ｷｭｳﾌ ﾅﾗ FNUM ﾆ 0 ｦ ｾｯﾄ
        qq->onkai = 255;
        if (qq->lfoswi & 0x11) return;
        qq->fnum = 0;	// 音程LFO未使用
        return;
    }

    qq->onkai = al;

    cl = (al >> 4) & 0x0f;	// cl=oct
    bx = al & 0x0f;			// bx=onkai

    ax = psg_tune_data[bx];
    if (cl > 0)
    {
        int carry;
        ax >>= cl - 1;
        carry = ax & 1;
        ax = (ax >> 1) + carry;
    }

    qq->fnum = ax;
}

//	Q値の計算
//		break	dx
uint8_t * PMD::calc_q(PartState * qq, uint8_t * si)
{
    int		ax, dh, dl;

    if (*si == 0xc1)
    {		// &&
        si++;
        qq->qdat = 0;
        return si;
    }

    dl = qq->qdata;
    if (qq->qdatb)
    {
        dl += (qq->leng * qq->qdatb) >> 8;
    }

    if (qq->qdat3)
    {		//	Random-Q
        ax = rnd((qq->qdat3 & 0x7f) + 1);
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
        if ((dh = qq->leng - qq->qdat2) < 0)
        {
            qq->qdat = 0;
            return si;
        }

        if (dl < dh)
        {
            qq->qdat = dl;
        }
        else
        {
            qq->qdat = dh;
        }
    }
    else
    {
        qq->qdat = dl;
    }

    return si;
}

//	ＰＳＧ　ＶＯＬＵＭＥ　ＳＥＴ
void PMD::volsetp(PartState * qq)
{
    int		ax, dl;

    if (qq->envf == 3 || (qq->envf == -1 && qq->eenv_count == 0)) return;

    if (qq->volpush)
    {
        dl = qq->volpush - 1;
    }
    else
    {
        dl = qq->volume;
    }

    //------------------------------------------------------------------------
    //	音量down計算
    //------------------------------------------------------------------------
    dl = ((256 - _OpenWork.ssg_voldown) * dl) >> 8;

    //------------------------------------------------------------------------
    //	Fadeout計算
    //------------------------------------------------------------------------
    dl = ((256 - _OpenWork.fadeout_volume) * dl) >> 8;

    //------------------------------------------------------------------------
    //	ENVELOPE 計算
    //------------------------------------------------------------------------
    if (dl <= 0)
    {
        _OPNA->SetReg(pmdwork.partb + 8 - 1, 0);
        return;
    }

    if (qq->envf == -1)
    {
        if (qq->eenv_volume == 0)
        {
            _OPNA->SetReg(pmdwork.partb + 8 - 1, 0);
            return;
        }
        dl = ((((dl * (qq->eenv_volume + 1))) >> 3) + 1) >> 1;
    }
    else
    {
        dl += qq->eenv_volume;
        if (dl <= 0)
        {
            _OPNA->SetReg(pmdwork.partb + 8 - 1, 0);
            return;
        }
        if (dl > 15) dl = 15;
    }

    //--------------------------------------------------------------------
    //	音量LFO計算
    //--------------------------------------------------------------------
    if ((qq->lfoswi & 0x22) == 0)
    {
        _OPNA->SetReg(pmdwork.partb + 8 - 1, dl);
        return;
    }

    if (qq->lfoswi & 2)
    {
        ax = qq->lfodat;
    }
    else
    {
        ax = 0;
    }

    if (qq->lfoswi & 0x20)
    {
        ax += qq->_lfodat;
    }

    dl = dl + ax;
    if (dl < 0)
    {
        _OPNA->SetReg(pmdwork.partb + 8 - 1, 0);
        return;
    }
    if (dl > 15) dl = 15;

    //------------------------------------------------------------------------
    //	出力
    //------------------------------------------------------------------------
    _OPNA->SetReg(pmdwork.partb + 8 - 1, dl);
}

//	ＰＳＧ　音程設定
void PMD::otodasip(PartState * qq)
{
    int		ax, dx;

    if (qq->fnum == 0) return;

    // PSG Portament set

    ax = qq->fnum + qq->porta_num;
    dx = 0;

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
            dx = (ax * qq->detune) >> 12;		// dx:ax=ax * qq->detune
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

    _OPNA->SetReg((pmdwork.partb - 1) * 2, ax & 0xff);
    _OPNA->SetReg((pmdwork.partb - 1) * 2 + 1, ax >> 8);
}

//	ＰＳＧ　ＫＥＹＯＮ
void PMD::keyonp(PartState * qq)
{
    int		ah, al;

    if (qq->onkai == 255) return;		// ｷｭｳﾌ ﾉ ﾄｷ

    ah = (1 << (pmdwork.partb - 1)) | (1 << (pmdwork.partb + 2));
    al = ((int32_t) _OPNA->GetReg(0x07) | ah);
    ah = ~(ah & qq->psgpat);
    al &= ah;
    _OPNA->SetReg(7, al);

    // PSG ﾉｲｽﾞ ｼｭｳﾊｽｳ ﾉ ｾｯﾄ

    if (_OpenWork.psnoi != _OpenWork.psnoi_last && effwork.effon == 0)
    {
        _OPNA->SetReg(6, _OpenWork.psnoi);
        _OpenWork.psnoi_last = _OpenWork.psnoi;
    }
}

//	ＬＦＯ処理
//		Don't Break cl
//		output		cy=1	変化があった
int PMD::lfo(PartState * qq)
{
    return lfop(qq);
}

int PMD::lfop(PartState * qq)
{
    int		ax, ch;

    if (qq->delay)
    {
        qq->delay--;
        return 0;
    }

    if (qq->extendmode & 2)
    {	// TimerAと合わせるか？
        // そうじゃないなら無条件にlfo処理
        ch = _OpenWork.TimerAtime - pmdwork.lastTimerAtime;
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

void PMD::lfo_main(PartState * qq)
{
    int		al, ax;

    if (qq->speed != 1)
    {
        if (qq->speed != 255) qq->speed--;
        return;
    }

    qq->speed = qq->speed2;

    if (qq->lfo_wave == 0 || qq->lfo_wave == 4 || qq->lfo_wave == 5)
    {
        //	三角波		lfowave = 0,4,5
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
                    al += al;	// lfowave=0,5の場合 timeを反転時２倍にする
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
        //	矩形波		lfowave = 2
        qq->lfodat = (qq->step * qq->time);
        md_inc(qq);
        qq->step = -qq->step;

    }
    else if (qq->lfo_wave == 6)
    {
        //	ワンショット	lfowave = 6
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
        //ノコギリ波	lfowave = 1
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
        //	ランダム波	lfowave = 3
        ax = abs(qq->step) * qq->time;
        qq->lfodat = ax - rnd(ax * 2);
        md_inc(qq);
    }
}

//	乱数発生ルーチン	INPUT : AX = MAX_RANDOM
//				OUTPUT: AX = RANDOM_NUMBER
int PMD::rnd(int ax)
{
    seed = (259 * seed + 3) & 0x7fff;
    return seed * ax / 32767;
}

// Change STEP value by value of MD command
void PMD::md_inc(PartState * qq)
{
    int		al;

    if (--qq->mdspd) return;

    qq->mdspd = qq->mdspd2;

    if (qq->mdc == 0) return;		// count = 0
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
    int		temp;

    temp = *a;
    *a = *b;
    *b = temp;
}

//	LFO1<->LFO2 change
void PMD::lfo_change(PartState * qq)
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

//	ポルタメント計算なのね
void PMD::porta_calc(PartState * qq)
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
int PMD::soft_env(PartState * qq)
{
    int		i, cl;

    if (qq->extendmode & 4)
    {
        if (_OpenWork.TimerAtime == pmdwork.lastTimerAtime) return 0;

        cl = 0;
        for (i = 0; i < _OpenWork.TimerAtime - pmdwork.lastTimerAtime; i++)
        {
            if (soft_env_main(qq))
            {
                cl = 1;
            }
        }
        return cl;
    }
    else
    {
        return soft_env_main(qq);
    }
}

int PMD::soft_env_main(PartState * qq)
{
    int		dl;

    if (qq->envf == -1)
    {
        return ext_ssgenv_main(qq);
    }

    dl = qq->eenv_volume;
    soft_env_sub(qq);
    if (dl == qq->eenv_volume)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int PMD::soft_env_sub(PartState * qq)
{
    if (qq->envf == 0)
    {
        // Attack
        if (--qq->eenv_ar != 0)
        {
            return 0;
        }

        qq->envf = 1;
        qq->eenv_volume = qq->eenv_dr;
        return 1;
    }

    if (qq->envf != 2)
    {
        // Decay
        if (qq->eenv_sr == 0) return 0;	// ＤＲ＝０の時は減衰しない
        if (--qq->eenv_sr != 0) return 0;
        qq->eenv_sr = qq->eenv_src;
        qq->eenv_volume--;

        if (qq->eenv_volume >= -15 || qq->eenv_volume < 15) return 0;
        qq->eenv_volume = -15;
        return 0;
    }


    // Release
    if (qq->eenv_rr == 0)
    {				// ＲＲ＝０の時はすぐに音消し
        qq->eenv_volume = -15;
        return 0;
    }

    if (--qq->eenv_rr != 0) return 0;
    qq->eenv_rr = qq->eenv_rrc;
    qq->eenv_volume--;

    if (qq->eenv_volume >= -15 && qq->eenv_volume < 15) return 0;
    qq->eenv_volume = -15;
    return 0;
}

//	拡張版
int PMD::ext_ssgenv_main(PartState * qq)
{
    int		dl;

    if (qq->eenv_count == 0) return 0;

    dl = qq->eenv_volume;
    esm_sub(qq, qq->eenv_count);

    if (dl == qq->eenv_volume) return 0;
    return -1;
}

void PMD::esm_sub(PartState * qq, int ah)
{
    if (--ah == 0)
    {
        //	[[[ Attack Rate ]]]
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
            if (qq->eenv_sl != 15) return;		// SL=0の場合はすぐSRに
            qq->eenv_count++;
            return;
        }
        else
        {
            if (qq->eenv_ar == 0) return;
            qq->eenv_arc++;
            return;
        }
    }

    if (--ah == 0)
    {
        //	[[[ Decay Rate ]]]
        if (qq->eenv_drc > 0)
        {	// 0以下の場合はカウントCHECK
            qq->eenv_volume -= qq->eenv_drc;
            if (qq->eenv_volume < 0 || qq->eenv_volume < qq->eenv_sl)
            {
                qq->eenv_volume = qq->eenv_sl;
                qq->eenv_count++;
                return;
            }

            if (qq->eenv_dr < 16)
            {
                qq->eenv_drc = (qq->eenv_dr - 16) * 2;
            }
            else
            {
                qq->eenv_drc = qq->eenv_dr - 16;
            }
            return;
        }

        if (qq->eenv_dr == 0) return;
        qq->eenv_drc++;
        return;
    }

    if (--ah == 0)
    {
        //	[[[ Sustain Rate ]]]
        if (qq->eenv_src > 0)
        {	// 0以下の場合はカウントCHECK
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

        if (qq->eenv_sr == 0) return;	// SR=0?
        qq->eenv_src++;
        return;
    }

    //	[[[ Release Rate ]]]
    if (qq->eenv_rrc > 0)
    {	// 0以下の場合はカウントCHECK
        if ((qq->eenv_volume -= qq->eenv_rrc) < 0)
        {
            qq->eenv_volume = 0;
        }

        qq->eenv_rrc = (qq->eenv_rr) * 2 - 16;
        return;
    }

    if (qq->eenv_rr == 0) return;
    qq->eenv_rrc++;
}

//	テンポ設定
void PMD::settempo_b(void)
{
    if (_OpenWork.TimerB_speed != _OpenWork.tempo_d)
    {
        _OpenWork.TimerB_speed = _OpenWork.tempo_d;
        _OPNA->SetReg(0x26, _OpenWork.TimerB_speed);
    }
}

//	小節のカウント
void PMD::syousetu_count(void)
{
    if (_OpenWork.opncount + 1 == _OpenWork.syousetu_lng)
    {
        _OpenWork.syousetu++;
        _OpenWork.opncount = 0;
    }
    else
    {
        _OpenWork.opncount++;
    }
}

//	ＯＰＮ割り込み許可処理
void PMD::opnint_start(void)
{
    memset(FMPart, 0, sizeof(FMPart));
    memset(SSGPart, 0, sizeof(SSGPart));
    memset(&ADPCMPart, 0, sizeof(ADPCMPart));
    memset(&RhythmPart, 0, sizeof(RhythmPart));
    memset(&DummyPart, 0, sizeof(DummyPart));
    memset(ExtPart, 0, sizeof(ExtPart));
    memset(&EffPart, 0, sizeof(EffPart));
    memset(PPZ8Part, 0, sizeof(PPZ8Part));

    _OpenWork.rhythmmask = 255;
    pmdwork.rhydmy = 255;
    data_init();
    opn_init();

    _OPNA->SetReg(0x07, 0xbf);
    mstop();
    setint();
    _OPNA->SetReg(0x29, 0x83);
}

//	DATA AREA の イニシャライズ
void PMD::data_init(void)
{
    int		i;
    int		partmask, keyon_flag;

    _OpenWork.fadeout_volume = 0;
    _OpenWork.fadeout_speed = 0;
    _OpenWork.fadeout_flag = 0;
    _OpenWork._FadeOutSpeedHQ = 0;

    for (i = 0; i < 6; i++)
    {
        partmask = FMPart[i].partmask;
        keyon_flag = FMPart[i].keyon_flag;
        memset(&FMPart[i], 0, sizeof(PartState));
        FMPart[i].partmask = partmask & 0x0f;
        FMPart[i].keyon_flag = keyon_flag;
        FMPart[i].onkai = 255;
        FMPart[i].onkai_def = 255;
    }

    for (i = 0; i < 3; i++)
    {
        partmask = SSGPart[i].partmask;
        keyon_flag = SSGPart[i].keyon_flag;
        memset(&SSGPart[i], 0, sizeof(PartState));
        SSGPart[i].partmask = partmask & 0x0f;
        SSGPart[i].keyon_flag = keyon_flag;
        SSGPart[i].onkai = 255;
        SSGPart[i].onkai_def = 255;
    }

    partmask = ADPCMPart.partmask;
    keyon_flag = ADPCMPart.keyon_flag;
    memset(&ADPCMPart, 0, sizeof(PartState));
    ADPCMPart.partmask = partmask & 0x0f;
    ADPCMPart.keyon_flag = keyon_flag;
    ADPCMPart.onkai = 255;
    ADPCMPart.onkai_def = 255;

    partmask = RhythmPart.partmask;
    keyon_flag = RhythmPart.keyon_flag;
    memset(&RhythmPart, 0, sizeof(PartState));
    RhythmPart.partmask = partmask & 0x0f;
    RhythmPart.keyon_flag = keyon_flag;
    RhythmPart.onkai = 255;
    RhythmPart.onkai_def = 255;

    for (i = 0; i < 3; i++)
    {
        partmask = ExtPart[i].partmask;
        keyon_flag = ExtPart[i].keyon_flag;
        memset(&ExtPart[i], 0, sizeof(PartState));
        ExtPart[i].partmask = partmask & 0x0f;
        ExtPart[i].keyon_flag = keyon_flag;
        ExtPart[i].onkai = 255;
        ExtPart[i].onkai_def = 255;
    }

    for (i = 0; i < 8; i++)
    {
        partmask = PPZ8Part[i].partmask;
        keyon_flag = PPZ8Part[i].keyon_flag;
        memset(&PPZ8Part[i], 0, sizeof(PartState));
        PPZ8Part[i].partmask = partmask & 0x0f;
        PPZ8Part[i].keyon_flag = keyon_flag;
        PPZ8Part[i].onkai = 255;
        PPZ8Part[i].onkai_def = 255;
    }

    pmdwork.tieflag = 0;
    _OpenWork.status = 0;
    _OpenWork._LoopCount = 0;
    _OpenWork.syousetu = 0;
    _OpenWork.opncount = 0;
    _OpenWork.TimerAtime = 0;
    pmdwork.lastTimerAtime = 0;

    pmdwork.omote_key[0] = 0;
    pmdwork.omote_key[1] = 0;
    pmdwork.omote_key[2] = 0;
    pmdwork.ura_key[0] = 0;
    pmdwork.ura_key[1] = 0;
    pmdwork.ura_key[2] = 0;

    pmdwork.fm3_alg_fb = 0;
    pmdwork.af_check = 0;

    _OpenWork.pcmstart = 0;
    _OpenWork.pcmstop = 0;
    pmdwork.pcmrepeat1 = 0;
    pmdwork.pcmrepeat2 = 0;
    pmdwork.pcmrelease = 0x8000;

    _OpenWork.kshot_dat = 0;
    _OpenWork.rshot_dat = 0;
    effwork.last_shot_data = 0;

    pmdwork.slotdetune_flag = 0;
    _OpenWork.slot_detune1 = 0;
    _OpenWork.slot_detune2 = 0;
    _OpenWork.slot_detune3 = 0;
    _OpenWork.slot_detune4 = 0;

    pmdwork.slot3_flag = 0;
    _OpenWork.ch3mode = 0x3f;

    pmdwork.fmsel = 0;

    _OpenWork.syousetu_lng = 96;

    _OpenWork.fm_voldown = _OpenWork._fm_voldown;
    _OpenWork.ssg_voldown = _OpenWork._ssg_voldown;
    _OpenWork.pcm_voldown = _OpenWork._pcm_voldown;
    _OpenWork.ppz_voldown = _OpenWork._ppz_voldown;
    _OpenWork.rhythm_voldown = _OpenWork._rhythm_voldown;
    _OpenWork.pcm86_vol = _OpenWork._pcm86_vol;
}

//	OPN INIT
void PMD::opn_init(void)
{
    int		i;

    _OPNA->ClearBuffer();
    _OPNA->SetReg(0x29, 0x83);

    _OpenWork.psnoi = 0;
    //@	if(effwork.effon == 0) {
    _OPNA->SetReg(0x06, 0x00);
    _OpenWork.psnoi_last = 0;
    //@	}

        //------------------------------------------------------------------------
        //	SSG-EG RESET (4.8s)
        //------------------------------------------------------------------------
    for (i = 0x90; i < 0x9f; i++)
    {
        if (i % 4 != 3)
        {
            _OPNA->SetReg(i, 0x00);
        }
    }

    for (i = 0x190; i < 0x19f; i++)
    {
        if (i % 4 != 3)
        {
            _OPNA->SetReg(i, 0x00);
        }
    }

    //------------------------------------------------------------------------
    //	PAN/HARDLFO DEFAULT
    //------------------------------------------------------------------------
    _OPNA->SetReg(0xb4, 0xc0);
    _OPNA->SetReg(0xb5, 0xc0);
    _OPNA->SetReg(0xb6, 0xc0);
    _OPNA->SetReg(0x1b4, 0xc0);
    _OPNA->SetReg(0x1b5, 0xc0);
    _OPNA->SetReg(0x1b6, 0xc0);

    _OpenWork.port22h = 0x00;
    _OPNA->SetReg(0x22, 0x00);

    //------------------------------------------------------------------------
    //	Rhythm Default = Pan : Mid , Vol : 15
    //------------------------------------------------------------------------
    for (i = 0; i < 6; i++)
    {
        _OpenWork.rdat[i] = 0xcf;
    }
    _OPNA->SetReg(0x10, 0xff);

    //------------------------------------------------------------------------
    //	リズムトータルレベル　セット
    //------------------------------------------------------------------------
    _OpenWork.rhyvol = 48 * 4 * (256 - _OpenWork.rhythm_voldown) / 1024;
    _OPNA->SetReg(0x11, _OpenWork.rhyvol);

    //------------------------------------------------------------------------
    //	ＰＣＭ　reset & ＬＩＭＩＴ　ＳＥＴ
    //------------------------------------------------------------------------
    _OPNA->SetReg(0x10c, 0xff);
    _OPNA->SetReg(0x10d, 0xff);

    for (i = 0; i < PCM_CNL_MAX; i++)
    {
        _PPZ8->SetPan(i, 5);
    }
}

void PMD::mstop(void)
{
    pmdwork.music_flag &= 0xfd;

    _OpenWork._IsPlaying = 0;
    _OpenWork.fadeout_speed = 0;
    _OpenWork._LoopCount = -1;
    _OpenWork.fadeout_volume = 0xFF;

    silence();
}

//	ALL SILENCE
void PMD::silence(void)
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
//@	if(effwork.effon == 0) {
    _OPNA->SetReg(0x07, 0xBF);
    _OPNA->SetReg(0x08, 0x00);
    _OPNA->SetReg(0x09, 0x00);
    _OPNA->SetReg(0x0a, 0x00);
//@	} else {
//@ opna->SetReg(0x07, (opna->GetReg(0x07) & 0x3f) | 0x9b);
//@	}

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
    if (_OpenWork.TimerAflag || _OpenWork.TimerBflag)
    {
        pmdwork.music_flag |= 1; // Not executed during TA/TB processing

        return;
    }

    mstart();
}

/// <summary>
/// Stops the driver.
/// </summary>
void PMD::Stop()
{
    if (_OpenWork.TimerAflag || _OpenWork.TimerBflag)
    {
        pmdwork.music_flag |= 2;
    }
    else
    {
        _OpenWork.fadeout_flag = 0;
        mstop();
    }

    ::memset(wavbuf2, 0, sizeof(wavbuf2));
    _Position = 0;
}

// Start playing
void PMD::mstart()
{
    // Set TimerB = 0 and Timer Reset (to match the length of the song every time)
    _OpenWork.tempo_d = 0;

    settempo_b();

    _OPNA->SetReg(0x27, 0x00); // TIMER RESET (both timer A and B)

    //------------------------------------------------------------------------
    //	演奏停止
    //------------------------------------------------------------------------
    pmdwork.music_flag &= 0xfe;
    mstop();

    //------------------------------------------------------------------------
    //	バッファ初期化
    //------------------------------------------------------------------------
    _PCMPtr = (char *) wavbuf2;    // Start position of remaining samples in buf
    _SamplesToDo = 0;           // Number of samples remaining in buf
    _Position = 0;                   // Time from start of playing (μs)

    //------------------------------------------------------------------------
    //	演奏準備
    //------------------------------------------------------------------------
    data_init();
    play_init();

    //------------------------------------------------------------------------
    //	OPN初期化
    //------------------------------------------------------------------------
    opn_init();

    //------------------------------------------------------------------------
    //	音楽の演奏を開始
    //------------------------------------------------------------------------
    setint();
    _OpenWork._IsPlaying = 1;
}

// Set the start address and initial value of each part
void PMD::play_init(void)
{
    int			i;
    uint16_t * p;

    _OpenWork.x68_flg = *(_OpenWork.mmlbuf - 1);

    //２．６追加分
    if (*_OpenWork.mmlbuf != 2 * (max_part2 + 1))
    {
        _OpenWork.prgdat_adr = _OpenWork.mmlbuf + *(uint16_t *) (&_OpenWork.mmlbuf[2 * (max_part2 + 1)]);
        _OpenWork.prg_flg = 1;
    }
    else
    {
        _OpenWork.prg_flg = 0;
    }

    p = (uint16_t *) _OpenWork.mmlbuf;

    //	Part 0,1,2,3,4,5(FM1?6)の時
    for (i = 0; i < 6; i++)
    {
        if (_OpenWork.mmlbuf[*p] == 0x80)
        {		//先頭が80hなら演奏しない
            FMPart[i].address = NULL;
        }
        else
        {
            FMPart[i].address = &_OpenWork.mmlbuf[*p];
        }

        FMPart[i].leng = 1;
        FMPart[i].keyoff_flag = -1;		// 現在keyoff中
        FMPart[i].mdc = -1;				// MDepth Counter (無限)
        FMPart[i].mdc2 = -1;			// 同上
        FMPart[i]._mdc = -1;			// 同上
        FMPart[i]._mdc2 = -1;			// 同上
        FMPart[i].onkai = 255;			// rest
        FMPart[i].onkai_def = 255;		// rest
        FMPart[i].volume = 108;			// FM  VOLUME DEFAULT= 108
        FMPart[i].fmpan = 0xc0;			// FM PAN = Middle
        FMPart[i].slotmask = 0xf0;		// FM SLOT MASK
        FMPart[i].neiromask = 0xff;		// FM Neiro MASK
        p++;
    }

    //	Part 6,7,8(PSG1?3)の時
    for (i = 0; i < 3; i++)
    {
        if (_OpenWork.mmlbuf[*p] == 0x80)
        {		//先頭が80hなら演奏しない
            SSGPart[i].address = NULL;
        }
        else
        {
            SSGPart[i].address = &_OpenWork.mmlbuf[*p];
        }

        SSGPart[i].leng = 1;
        SSGPart[i].keyoff_flag = -1;	// 現在keyoff中
        SSGPart[i].mdc = -1;			// MDepth Counter (無限)
        SSGPart[i].mdc2 = -1;			// 同上
        SSGPart[i]._mdc = -1;			// 同上
        SSGPart[i]._mdc2 = -1;			// 同上
        SSGPart[i].onkai = 255;			// rest
        SSGPart[i].onkai_def = 255;		// rest
        SSGPart[i].volume = 8;			// PSG VOLUME DEFAULT= 8
        SSGPart[i].psgpat = 7;			// PSG = TONE
        SSGPart[i].envf = 3;			// PSG ENV = NONE/normal
        p++;

    }

    //	Part 9(OPNA/ADPCM)の時
    if (_OpenWork.mmlbuf[*p] == 0x80)
    {		//先頭が80hなら演奏しない
        ADPCMPart.address = NULL;
    }
    else
    {
        ADPCMPart.address = &_OpenWork.mmlbuf[*p];
    }

    ADPCMPart.leng = 1;
    ADPCMPart.keyoff_flag = -1;		// 現在keyoff中
    ADPCMPart.mdc = -1;				// MDepth Counter (無限)
    ADPCMPart.mdc2 = -1;			// 同上
    ADPCMPart._mdc = -1;			// 同上
    ADPCMPart._mdc2 = -1;			// 同上
    ADPCMPart.onkai = 255;			// rest
    ADPCMPart.onkai_def = 255;		// rest
    ADPCMPart.volume = 128;			// PCM VOLUME DEFAULT= 128
    ADPCMPart.fmpan = 0xc0;			// PCM PAN = Middle
    p++;

    //	Part 10(Rhythm)の時
    if (_OpenWork.mmlbuf[*p] == 0x80)
    {		//先頭が80hなら演奏しない
        RhythmPart.address = NULL;
    }
    else
    {
        RhythmPart.address = &_OpenWork.mmlbuf[*p];
    }

    RhythmPart.leng = 1;
    RhythmPart.keyoff_flag = -1;	// 現在keyoff中
    RhythmPart.mdc = -1;			// MDepth Counter (無限)
    RhythmPart.mdc2 = -1;			// 同上
    RhythmPart._mdc = -1;			// 同上
    RhythmPart._mdc2 = -1;			// 同上
    RhythmPart.onkai = 255;			// rest
    RhythmPart.onkai_def = 255;		// rest
    RhythmPart.volume = 15;			// PPSDRV volume
    p++;

    //------------------------------------------------------------------------
    //	Rhythm のアドレステーブルをセット
    //------------------------------------------------------------------------

    _OpenWork.radtbl = (uint16_t *) &_OpenWork.mmlbuf[*p];
    _OpenWork.rhyadr = (uint8_t *) &pmdwork.rhydmy;
}

//	インタラプト　設定
//	FM音源専用
void PMD::setint(void)
{
    //
    // ＯＰＮ割り込み初期設定
    //

    _OpenWork.tempo_d = 200;
    _OpenWork.tempo_d_push = 200;

    calc_tb_tempo();
    settempo_b();

    _OPNA->SetReg(0x25, 0x00);			// TIMER A SET (9216μs固定)
    _OPNA->SetReg(0x24, 0x00);			// 一番遅くて丁度いい
    _OPNA->SetReg(0x27, 0x3f);			// TIMER ENABLE

    //
    //　小節カウンタリセット
    //

    _OpenWork.opncount = 0;
    _OpenWork.syousetu = 0;
    _OpenWork.syousetu_lng = 96;
}

//	T->t 変換
//		input	[tempo_d]
//		output	[tempo_48]
void PMD::calc_tb_tempo(void)
{
    //	TEMPO = 0x112C / [ 256 - TB ]	timerB -> tempo
    int temp;

    if (256 - _OpenWork.tempo_d == 0)
    {
        temp = 255;
    }
    else
    {
        temp = (0x112c * 2 / (256 - _OpenWork.tempo_d) + 1) / 2;
        if (temp > 255) temp = 255;
    }

    _OpenWork.tempo_48 = temp;
    _OpenWork.tempo_48_push = temp;
}

//	t->T 変換
//		input	[tempo_48]
//		output	[tempo_d]
void PMD::calc_tempo_tb(void)
{
    int		al;

    //	TB = 256 - [ 112CH / TEMPO ]	tempo -> timerB

    if (_OpenWork.tempo_48 >= 18)
    {
        al = 256 - 0x112c / _OpenWork.tempo_48;
        if (0x112c % _OpenWork.tempo_48 >= 128)
        {
            al--;
        }
        //al = 256 - (0x112c * 2 / open_work.tempo_48 + 1) / 2;
    }
    else
    {
        al = 0;
    }
    _OpenWork.tempo_d = al;
    _OpenWork.tempo_d_push = al;
}

//	ＰＣＭメモリからメインメモリへのデータ取り込み
//
//	INPUTS 	.. pcmstart   	to Start Address
//			.. pcmstop    	to Stop  Address
//			.. buf			to PCMDATA_Buffer
void PMD::pcmread(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf)
{
    int		i;

    _OPNA->SetReg(0x100, 0x01);
    _OPNA->SetReg(0x110, 0x00);
    _OPNA->SetReg(0x110, 0x80);
    _OPNA->SetReg(0x100, 0x20);
    _OPNA->SetReg(0x101, 0x02);		// x8
    _OPNA->SetReg(0x10c, 0xff);
    _OPNA->SetReg(0x10d, 0xff);
    _OPNA->SetReg(0x102, pcmstart & 0xff);
    _OPNA->SetReg(0x103, pcmstart >> 8);
    _OPNA->SetReg(0x104, 0xff);
    _OPNA->SetReg(0x105, 0xff);

    *buf = _OPNA->GetReg(0x108);		// 無駄読み
    *buf = _OPNA->GetReg(0x108);		// 無駄読み

    for (i = 0; i < (pcmstop - pcmstart) * 32; i++)
    {
        *buf = _OPNA->GetReg(0x108);
        buf++;
        _OPNA->SetReg(0x110, 0x80);
    }
}

//	ＰＣＭメモリへメインメモリからデータを送る (x8,高速版)
//
//	INPUTS 	.. pcmstart   	to Start Address
//			.. pcmstop    	to Stop  Address
//			.. buf			to PCMDATA_Buffer
void PMD::pcmstore(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf)
{
    int		i;

    _OPNA->SetReg(0x100, 0x01);
    //	opna->SetReg(0x110, 0x17);	// brdy以外はマスク(=timer割り込みは掛からない)
    _OPNA->SetReg(0x110, 0x80);
    _OPNA->SetReg(0x100, 0x60);
    _OPNA->SetReg(0x101, 0x02);	// x8
    _OPNA->SetReg(0x10c, 0xff);
    _OPNA->SetReg(0x10d, 0xff);
    _OPNA->SetReg(0x102, pcmstart & 0xff);
    _OPNA->SetReg(0x103, pcmstart >> 8);
    _OPNA->SetReg(0x104, 0xff);
    _OPNA->SetReg(0x105, 0xff);

    for (i = 0; i < (pcmstop - pcmstart) * 32; i++)
    {
        _OPNA->SetReg(0x108, *buf++);
    }
}

// LoadPPC (internal processing)
int PMD::LoadPPCInternal(TCHAR * filename)
{
    if (*filename == '\0')
        return ERR_OPEN_PPC_FILE;

    if (pfileio->Open(filename, FileIO::flags_readonly) == false)
        return ERR_OPEN_PPC_FILE;

    int Size = (int) pfileio->GetFileSize(filename);

    uint8_t * pcmbuf = (uint8_t *) ::malloc(Size);

    if (pcmbuf == NULL)
        return ERR_OUT_OF_MEMORY;

    pfileio->Read(pcmbuf, Size);
    pfileio->Close();

    int result = LoadPPCInternal(pcmbuf, Size);

    _FilePath.Strcpy(_OpenWork.ppcfilename, filename);

    free(pcmbuf);

    return result;
}

// LoadPPC 3 (from memory)
int PMD::LoadPPCInternal(uint8_t * pcmdata, int size)
{
    if (size < 0x10)
    {
        _OpenWork.ppcfilename[0] = '\0';

        return ERR_WRONG_PPC_FILE; // Not PPC/PVI
    }

    bool FoundPVI;

    int			i;
    uint8_t		tempbuf[0x26 * 32];
    uint8_t		tempbuf2[30 + 4 * 256 + 128 + 2];
    uint16_t * pcmdata2;
    uint16_t	pcmstart, pcmstop;
    int			bx = 0;

    if (::strncmp((char *) pcmdata, PVIHeader, sizeof(PVIHeader) - 1) == 0 && pcmdata[10] == 2)
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
                pcmends.pcmadrs[i][0] = *(uint16_t *) &pcmdata[16 + i * 4] + 0x26;
                pcmends.pcmadrs[i][1] = *(uint16_t *) &pcmdata[18 + i * 4] + 0x26;
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

        pcmends.pcmends = bx;
    }
    else
    if (::strncmp((char *) pcmdata, PPCHeader, sizeof(PPCHeader) - 1) == 0)
    {   // PPC
        FoundPVI = false;

        pcmdata2 = (uint16_t *) pcmdata + 30 / 2;

        if (size < 30 + 4 * 256 + 2)
        {   // not PPC
            _OpenWork.ppcfilename[0] = '\0';

            return ERR_WRONG_PPC_FILE; // Not PPC/PVI
        }

        pcmends.pcmends = *pcmdata2++;

        for (i = 0; i < 256; i++)
        {
            pcmends.pcmadrs[i][0] = *pcmdata2++;
            pcmends.pcmadrs[i][1] = *pcmdata2++;
        }
    }
    else
    {   // Not PPC/PVI
        _OpenWork.ppcfilename[0] = '\0';

        return ERR_WRONG_PPC_FILE; // Not PPC/PVI
    }

    // Compare PMD work and PCMRAM header
    pcmread(0, 0x25, tempbuf);

    // Skip the "ADPCM?" header
    // Ignore file name (PMDWin specification)
    if (memcmp(&tempbuf[30], &pcmends, sizeof(pcmends)) == 0)
        return WARNING_PPC_ALREADY_LOAD;

    // Write PMD work to PCMRAM head
    memcpy(tempbuf2, PPCHeader, sizeof(PPCHeader) - 1);
    memcpy(&tempbuf2[sizeof(PPCHeader) - 1], &pcmends.pcmends, sizeof(tempbuf2) - (sizeof(PPCHeader) - 1));
    pcmstore(0, 0x25, tempbuf2);

    // Write PCMDATA to PCMRAM
    if (FoundPVI)
    {
        pcmdata2 = (uint16_t *) (pcmdata + 0x10 + sizeof(uint16_t) * 2 * 128);

        if (size < (int) (pcmends.pcmends - (0x10 + sizeof(uint16_t) * 2 * 128)) * 32)
        {
            // Not PVI
            _OpenWork.ppcfilename[0] = '\0';

            return ERR_WRONG_PPC_FILE;
        }
    }
    else
    {
        pcmdata2 = (uint16_t *) pcmdata + (30 + 4 * 256 + 2) / 2;

        if (size < (pcmends.pcmends - ((30 + 4 * 256 + 2) / 2)) * 32)
        {
            // Not PPC
            _OpenWork.ppcfilename[0] = '\0';

            return ERR_WRONG_PPC_FILE;
        }
    }

    pcmstart = 0x26;
    pcmstop = pcmends.pcmends;
    pcmstore(pcmstart, pcmstop, (uint8_t *) pcmdata2);

    return PMDWIN_OK;
}

// Find PCM in search directory
//		input
//			filename	: 検索するファイル(ファイル名部分のみ)
//			currentdir	: 最初に検索する場所があるなら指定
//						  open_work.pcmdir より優先する
//						  NULL なら無視
//  Returns search result (full path). NULL if not found.
TCHAR * PMD::FindPCMSamples(TCHAR * filePath, const TCHAR * filename, const TCHAR * currentDirectory)
{
    TCHAR FilePath[_MAX_PATH];

    if (currentDirectory != NULL)
    {
        _FilePath.Makepath_dir_filename(FilePath, currentDirectory, filename);

        if (pfileio->GetFileSize(FilePath) >= 0)
        {
            _FilePath.Strcpy(filePath, FilePath);

            return filePath;
        }
    }

    int i = -1;

    do
    {
        i++;

        if (_OpenWork.pcmdir[i][0] == '\0')
        {
            *filePath = '\0';

            return filePath;
        }

        _FilePath.Makepath_dir_filename(FilePath, _OpenWork.pcmdir[i], filename);

    }
    while (pfileio->GetFileSize(FilePath) < 0);

    _FilePath.Strcpy(filePath, FilePath);

    return filePath;
}

//	fm effect
uint8_t * PMD::fm_efct_set(PartState * qq, uint8_t * si)
{
    return si + 1;
}

uint8_t * PMD::ssg_efct_set(PartState * qq, uint8_t * si)
{
    int		al;

    al = *si++;

    if (qq->partmask) return si;

    if (al)
    {
        eff_on2(qq, al);
    }
    else
    {
        effend();
    }
    return si;
}

// Fade In / Out
void PMD::fout(void)
{
    if (_OpenWork.fadeout_speed == 0)
        return;

    if (_OpenWork.fadeout_speed > 0)
    {
        if (_OpenWork.fadeout_speed + _OpenWork.fadeout_volume < 256)
        {
            _OpenWork.fadeout_volume += _OpenWork.fadeout_speed;
        }
        else
        {
            _OpenWork.fadeout_volume = 255;
            _OpenWork.fadeout_speed  =   0;

            if (_OpenWork.fade_stop_flag == 1)
                pmdwork.music_flag |= 2;
        }
    }
    else
    {   // Fade in
        if (_OpenWork.fadeout_speed + _OpenWork.fadeout_volume > 255)
        {
            _OpenWork.fadeout_volume += _OpenWork.fadeout_speed;
        }
        else
        {
            _OpenWork.fadeout_volume = 0;
            _OpenWork.fadeout_speed = 0;

            _OPNA->SetReg(0x11, _OpenWork.rhyvol);
        }
    }
}

// Acquisition of memo (internal operation, conversion from 2-byte half-width to half-width characters)
char * PMD::_getmemo2(char * dest, const uint8_t * musdata, int size, int index)
{
    char * buf;
    char * rslt;

    if ((buf = (char *) malloc(MAX_MDATA_SIZE * 1024)) == NULL)
    {
        *dest = '\0';
        return NULL;
    }

    GetNoteInternal(musdata, size, index, buf);

    rslt = zen2tohan(dest, buf);

    free(buf);

    return rslt;
}

// Get note (under the hood, from file name)
int PMD::_fgetmemo(char * dest, TCHAR * filename, int al)
{
    uint8_t * mmlbuf;
    int		size;

    if (filename != NULL)
    {
        if (*filename != '\0')
        {
            if (pfileio->Open(filename) == false)
                return ERR_OPEN_MUSIC_FILE;

            if ((mmlbuf = (uint8_t *) malloc(MAX_MDATA_SIZE * 1024)) == NULL)
            {
                *dest = '\0';
                return ERR_OUT_OF_MEMORY;
            }

            size = pfileio->Read(mmlbuf, MAX_MDATA_SIZE * 1024);
            GetNoteInternal(mmlbuf, size, al, dest);

            free(mmlbuf);

            pfileio->Close();

            return PMDWIN_OK;
        }
    }

    GetNoteInternal(NULL, 0, al, dest);

    return PMDWIN_OK;
}

// Acquisition of memo (internal operation, conversion from /2-byte half-width to half-width characters from file name)
int PMD::_fgetmemo2(char * dest, TCHAR * filename, int al)
{
    char * buf;
    int		rslt;

    if ((buf = (char *) malloc(MAX_MDATA_SIZE * 1024)) == NULL)
    {
        *dest = '\0';

        return ERR_OUT_OF_MEMORY;
    }

    if ((rslt = _fgetmemo(buf, filename, al)) == PMDWIN_OK)
    {
        zen2tohan(dest, buf);
    }
    else
    {
        *dest = '\0';
    }

    free(buf);

    return rslt;
}
