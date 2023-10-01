
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
    Driver _Driver;
    Effect _Effect;

    Channel _FMTrack[MaxFMTracks];
    Channel _SSGTrack[MaxSSGTracks];
    Channel _ADPCMTrack;
    Channel _RhythmTrack;
    Channel _ExtensionTrack[MaxExtTracks];
    Channel _DummyTrack;
    Channel _EffectTrack;
    Channel _PPZChannel[MaxPPZ8Tracks];

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

    void FMMain(Channel * track);
    void SSGMain(Channel * track);
    void RhythmMain(Channel * track);
    void ADPCMMain(Channel * track);
    void PCM86Main(Channel * track);
    void PPZMain(Channel * track);

    uint8_t * ExecuteFMCommand(Channel * track, uint8_t * si);
    uint8_t * ExecuteSSGCommand(Channel * track, uint8_t * si);
    uint8_t * ExecuteADPCMCommand(Channel * track, uint8_t * si);
    uint8_t * ExecuteRhythmCommand(Channel * track, uint8_t * si);

    uint8_t * ExecutePCM86Command(Channel * track, uint8_t * si);
    uint8_t * ExecutePPZCommand(Channel * track, uint8_t * si);

    uint8_t * RhythmOn(Channel * track, int al, uint8_t * bx, bool * success);

    void RhythmPlayEffect(Channel * track, int al);
    void EffectMain(Channel * track, int al);
    void PlayEffect();
    void StartEffect(const int * si);
    void StopEffect();
    void Sweep();

    uint8_t * PDRSwitchCommand(Channel * track, uint8_t * si);

    void GetText(const uint8_t * data, size_t size, int al, char * text) const noexcept;

    int MuteFMPart(Channel * track);
    void SetFMKeyOff(Channel * track);
    void SetSSGKeyOff(Channel * track);
    void SetADPCMKeyOff(Channel * track);
    void SetP86KeyOff(Channel * track);
    void SetPPZKeyOff(Channel * track);

    bool CheckSSGDrum(Channel * track, int al);

    uint8_t * special_0c0h(Channel * track, uint8_t * si, uint8_t al);

    uint8_t * _vd_fm(Channel * track, uint8_t * si);
    uint8_t * _vd_ssg(Channel * track, uint8_t * si);
    uint8_t * _vd_pcm(Channel * track, uint8_t * si);
    uint8_t * _vd_rhythm(Channel * track, uint8_t * si);
    uint8_t * _vd_ppz(Channel * track, uint8_t * si);

    uint8_t * ChangeTempoCommand(uint8_t * si);
    uint8_t * ChangeProgramCommand(Channel * track, uint8_t * si);
    uint8_t * comatm(Channel * track, uint8_t * si);
    uint8_t * comat8(Channel * track, uint8_t * si);
    uint8_t * comatz(Channel * track, uint8_t * si);
    uint8_t * SetStartOfLoopCommand(Channel * track, uint8_t * si);
    uint8_t * SetEndOfLoopCommand(Channel * track, uint8_t * si);
    uint8_t * ExitLoopCommand(Channel * track, uint8_t * si);
    uint8_t * SetSSGEnvelopeSpeedToExtend(Channel * track, uint8_t * si);

    int StartLFO(Channel * track, int al);
    int StartPCMLFO(Channel * track, int al);

    uint8_t * SetLFOParameter(Channel * track, uint8_t * si);
    uint8_t * psgenvset(Channel * track, uint8_t * si);
    uint8_t * RhythmInstrumentCommand(uint8_t * si);
    uint8_t * SetRhythmInstrumentVolumeCommand(uint8_t * si);
    uint8_t * SetRhythmOutputPosition(uint8_t * si);
    uint8_t * SetRhythmMasterVolumeCommand(uint8_t * si);
    uint8_t * rmsvs_sft(uint8_t * si);
    uint8_t * rhyvs_sft(uint8_t * si);

    uint8_t * SetSSGVolumeCommand(Channel * track, uint8_t * si);
    uint8_t * vol_one_up_pcm(Channel * track, uint8_t * si);
    uint8_t * DecreaseSoundSourceVolumeCommand(Channel * track, uint8_t * si);
    uint8_t * SetSSGPortamento(Channel * track, uint8_t * si);
    uint8_t * SetADPCMPortamento(Channel * track, uint8_t * si);
    uint8_t * SetPPZPortamento(Channel * track, uint8_t * si);
    uint8_t * SetSSGPseudoEchoCommand(uint8_t * si);
    uint8_t * mdepth_count(Channel * track, uint8_t * si);
    uint8_t * GetToneData(Channel * track, int dl);

    void SetTone(Channel * track, int dl);

    int oshift(Channel * track, int al);
    int oshiftp(Channel * track, int al);
    void SetFMTone(Channel * track, int al);
    void SetSSGTone(Channel * track, int al);
    void SetADPCMTone(Channel * track, int al);
    void SetP86Tone(Channel * track, int al);
    void SetPPZTone(Channel * track, int al);

    uint8_t * SetFMPanning(Channel * track, uint8_t * si);
    uint8_t * SetFMPanningExtend(Channel * track, uint8_t * si);
    uint8_t * SetADPCMPanning(Channel * track, uint8_t * si);
    uint8_t * SetP86Panning(Channel * track, uint8_t * si);
    uint8_t * SetPPZPanning(Channel * track, uint8_t * si);
    uint8_t * pansetz_ex(Channel * track, uint8_t * si);
    void SetFMPanningMain(Channel * track, int al);

    uint8_t calc_panout(Channel * track);
    uint8_t * CalculateQ(Channel * track, uint8_t * si);
    void fm_block_calc(int * cx, int * ax);
    int SetFMChannel3Mode(Channel * track);
    void cm_clear(int * ah, int * al);
    void SetFMChannel3Mode2(Channel * track);
    void ch3_special(Channel * track, int ax, int cx);

    void SetFMVolumeCommand(Channel * track);
    void SetSSGVolume2(Channel * track);
    void SetADPCMVolumeCommand(Channel * track);
    void SetPCM86Volume(Channel * track);
    void SetPPZVolume(Channel * track);

    void SetFMPitch(Channel * track);
    void SetSSGPitch(Channel * track);
    void SetADPCMPitch(Channel * track);
    void SetPCM86Pitch(Channel * track);
    void SetPPZPitch(Channel * track);

    void SetFMKeyOn(Channel * track);
    void SetSSGKeyOn(Channel * track);
    void SetADPCMKeyOn(Channel * track);
    void SetP86KeyOn(Channel * track);
    void SetPPZKeyOn(Channel * track);

    int lfo(Channel * track);
    int SetSSGLFO(Channel * track);
    uint8_t * lfoswitch(Channel * track, uint8_t * si);
    void lfoinit_main(Channel * track);
    void SwapLFO(Channel * track);
    void StopLFO(Channel * track);
    void lfin1(Channel * track);
    void LFOMain(Channel * track);

    int rnd(int ax);

    void fmlfo_sub(Channel * track, int al, int bl, uint8_t * vol_tbl);
    void volset_slot(int dh, int dl, int al);
    void CalculatePortamento(Channel * track);
    int SSGPCMSoftwareEnvelope(Channel * track);
    int SSGPCMSoftwareEnvelopeMain(Channel * track);
    int SSGPCMSoftwareEnvelopeSub(Channel * track);
    int ExtendedSSGPCMSoftwareEnvelopeMain(Channel * track);
    void ExtendedSSGPCMSoftwareEnvelopeSub(Channel * track, int ah);
    void md_inc(Channel * track);

    uint8_t * pcmrepeat_set(Channel * track, uint8_t * si);
    uint8_t * pcmrepeat_set8(Channel * track, uint8_t * si);
    uint8_t * ppzrepeat_set(Channel * track, uint8_t * si);
    uint8_t * SetADPCMPanningExtend(Channel * track, uint8_t * si);
    uint8_t * SetP86PanningExtend(Channel * track, uint8_t * si);
    uint8_t * pcm_mml_part_mask(Channel * track, uint8_t * si);
    uint8_t * pcm_mml_part_mask8(Channel * track, uint8_t * si);
    uint8_t * ppz_mml_part_mask(Channel * track, uint8_t * si);

    void WritePCMData(uint16_t pcmstart, uint16_t pcmstop, const uint8_t * pcmData);
    void ReadPCMData(uint16_t pcmstart, uint16_t pcmstop, uint8_t * pcmData);

    uint8_t * hlfo_set(Channel * track, uint8_t * si);
    uint8_t * IncreaseFMVolumeCommand(Channel * track, uint8_t * si);
    uint8_t * SetFMPortamentoCommand(Channel * track, uint8_t * si);
    uint8_t * SetSlotMask(Channel * track, uint8_t * si);
    uint8_t * slotdetune_set(Channel * track, uint8_t * si);
    uint8_t * slotdetune_set2(Channel * track, uint8_t * si);
    void InitializeFMChannel3(Channel * track, uint8_t * ax);
    uint8_t * SetFMChannel3ModeEx(Channel * track, uint8_t * si);
    uint8_t * InitializePPZ(Channel * track, uint8_t * si);
    uint8_t * volmask_set(Channel * track, uint8_t * si);
    uint8_t * fm_mml_part_mask(Channel * track, uint8_t * si);
    uint8_t * ssg_mml_part_mask(Channel * track, uint8_t * si);
    uint8_t * rhythm_mml_part_mask(Channel * track, uint8_t * si);
    uint8_t * _lfoswitch(Channel * track, uint8_t * si);
    uint8_t * _volmask_set(Channel * track, uint8_t * si);
    uint8_t * tl_set(Channel * track, uint8_t * si);
    uint8_t * fb_set(Channel * track, uint8_t * si);
    uint8_t * SetFMEffect(Channel * track, uint8_t * si);
    uint8_t * SetSSGEffect(Channel * track, uint8_t * si);

    void Fade();
    void ResetTone(Channel * track);

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
