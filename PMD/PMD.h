
// Based on PMDWin code by C60

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>

#include "OPNA.h"
#include "OPNAW.h"
#include "PPZ.h"
#include "PPS.h"
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

#define max_part1       22 // Number of parts to be cleared to 0 (for PDPPZ)
#define max_part2       11 // Number of parts to initialize (for PMDPPZ)

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

    bool Initialize(const WCHAR * path);

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

    void SetFMWait(int nsec);
    void SetSSGWait(int nsec);
    void SetRhythmWait(int nsec);
    void SetADPCMWait(int nsec);

    void SetFadeOutSpeed(int speed);
    void SetFadeOutDurationHQ(int speed);

    void SetEventNumber(int pos);
    int GetEventNumber();

    WCHAR * GetPCMFileName(WCHAR * dest);
    WCHAR * GetPPZFileName(WCHAR * dest, int bufnum);

    void UsePPS(bool value) noexcept;
    void UseSSG(bool value) noexcept;

    void EnablePMDB2CompatibilityMode(bool value);
    bool GetPMDB2CompatibilityMode();

    void SetPPSInterpolation(bool ip);
    void SetP86Interpolation(bool ip);

    int maskon(int ch);
    int maskoff(int ch);

    void setfmvoldown(int voldown);
    void setssgvoldown(int voldown);
    void setrhythmvoldown(int voldown);
    void setadpcmvoldown(int voldown);
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
    Track * GetTrack(int ch);

