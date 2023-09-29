
// Based on PMDWin code by C60 / Masahiro Kajihara

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>

#include "OPNAW.h"
#include "PPS.h"
#include "PPZ8.h"
#include "P86.h"

#include "PMDPrivate.h"

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
    void SetRSSDelay(int nsec);

    void SetFadeOutSpeed(int speed);
    void SetFadeOutDurationHQ(int speed);

    void SetEventNumber(int pos);
    int GetEventNumber();

    WCHAR * GetPCMFileName(WCHAR * dest);
    WCHAR * GetPPZFileName(WCHAR * dest, int bufnum);

    void UsePPS(bool value) noexcept;
    void UseRhythm(bool value) noexcept;

    bool HasADPCMROM() const noexcept { return (_OPNAW != nullptr) && _OPNAW->HasADPCMROM(); }
    bool HasPercussionSamples() const noexcept { return (_OPNAW != nullptr) && _OPNAW->HasPercussionSamples(); }

    void EnablePMDB2CompatibilityMode(bool value);
    bool GetPMDB2CompatibilityMode();

    void SetPPSInterpolation(bool ip);
    void SetP86Interpolation(bool ip);

    int DisableChannel(int ch);
    int EnableChannel(int ch);

    void setfmvoldown(int voldown);
    void setssgvoldown(int voldown);
    void setadpcmvoldown(int voldown);
    void setrhythmvoldown(int voldown);
    void setppzvoldown(int voldown);

    int getfmvoldown();
    int getfmvoldown2();
    int getssgvoldown();
    int getssgvoldown2();
    int getrhythmvoldown();
    int getrhythmvoldown2();
    int getadpcmvoldown();
    int getadpcmvoldown2();
    int getppzvoldown();
    int getppzvoldown2();

    bool GetNote(const uint8_t * data, size_t size, int al, char * text, size_t textSize);

    int LoadPPC(const WCHAR * filePath);
    int LoadPPS(const WCHAR * filePath);
    int LoadP86(const WCHAR * filePath);
    int LoadPPZ(const WCHAR * filePath, int bufnum);

    State * GetState() { return &_State; }
    Channel * GetTrack(int ch);

private:
    File * _File;

    OPNAW * _OPNAW;

    PPZ8Driver * _PPZ8; 
    PPSDriver * _PPS;
    P86Driver * _P86;

    State _State;
    DriverState _DriverState;
    EffectState _EffectState;

    Channel _FMTrack[MaxFMTracks];
    Channel _SSGTrack[MaxSSGTracks];
    Channel _ADPCMTrack;
    Channel _RhythmTrack;
    Channel _ExtensionTrack[MaxExtTracks];
    Channel _DummyTrack;
    Channel _EffectTrack;
    Channel _PPZ8Track[MaxPPZ8Tracks];

    static const size_t MaxSamples = 30000;

    Stereo16bit _SampleSrc[MaxSamples];
    Stereo32bit _SampleDst[MaxSamples];
    Stereo32bit _SampleTmp[MaxSamples];

    Stereo16bit * _SamplePtr;
    size_t _SamplesToDo;

    int64_t _Position;          // Time from start of playing (in μs)
    int64_t _FadeOutPosition;   // SetFadeOutDurationHQ start time
    int _Seed;                  // Random seed

    uint8_t _MData[MAX_MDATA_SIZE * 1024];
    uint8_t _VData[MAX_VDATA_SIZE * 1024];
    uint8_t _EData[MAX_EDATA_SIZE * 1024];
    PCMEnds pcmends;

