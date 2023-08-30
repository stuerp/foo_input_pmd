
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
    void InitializeInternal();
    void StartOPNInterrupt();
    void InitializeDataArea();
    void opn_init();

    void DriverStart();
    void DriverStop();

    void Silence();
    void InitializeTracks();
    void setint();
    void calc_tb_tempo();
    void calc_tempo_tb();
    void settempo_b();
    void HandleTimerA();
    void HandleTimerB();
    void DriverMain();
    void syousetu_count();

    void FMMain(Track * ps);
    void PSGMain(Track * ps);
    void RhythmMain(Track * ps);
    void ADPCMMain(Track * ps);
    void PCM86Main(Track * ps);
    void PPZ8Main(Track * ps);

    uint8_t * FMCommands(Track * ps, uint8_t * si);
    uint8_t * PSGCommands(Track * ps, uint8_t * si);
    uint8_t * RhythmCommands(Track * ps, uint8_t * si);
    uint8_t * ADPCMCommands(Track * ps, uint8_t * si);
    uint8_t * PCM86Commands(Track * ps, uint8_t * si);
    uint8_t * PPZ8Commands(Track * ps, uint8_t * si);

    uint8_t * rhythmon(Track * ps, uint8_t * bx, int al, int * result);

    void effgo(Track * ps, int al);
    void eff_on2(Track * ps, int al);
    void eff_main(Track * ps, int al);
    void effplay();
    void efffor(const int * si);
    void effend();
    void effsweep();

    uint8_t * pdrswitch(Track * ps, uint8_t * si);

    void GetText(const uint8_t * data, size_t size, int al, char * text) const noexcept;

    int MuteFMPart(Track * ps);
    void keyoff(Track * ps);
    void kof1(Track * ps);
    void keyoffp(Track * ps);
    void keyoffm(Track * ps);
    void keyoff8(Track * ps);
    void keyoffz(Track * ps);

    bool ssgdrum_check(Track * ps, int al);

    uint8_t * special_0c0h(Track * ps, uint8_t * si, uint8_t al);

    uint8_t * _vd_fm(Track * ps, uint8_t * si);
    uint8_t * _vd_ssg(Track * ps, uint8_t * si);
    uint8_t * _vd_pcm(Track * ps, uint8_t * si);
    uint8_t * _vd_rhythm(Track * ps, uint8_t * si);
    uint8_t * _vd_ppz(Track * ps, uint8_t * si);

    uint8_t * comt(uint8_t * si);
    uint8_t * comat(Track * ps, uint8_t * si);
    uint8_t * comatm(Track * ps, uint8_t * si);
    uint8_t * comat8(Track * ps, uint8_t * si);
    uint8_t * comatz(Track * ps, uint8_t * si);
    uint8_t * comstloop(Track * ps, uint8_t * si);
    uint8_t * comedloop(Track * ps, uint8_t * si);
    uint8_t * comexloop(Track * ps, uint8_t * si);
    uint8_t * extend_psgenvset(Track * ps, uint8_t * si);

    int lfoinit(Track * ps, int al);
    int lfoinitp(Track * ps, int al);

    uint8_t * lfoset(Track * ps, uint8_t * si);
    uint8_t * psgenvset(Track * ps, uint8_t * si);
    uint8_t * rhykey(uint8_t * si);
    uint8_t * rhyvs(uint8_t * si);
    uint8_t * rpnset(uint8_t * si);
    uint8_t * rmsvs(uint8_t * si);
    uint8_t * rmsvs_sft(uint8_t * si);
    uint8_t * rhyvs_sft(uint8_t * si);

    uint8_t * vol_one_up_psg(Track * ps, uint8_t * si);
    uint8_t * vol_one_up_pcm(Track * ps, uint8_t * si);
    uint8_t * vol_one_down(Track * ps, uint8_t * si);
    uint8_t * portap(Track * ps, uint8_t * si);
    uint8_t * portam(Track * ps, uint8_t * si);
    uint8_t * portaz(Track * ps, uint8_t * si);
    uint8_t * psgnoise_move(uint8_t * si);
    uint8_t * mdepth_count(Track * ps, uint8_t * si);
    uint8_t * toneadr_calc(Track * ps, int dl);

    void neiroset(Track * ps, int dl);

    int oshift(Track * ps, int al);
    int oshiftp(Track * ps, int al);
    void fnumset(Track * ps, int al);
    void fnumsetp(Track * ps, int al);
    void fnumsetm(Track * ps, int al);
    void fnumset8(Track * ps, int al);
    void fnumsetz(Track * ps, int al);

    uint8_t * panset(Track * ps, uint8_t * si);
    uint8_t * panset_ex(Track * ps, uint8_t * si);
    uint8_t * pansetm(Track * ps, uint8_t * si);
    uint8_t * panset8(Track * ps, uint8_t * si);
    uint8_t * pansetz(Track * ps, uint8_t * si);
    uint8_t * pansetz_ex(Track * ps, uint8_t * si);
    void panset_main(Track * ps, int al);

    uint8_t calc_panout(Track * ps);
    uint8_t * calc_q(Track * ps, uint8_t * si);
    void fm_block_calc(int * cx, int * ax);
    int ch3_setting(Track * ps);
    void cm_clear(int * ah, int * al);
    void ch3mode_set(Track * ps);
    void ch3_special(Track * ps, int ax, int cx);

    void volset(Track * ps);
    void volsetp(Track * ps);
    void volsetm(Track * ps);
    void volset8(Track * ps);
    void volsetz(Track * ps);

    void Otodasi(Track * ps);
    void OtodasiP(Track * ps);
    void OtodasiM(Track * ps);
    void Otodasi8(Track * ps);
    void OtodasiZ(Track * ps);

    void keyon(Track * ps);
    void keyonp(Track * ps);
    void keyonm(Track * ps);
    void keyon8(Track * ps);
    void keyonz(Track * ps);

    int lfo(Track * ps);
    int lfop(Track * ps);
    uint8_t * lfoswitch(Track * ps, uint8_t * si);
    void lfoinit_main(Track * ps);
    void lfo_change(Track * ps);
    void lfo_exit(Track * ps);
    void lfin1(Track * ps);
    void lfo_main(Track * ps);

    int rnd(int ax);

    void fmlfo_sub(Track * ps, int al, int bl, uint8_t * vol_tbl);
    void volset_slot(int dh, int dl, int al);
    void porta_calc(Track * ps);
    int soft_env(Track * ps);
    int soft_env_main(Track * ps);
    int soft_env_sub(Track * ps);
    int ext_ssgenv_main(Track * ps);
    void esm_sub(Track * ps, int ah);
    void md_inc(Track * ps);

    uint8_t * pcmrepeat_set(Track * ps, uint8_t * si);
    uint8_t * pcmrepeat_set8(Track * ps, uint8_t * si);
    uint8_t * ppzrepeat_set(Track * ps, uint8_t * si);
    uint8_t * pansetm_ex(Track * ps, uint8_t * si);
    uint8_t * panset8_ex(Track * ps, uint8_t * si);
    uint8_t * pcm_mml_part_mask(Track * ps, uint8_t * si);
    uint8_t * pcm_mml_part_mask8(Track * ps, uint8_t * si);
    uint8_t * ppz_mml_part_mask(Track * ps, uint8_t * si);

    void pcmstore(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);
    void pcmread(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);

    uint8_t * hlfo_set(Track * ps, uint8_t * si);
    uint8_t * vol_one_up_fm(Track * ps, uint8_t * si);
    uint8_t * porta(Track * ps, uint8_t * si);
    uint8_t * slotmask_set(Track * ps, uint8_t * si);
    uint8_t * slotdetune_set(Track * ps, uint8_t * si);
    uint8_t * slotdetune_set2(Track * ps, uint8_t * si);
    void fm3_partinit(Track * ps, uint8_t * ax);
    uint8_t * fm3_extpartset(Track * ps, uint8_t * si);
    uint8_t * ppz_extpartset(Track * ps, uint8_t * si);
    uint8_t * volmask_set(Track * ps, uint8_t * si);
    uint8_t * fm_mml_part_mask(Track * ps, uint8_t * si);
    uint8_t * ssg_mml_part_mask(Track * ps, uint8_t * si);
    uint8_t * rhythm_mml_part_mask(Track * ps, uint8_t * si);
    uint8_t * _lfoswitch(Track * ps, uint8_t * si);
    uint8_t * _volmask_set(Track * ps, uint8_t * si);
    uint8_t * tl_set(Track * ps, uint8_t * si);
    uint8_t * fb_set(Track * ps, uint8_t * si);
    uint8_t * fm_efct_set(Track * ps, uint8_t * si);
    uint8_t * ssg_efct_set(Track * ps, uint8_t * si);

    void Fade();
    void neiro_reset(Track * ps);

    int LoadPPCInternal(const WCHAR * filename);
    int LoadPPCInternal(uint8_t * pcmdata, int size);

    WCHAR * FindFile(WCHAR * dest, const WCHAR * filename);
    void swap(int * a, int * b);

    inline int Limit(int v, int max, int min)
    {
        return v > max ? max : (v < min ? min : v);
    }
};
#pragma warning(default: 4820) // x bytes padding added after last data member
