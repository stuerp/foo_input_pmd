
// Based on PMDWin code by C60 / Masahiro Kajihara

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>

#include "OPNAW.h"
#include "PPS.h"
#include "PPZ.h"
#include "P86.h"

#include "PMDPrivate.h"
#include "Driver.h"
#include "Effect.h"

typedef int Sample;

#pragma pack(push)
#pragma pack(2)
struct PCMEnds
{
    uint16_t Count;
    uint16_t Address[256][2];
};
#pragma pack(pop)

#define	PVIHeader   "PVI2"
#define	PPCHeader   "ADPCM DATA for  PMD ver.4.4-  "

#define MaxParts    12

#define MAX_MDATA_SIZE  64 // KB
#define MAX_VDATA_SIZE   8 // KB
#define MAX_EDATA_SIZE  64 // KB

#define fmvd_init        0 // 98 has a smaller FM sound source than 88

#pragma warning(disable: 4820) // x bytes padding added after last data member
class PMD
{
public:
    PMD();
    virtual ~PMD();

    bool Initialize(const WCHAR * directoryPath);

    static bool IsPMD(const uint8_t * data, size_t size) noexcept;

    int Load(const uint8_t * data, size_t size);

    void Start();
    void Stop();

    void Render(int16_t * sampleData, size_t sampleCount);

    uint32_t GetLoopNumber();

    bool GetLength(int * songlength, int * loopLength);
    bool GetLengthInEvents(int * songLength, int * loopLength);

    uint32_t GetPosition();
    void SetPosition(uint32_t position);

    bool LoadRythmSample(WCHAR * path);
    bool SetSearchPaths(std::vector<const WCHAR *> & paths);
    
    void SetSynthesisRate(uint32_t value);
    void SetPPZSynthesisRate(uint32_t value);
    void SetFM55kHzSynthesisMode(bool flag);
    void SetPPZInterpolation(bool flag);

    void SetFMDelay(int nsec);
    void SetSSGDelay(int nsec);
    void SetADPCMDelay(int nsec);
    void SetRhythmDelay(int nsec);

    void SetFadeOutSpeed(int speed);
    void SetFadeOutDurationHQ(int speed);

    void SetEventNumber(int pos);
    int GetEventNumber();

    WCHAR * GetPCMFileName(WCHAR * fileName);
    WCHAR * GetPPZFileName(WCHAR * fileName, int bufnum);

    void UsePPS(bool value) noexcept;
    void UseRhythm(bool value) noexcept;

    bool HasADPCMROM() const noexcept
    {
        return (_OPNAW != nullptr) && _OPNAW->HasADPCMROM();
    }

    bool HasPercussionSamples() const noexcept
    {
        return (_OPNAW != nullptr) && _OPNAW->HasPercussionSamples();
    }

    void SetPPSInterpolation(bool ip);
    void SetP86Interpolation(bool ip);

    int DisableChannel(int ch);
    int EnableChannel(int ch);

    void SetFMVolumeDown(int value)
    {
        _State.FMVolumeDown = _State.DefaultFMVolumeDown = value;
    }

    int GetFMVolumeDown() const noexcept
    {
        return _State.FMVolumeDown;
    }

    int GetDefaultFMVolumeDown() const noexcept
    {
        return _State.DefaultFMVolumeDown;
    }

    void SetSSGVolumeDown(int value)
    {
        _State.SSGVolumeDown = _State.DefaultSSGVolumeDown = value;
    }

    int GetSSGVolumeDown()
    {
        return _State.SSGVolumeDown;
    }

    int GetDefaultSSGVolumeDown()
    {
        return _State.DefaultSSGVolumeDown;
    }

    void SetADPCMVolumeDown(int value)
    {
        _State.ADPCMVolumeDown = _State.DefaultADPCMVolumeDown = value;
    }

    int GetADPCMVolumeDown() const noexcept
    {
        return _State.ADPCMVolumeDown;
    }

    int GetDefaultADPCMVolumeDown() const noexcept
    {
        return _State.DefaultADPCMVolumeDown;
    }

    void SetRhythmVolumeDown(int value)
    {
        _State.RhythmVolumeDown = _State.DefaultRhythmVolumeDown = value;

        _State.RhythmVolume = 48 * 4 * (256 - _State.RhythmVolumeDown) / 1024;

        _OPNAW->SetReg(0x11, (uint32_t) _State.RhythmVolume);
    }