private:
    File * _File;

    OPNAW * _OPNA;
    PPZ8 * _PPZ8; 
    PPSDRV * _PPS;
    P86DRV * _P86;

    State _State;
    DriverState _DriverState;
    EffectState _EffectState;

    Track _FMTrack[MaxFMTracks];
    Track _SSGTrack[MaxSSGTracks];
    Track _ADPCMTrack;
    Track _RhythmTrack;
    Track _ExtensionTrack[MaxExtTracks];
    Track _DummyTrack;
    Track _EffectTrack;
    Track _PPZ8Track[MaxPPZ8Tracks];

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
    void InitializeDataArea();
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

    void FMMain(Track * track);
    void PSGMain(Track * track);
    void RhythmMain(Track * track);
    void ADPCMMain(Track * track);
    void PCM86Main(Track * track);
    void PPZ8Main(Track * track);

    uint8_t * FMCommands(Track * track, uint8_t * si);
    uint8_t * PSGCommands(Track * track, uint8_t * si);
    uint8_t * RhythmCommands(Track * track, uint8_t * si);
    uint8_t * ADPCMCommands(Track * track, uint8_t * si);
    uint8_t * PCM86Commands(Track * track, uint8_t * si);
    uint8_t * PPZ8Commands(Track * track, uint8_t * si);

    uint8_t * rhythmon(Track * track, uint8_t * bx, int al, int * result);

    void effgo(Track * track, int al);
    void eff_on2(Track * track, int al);
    void eff_main(Track * track, int al);
    void effplay();
    void efffor(const int * si);
    void effend();
    void effsweep();

    uint8_t * pdrswitch(Track * track, uint8_t * si);

    void GetText(const uint8_t * data, size_t size, int al, char * text) const noexcept;

    int MuteFMPart(Track * track);
    void KeyOff(Track * track);
    void KeyOffEx(Track * track);
    void keyoffp(Track * track);
    void keyoffm(Track * track);
    void keyoff8(Track * track);
    void keyoffz(Track * track);

    bool ssgdrum_check(Track * track, int al);

    uint8_t * special_0c0h(Track * track, uint8_t * si, uint8_t al);

    uint8_t * _vd_fm(Track * track, uint8_t * si);
    uint8_t * _vd_ssg(Track * track, uint8_t * si);
    uint8_t * _vd_pcm(Track * track, uint8_t * si);
    uint8_t * _vd_rhythm(Track * track, uint8_t * si);
    uint8_t * _vd_ppz(Track * track, uint8_t * si);

    uint8_t * comt(uint8_t * si);
    uint8_t * ProgramChange(Track * track, uint8_t * si);
    uint8_t * comatm(Track * track, uint8_t * si);
    uint8_t * comat8(Track * track, uint8_t * si);
    uint8_t * comatz(Track * track, uint8_t * si);
    uint8_t * comstloop(Track * track, uint8_t * si);
    uint8_t * comedloop(Track * track, uint8_t * si);
    uint8_t * comexloop(Track * track, uint8_t * si);
    uint8_t * extend_psgenvset(Track * track, uint8_t * si);

    int lfoinit(Track * track, int al);
    int lfoinitp(Track * track, int al);

    uint8_t * lfoset(Track * track, uint8_t * si);
    uint8_t * psgenvset(Track * track, uint8_t * si);
    uint8_t * rhykey(uint8_t * si);
    uint8_t * rhyvs(uint8_t * si);
    uint8_t * rpnset(uint8_t * si);
    uint8_t * rmsvs(uint8_t * si);
    uint8_t * rmsvs_sft(uint8_t * si);
    uint8_t * rhyvs_sft(uint8_t * si);

    uint8_t * vol_one_up_psg(Track * track, uint8_t * si);
    uint8_t * vol_one_up_pcm(Track * track, uint8_t * si);
    uint8_t * vol_one_down(Track * track, uint8_t * si);
    uint8_t * portap(Track * track, uint8_t * si);
    uint8_t * portam(Track * track, uint8_t * si);
    uint8_t * portaz(Track * track, uint8_t * si);
    uint8_t * psgnoise_move(uint8_t * si);
    uint8_t * mdepth_count(Track * track, uint8_t * si);
    uint8_t * toneadr_calc(Track * track, int dl);

    void SetTone(Track * track, int dl);

    int oshift(Track * track, int al);
    int oshiftp(Track * track, int al);
    void fnumset(Track * track, int al);
    void SetPSGTune(Track * track, int al);
    void fnumsetm(Track * track, int al);
    void fnumset8(Track * track, int al);
    void fnumsetz(Track * track, int al);

    uint8_t * panset(Track * track, uint8_t * si);
    uint8_t * panset_ex(Track * track, uint8_t * si);
    uint8_t * pansetm(Track * track, uint8_t * si);
    uint8_t * panset8(Track * track, uint8_t * si);
    uint8_t * pansetz(Track * track, uint8_t * si);
    uint8_t * pansetz_ex(Track * track, uint8_t * si);
    void panset_main(Track * track, int al);

    uint8_t calc_panout(Track * track);
    uint8_t * calc_q(Track * track, uint8_t * si);
    void fm_block_calc(int * cx, int * ax);
    int ch3_setting(Track * track);
    void cm_clear(int * ah, int * al);
    void ch3mode_set(Track * track);
    void ch3_special(Track * track, int ax, int cx);

    void volset(Track * track);
    void volsetp(Track * track);
    void volsetm(Track * track);
    void volset8(Track * track);
    void volsetz(Track * track);

    void Otodasi(Track * track);
    void OtodasiP(Track * track);
    void OtodasiM(Track * track);
    void Otodasi8(Track * track);
    void OtodasiZ(Track * track);

    void KeyOn(Track * track);
    void keyonp(Track * track);
    void keyonm(Track * track);
    void keyon8(Track * track);
    void keyonz(Track * track);

    int lfo(Track * track);
    int lfop(Track * track);
    uint8_t * lfoswitch(Track * track, uint8_t * si);
    void lfoinit_main(Track * track);
    void SwapLFO(Track * track);
    void lfo_exit(Track * track);
    void lfin1(Track * track);
    void lfo_main(Track * track);

    int rnd(int ax);

    void fmlfo_sub(Track * track, int al, int bl, uint8_t * vol_tbl);
    void volset_slot(int dh, int dl, int al);
    void porta_calc(Track * track);
    int soft_env(Track * track);
    int soft_env_main(Track * track);
    int soft_env_sub(Track * track);
    int ext_ssgenv_main(Track * track);
    void esm_sub(Track * track, int ah);
    void md_inc(Track * track);

    uint8_t * pcmrepeat_set(Track * track, uint8_t * si);
    uint8_t * pcmrepeat_set8(Track * track, uint8_t * si);
    uint8_t * ppzrepeat_set(Track * track, uint8_t * si);
    uint8_t * pansetm_ex(Track * track, uint8_t * si);
    uint8_t * panset8_ex(Track * track, uint8_t * si);
    uint8_t * pcm_mml_part_mask(Track * track, uint8_t * si);
    uint8_t * pcm_mml_part_mask8(Track * track, uint8_t * si);
    uint8_t * ppz_mml_part_mask(Track * track, uint8_t * si);

    void pcmstore(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);
    void pcmread(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);

    uint8_t * hlfo_set(Track * track, uint8_t * si);
    uint8_t * vol_one_up_fm(Track * track, uint8_t * si);
    uint8_t * porta(Track * track, uint8_t * si);
    uint8_t * SetSlotMask(Track * track, uint8_t * si);
    uint8_t * slotdetune_set(Track * track, uint8_t * si);
    uint8_t * slotdetune_set2(Track * track, uint8_t * si);
    void fm3_partinit(Track * track, uint8_t * ax);
    uint8_t * fm3_extpartset(Track * track, uint8_t * si);
    uint8_t * ppz_extpartset(Track * track, uint8_t * si);
    uint8_t * volmask_set(Track * track, uint8_t * si);
    uint8_t * fm_mml_part_mask(Track * track, uint8_t * si);
    uint8_t * ssg_mml_part_mask(Track * track, uint8_t * si);
    uint8_t * rhythm_mml_part_mask(Track * track, uint8_t * si);
    uint8_t * _lfoswitch(Track * track, uint8_t * si);
    uint8_t * _volmask_set(Track * track, uint8_t * si);
    uint8_t * tl_set(Track * track, uint8_t * si);
    uint8_t * fb_set(Track * track, uint8_t * si);
    uint8_t * fm_efct_set(Track * track, uint8_t * si);
    uint8_t * ssg_efct_set(Track * track, uint8_t * si);

    void Fade();
    void ResetTone(Track * track);

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
