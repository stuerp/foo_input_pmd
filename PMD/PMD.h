
/** $VER: PMD.h (2023.10.29) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

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

#include "State.h"
#include "Driver.h"
#include "Effect.h"

typedef int Sample;

#pragma pack(push)
#pragma pack(2)
struct SampleBank
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
    bool GetLengthInTicks(int * tickCount, int * loopTickCount);

    uint32_t GetPosition();
    void SetPosition(uint32_t position);

    int GetPositionInTicks();
    void SetPositionInTicks(int ticks);

    bool SetSearchPaths(std::vector<const WCHAR *> & paths);
    
    void SetOutputFrequency(uint32_t value) noexcept;
    void SetFMInterpolation(bool flag);

    void SetPPZOutputFrequency(uint32_t value) noexcept;
    void SetPPZInterpolation(bool flag);

    void SetFadeOutSpeed(int speed);
    void SetFadeOutDurationHQ(int speed);

    // Gets the PCM file path.
    std::wstring& GetPCMFilePath()
    {
        return _PCMFilePath;
    }

    // Gets the PCM file name.
    std::wstring& GetPCMFileName()
    {
        return _PCMFileName;
    }

    // Gets the PPS file path.
    std::wstring& GetPPSFilePath()
    {
        return _PPSFilePath;
    }

    // Gets the PPS file name.
    std::wstring& GetPPSFileName()
    {
        return _PPSFileName;
    }

    // Gets the PPZ file path.
    std::wstring& GetPPZFilePath(size_t bufferNumber)
    {
        return _PPZ->_PPZBank[bufferNumber]._FilePath;
    }

    // Gets the PPZ file name.
    std::wstring& GetPPZFileName(size_t bufferNumber)
    {
        return _PPZFileName[bufferNumber];
    }

    void UsePPS(bool value) noexcept;
    void UseSSG(bool value) noexcept;

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

    void SetFMVolumeAdjustment(int value)
    {
        _State.FMVolumeAdjust = _State.DefaultFMVolumeAdjust = value;
    }

    int GetFMVolumeAdjustment() const noexcept
    {
        return _State.FMVolumeAdjust;
    }

    int GetDefaultFMVolumeAdjustment() const noexcept
    {
        return _State.DefaultFMVolumeAdjust;
    }

    void SetSSGVolumeAdjustment(int value)
    {
        _State.SSGVolumeAdjust = _State.DefaultSSGVolumeAdjust = value;
    }

    int GetSSGVolumeAdjustment()
    {
        return _State.SSGVolumeAdjust;
    }

    int GetDefaultSSGVolumeAdjustment()
    {
        return _State.DefaultSSGVolumeAdjust;
    }

    void SetADPCMVolumeAdjustment(int value)
    {
        _State.ADPCMVolumeAdjust = _State.DefaultADPCMVolumeAdjust = value;
    }

    int GetADPCMVolumeAdjustment() const noexcept
    {
        return _State.ADPCMVolumeAdjust;
    }

    int GetDefaultADPCMVolumeAdjustment() const noexcept
    {
        return _State.DefaultADPCMVolumeAdjust;
    }

    void SetRhythmVolumeAdjustment(int value)
    {
        _State.RhythmVolumeAdjust = _State.DefaultRhythmVolumeAdjust = value;

        _State.RhythmVolume = 48 * 4 * (256 - _State.RhythmVolumeAdjust) / 1024;

        _OPNAW->SetReg(0x11, (uint32_t) _State.RhythmVolume);
    }

    int GetRhythmVolumeAdjustment() const noexcept
    {
        return _State.RhythmVolumeAdjust;
    }

    int GetDefaultRhythmVolumeAdjustment() const noexcept
    {
        return _State.DefaultRhythmVolumeAdjust;
    }

    void SetPPZVolumeAdjustment(int value)
    {
        _State.PPZVolumeAdjust = _State.DefaultPPZVolumeAdjust = value;
    }

    int GetPPZVolumeAdjustment() const noexcept
    {
        return _State.PPZVolumeAdjust;
    }

    int GetDefaultPPZVolumeAdjustment() const noexcept
    {
        return _State.DefaultPPZVolumeAdjust;
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

    uint8_t * ExecuteCommand(Channel * channel, uint8_t * si, uint8_t command);

    void Mute();
    void InitializeChannels();
    void InitializeInterrupt();
    void ConvertTimerBTempoToMetronomeTempo();
    void ConvertMetronomeTempoToTimerBTempo();
    void SetTimerBTempo();
    void HandleTimerA();
    void HandleTimerB();
    void IncreaseBarCounter();

    void GetText(const uint8_t * data, size_t size, int al, char * text) const noexcept;

    int MuteFMChannel(Channel * channel);

    bool CheckSSGDrum(Channel * channel, int al);

    #pragma region(FM Sound Source)
    void FMMain(Channel * channel);
    uint8_t * ExecuteFMCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreaseFMVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMInstrument(Channel * channel, uint8_t * si);
    uint8_t * SetFMPanning(Channel * channel, uint8_t * si);
    uint8_t * SetFMPanningExtend(Channel * channel, uint8_t * si);
    uint8_t * SetFMPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMVolumeMaskSlotCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMMaskCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMSlotCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMAbsoluteDetuneCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMRelativeDetuneCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMChannel3ModeEx(Channel * channel, uint8_t * si);
    uint8_t * SetFMTLSettingCommand(Channel * channel, uint8_t * si);
    uint8_t * SetFMFeedbackLoops(Channel * channel, uint8_t * si);
    uint8_t * SetFMEffect(Channel * channel, uint8_t * si);

    void SetFMVolumeCommand(Channel * channel);
    void SetFMPitch(Channel * channel);
    void FMKeyOn(Channel * channel);
    void FMKeyOff(Channel * channel);
    void SetFMDelay(int nsec);
    void SetFMTone(Channel * channel, int al);

    void InitializeFMInstrument(Channel * channel, int instrumentNumber, bool setFM3 = false);
    uint8_t * GetFMInstrumentDefinition(Channel * channel, int dl);
    void ResetFMInstrument(Channel * channel);

    void SetFMPannningInternal(Channel * channel, int al);
    #pragma endregion

    #pragma region(SSG Sound Source)
    void SSGMain(Channel * channel);
    uint8_t * ExecuteSSGCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreaseSSGVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEnvelopeFormat1Command(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEnvelopeFormat2Command(Channel * channel, uint8_t * si);
    uint8_t * SetSSGPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGMaskCommand(Channel * channel, uint8_t * si);
    uint8_t * SetSSGEffect(Channel * channel, uint8_t * si);

    void SetSSGVolume(Channel * channel);
    void SetSSGPitch(Channel * channel);
    void SSGKeyOn(Channel * channel);
    void SSGKeyOff(Channel * channel);
    void SetSSGDelay(int nsec);
    void SetSSGTone(Channel * channel, int al);

    void SetSSGInstrument(Channel * channel, int al);
    #pragma endregion

    #pragma region(ADPCM Sound Source)
    void ADPCMMain(Channel * channel);
    uint8_t * ExecuteADPCMCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreaseADPCMVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMInstrument(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPanning(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMRepeatCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMMaskCommand(Channel * channel, uint8_t * si);
    uint8_t * SetADPCMPanningExtend(Channel * channel, uint8_t * si);

    void SetADPCMVolumeCommand(Channel * channel);
    void SetADPCMPitch(Channel * channel);
    void ADPCMKeyOn(Channel * channel);
    void ADPCMKeyOff(Channel * channel);
    void SetADPCMDelay(int nsec);
    void SetADPCMTone(Channel * channel, int al);
    #pragma endregion

    #pragma region(Rhythm Sound Source)
    void RhythmMain(Channel * channel);
    uint8_t * ExecuteRhythmCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreaseRhythmVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetRhythmMaskCommand(Channel * channel, uint8_t * si);
    uint8_t * RhythmKeyOn(Channel * channel, int al, uint8_t * bx, bool * success);
    uint8_t * OPNARhythmKeyOn(uint8_t * si);
    uint8_t * SetOPNARhythmVolumeCommand(uint8_t * si);
    uint8_t * SetOPNARhythmPanningCommand(uint8_t * si);
    uint8_t * SetOPNARhythmMasterVolumeCommand(uint8_t * si);
    uint8_t * ModifyOPNARhythmMasterVolume(uint8_t * si);
    uint8_t * ModifyOPNARhythmVolume(uint8_t * si);

    void SetRhythmDelay(int nsec);
    #pragma endregion

    #pragma region(P86)
    void P86Main(Channel * channel);
    uint8_t * ExecuteP86Command(Channel * channel, uint8_t * si);
    uint8_t * SetP86Instrument(Channel * channel, uint8_t * si);
    uint8_t * SetP86Panning(Channel * channel, uint8_t * si);
    uint8_t * SetP86RepeatCommand(Channel * channel, uint8_t * si);
    uint8_t * SetP86PanningExtend(Channel * channel, uint8_t * si);
    uint8_t * SetP86MaskCommand(Channel * channel, uint8_t * si);

    void SetP86Volume(Channel * channel);
    void SetP86Pitch(Channel * channel);
    void P86KeyOn(Channel * channel);
    void P86KeyOff(Channel * channel);
    void SetP86Tone(Channel * channel, int al);
    #pragma endregion

    #pragma region(PPZ)
    void PPZMain(Channel * channel);
    uint8_t * ExecutePPZCommand(Channel * channel, uint8_t * si);
    uint8_t * DecreasePPZVolumeCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZInstrument(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPortamentoCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPanning(Channel * channel, uint8_t * si);
    uint8_t * SetPPZPanningExtend(Channel * channel, uint8_t * si);
    uint8_t * SetPPZRepeatCommand(Channel * channel, uint8_t * si);
    uint8_t * SetPPZMaskCommand(Channel * channel, uint8_t * si);

    void SetPPZVolume(Channel * channel);
    void SetPPZPitch(Channel * channel);
    void PPZKeyOn(Channel * channel);
    void PPZKeyOff(Channel * channel);
    void SetPPZTone(Channel * channel, int al);
    #pragma endregion

    #pragma region(Effect)
    void EffectMain(Channel * channel, int al);

    void PlayEffect();
    void StartEffect(const int * si);
    void StopEffect();
    void Sweep();
    #pragma endregion

    #pragma region(LFO)
    void LFOMain(Channel * channel);
    void InitializeLFO(Channel * channel);
    void InitializeLFOMain(Channel * channel);

    int SetLFO(Channel * channel);
    int SetSSGLFO(Channel * channel);
    void SwapLFO(Channel * channel);
    int StartLFO(Channel * channel, int al);
    int StartPCMLFO(Channel * channel, int al);
    void StopLFO(Channel * channel);
    #pragma endregion

    uint8_t * SpecialC0ProcessingCommand(Channel * channel, uint8_t * si, uint8_t value);
    uint8_t * ChangeTempoCommand(uint8_t * si);
    uint8_t * SetSSGPseudoEchoCommand(uint8_t * si);

    uint8_t * SetStartOfLoopCommand(Channel * channel, uint8_t * si);
    uint8_t * SetEndOfLoopCommand(Channel * channel, uint8_t * si);
    uint8_t * ExitLoopCommand(Channel * channel, uint8_t * si);

    uint8_t * SetModulation(Channel * channel, uint8_t * si);

    uint8_t * IncreaseVolumeForNextNote(Channel * channel, uint8_t * si, int maxVolume);
    uint8_t * DecreaseVolumeForNextNote(Channel * channel, uint8_t * si);
    uint8_t * SetMDepthCountCommand(Channel * channel, uint8_t * si);

    uint8_t * CalculateQ(Channel * channel, uint8_t * si);
    uint8_t * SetModulationMask(Channel * channel, uint8_t * si);

    uint8_t * SetHardwareLFOCommand(Channel * channel, uint8_t * si);
    uint8_t * InitializePPZ(Channel * channel, uint8_t * si);
    uint8_t * SetHardwareLFOSwitchCommand(Channel * channel, uint8_t * si);
    uint8_t * SetVolumeMask(Channel * channel, uint8_t * si);

    uint8_t * PDRSwitchCommand(Channel * channel, uint8_t * si);

    int Transpose(Channel * channel, int al);
    int TransposeSSG(Channel * channel, int al);

    uint8_t CalcPanOut(Channel * channel);
    void CalcFMBlock(int * cx, int * ax);
    bool SetFMChannel3Mode(Channel * channel);
    void SetFMChannel3Mode2(Channel * channel);
    void ClearFM3(int& ah, int& al);
    void InitializeFMChannel3(Channel * channel, uint8_t * ax);
    void SpecialFM3Processing(Channel * channel, int ax, int cx);

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

    void Fade();

    int LoadPPCInternal(const WCHAR * filename);
    int LoadPPCInternal(uint8_t * pcmdata, int size);

    void FindFile(const WCHAR * filename, WCHAR * filePath, size_t size) const noexcept;

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

    SampleBank _SampleBank;

    std::wstring _PCMFileName;      // P86 or PPC
    std::wstring _PCMFilePath;

    std::wstring _PPSFileName;      // PPS
    std::wstring _PPSFilePath;

    std::wstring _PPZFileName[2];   // PVI or PPZ
    std::wstring _PPZFilePath[2];

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
