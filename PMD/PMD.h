
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
struct SampleInfo
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

    bool GetLength(int * songLength, int * loopLength, int * tickCount, int * loopTickCount);
    bool GetLength(int * songlength, int * loopLength);
    bool GetLengthInTicks(int * songLength, int * loopLength);

    uint32_t GetPosition();
    void SetPosition(uint32_t position);

    bool LoadRythmSamples(WCHAR * path);
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

    void SetPositionInTicks(int ticks);
    int GetPositionInTicks();

    WCHAR * GetPCMFilePath(WCHAR * filePath, size_t size) const;
    WCHAR * GetPPZFilePath(WCHAR * filePath, size_t size, size_t bufferNumber) const;

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

    bool IsPlaying() const noexcept
    {
        return _IsPlaying;
    }

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
    void InitializeChannels();
    void InitializeInterrupt();
    void ConvertTBToTempo();
    void ConvertTempoToTB();
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

    void SSGPlayEffect(Channel * channel, int al);

    void PlayEffect();
    void StartEffect(const int * si);
    void StopEffect();
    void Sweep();

    void GetText(const uint8_t * data, size_t size, int al, char * text) const noexcept;

    int MuteFMChannel(Channel * channel);

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
    uint8_t * DecreaseFMVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMInstrumentCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMPanValueCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMPanValueExtendedCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * IncreaseFMVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMVolumeMaskSlotCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMMaskCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMSlotCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMAbsoluteDetuneCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMRelativeDetuneCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMChannel3ModeEx(Channel * channel, uint8_t * si);
    uint8_t * SetFMTLSettingCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMFBSettingCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMEffect(Channel * channel, uint8_t * si);

    void SetFMVolumeCommand(Channel * channel);
    void SetFMPitch(Channel * channel);
    void SetFMKeyOn(Channel * channel);

    uint8_t * GetFMInstrumentDefinition(Channel * channel, int dl);
    void ResetFMInstrument(Channel * channel);

    uint8_t * ExecuteSSGCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreaseSSGVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEnvelopeFormat1Command(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEnvelopeFormat2Command(Channel * channel, uint8_t * si);
    uint8_t * SetSSGVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGMaskCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEffect(Channel * channel, uint8_t * si);

    void SetSSGVolume(Channel * channel);
    void SetSSGPitch(Channel * channel);
    void SetSSGKeyOn(Channel * channel);

    uint8_t * ExecuteADPCMCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreaseADPCMVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMInstrumentCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPanningCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMRepeatCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMMaskCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPanningExtendCommand(Channel * channel, uint8_t * si);

    void SetADPCMVolumeCommand(Channel * channel);
    void SetADPCMPitch(Channel * channel);
    void SetADPCMKeyOn(Channel * channel);

    uint8_t * ExecuteRhythmCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreaseRhythmVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetRhythmMaskCommand(Channel * channel, uint8_t * si);
    uint8_t * RhythmOn(Channel * channel, int al, uint8_t * bx, bool * success);
    uint8_t * RhythmInstrumentCommand(uint8_t * si);
    uint8_t * SetRhythmInstrumentVolumeCommand(uint8_t * si);
    uint8_t * SetRhythmPanCommand(uint8_t * si);
    uint8_t * SetRhythmMasterVolumeCommand(uint8_t * si);
    uint8_t * SetRhythmVolume(uint8_t * si);
    uint8_t * SetRhythmPanValue(uint8_t * si);

    uint8_t * ExecuteP86Command(Channel * channel, uint8_t * si);
    uint8_t * SetP86InstrumentCommand(Channel * channel, uint8_t * si);
    uint8_t * SetP86PanValueCommand(Channel * channel, uint8_t * si);
    uint8_t * SetP86RepeatCommand(Channel * channel, uint8_t * si);
    uint8_t * SetP86PanValueExtendedCommand(Channel * channel, uint8_t * si);
    uint8_t * SetP86MaskCommand(Channel * channel, uint8_t * si);

    void SetPCM86Volume(Channel * channel);
    void SetPCM86Pitch(Channel * channel);
    void SetP86KeyOn(Channel * channel);

    uint8_t * ExecutePPZCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreasePPZVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZInstrumentCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPanValueCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPanValueExtendedCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZRepeatCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZMaskCommand(Channel * channel, uint8_t * si);

    void SetPPZVolume(Channel * channel);
    void SetPPZPitch(Channel * channel);
    void SetPPZKeyOn(Channel * channel);

    uint8_t * SpecialC0ProcessingCommand(Channel * channel, uint8_t * si, uint8_t value);
    uint8_t * ChangeTempoCommand(uint8_t * si);
    uint8_t * SetSSGPseudoEchoCommand(uint8_t * si);

    uint8_t * SetStartOfLoopCommand(Channel * channel, uint8_t * si);
    uint8_t * SetEndOfLoopCommand(Channel * channel, uint8_t * si);
    uint8_t * ExitLoopCommand(Channel * channel, uint8_t * si);

    uint8_t * SetLFOParameter(Channel * channel, uint8_t * si);

    uint8_t * IncreasePCMVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreaseVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetMDepthCountCommand(Channel * channel, uint8_t * si);

    uint8_t * CalculateQ(Channel * channel, uint8_t * si);
    uint8_t * lfoswitch(Channel * channel, uint8_t * si);

    uint8_t * SetHardLFOCommand(Channel * channel, uint8_t * si);
    uint8_t * InitializePPZ(Channel * channel, uint8_t * si);
    uint8_t * SetHardwareLFOSwitchCommand(Channel * channel, uint8_t * si);
    uint8_t * SetVolumeMask(Channel * channel, uint8_t * si);

    uint8_t * PDRSwitchCommand(Channel * channel, uint8_t * si);

    int StartLFO(Channel * channel, int al);
    int StartPCMLFO(Channel * channel, int al);

    void ActivateFMInstrumentDefinition(Channel * channel, int dl);

    int oshift(Channel * channel, int al);
    int oshiftp(Channel * channel, int al);

    void SetFMPanValueInternal(Channel * channel, int al);

    uint8_t CalcPanOut(Channel * channel);
    void CalcFMBlock(int * cx, int * ax);
    int SetFMChannel3Mode(Channel * channel);
    void SetFMChannel3Mode2(Channel * channel);
    void ClearFM3(int& ah, int& al);
    void SpecialFM3Processing(Channel * channel, int ax, int cx);

    void LFOMain(Channel * channel);
    void InitializeLFO(Channel * channel);
    int lfo(Channel * channel);
    void lfoinit_main(Channel * channel);
    int SetSSGLFO(Channel * channel);
    void SwapLFO(Channel * channel);
    void StopLFO(Channel * channel);

    int rnd(int ax);

    void CalcFMLFO(Channel * channel, int al, int bl, uint8_t * vol_tbl);
    void CalcVolSlot(int dh, int dl, int al);
    void CalculatePortamento(Channel * channel);

    int SSGPCMSoftwareEnvelope(Channel * channel);
    int SSGPCMSoftwareEnvelopeMain(Channel * channel);
    int SSGPCMSoftwareEnvelopeSub(Channel * channel);
    int ExtendedSSGPCMSoftwareEnvelopeMain(Channel * channel);

    void ExtendedSSGPCMSoftwareEnvelopeSub(Channel * channel, int ah);
    void SetStepUsingMDValue(Channel * channel);

    void WritePCMData(uint16_t pcmstart, uint16_t pcmstop, const uint8_t * pcmData);
    void ReadPCMData(uint16_t pcmstart, uint16_t pcmstop, uint8_t * pcmData);

    void InitializeFMChannel3(Channel * channel, uint8_t * ax);

    void Fade();

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
    std::vector<std::wstring> _SearchPath;

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
    SampleInfo _SampleInfo;

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
