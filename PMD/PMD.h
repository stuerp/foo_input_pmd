
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
    
    void SetSynthesisRate(int value);
    void SetPPZSynthesisRate(int value);
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

    void EnablePPS(bool value);
    void EnablePlayRythmWithSSG(bool value);
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

    int LoadPPC(WCHAR * filename);
    int LoadPPS(WCHAR * filename);
    int LoadP86(WCHAR * filename);
    int LoadPPZ(WCHAR * filename, int bufnum);

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
    void data_init();
    void opn_init();

    void mstart();
    void mstop();

    void silence();
    void play_init();
    void setint();
    void calc_tb_tempo();
    void calc_tempo_tb();
    void settempo_b();
    void TimerA_main();
    void TimerB_main();
    void mmain();
    void syousetu_count();

    void fmmain(PartState * qq);
    void psgmain(PartState * qq);
    void rhythmmain(PartState * qq);
    void adpcmmain(PartState * qq);
    void pcm86main(PartState * qq);
    void ppz8main(PartState * qq);

    uint8_t * rhythmon(PartState * qq, uint8_t * bx, int al, int * result);

    void effgo(PartState * qq, int al);
    void eff_on2(PartState * qq, int al);
    void eff_main(PartState * qq, int al);
    void effplay();
    void efffor(const int * si);
    void effend();
    void effsweep();

    uint8_t * pdrswitch(PartState * qq, uint8_t * si);

    char * GetNoteInternal(const uint8_t * data, size_t size, int al, char * text);

    int silence_fmpart(PartState * qq);
    void keyoff(PartState * qq);
    void kof1(PartState * qq);
    void keyoffp(PartState * qq);
    void keyoffm(PartState * qq);
    void keyoff8(PartState * qq);
    void keyoffz(PartState * qq);

    int ssgdrum_check(PartState * qq, int al);

    uint8_t * commands(PartState * qq, uint8_t * si);
    uint8_t * commandsp(PartState * qq, uint8_t * si);
    uint8_t * commandsr(PartState * qq, uint8_t * si);
    uint8_t * commandsm(PartState * qq, uint8_t * si);
    uint8_t * commands8(PartState * qq, uint8_t * si);
    uint8_t * commandsz(PartState * qq, uint8_t * si);

    uint8_t * special_0c0h(PartState * qq, uint8_t * si, uint8_t al);

    uint8_t * _vd_fm(PartState * qq, uint8_t * si);
    uint8_t * _vd_ssg(PartState * qq, uint8_t * si);
    uint8_t * _vd_pcm(PartState * qq, uint8_t * si);
    uint8_t * _vd_rhythm(PartState * qq, uint8_t * si);
    uint8_t * _vd_ppz(PartState * qq, uint8_t * si);

    uint8_t * comt(uint8_t * si);
    uint8_t * comat(PartState * qq, uint8_t * si);
    uint8_t * comatm(PartState * qq, uint8_t * si);
    uint8_t * comat8(PartState * qq, uint8_t * si);
    uint8_t * comatz(PartState * qq, uint8_t * si);
    uint8_t * comstloop(PartState * qq, uint8_t * si);
    uint8_t * comedloop(PartState * qq, uint8_t * si);
    uint8_t * comexloop(PartState * qq, uint8_t * si);
    uint8_t * extend_psgenvset(PartState * qq, uint8_t * si);

    int lfoinit(PartState * qq, int al);
    int lfoinitp(PartState * qq, int al);

    uint8_t * lfoset(PartState * qq, uint8_t * si);
    uint8_t * psgenvset(PartState * qq, uint8_t * si);
    uint8_t * rhykey(uint8_t * si);
    uint8_t * rhyvs(uint8_t * si);
    uint8_t * rpnset(uint8_t * si);
    uint8_t * rmsvs(uint8_t * si);
    uint8_t * rmsvs_sft(uint8_t * si);
    uint8_t * rhyvs_sft(uint8_t * si);

    uint8_t * vol_one_up_psg(PartState * qq, uint8_t * si);
    uint8_t * vol_one_up_pcm(PartState * qq, uint8_t * si);
    uint8_t * vol_one_down(PartState * qq, uint8_t * si);
    uint8_t * portap(PartState * qq, uint8_t * si);
    uint8_t * portam(PartState * qq, uint8_t * si);
    uint8_t * portaz(PartState * qq, uint8_t * si);
    uint8_t * psgnoise_move(uint8_t * si);
    uint8_t * mdepth_count(PartState * qq, uint8_t * si);
    uint8_t * toneadr_calc(PartState * qq, int dl);

    void neiroset(PartState * qq, int dl);

    int oshift(PartState * qq, int al);
    int oshiftp(PartState * qq, int al);
    void fnumset(PartState * qq, int al);
    void fnumsetp(PartState * qq, int al);
    void fnumsetm(PartState * qq, int al);
    void fnumset8(PartState * qq, int al);
    void fnumsetz(PartState * qq, int al);

    uint8_t * panset(PartState * qq, uint8_t * si);
    uint8_t * panset_ex(PartState * qq, uint8_t * si);
    uint8_t * pansetm(PartState * qq, uint8_t * si);
    uint8_t * panset8(PartState * qq, uint8_t * si);
    uint8_t * pansetz(PartState * qq, uint8_t * si);
    uint8_t * pansetz_ex(PartState * qq, uint8_t * si);
    void panset_main(PartState * qq, int al);

    uint8_t calc_panout(PartState * qq);
    uint8_t * calc_q(PartState * qq, uint8_t * si);
    void fm_block_calc(int * cx, int * ax);
    int ch3_setting(PartState * qq);
    void cm_clear(int * ah, int * al);
    void ch3mode_set(PartState * qq);
    void ch3_special(PartState * qq, int ax, int cx);

    void volset(PartState * qq);
    void volsetp(PartState * qq);
    void volsetm(PartState * qq);
    void volset8(PartState * qq);
    void volsetz(PartState * qq);

    void otodasi(PartState * qq);
    void otodasip(PartState * qq);
    void otodasim(PartState * qq);
    void otodasi8(PartState * qq);
    void otodasiz(PartState * qq);

    void keyon(PartState * qq);
    void keyonp(PartState * qq);
    void keyonm(PartState * qq);
    void keyon8(PartState * qq);
    void keyonz(PartState * qq);

    int lfo(PartState * qq);
    int lfop(PartState * qq);
    uint8_t * lfoswitch(PartState * qq, uint8_t * si);
    void lfoinit_main(PartState * qq);
    void lfo_change(PartState * qq);
    void lfo_exit(PartState * qq);
    void lfin1(PartState * qq);
    void lfo_main(PartState * qq);

    int rnd(int ax);

    void fmlfo_sub(PartState * qq, int al, int bl, uint8_t * vol_tbl);
    void volset_slot(int dh, int dl, int al);
    void porta_calc(PartState * qq);
    int soft_env(PartState * qq);
    int soft_env_main(PartState * qq);
    int soft_env_sub(PartState * qq);
    int ext_ssgenv_main(PartState * qq);
    void esm_sub(PartState * qq, int ah);
    void md_inc(PartState * qq);

    uint8_t * pcmrepeat_set(PartState * qq, uint8_t * si);
    uint8_t * pcmrepeat_set8(PartState * qq, uint8_t * si);
    uint8_t * ppzrepeat_set(PartState * qq, uint8_t * si);
    uint8_t * pansetm_ex(PartState * qq, uint8_t * si);
    uint8_t * panset8_ex(PartState * qq, uint8_t * si);
    uint8_t * pcm_mml_part_mask(PartState * qq, uint8_t * si);
    uint8_t * pcm_mml_part_mask8(PartState * qq, uint8_t * si);
    uint8_t * ppz_mml_part_mask(PartState * qq, uint8_t * si);

    void pcmstore(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);
    void pcmread(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);

    uint8_t * hlfo_set(PartState * qq, uint8_t * si);
    uint8_t * vol_one_up_fm(PartState * qq, uint8_t * si);
    uint8_t * porta(PartState * qq, uint8_t * si);
    uint8_t * slotmask_set(PartState * qq, uint8_t * si);
    uint8_t * slotdetune_set(PartState * qq, uint8_t * si);
    uint8_t * slotdetune_set2(PartState * qq, uint8_t * si);
    void fm3_partinit(PartState * qq, uint8_t * ax);
    uint8_t * fm3_extpartset(PartState * qq, uint8_t * si);
    uint8_t * ppz_extpartset(PartState * qq, uint8_t * si);
    uint8_t * volmask_set(PartState * qq, uint8_t * si);
    uint8_t * fm_mml_part_mask(PartState * qq, uint8_t * si);
    uint8_t * ssg_mml_part_mask(PartState * qq, uint8_t * si);
    uint8_t * rhythm_mml_part_mask(PartState * qq, uint8_t * si);
    uint8_t * _lfoswitch(PartState * qq, uint8_t * si);
    uint8_t * _volmask_set(PartState * qq, uint8_t * si);
    uint8_t * tl_set(PartState * qq, uint8_t * si);
    uint8_t * fb_set(PartState * qq, uint8_t * si);
    uint8_t * fm_efct_set(PartState * qq, uint8_t * si);
    uint8_t * ssg_efct_set(PartState * qq, uint8_t * si);

    void Fade();
    void neiro_reset(PartState * qq);

    int LoadPPCInternal(WCHAR * filename);
    int LoadPPCInternal(uint8_t * pcmdata, int size);

    WCHAR * FindSampleFile(WCHAR * dest, const WCHAR * filename);
    void swap(int * a, int * b);

    inline int Limit(int v, int max, int min)
    {
        return v > max ? max : (v < min ? min : v);
    }
};