    int GetRhythmVolumeDown() const noexcept
    {
        return _State.RhythmVolumeDown;
    }

    int GetDefaultRhythmVolumeDown() const noexcept
    {
        return _State.DefaultRhythmVolumeDown;
    }

    void SetPPZVolumeDown(int value)
    {
        _State.PPZVolumeDown = _State.DefaultPPZVolumeDown = value;
    }

    int GetPPZVolumeDown() const noexcept
    {
        return _State.PPZVolumeDown;
    }

    int GetDefaultPPZVolumeDown() const noexcept
    {
        return _State.DefaultPPZVolumeDown;
    }

    /// <summary>
    /// Enables or disables PMDB2.COM compatibility.
    /// </summary>
    void SetPMDB2CompatibilityMode(bool value)
    {
        _State.PMDB2CompatibilityMode = _State.DefaultPMDB2CompatibilityMode = value;
    }

    /// <summary>
    /// Returns true if PMD86's PCM is compatible with PMDB2.COM (For songs targetting the Speakboard or compatible sound board which has a YM2608 with ADPCM functionality enabled)
    /// </summary>
    bool GetPMDB2CompatibilityMode() const noexcept
    {
        return _State.PMDB2CompatibilityMode;
    }

    bool GetMemo(const uint8_t * data, size_t size, int al, char * text, size_t textSize);

    int LoadPPC(const WCHAR * filePath);
    int LoadPPS(const WCHAR * filePath);
    int LoadP86(const WCHAR * filePath);
    int LoadPPZ(const WCHAR * filePath, int bufnum);

    bool IsPlaying() const noexcept
    {
        return _IsPlaying;
    }

    Channel * GetChannel(int ch);

private:
    void Reset();
    void StartOPNInterrupt();
    void InitializeState();
    void InitializeOPN();

    void DriverMain();
    void DriverStart();
    void DriverStop();

    void Silence();
    void InitializeTracks();
    void InitializeInterrupt();
    void calc_tb_tempo();
    void calc_tempo_tb();
    void SetTimerBTempo();
    void HandleTimerA();
    void HandleTimerB();
    void IncreaseBarCounter();

    void FMMain(Channel * channel);
    void SSGMain(Channel * channel);
    void RhythmMain(Channel * channel);
    void ADPCMMain(Channel * channel);
    void PCM86Main(Channel * channel);
    void PPZMain(Channel * channel);
    void EffectMain(Channel * channel, int al);

    void RhythmPlayEffect(Channel * channel, int al);

    void PlayEffect();
    void StartEffect(const int * si);
    void StopEffect();
    void Sweep();

    void GetText(const uint8_t * data, size_t size, int al, char * text) const noexcept;

    int MuteFMPart(Channel * channel);

    void SetFMKeyOff(Channel * channel);
    void SetSSGKeyOff(Channel * channel);
    void SetADPCMKeyOff(Channel * channel);
    void SetP86KeyOff(Channel * channel);
    void SetPPZKeyOff(Channel * channel);

    void SetFMTone(Channel * channel, int al);
    void SetSSGTone(Channel * channel, int al);
    void SetADPCMTone(Channel * channel, int al);
    void SetP86Tone(Channel * channel, int al);
    void SetPPZTone(Channel * channel, int al);

    bool CheckSSGDrum(Channel * channel, int al);

    uint8_t * ExecuteFMCommand(Channel * channel, uint8_t * si);
    uint8_t * ExecuteSSGCommand(Channel * channel, uint8_t * si);
    uint8_t * ExecuteADPCMCommand(Channel * channel, uint8_t * si);
    uint8_t * ExecuteRhythmCommand(Channel * channel, uint8_t * si);

    uint8_t * ExecutePCM86Command(Channel * channel, uint8_t * si);
    uint8_t * ExecutePPZCommand(Channel * channel, uint8_t * si);

    uint8_t * special_0c0h(Channel * channel, uint8_t * si, uint8_t al);
    uint8_t * ChangeTempoCommand(uint8_t * si);
    uint8_t * SetSSGPseudoEchoCommand(uint8_t * si);

    uint8_t * _vd_fm(Channel * channel, uint8_t * si);
    uint8_t * _vd_ssg(Channel * channel, uint8_t * si);
    uint8_t * _vd_pcm(Channel * channel, uint8_t * si);
    uint8_t * _vd_rhythm(Channel * channel, uint8_t * si);
    uint8_t * _vd_ppz(Channel * channel, uint8_t * si);

