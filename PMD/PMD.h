
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

    int DisableChannel(int ch);
    int EnableChannel(int ch);

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

    Work * GetOpenWork();
    Channel * GetOpenPartWork(int ch);

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

    void FMMain(Channel * ps);
    void PSGMain(Channel * ps);
    void RhythmMain(Channel * ps);
    void ADPCMMain(Channel * ps);
    void PCM86Main(Channel * ps);
    void PPZ8Main(Channel * ps);

    uint8_t * FMCommands(Channel * ps, uint8_t * si);
    uint8_t * PSGCommands(Channel * ps, uint8_t * si);
    uint8_t * RhythmCommands(Channel * ps, uint8_t * si);
    uint8_t * ADPCMCommands(Channel * ps, uint8_t * si);
    uint8_t * PCM86Commands(Channel * ps, uint8_t * si);
    uint8_t * PPZ8Commands(Channel * ps, uint8_t * si);

    uint8_t * rhythmon(Channel * ps, uint8_t * bx, int al, int * result);

    void effgo(Channel * ps, int al);
    void eff_on2(Channel * ps, int al);
    void eff_main(Channel * ps, int al);
    void effplay();
    void efffor(const int * si);
    void effend();
    void effsweep();

    uint8_t * pdrswitch(Channel * ps, uint8_t * si);

    char * GetNoteInternal(const uint8_t * data, size_t size, int al, char * text);

    int MuteFMPart(Channel * ps);
    void keyoff(Channel * ps);
    void kof1(Channel * ps);
    void keyoffp(Channel * ps);
    void keyoffm(Channel * ps);
    void keyoff8(Channel * ps);
    void keyoffz(Channel * ps);

    int ssgdrum_check(Channel * ps, int al);

    uint8_t * special_0c0h(Channel * ps, uint8_t * si, uint8_t al);

    uint8_t * _vd_fm(Channel * ps, uint8_t * si);
    uint8_t * _vd_ssg(Channel * ps, uint8_t * si);
    uint8_t * _vd_pcm(Channel * ps, uint8_t * si);
    uint8_t * _vd_rhythm(Channel * ps, uint8_t * si);
    uint8_t * _vd_ppz(Channel * ps, uint8_t * si);

    uint8_t * comt(uint8_t * si);
    uint8_t * comat(Channel * ps, uint8_t * si);
    uint8_t * comatm(Channel * ps, uint8_t * si);
    uint8_t * comat8(Channel * ps, uint8_t * si);
    uint8_t * comatz(Channel * ps, uint8_t * si);
    uint8_t * comstloop(Channel * ps, uint8_t * si);
    uint8_t * comedloop(Channel * ps, uint8_t * si);
    uint8_t * comexloop(Channel * ps, uint8_t * si);
    uint8_t * extend_psgenvset(Channel * ps, uint8_t * si);

    int lfoinit(Channel * ps, int al);
    int lfoinitp(Channel * ps, int al);

    uint8_t * lfoset(Channel * ps, uint8_t * si);
    uint8_t * psgenvset(Channel * ps, uint8_t * si);
    uint8_t * rhykey(uint8_t * si);
    uint8_t * rhyvs(uint8_t * si);
    uint8_t * rpnset(uint8_t * si);
    uint8_t * rmsvs(uint8_t * si);
    uint8_t * rmsvs_sft(uint8_t * si);
    uint8_t * rhyvs_sft(uint8_t * si);

    uint8_t * vol_one_up_psg(Channel * ps, uint8_t * si);
    uint8_t * vol_one_up_pcm(Channel * ps, uint8_t * si);
    uint8_t * vol_one_down(Channel * ps, uint8_t * si);
    uint8_t * portap(Channel * ps, uint8_t * si);
    uint8_t * portam(Channel * ps, uint8_t * si);
    uint8_t * portaz(Channel * ps, uint8_t * si);
    uint8_t * psgnoise_move(uint8_t * si);
    uint8_t * mdepth_count(Channel * ps, uint8_t * si);
    uint8_t * toneadr_calc(Channel * ps, int dl);

    void neiroset(Channel * ps, int dl);

    int oshift(Channel * ps, int al);
    int oshiftp(Channel * ps, int al);
    void fnumset(Channel * ps, int al);
    void fnumsetp(Channel * ps, int al);
    void fnumsetm(Channel * ps, int al);
    void fnumset8(Channel * ps, int al);
    void fnumsetz(Channel * ps, int al);

    uint8_t * panset(Channel * ps, uint8_t * si);
    uint8_t * panset_ex(Channel * ps, uint8_t * si);
    uint8_t * pansetm(Channel * ps, uint8_t * si);
    uint8_t * panset8(Channel * ps, uint8_t * si);
    uint8_t * pansetz(Channel * ps, uint8_t * si);
    uint8_t * pansetz_ex(Channel * ps, uint8_t * si);
    void panset_main(Channel * ps, int al);

    uint8_t calc_panout(Channel * ps);
    uint8_t * calc_q(Channel * ps, uint8_t * si);
    void fm_block_calc(int * cx, int * ax);
    int ch3_setting(Channel * ps);
    void cm_clear(int * ah, int * al);
    void ch3mode_set(Channel * ps);
    void ch3_special(Channel * ps, int ax, int cx);

    void volset(Channel * ps);
    void volsetp(Channel * ps);
    void volsetm(Channel * ps);
    void volset8(Channel * ps);
    void volsetz(Channel * ps);

    void otodasi(Channel * ps);
    void otodasip(Channel * ps);
    void otodasim(Channel * ps);
    void otodasi8(Channel * ps);
    void otodasiz(Channel * ps);

    void keyon(Channel * ps);
    void keyonp(Channel * ps);
    void keyonm(Channel * ps);
    void keyon8(Channel * ps);
    void keyonz(Channel * ps);

    int lfo(Channel * ps);
    int lfop(Channel * ps);
    uint8_t * lfoswitch(Channel * ps, uint8_t * si);
    void lfoinit_main(Channel * ps);
    void lfo_change(Channel * ps);
    void lfo_exit(Channel * ps);
    void lfin1(Channel * ps);
    void lfo_main(Channel * ps);

    int rnd(int ax);

    void fmlfo_sub(Channel * ps, int al, int bl, uint8_t * vol_tbl);
    void volset_slot(int dh, int dl, int al);
    void porta_calc(Channel * ps);
    int soft_env(Channel * ps);
    int soft_env_main(Channel * ps);
    int soft_env_sub(Channel * ps);
    int ext_ssgenv_main(Channel * ps);
    void esm_sub(Channel * ps, int ah);
    void md_inc(Channel * ps);

    uint8_t * pcmrepeat_set(Channel * ps, uint8_t * si);
    uint8_t * pcmrepeat_set8(Channel * ps, uint8_t * si);
    uint8_t * ppzrepeat_set(Channel * ps, uint8_t * si);
    uint8_t * pansetm_ex(Channel * ps, uint8_t * si);
    uint8_t * panset8_ex(Channel * ps, uint8_t * si);
    uint8_t * pcm_mml_part_mask(Channel * ps, uint8_t * si);
    uint8_t * pcm_mml_part_mask8(Channel * ps, uint8_t * si);
    uint8_t * ppz_mml_part_mask(Channel * ps, uint8_t * si);

    void pcmstore(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);
    void pcmread(uint16_t pcmstart, uint16_t pcmstop, uint8_t * buf);

    uint8_t * hlfo_set(Channel * ps, uint8_t * si);
    uint8_t * vol_one_up_fm(Channel * ps, uint8_t * si);
    uint8_t * porta(Channel * ps, uint8_t * si);
    uint8_t * slotmask_set(Channel * ps, uint8_t * si);
    uint8_t * slotdetune_set(Channel * ps, uint8_t * si);
    uint8_t * slotdetune_set2(Channel * ps, uint8_t * si);
    void fm3_partinit(Channel * ps, uint8_t * ax);
    uint8_t * fm3_extpartset(Channel * ps, uint8_t * si);
    uint8_t * ppz_extpartset(Channel * ps, uint8_t * si);
    uint8_t * volmask_set(Channel * ps, uint8_t * si);
    uint8_t * fm_mml_part_mask(Channel * ps, uint8_t * si);
    uint8_t * ssg_mml_part_mask(Channel * ps, uint8_t * si);
    uint8_t * rhythm_mml_part_mask(Channel * ps, uint8_t * si);
    uint8_t * _lfoswitch(Channel * ps, uint8_t * si);
    uint8_t * _volmask_set(Channel * ps, uint8_t * si);
    uint8_t * tl_set(Channel * ps, uint8_t * si);
    uint8_t * fb_set(Channel * ps, uint8_t * si);
    uint8_t * fm_efct_set(Channel * ps, uint8_t * si);
    uint8_t * ssg_efct_set(Channel * ps, uint8_t * si);

    void Fade();
    void neiro_reset(Channel * ps);

    int LoadPPCInternal(const WCHAR * filename);
    int LoadPPCInternal(uint8_t * pcmdata, int size);

    WCHAR * FindFile(WCHAR * dest, const WCHAR * filename);
    void swap(int * a, int * b);

    inline int Limit(int v, int max, int min)
    {
        return v > max ? max : (v < min ? min : v);
    }

private:
    File * _File;

    OPNAW * _OPNAW;
    P86DRV * _P86;
    PPSDRV * _PPS;
    PPZ8 * _PPZ; 

    Work _Work;

    Channel _FMChannel[MaxFMChannels];
    Channel _SSGChannel[MaxSSGChannels];
    Channel _ADPCMChannel;
    Channel _RhythmChannel;
    Channel _ExtensionChannel[MaxExtensionChannels];
    Channel _DummyChannel;
    Channel _EffectChannel;
    Channel _PPZChannel[MaxPPZChannels];

    PMDWork _PMDWork;
    EffectState _EffectState;

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
};
#pragma warning(default: 4820) // x bytes padding added after last data member
