
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
    uint16_t pcmends;
    uint16_t pcmadrs[256][2];
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

    int Load(const uint8_t * data, size_t size);

    void Start();
    void Stop();

    void Render(int16_t * sampleData, int sampleCount);

    uint32_t GetLoopNumber();

    bool GetLength(int * songlength, int * loopLength);
    bool GetLengthInEvents(int * songLength, int * loopLength);

    uint32_t GetPosition();
    void SetPosition(uint32_t position);

    bool LoadRythmSample(WCHAR * path);
    bool SetSearchPaths(std::vector<const WCHAR *> & paths);
    
    void SetSynthesisRate(uint32_t value);
    void SetPPZSynthesisRate(uint32_t value);
    void EnableFM55kHzSynthesis(bool flag);
    void EnablePPZInterpolation(bool flag);

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

    void setppsinterpolation(bool ip);
    void setp86interpolation(bool ip);

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

    OPEN_WORK * GetOpenWork();
    PartState * GetOpenPartWork(int ch);

private:
    File * _File;

    OPNAW * _OPNA;
    PPZ8 * _PPZ8; 
    PPSDRV * _PPS;
    P86DRV * _P86;

    OPEN_WORK _OpenWork;

    PartState FMPart[NumOfFMPart];
    PartState SSGPart[NumOfSSGPart];
    PartState ADPCMPart;
    PartState RhythmPart;
    PartState ExtPart[NumOfExtPart];
    PartState DummyPart;
    PartState EffPart;
    PartState PPZ8Part[NumOfPPZ8Part];

    PMDWORK pmdwork;
    EffectState effwork;

    Stereo16bit wavbuf2[nbufsample];
    StereoSample wavbuf[nbufsample];
    StereoSample wavbuf_conv[nbufsample];

    uint8_t * _PCMPtr;          // Start position of remaining samples in buf
    int _SamplesToDo;           // Number of samples remaining in buf
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
    void play_init();
    void setint();
    void calc_tb_tempo();
    void calc_tempo_tb();
    void settempo_b();
    void HandleTimerA();
    void HandleTimerB();
    void DriverMain();
    void syousetu_count();

    void FMMain(PartState * ps);
    void PSGMain(PartState * ps);
    void RhythmMain(PartState * ps);
    void ADPCMMain(PartState * ps);
    void PCM86Main(PartState * ps);
    void PPZ8Main(PartState * ps);

    uint8_t * FMCommands(PartState * ps, uint8_t * si);
    uint8_t * PSGCommands(PartState * ps, uint8_t * si);
    uint8_t * RhythmCommands(PartState * ps, uint8_t * si);
    uint8_t * ADPCMCommands(PartState * ps, uint8_t * si);
    uint8_t * PCM86Commands(PartState * ps, uint8_t * si);
    uint8_t * PPZ8Commands(PartState * ps, uint8_t * si);

    uint8_t * rhythmon(PartState * ps, uint8_t * bx, int al, int * result);

    void effgo(PartState * ps, int al);
    void eff_on2(PartState * ps, int al);
    void eff_main(PartState * ps, int al);
    void effplay();
    void efffor(const int * si);
    void effend();
    void effsweep();

    uint8_t * pdrswitch(PartState * ps, uint8_t * si);

    char * GetNoteInternal(const uint8_t * data, size_t size, int al, char * text);

    int MuteFMPart(PartState * ps);
    void keyoff(PartState * ps);
    void kof1(PartState * ps);
    void keyoffp(PartState * ps);
    void keyoffm(PartState * ps);
    void keyoff8(PartState * ps);
    void keyoffz(PartState * ps);

    int ssgdrum_check(PartState * ps, int al);

    uint8_t * special_0c0h(PartState * ps, uint8_t * si, uint8_t al);

    uint8_t * _vd_fm(PartState * ps, uint8_t * si);
    uint8_t * _vd_ssg(PartState * ps, uint8_t * si);
    uint8_t * _vd_pcm(PartState * ps, uint8_t * si);
    uint8_t * _vd_rhythm(PartState * ps, uint8_t * si);
    uint8_t * _vd_ppz(PartState * ps, uint8_t * si);

    uint8_t * comt(uint8_t * si);
    uint8_t * comat(PartState * ps, uint8_t * si);
    uint8_t * comatm(PartState * ps, uint8_t * si);
    uint8_t * comat8(PartState * ps, uint8_t * si);
    uint8_t * comatz(PartState * ps, uint8_t * si);
    uint8_t * comstloop(PartState * ps, uint8_t * si);
    uint8_t * comedloop(PartState * ps, uint8_t * si);
    uint8_t * comexloop(PartState * ps, uint8_t * si);
    uint8_t * extend_psgenvset(PartState * ps, uint8_t * si);

    int lfoinit(PartState * ps, int al);
    int lfoinitp(PartState * ps, int al);

    uint8_t * lfoset(PartState * ps, uint8_t * si);
    uint8_t * psgenvset(PartState * ps, uint8_t * si);
    uint8_t * rhykey(uint8_t * si);
    uint8_t * rhyvs(uint8_t * si);
    uint8_t * rpnset(uint8_t * si);
    uint8_t * rmsvs(uint8_t * si);
    uint8_t * rmsvs_sft(uint8_t * si);
    uint8_t * rhyvs_sft(uint8_t * si);

    uint8_t * vol_one_up_psg(PartState * ps, uint8_t * si);
    uint8_t * vol_one_up_pcm(PartState * ps, uint8_t * si);
    uint8_t * vol_one_down(PartState * ps, uint8_t * si);
    uint8_t * portap(PartState * ps, uint8_t * si);
    uint8_t * portam(PartState * ps, uint8_t * si);
    uint8_t * portaz(PartState * ps, uint8_t * si);
    uint8_t * psgnoise_move(uint8_t * si);
    uint8_t * mdepth_count(PartState * ps, uint8_t * si);
    uint8_t * toneadr_calc(PartState * ps, int dl);

    void neiroset(PartState * ps, int dl);

    int oshift(PartState * ps, int al);
    int oshiftp(PartState * ps, int al);
    void fnumset(PartState * ps, int al);
    void fnumsetp(PartState * ps, int al);
    void fnumsetm(PartState * ps, int al);
    void fnumset8(PartState * ps, int al);
    void fnumsetz(PartState * ps, int al);

    uint8_t * panset(PartState * ps, uint8_t * si);
    uint8_t * panset_ex(PartState * ps, uint8_t * si);
    uint8_t * pansetm(PartState * ps, uint8_t * si);
    uint8_t * panset8(PartState * ps, uint8_t * si);
    uint8_t * pansetz(PartState * ps, uint8_t * si);
    uint8_t * pansetz_ex(PartState * ps, uint8_t * si);
    void panset_main(PartState * ps, int al);

    uint8_t calc_panout(PartState * ps);
    uint8_t * calc_q(PartState * ps, uint8_t * si);
    void fm_block_calc(int * cx, int * ax);
    int ch3_setting(PartState * ps);
    void cm_clear(int * ah, int * al);
    void ch3mode_set(PartState * ps);
    void ch3_special(PartState * ps, int ax, int cx);

    void volset(PartState * ps);
    void volsetp(PartState * ps);
    void volsetm(PartState * ps);
    void volset8(PartState * ps);
    void volsetz(PartState * ps);

    void otodasi(PartState * ps);
    void otodasip(PartState * ps);
    void otodasim(PartState * ps);
    void otodasi8(PartState * ps);
    void otodasiz(PartState * ps);

    void keyon(PartState * ps);
    void keyonp(PartState * ps);
    void keyonm(PartState * ps);
    void keyon8(PartState * ps);
    void keyonz(PartState * ps);

    int lfo(PartState * ps);
    int lfop(PartState * ps);
    uint8_t * lfoswitch(PartState * ps, uint8_t * si);
    void lfoinit_main(PartState * ps);
    void lfo_change(PartState * ps);
    void lfo_exit(PartState * ps);
    void lfin1(PartState * ps);
    void lfo_main(PartState * ps);

    int rnd(int ax);

    void fmlfo_sub(PartState * ps, int al, int bl, uint8_t * vol_tbl);
    void volset_slot(int dh, int dl, int al);
    void porta_calc(PartState * ps);
    int soft_env(PartState * ps);
    int soft_env_main(PartState * ps);
    int soft_env_sub(PartState * ps);
    int ext_ssgenv_main(PartState * ps);
    void esm_sub(PartState * ps, int ah);
    void md_inc(PartState * ps);

    uint8_t * pcmrepeat_set(PartState * ps, uint8_t * si);
    uint8_t * pcmrepeat_set8(PartState * ps, uint8_t * si);
    uint8_t * ppzrepeat_set(PartState * ps, uint8_t * si);
    uint8_t * pansetm_ex(PartState * ps, uint8_t * si);
    uint8_t * panset8_ex(PartState * ps, uint8_t * si);
    uint8_t * pcm_mml_part_mask(PartState * ps, uint8_t * si);
    uint8_t * pcm_mml_part_mask8(PartState * ps, uint8_t * si);
    uint8_t * ppz_mml_part_mask(PartState * ps, uint8_t * si);

    void pcmstore(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);
    void pcmread(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);

    uint8_t * hlfo_set(PartState * ps, uint8_t * si);
    uint8_t * vol_one_up_fm(PartState * ps, uint8_t * si);
    uint8_t * porta(PartState * ps, uint8_t * si);
    uint8_t * slotmask_set(PartState * ps, uint8_t * si);
    uint8_t * slotdetune_set(PartState * ps, uint8_t * si);
    uint8_t * slotdetune_set2(PartState * ps, uint8_t * si);
    void fm3_partinit(PartState * ps, uint8_t * ax);
    uint8_t * fm3_extpartset(PartState * ps, uint8_t * si);
    uint8_t * ppz_extpartset(PartState * ps, uint8_t * si);
    uint8_t * volmask_set(PartState * ps, uint8_t * si);
    uint8_t * fm_mml_part_mask(PartState * ps, uint8_t * si);
    uint8_t * ssg_mml_part_mask(PartState * ps, uint8_t * si);
    uint8_t * rhythm_mml_part_mask(PartState * ps, uint8_t * si);
    uint8_t * _lfoswitch(PartState * ps, uint8_t * si);
    uint8_t * _volmask_set(PartState * ps, uint8_t * si);
    uint8_t * tl_set(PartState * ps, uint8_t * si);
    uint8_t * fb_set(PartState * ps, uint8_t * si);
    uint8_t * fm_efct_set(PartState * ps, uint8_t * si);
    uint8_t * ssg_efct_set(PartState * ps, uint8_t * si);

    void Fade();
    void neiro_reset(PartState * ps);

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
