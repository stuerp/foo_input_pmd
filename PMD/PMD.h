
/** $VER: PMDDriver.h (2026.01.04) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#pragma once

#include <pch.h>

#include "Driver.h"

#include "OPNAW.h"

#include "PPS.h"
#include "PPZ.h"
#include "P86.h"

#include "State.h"
#include "Effect.h"

typedef int sample_t;

#pragma pack(push)
#pragma pack(2)
struct sample_bank_t
{
    uint16_t Count;
    uint16_t Address[256][2];
};
#pragma pack(pop)

#pragma warning(disable: 4820) // x bytes padding added after last data member

class pmd_driver_t
{
public:
    pmd_driver_t();
    virtual ~pmd_driver_t();

    bool Initialize(const WCHAR * directoryPath) noexcept;

    static bool IsPMD(const uint8_t * data, size_t size) noexcept;

    int Load(const uint8_t * data, size_t size);

    void Start();
    void Stop();

    void Render(int16_t * sampleData, size_t sampleCount);

    uint32_t GetLoopNumber() const noexcept;

    bool GetLength(int * songLength, int * loopLength, int * tickCount, int * loopTickCount);

    uint32_t GetPosition() const noexcept;
    void SetPosition(uint32_t position);

    int GetPositionInTicks() const noexcept;
    void SetPositionInTicks(int ticks);

    bool SetSearchPaths(std::vector<const WCHAR *> & paths);
    
    void SetSampleRate(uint32_t value) noexcept;
    void SetFMInterpolation(bool flag);

    void SetPPZSampleRate(uint32_t value) noexcept;
    void SetPPZInterpolation(bool flag);

    void SetFadeOutSpeed(int speed);
    void SetFadeOutDurationHQ(int speed);

    // Gets the PCM file path.
    std::wstring & GetPCMFilePath()
    {
        return _PCMFilePath;
    }

    // Gets the PCM file name.
    std::wstring & GetPCMFileName()
    {
        return _PCMFileName;
    }

    // Gets the PPS file path.
    std::wstring & GetPPSFilePath()
    {
        return _PPSFilePath;
    }

    // Gets the PPS file name.
    std::wstring & GetPPSFileName()
    {
        return _PPSFileName;
    }

    // Gets the PPZ file path.
    std::wstring & GetPPZFilePath(size_t bufferNumber)
    {
        return _PPZ->_PPZBanks[bufferNumber]._FilePath;
    }

    // Gets the PPZ file name.
    std::wstring & GetPPZFileName(size_t bufferNumber)
    {
        return _PPZFileName[bufferNumber];
    }

    void UsePPSForDrums(bool value) noexcept;
    void UseSSGForDrums(bool value) noexcept;

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

    int GetSSGVolumeAdjustment() const noexcept
    {
        return _State.SSGVolumeAdjust;
    }

    int GetDefaultSSGVolumeAdjustment() const noexcept
    {
        return _State.DefaultSSGVolumeAdjust;
    }

    void SetADPCMVolumeAdjustment(int value) noexcept
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
        _State._RhythmVolumeAdjust = _State.DefaultRhythmVolumeAdjust = value;

        _State._RhythmVolume = 48 * 4 * (256 - _State._RhythmVolumeAdjust) / 1024;

        _OPNAW->SetReg(0x11, (uint32_t) _State._RhythmVolume);  // Rhythm Part: Set RTL (Total Level)
    }

    int GetRhythmVolumeAdjustment() const noexcept
    {
        return _State._RhythmVolumeAdjust;
    }

    int GetDefaultRhythmVolumeAdjustment() const noexcept
    {
        return _State.DefaultRhythmVolumeAdjust;
    }

    void SetPPZVolumeAdjustment(int value) noexcept
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
    void SetPMDB2CompatibilityMode(bool value) noexcept
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

    channel_t * GetChannel(int channelNumber) const noexcept;

private:
    void Reset();
    void StartOPNInterrupt();
    void InitializeState();
    void InitializeOPN();

    void DriverMain();
    void DriverStart();
    void DriverStop();

    uint8_t * ExecuteCommand(channel_t * channel, uint8_t * si, uint8_t command);

    void Mute();
    void InitializeChannels();
    void InitializeInterrupt();
    void ConvertTimerBTempoToMetronomeTempo();
    void ConvertMetronomeTempoToTimerBTempo();
    void SetTimerBTempo();
    void HandleTimerAInterrupt();
    void HandleTimerBInterrupt();
    void IncreaseBarCounter();

    void GetText(const uint8_t * data, size_t size, int al, char * text, size_t max) const noexcept;

    bool CheckSSGDrum(channel_t * channel, int al);

    #pragma region FM Sound Source

    void FMMain(channel_t * channel) noexcept;

    uint8_t * ExecuteFMCommand(channel_t * channel, uint8_t * si);
    uint8_t * DecreaseFMVolumeCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetFMInstrument(channel_t * channel, uint8_t * si);
    uint8_t * SetFMPan1(channel_t * channel, uint8_t * si);
    uint8_t * SetFMPan2(channel_t * channel, uint8_t * si);
    uint8_t * SetFMPortamentoCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetFMVolumeMaskSlotCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetFMChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept;
    uint8_t * SetFMSlotCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetFMAbsoluteDetuneCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetFMRelativeDetuneCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetFMChannel3ModeEx(channel_t * channel, uint8_t * si);
    uint8_t * SetFMTrueLevelCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetFMFeedbackLoopCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetFMEffect(channel_t * channel, uint8_t * si);

    void SetFMVolumeCommand(channel_t * channel);
    void SetFMPitch(channel_t * channel);
    void FMKeyOn(channel_t * channel);
    void FMKeyOff(channel_t * channel);
    void SetFMDelay(int nsec);
    void SetFMTone(channel_t * channel, int al);

    bool SetFMChannelLFOs(channel_t * channel);
    void SetFMChannel3LFOs(channel_t * channel);

    void InitializeFMInstrument(channel_t * channel, int instrumentNumber, bool setFM3 = false);
    uint8_t * GetFMInstrumentDefinition(channel_t * channel, int dl);
    void ResetFMInstrument(channel_t * channel);

    void SetFMPannningInternal(channel_t * channel, int al);

    int MuteFMChannel(channel_t * channel);

    void ClearFM3(int & ah, int & al) noexcept;

    #pragma endregion

    #pragma region SSG Sound Source

    void SSGMain(channel_t * channel);

    uint8_t * ExecuteSSGCommand(channel_t * channel, uint8_t * si);
    uint8_t * DecreaseSSGVolumeCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetSSGEnvelopeFormat1Command(channel_t * channel, uint8_t * si);
    uint8_t * SetSSGEnvelopeFormat2Command(channel_t * channel, uint8_t * si);
    uint8_t * SetSSGPortamentoCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetSSGChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept;
    uint8_t * SetSSGEffect(channel_t * channel, uint8_t * si);

    void SetSSGVolume(channel_t * channel);
    void SetSSGPitch(channel_t * channel);
    void SSGKeyOn(channel_t * channel);
    void SSGKeyOff(channel_t * channel);
    void SetSSGDelay(int nsec);
    void SetSSGTone(channel_t * channel, int al);

    void SetSSGDrumInstrument(channel_t * channel, int al);

    #pragma endregion

    #pragma region ADPCM Sound Source

    void ADPCMMain(channel_t * channel);

    uint8_t * ExecuteADPCMCommand(channel_t * channel, uint8_t * si);
    uint8_t * DecreaseADPCMVolumeCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetADPCMInstrument(channel_t * channel, uint8_t * si);
    uint8_t * SetADPCMPortamentoCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetADPCMPan1(channel_t * channel, uint8_t * si);
    uint8_t * SetADPCMRepeatCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetADPCMChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept;
    uint8_t * SetADPCMPan2(channel_t * channel, uint8_t * si);

    void SetADPCMVolumeCommand(channel_t * channel);
    void SetADPCMPitch(channel_t * channel);
    void ADPCMKeyOn(channel_t * channel);
    void ADPCMKeyOff(channel_t * channel);
    void SetADPCMDelay(int nsec);
    void SetADPCMTone(channel_t * channel, int al);

    #pragma endregion

    #pragma region Rhythm Sound Source

    void RhythmMain(channel_t * channel);

    uint8_t * ExecuteRhythmCommand(channel_t * channel, uint8_t * si);
    uint8_t * PlayOPNARhythm(uint8_t * si);
    uint8_t * DecreaseRhythmVolumeCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetRhythmChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept;
    uint8_t * RhythmKeyOn(channel_t * channel, int al, uint8_t * bx, bool * success);
    uint8_t * SetOPNARhythmVolumeCommand(uint8_t * si);
    uint8_t * SetOPNARhythmPanningCommand(uint8_t * si);
    uint8_t * SetOPNARhythmMasterVolumeCommand(uint8_t * si);
    uint8_t * SetRelativeOPNARhythmMasterVolume(uint8_t * si);
    uint8_t * SetRelativeOPNARhythmVolume(uint8_t * si);

    void SetRhythmDelay(int nsec);

    #pragma endregion

    #pragma region P86

    void P86Main(channel_t * channel);

    uint8_t * ExecuteP86Command(channel_t * channel, uint8_t * si);
    uint8_t * SetP86Instrument(channel_t * channel, uint8_t * si);
    uint8_t * SetP86Pan1(channel_t * channel, uint8_t * si);
    uint8_t * SetP86RepeatCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetP86Pan2(channel_t * channel, uint8_t * si);
    uint8_t * SetP86ChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept;

    void SetP86Volume(channel_t * channel);
    void SetP86Pitch(channel_t * channel);
    void P86KeyOn(channel_t * channel);
    void P86KeyOff(channel_t * channel);
    void SetP86Tone(channel_t * channel, int al);

    #pragma endregion

    #pragma region PPZ

    void PPZMain(channel_t * channel);

    uint8_t * ExecutePPZCommand(channel_t * channel, uint8_t * si);
    uint8_t * DecreasePPZVolumeCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetPPZInstrument(channel_t * channel, uint8_t * si);
    uint8_t * SetPPZPortamentoCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetPPZPan1(channel_t * channel, uint8_t * si);
    uint8_t * SetPPZPan2(channel_t * channel, uint8_t * si);
    uint8_t * SetPPZRepeatCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetPPZChannelMaskCommand(channel_t * channel, uint8_t * si) noexcept;

    void SetPPZVolume(channel_t * channel);
    void SetPPZPitch(channel_t * channel);
    void PPZKeyOn(channel_t * channel);
    void PPZKeyOff(channel_t * channel);
    void SetPPZTone(channel_t * channel, int al);

    #pragma endregion

    #pragma region Effect

    void SSGEffectMain(channel_t * channel, int al);

    void SSGPlayEffect() noexcept;
    void SSGStartEffect(const int * si);
    void SSGStopEffect();
    void SSGSweep();

    #pragma endregion

    #pragma region LFO

    void LFOMain(channel_t * channel);

    void InitializeLFO(channel_t * channel);
    void InitializeLFOMain(channel_t * channel);

    int SetLFO(channel_t * channel);
    int SetSSGLFO(channel_t * channel);
    void SwapLFO(channel_t * channel) noexcept;
    int StartLFO(channel_t * channel, int al);
    int StartPCMLFO(channel_t * channel, int al);
    void StopLFO(channel_t * channel);

    #pragma endregion

    uint8_t * SpecialC0ProcessingCommand(channel_t * channel, uint8_t * si, uint8_t value) noexcept;
    uint8_t * SetTempoCommand(uint8_t * si);
    uint8_t * SetSSGNoiseFrequencyCommand(uint8_t * si);

    uint8_t * SetStartOfLoopCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetEndOfLoopCommand(channel_t * channel, uint8_t * si);
    uint8_t * ExitLoopCommand(channel_t * channel, uint8_t * si);

    uint8_t * SetModulation(channel_t * channel, uint8_t * si) noexcept;

    uint8_t * IncreaseVolumeForNextNote(channel_t * channel, uint8_t * si, int maxVolume);
    uint8_t * DecreaseVolumeForNextNote(channel_t * channel, uint8_t * si);
    uint8_t * SetMDepthCountCommand(channel_t * channel, uint8_t * si) const noexcept;

    uint8_t * CalculateQ(channel_t * channel, uint8_t * si);
    uint8_t * SetModulationMask(channel_t * channel, uint8_t * si);

    uint8_t * SetHardwareLFOCommand(channel_t * channel, uint8_t * si);
    uint8_t * InitializePPZ(channel_t * channel, uint8_t * si);
    uint8_t * SetHardwareLFOSwitchCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetVolumeMask(channel_t * channel, uint8_t * si);

    uint8_t * SetPDROperationModeControlCommand(channel_t * channel, uint8_t * si);

    int Transpose(channel_t * channel, int al);
    int TransposeSSG(channel_t * channel, int al);

    uint8_t CalcPanOut(channel_t * channel);
    void CalcFMBlock(int * cx, int * ax);
    void InitializeFMChannel3(channel_t * channel, uint8_t * ax);
    void SpecialFM3Processing(channel_t * channel, int ax, int cx);

    int rnd(int ax);

    void CalcFMLFO(channel_t * channel, int al, int bl, uint8_t * vol_tbl);
    void CalcVolSlot(int dh, int dl, int al);
    void CalculatePortamento(channel_t * channel);

    int SSGPCMSoftwareEnvelope(channel_t * channel);
    int SSGPCMSoftwareEnvelopeMain(channel_t * channel);
    int SSGPCMSoftwareEnvelopeSub(channel_t * channel);
    int ExtendedSSGPCMSoftwareEnvelopeMain(channel_t * channel);

    void ExtendedSSGPCMSoftwareEnvelopeSub(channel_t * channel, int ah);
    void SetStepUsingMDValue(channel_t * channel);

    void WritePCMData(uint16_t pcmstart, uint16_t pcmstop, const uint8_t * pcmData);
    void ReadPCMData(uint16_t pcmstart, uint16_t pcmstop, uint8_t * pcmData);

    void Fade();

    int LoadPPCInternal(const WCHAR * filename);
    int LoadPPCInternal(uint8_t * data, size_t size) noexcept;

    void FindFile(const WCHAR * filename, WCHAR * filePath, size_t size) const noexcept;

private:
    std::vector<std::wstring> _SearchPath;

    File * _File;

    opnaw_t * _OPNAW;

    ppz_t * _PPZ; 
    pps_t * _PPS;
    p86_t * _P86;

    bool _UsePPSForDrums;   // Use the PPS.
    bool _UseSSGForDrums;   // Use the SSG to play drum instruments for the K/R commands.

    int _SSGNoiseFrequency;

    State _State;
    Driver _Driver;
    effect_t _SSGEffect;

    channel_t _FMChannels[MaxFMChannels];
    channel_t _SSGChannels[MaxSSGChannels];
    channel_t _ADPCMChannel;
    channel_t _RhythmChannel;

    channel_t _FMExtensionChannels[MaxFMExtensionChannels];
    channel_t _EffectChannel;

    channel_t _PPZChannels[MaxPPZChannels];

    channel_t _DummyChannel;

    static const size_t MaxFrames = 30000;

    frame16_t _SrcFrames[MaxFrames];
    frame32_t _DstFrames[MaxFrames];
    frame32_t _TmpFrames[MaxFrames];

    uint8_t _MData[64 * 1024];
    uint8_t _VData[ 8 * 1024];
    uint8_t _EData[64 * 1024];

    sample_bank_t _SampleBank;

    std::wstring _PCMFileName;      // P86 or PPC
    std::wstring _PCMFilePath;

    std::wstring _PPSFileName;      // PPS
    std::wstring _PPSFilePath;

    std::wstring _PPZFileName[2];   // PVI or PPZ
    std::wstring _PPZFilePath[2];

    #pragma region Dynamic Settings

    bool _InTimerAInterrupt;
    bool _InTimerBInterrupt;

    bool _IsPlaying;
    bool _IsUsingP86;

    frame16_t * _FramePtr;
    size_t _FramesToDo;

    int64_t _Position;          // Time from start of playing (in μs)
    int64_t _FadeOutPosition;   // SetFadeOutDurationHQ start time
    int _Seed;                  // Random seed

    int _OldSSGNoiseFrequency;
    int _OldInstrumentNumber;

    #pragma endregion
};

#pragma warning(default: 4820) // x bytes padding added after last data member