protected:
    void Reset();
    void StartOPNInterrupt();
    void InitializeState();
    void InitializeOPN();

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
    void DriverMain();
    void IncreaseBarCounter();

    void FMMain(Channel * channel);
    void SSGMain(Channel * channel);
    void RhythmMain(Channel * channel);
    void ADPCMMain(Channel * channel);
    void PCM86Main(Channel * channel);
    void PPZ8Main(Channel * channel);

    uint8_t * ExecuteFMCommand(Channel * channel, uint8_t * si);
    uint8_t * ExecuteSSGCommand(Channel * channel, uint8_t * si);
    uint8_t * ExecuteADPCMCommand(Channel * channel, uint8_t * si);
    uint8_t * ExecuteRhythmCommand(Channel * channel, uint8_t * si);

    uint8_t * ExecutePCM86Command(Channel * channel, uint8_t * si);
    uint8_t * ExecutePPZ8Command(Channel * channel, uint8_t * si);

    uint8_t * RhythmOn(Channel * channel, int al, uint8_t * bx, bool * success);

    void effgo(Channel * channel, int al);
    void eff_on2(Channel * channel, int al);
    void EffectMain(Channel * channel, int al);
    void effplay();
    void efffor(const int * si);
    void EffectStop();
    void EffectSweep();

    uint8_t * PDRSwitchCommand(Channel * channel, uint8_t * si);

    void GetText(const uint8_t * data, size_t size, int al, char * text) const noexcept;

    int MuteFMPart(Channel * channel);

    void KeyOff(Channel * channel);
    void KeyOffEx(Channel * channel);
    void keyoffp(Channel * channel);
    void keyoffm(Channel * channel);
    void keyoff8(Channel * channel);
    void keyoffz(Channel * channel);

    uint8_t * ChangeTempoCommand(uint8_t * si);
    uint8_t * ChangeProgramCommand(Channel * channel, uint8_t * si);
    uint8_t * comatm(Channel * channel, uint8_t * si);
    uint8_t * comat8(Channel * channel, uint8_t * si);
    uint8_t * comatz(Channel * channel, uint8_t * si);
    uint8_t * SetStartOfLoopCommand(Channel * channel, uint8_t * si);
    uint8_t * SetEndOfLoopCommand(Channel * channel, uint8_t * si);
    uint8_t * ExitLoopCommand(Channel * channel, uint8_t * si);

    uint8_t * DecreaseSoundSourceVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * IncreasePCMVolumeCommand(Channel * channel, uint8_t * si);

    #pragma region(FM)
    void SetFMVolumeCommand(Channel * channel);
    uint8_t * IncreaseFMVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMPanCommando(Channel * channel, uint8_t * si);
    uint8_t * SetFMEffect(Channel * channel, uint8_t * si);
    #pragma endregion

    #pragma region(SSG)
    uint8_t * SetSSGVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEnvelopeSpeedToExtend(Channel * channel, uint8_t * si);
    void SetSSGTune(Channel * channel, int al);
    uint8_t * SetSSGEffect(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEnvelopeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGPseudoEchoCommand(uint8_t * si);
    uint8_t * SetSSGPortamento(Channel * channel, uint8_t * si);
    bool CheckSSGDrum(Channel * channel, int al);
    #pragma endregion

    #pragma region(Rhythm)
    uint8_t * SetRhythmInstrumentCommand(uint8_t * si);
    uint8_t * SetRhythmInstrumentVolumeCommand(uint8_t * si);
    uint8_t * SetRhythmOutputPositionCommand(uint8_t * si);
    uint8_t * SetRhythmMasterVolumeCommand(uint8_t * si);
    uint8_t * IncreaseRhythmVolumeCommand(uint8_t * si);
    uint8_t * rhyvs_sft(uint8_t * si);
    #pragma endregion

    #pragma region(ADPCM)
    void SetADPCMVolumeCommand(Channel * channel);
    uint8_t * SetADPCMPortamento(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPanCommand(Channel * channel, uint8_t * si);
    #pragma endregion

    #pragma region(PCM86)
    uint8_t * SetPCM86PanCommand(Channel * channel, uint8_t * si);
    #pragma endregion

    #pragma region(PPZ)
    uint8_t * SetPPZPortamento(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPanCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPanExtendCommand(Channel * channel, uint8_t * si);
    #pragma endregion

    uint8_t * mdepth_count(Channel * channel, uint8_t * si);
    uint8_t * GetToneData(Channel * channel, int dl);

    void LFOMain(Channel * channel);
    void lfoinit_main(Channel * channel);
    int lfoinit(Channel * channel, int al);
    int lfoinitp(Channel * channel, int al);
    uint8_t * SetLFOParameter(Channel * channel, uint8_t * si);
    int lfo(Channel * channel);
    int lfop(Channel * channel);
    uint8_t * lfoswitch(Channel * channel, uint8_t * si);
    void SwapLFO(Channel * channel);
    void lfo_exit(Channel * channel);
    void lfin1(Channel * channel);

    void SetTone(Channel * channel, int dl);

    int oshift(Channel * channel, int al);
    int oshiftp(Channel * channel, int al);

    void fnumset(Channel * channel, int al);
    void fnumsetm(Channel * channel, int al);
    void fnumset8(Channel * channel, int al);
    void fnumsetz(Channel * channel, int al);

    uint8_t * panset_ex(Channel * channel, uint8_t * si);
    void panset_main(Channel * channel, int al);

    uint8_t calc_panout(Channel * channel);
    uint8_t * calc_q(Channel * channel, uint8_t * si);
    void fm_block_calc(int * cx, int * ax);
    int ch3_setting(Channel * channel);
    void cm_clear(int * ah, int * al);
    void ch3mode_set(Channel * channel);
    void ch3_special(Channel * channel, int ax, int cx);

    void volsetp(Channel * channel);
    void volset8(Channel * channel);
    void volsetz(Channel * channel);

    void Otodasi(Channel * channel);
    void OtodasiP(Channel * channel);
    void OtodasiM(Channel * channel);
    void Otodasi8(Channel * channel);
    void OtodasiZ(Channel * channel);

    void KeyOn(Channel * channel);
    void keyonp(Channel * channel);
    void keyonm(Channel * channel);
    void keyon8(Channel * channel);
    void keyonz(Channel * channel);

    int rnd(int ax);

    void fmlfo_sub(Channel * channel, int al, int bl, uint8_t * vol_tbl);
    void volset_slot(int dh, int dl, int al);
    void porta_calc(Channel * channel);
    int soft_env(Channel * channel);
    int soft_env_main(Channel * channel);
    int soft_env_sub(Channel * channel);
    int ext_ssgenv_main(Channel * channel);
    void esm_sub(Channel * channel, int ah);
    void md_inc(Channel * channel);

    uint8_t * pcmrepeat_set(Channel * channel, uint8_t * si);
    uint8_t * pcmrepeat_set8(Channel * channel, uint8_t * si);
    uint8_t * ppzrepeat_set(Channel * channel, uint8_t * si);
    uint8_t * pansetm_ex(Channel * channel, uint8_t * si);
    uint8_t * panset8_ex(Channel * channel, uint8_t * si);

    void WritePCMData(uint16_t pcmstart, uint16_t pcmstop, const uint8_t * pcmData);
    void ReadPCMData(uint16_t pcmstart, uint16_t pcmstop, uint8_t * pcmData);

    uint8_t * hlfo_set(Channel * channel, uint8_t * si);
    uint8_t * SetSlotMask(Channel * channel, uint8_t * si);
    uint8_t * slotdetune_set(Channel * channel, uint8_t * si);
    uint8_t * slotdetune_set2(Channel * channel, uint8_t * si);
    void fm3_partinit(Channel * channel, uint8_t * ax);
    uint8_t * fm3_extpartset(Channel * channel, uint8_t * si);
    uint8_t * ppz_extpartset(Channel * channel, uint8_t * si);
    uint8_t * volmask_set(Channel * channel, uint8_t * si);
    uint8_t * _lfoswitch(Channel * channel, uint8_t * si);
    uint8_t * _volmask_set(Channel * channel, uint8_t * si);
    uint8_t * tl_set(Channel * channel, uint8_t * si);
    uint8_t * fb_set(Channel * channel, uint8_t * si);

    uint8_t * fm_mml_part_mask(Channel * channel, uint8_t * si);
    uint8_t * ssg_mml_part_mask(Channel * channel, uint8_t * si);
    uint8_t * pcm_mml_part_mask(Channel * channel, uint8_t * si);
    uint8_t * pcm_mml_part_mask8(Channel * channel, uint8_t * si);
    uint8_t * ppz_mml_part_mask(Channel * channel, uint8_t * si);
    uint8_t * rhythm_mml_part_mask(Channel * channel, uint8_t * si);

    uint8_t * special_0c0h(Channel * channel, uint8_t * si, uint8_t al);

    uint8_t * _vd_fm(Channel * channel, uint8_t * si);
    uint8_t * _vd_ssg(Channel * channel, uint8_t * si);
    uint8_t * _vd_pcm(Channel * channel, uint8_t * si);
    uint8_t * _vd_rhythm(Channel * channel, uint8_t * si);
    uint8_t * _vd_ppz(Channel * channel, uint8_t * si);

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
};
#pragma warning(default: 4820) // x bytes padding added after last data member