    uint8_t * ChangeProgramCommand(Channel * channel, uint8_t * si);
    uint8_t * comatm(Channel * channel, uint8_t * si);
    uint8_t * comat8(Channel * channel, uint8_t * si);
    uint8_t * comatz(Channel * channel, uint8_t * si);
    uint8_t * SetStartOfLoopCommand(Channel * channel, uint8_t * si);
    uint8_t * SetEndOfLoopCommand(Channel * channel, uint8_t * si);
    uint8_t * ExitLoopCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEnvelopeSpeedToExtend(Channel * channel, uint8_t * si);

    uint8_t * SetLFOParameter(Channel * channel, uint8_t * si);
    uint8_t * psgenvset(Channel * channel, uint8_t * si);

    uint8_t * SetSSGVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * vol_one_up_pcm(Channel * channel, uint8_t * si);
    uint8_t * DecreaseSoundSourceVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGPortamento(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPortamento(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPortamento(Channel * channel, uint8_t * si);
    uint8_t * mdepth_count(Channel * channel, uint8_t * si);
    uint8_t * GetToneData(Channel * channel, int dl);

    uint8_t * SetFMPanning(Channel * channel, uint8_t * si);
    uint8_t * SetFMPanningExtend(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPanning(Channel * channel, uint8_t * si);
    uint8_t * SetP86Panning(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPanning(Channel * channel, uint8_t * si);
    uint8_t * pansetz_ex(Channel * channel, uint8_t * si);

    uint8_t * CalculateQ(Channel * channel, uint8_t * si);
    uint8_t * lfoswitch(Channel * channel, uint8_t * si);

    uint8_t * pcmrepeat_set(Channel * channel, uint8_t * si);
    uint8_t * pcmrepeat_set8(Channel * channel, uint8_t * si);
    uint8_t * ppzrepeat_set(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPanningExtend(Channel * channel, uint8_t * si);
    uint8_t * SetP86PanningExtend(Channel * channel, uint8_t * si);
    uint8_t * pcm_mml_part_mask(Channel * channel, uint8_t * si);
    uint8_t * pcm_mml_part_mask8(Channel * channel, uint8_t * si);
    uint8_t * ppz_mml_part_mask(Channel * channel, uint8_t * si);

    uint8_t * hlfo_set(Channel * channel, uint8_t * si);
    uint8_t * IncreaseFMVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSlotMask(Channel * channel, uint8_t * si);
    uint8_t * slotdetune_set(Channel * channel, uint8_t * si);
    uint8_t * slotdetune_set2(Channel * channel, uint8_t * si);
    uint8_t * SetFMChannel3ModeEx(Channel * channel, uint8_t * si);
    uint8_t * InitializePPZ(Channel * channel, uint8_t * si);
    uint8_t * volmask_set(Channel * channel, uint8_t * si);
    uint8_t * fm_mml_part_mask(Channel * channel, uint8_t * si);
    uint8_t * ssg_mml_part_mask(Channel * channel, uint8_t * si);
    uint8_t * rhythm_mml_part_mask(Channel * channel, uint8_t * si);
    uint8_t * _lfoswitch(Channel * channel, uint8_t * si);
    uint8_t * _volmask_set(Channel * channel, uint8_t * si);
    uint8_t * tl_set(Channel * channel, uint8_t * si);
    uint8_t * fb_set(Channel * channel, uint8_t * si);
    uint8_t * SetFMEffect(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEffect(Channel * channel, uint8_t * si);

    uint8_t * PDRSwitchCommand(Channel * channel, uint8_t * si);

    uint8_t * RhythmOn(Channel * channel, int al, uint8_t * bx, bool * success);
    uint8_t * RhythmInstrumentCommand(uint8_t * si);
    uint8_t * SetRhythmInstrumentVolumeCommand(uint8_t * si);
    uint8_t * SetRhythmOutputPosition(uint8_t * si);
    uint8_t * SetRhythmMasterVolumeCommand(uint8_t * si);
    uint8_t * rmsvs_sft(uint8_t * si);
    uint8_t * rhyvs_sft(uint8_t * si);

    int StartLFO(Channel * channel, int al);
    int StartPCMLFO(Channel * channel, int al);

    void SetTone(Channel * channel, int dl);

    int oshift(Channel * channel, int al);
    int oshiftp(Channel * channel, int al);

    void SetFMPanningMain(Channel * channel, int al);

    uint8_t calc_panout(Channel * channel);
    void fm_block_calc(int * cx, int * ax);
    int SetFMChannel3Mode(Channel * channel);
    void cm_clear(int * ah, int * al);
    void SetFMChannel3Mode2(Channel * channel);
    void ch3_special(Channel * channel, int ax, int cx);

    void SetFMVolumeCommand(Channel * channel);
    void SetSSGVolume2(Channel * channel);
    void SetADPCMVolumeCommand(Channel * channel);
    void SetPCM86Volume(Channel * channel);
    void SetPPZVolume(Channel * channel);

    void SetFMPitch(Channel * channel);
    void SetSSGPitch(Channel * channel);
    void SetADPCMPitch(Channel * channel);
    void SetPCM86Pitch(Channel * channel);
    void SetPPZPitch(Channel * channel);

    void SetFMKeyOn(Channel * channel);
    void SetSSGKeyOn(Channel * channel);
    void SetADPCMKeyOn(Channel * channel);
    void SetP86KeyOn(Channel * channel);
    void SetPPZKeyOn(Channel * channel);

    int lfo(Channel * channel);
    int SetSSGLFO(Channel * channel);
    void lfoinit_main(Channel * channel);
    void SwapLFO(Channel * channel);
    void StopLFO(Channel * channel);
    void lfin1(Channel * channel);
    void LFOMain(Channel * channel);

    int rnd(int ax);

    void fmlfo_sub(Channel * channel, int al, int bl, uint8_t * vol_tbl);
    void volset_slot(int dh, int dl, int al);
    void CalculatePortamento(Channel * channel);

    int SSGPCMSoftwareEnvelope(Channel * channel);
    int SSGPCMSoftwareEnvelopeMain(Channel * channel);
    int SSGPCMSoftwareEnvelopeSub(Channel * channel);
    int ExtendedSSGPCMSoftwareEnvelopeMain(Channel * channel);

    void ExtendedSSGPCMSoftwareEnvelopeSub(Channel * channel, int ah);
    void md_inc(Channel * channel);

    void WritePCMData(uint16_t pcmstart, uint16_t pcmstop, const uint8_t * pcmData);
    void ReadPCMData(uint16_t pcmstart, uint16_t pcmstop, uint8_t * pcmData);

    void InitializeFMChannel3(Channel * channel, uint8_t * ax);

    void Fade();
    void ResetTone(Channel * channel);

    int LoadPPCInternal(const WCHAR * filename);
    int LoadPPCInternal(uint8_t * pcmdata, int size);

    WCHAR * FindFile(WCHAR * dest, const WCHAR * filename);

    inline void Swap(int * a, int * b) const noexcept
    {
        int t = *a;

        *a = *b;
        *b = t;
    }

    inline int Limit(int value, int max, int min) const noexcept
    {
        return value > max ? max : (value < min ? min : value);
    }

private:
    File * _File;

    OPNAW * _OPNAW;

    PPZDriver * _PPZ; 
    PPSDriver * _PPS;
    P86Driver * _P86;

    State _State;
    Driver _Driver;
    Effect _Effect;

    Channel _FMChannel[MaxFMChannels];
    Channel _SSGChannel[MaxSSGChannels];
    Channel _ADPCMChannel;
    Channel _RhythmChannel;
    Channel _FMExtensionChannel[MaxFMExtensionChannels];
    Channel _DummyChannel;
    Channel _EffectChannel;
    Channel _PPZChannel[MaxPPZChannels];

    static const size_t MaxSamples = 30000;

    Stereo16bit _SampleSrc[MaxSamples];
    Stereo32bit _SampleDst[MaxSamples];
    Stereo32bit _SampleTmp[MaxSamples];

    uint8_t _MData[MAX_MDATA_SIZE * 1024];
    uint8_t _VData[MAX_VDATA_SIZE * 1024];
    uint8_t _EData[MAX_EDATA_SIZE * 1024];
    PCMEnds pcmends;

    #pragma region(Dynamic Settings)

    bool _IsPlaying;

    Stereo16bit * _SamplePtr;
    size_t _SamplesToDo;

    int64_t _Position;          // Time from start of playing (in μs)
    int64_t _FadeOutPosition;   // SetFadeOutDurationHQ start time
    int _Seed;                  // Random seed
    #pragma endregion
};
#pragma warning(default: 4820) // x bytes padding added after last data member
