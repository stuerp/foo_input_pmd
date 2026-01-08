
/** $VER: PMDDriver.h (2026.01.07) PMD driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#pragma once

#include <pch.h>

#include "Driver.h"

#include "OPNAW.h"

#include "PPS.h"
#include "PPZ8.h"
#include "P86.h"

#include "State.h"
#include "Effect.h"

typedef int32_t sample_t;

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

    int32_t Load(const uint8_t * data, size_t size) noexcept;

    void Start() noexcept;
    void Stop() noexcept;

    void Render(int16_t * sampleData, size_t sampleCount) noexcept;

    uint32_t GetLoopNumber() const noexcept;

    bool GetLength(uint32_t & songLength, uint32_t & loopLength, uint32_t & songTicks, uint32_t & loopTicks) noexcept;

    uint32_t GetPosition() const noexcept;
    void SetPosition(uint32_t position);

    uint32_t GetPositionInTicks() const noexcept;
    void SetPositionInTicks(int ticks);

    bool SetSearchPaths(std::vector<const WCHAR *> & paths);
    
    void SetSampleRate(uint32_t value) noexcept;
    void SetInterpolation(bool flag);

    void SetPPZSampleRate(uint32_t value) noexcept;
    void SetPPZInterpolation(bool flag);

    void SetFadeOutSpeed(int speed);
    void SetFadeOutDurationHQ(int speed);

    // Gets the PCM file path.
    std::wstring & GetPCMFilePath() noexcept
    {
        return _PCMFilePath;
    }

    // Gets the PCM file name.
    std::wstring & GetPCMFileName() noexcept
    {
        return _PCMFileName;
    }

    // Gets the PPS file path.
    std::wstring & GetPPSFilePath() noexcept
    {
        return _PPSFilePath;
    }

    // Gets the PPS file name.
    std::wstring & GetPPSFileName() noexcept
    {
        return _PPSFileName;
    }

    // Gets the PPZ file path.
    std::wstring & GetPPZFilePath(size_t bufferNumber) noexcept
    {
        return _PPZ8->_PPZBanks[bufferNumber]._FilePath;
    }

    // Gets the PPZ file name.
    std::wstring & GetPPZFileName(size_t bufferNumber) noexcept
    {
        return _PPZFileName[bufferNumber];
    }

    int32_t GetVersion() const noexcept
    {
        return _Version;
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

    bool GetMemo(const uint8_t * data, size_t size, int al, char * text, size_t textSize);

    channel_t * GetChannel(int channelNumber) const noexcept;

private:
    void Reset();
    void StartOPNInterrupt();
    void InitializeState();
    void InitializeOPNA();

    void DriverMain() noexcept;
    void DriverStart() noexcept;
    void DriverStop() noexcept;

    uint8_t * ExecuteCommand(channel_t * channel, uint8_t * si, uint8_t command);

    void Mute();
    void InitializeChannels();
    void InitializeTimers();
    void ConvertTimerBTempoToMetronomeTempo();
    void ConvertMetronomeTempoToTimerBTempo();
    void SetTimerBTempo();
    void HandleTimerAInterrupt();
    void HandleTimerBInterrupt();
    void IncreaseBarCounter();

    void GetText(const uint8_t * data, size_t size, int al, char * text, size_t max) const noexcept;

    bool SSGCheckDrums(channel_t * channel, int al);

    #pragma region FM Sound Source

    void FMMain(channel_t * channel) noexcept;

    uint8_t * FMExecuteCommand(channel_t * channel, uint8_t * si);

    void FMKeyOn(channel_t * channel);
    void FMKeyOff(channel_t * channel);

    uint8_t * DecreaseFMVolumeCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetFMInstrument(channel_t * channel, uint8_t * si);
    uint8_t * SetFMPan1(channel_t * channel, uint8_t * si);
    uint8_t * SetFMPan2(channel_t * channel, uint8_t * si);
    uint8_t * SetFMPortamentoCommand(channel_t * channel, uint8_t * si);
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

    uint8_t * SSGExecuteCommand(channel_t * channel, uint8_t * si);
    uint8_t * SSGDecreaseVolume(channel_t * channel, uint8_t * si);
    uint8_t * SSGSetEnvelope1(channel_t * channel, uint8_t * si);
    uint8_t * SSGSetEnvelope2(channel_t * channel, uint8_t * si);
    uint8_t * SSGSetPortamento(channel_t * channel, uint8_t * si);
    uint8_t * SSGSetChannelMask(channel_t * channel, uint8_t * si) noexcept;
    uint8_t * SetSSGEffect(channel_t * channel, uint8_t * si);
    uint8_t * SSGSetNoiseFrequency(uint8_t * si);

    void SSGSetVolume(channel_t * channel);
    void SetSSGPitch(channel_t * channel);
    void SSGKeyOn(channel_t * channel);
    void SSGKeyOff(channel_t * channel);
    void SSGSetTone(channel_t * channel, int al);

    void SSGSetDrumInstrument(channel_t * channel, int al);

    #pragma endregion

    #pragma region ADPCM Sound Source

    void ADPCMMain(channel_t * channel);

    uint8_t * ADPCMExecuteCommand(channel_t * channel, uint8_t * si);
    uint8_t * ADPCMDecreaseVolume(channel_t * channel, uint8_t * si);
    uint8_t * ADPCMSetInstrument(channel_t * channel, uint8_t * si);
    uint8_t * ADPCMSetPortamento(channel_t * channel, uint8_t * si);
    uint8_t * ADPCMSetPan1(channel_t * channel, uint8_t * si);
    uint8_t * ADPCMSetRepeat(channel_t * channel, uint8_t * si);
    uint8_t * ADPCMSetChannelMask(channel_t * channel, uint8_t * si) noexcept;
    uint8_t * ADPCMSetPan2(channel_t * channel, uint8_t * si);

    void ADPCMSetVolume(channel_t * channel);
    void ADPCMSetPitch(channel_t * channel);
    void ADPCMKeyOn(channel_t * channel);
    void ADPCMKeyOff(channel_t * channel);
    void ADPCMSetTone(channel_t * channel, int32_t al);

    #pragma endregion

    #pragma region Rhythm Sound Source

    void RhythmMain(channel_t * channel);

    uint8_t * RhythmExecuteCommand(channel_t * channel, uint8_t * si);
    uint8_t * RhythmDecreaseVolume(channel_t * channel, uint8_t * si);
    uint8_t * RhythmSetChannelMask(channel_t * channel, uint8_t * si) noexcept;
    uint8_t * RhythmKeyOn(channel_t * channel, int32_t note, uint8_t * rhythmData, bool * success) noexcept;

    uint8_t * RhythmControl(uint8_t * si);
    uint8_t * RhythmSetVolume(uint8_t * si);
    uint8_t * RhythmSetPan(uint8_t * si);
    uint8_t * RhythmSetMasterVolume(uint8_t * si);
    uint8_t * RhythmSetRelativeMasterVolume(uint8_t * si);
    uint8_t * RhythmSetRelativeVolume(uint8_t * si);

    #pragma endregion

    #pragma region P86

    void P86Main(channel_t * channel);

    uint8_t * P86ExecuteCommand(channel_t * channel, uint8_t * si);
    uint8_t * P86SetInstrument(channel_t * channel, uint8_t * si);
    uint8_t * P86SetRepeat(channel_t * channel, uint8_t * si);
    uint8_t * P86SetPan1(channel_t * channel, uint8_t * si);
    uint8_t * P86SetPan2(channel_t * channel, uint8_t * si);
    uint8_t * P86SetChannelMask(channel_t * channel, uint8_t * si) noexcept;

    void P86SetVolume(channel_t * channel);
    void P86SetPitch(channel_t * channel);
    void P86KeyOn(channel_t * channel);
    void P86KeyOff(channel_t * channel);
    void P86SetTone(channel_t * channel, int al);

    #pragma endregion

    #pragma region PPZ8

    void PPZ8Main(channel_t * channel);

    uint8_t * PPZ8Initialize(channel_t * channel, uint8_t * si);

    uint8_t * PPZ8ExecuteCommand(channel_t * channel, uint8_t * si);
    uint8_t * PPZ8DecreaseVolume(channel_t * channel, uint8_t * si);
    uint8_t * PPZ8SethannelMask(channel_t * channel, uint8_t * si) noexcept;
    uint8_t * PPZ8SetInstrument(channel_t * channel, uint8_t * si);
    uint8_t * PPZ8SetPortamento(channel_t * channel, uint8_t * si);
    uint8_t * PPZ8SetPan1(channel_t * channel, uint8_t * si);
    uint8_t * PPZ8SetPan2(channel_t * channel, uint8_t * si);
    uint8_t * PPZ8SetRepeat(channel_t * channel, uint8_t * si);

    void PPZ8SetVolume(channel_t * channel);
    void PPZ8SetPitch(channel_t * channel);
    void PPZ8KeyOn(channel_t * channel);
    void PPZ8KeyOff(channel_t * channel);
    void PPZ8SetTone(channel_t * channel, int tone);

    #pragma endregion

    #pragma region SSG Effect

    void SSGEffectMain(channel_t * channel, int al);

    void SSGPlayEffect() noexcept;
    void SSGStartEffect(const int * si);
    void SSGStopEffect();
    void SSGSweep();

    #pragma endregion

    #pragma region Software LFO

    void LFOMain(channel_t * channel);

    void LFOInitialize(channel_t * channel);
    void LFOReset(channel_t * channel);

    int SetLFO(channel_t * channel);
    int SetSSGLFO(channel_t * channel);
    void LFOSwap(channel_t * channel) noexcept;
    int StartLFO(channel_t * channel, int al);
    int StartPCMLFO(channel_t * channel, int al);
    void StopLFO(channel_t * channel);

    uint8_t * LFO1SetModulation(channel_t * channel, uint8_t * si) noexcept;

    uint8_t * LFO1SetSwitch(channel_t * channel, uint8_t * si);
    uint8_t * LFO2SetSwitch(channel_t * channel, uint8_t * si);

    uint8_t * LFO1SetSlotMask(channel_t * channel, uint8_t * si);
    uint8_t * LFO2SetSlotMask(channel_t * channel, uint8_t * si);

    uint8_t * LFOSetRiseFallType(channel_t * channel, uint8_t * si) const noexcept;

    #pragma endregion

    uint8_t * SpecialC0ProcessingCommand(channel_t * channel, uint8_t * si, uint8_t value) noexcept;
    uint8_t * SetTempoCommand(uint8_t * si);

    uint8_t * SetStartOfLoopCommand(channel_t * channel, uint8_t * si);
    uint8_t * SetEndOfLoopCommand(channel_t * channel, uint8_t * si);
    uint8_t * ExitLoopCommand(channel_t * channel, uint8_t * si);

    uint8_t * IncreaseVolumeForNextNote(channel_t * channel, uint8_t * si, int maxVolume);
    uint8_t * DecreaseVolumeForNextNote(channel_t * channel, uint8_t * si);

    uint8_t * SetHardwareLFO_PMS_AMS(channel_t * channel, uint8_t * si);

    uint8_t * CalculateQ(channel_t * channel, uint8_t * si);

    uint8_t * RhythmSetLFOControl(channel_t * channel, uint8_t * si) const noexcept;

    int32_t Transpose(channel_t * channel, int al);

    uint8_t CalcPanOut(channel_t * channel);
    void CalcFMBlock(int * cx, int * ax);
    void InitializeFMChannel3(channel_t * channel, uint8_t * ax) const noexcept;
    void SpecialFM3Processing(channel_t * channel, int ax, int cx);

    int32_t rnd(int32_t ax) noexcept;

    void CalcFMLFO(channel_t * channel, int al, int bl, uint8_t * vol_tbl);
    void CalcVolSlot(int dh, int dl, int al);
    void CalculatePortamento(channel_t * channel);

    int32_t SSGPCMSoftwareEnvelope(channel_t * channel) noexcept;
    int SSGPCMSoftwareEnvelopeMain(channel_t * channel);
    int SSGPCMSoftwareEnvelopeSub(channel_t * channel);
    int ExtendedSSGPCMSoftwareEnvelopeMain(channel_t * channel);

    void ExtendedSSGPCMSoftwareEnvelopeSub(channel_t * channel, int ah);
    void LFOSetStepUsingMDValue(channel_t * channel);

    void WritePCMData(uint16_t pcmstart, uint16_t pcmstop, const uint8_t * pcmData);
    void ReadPCMData(uint16_t pcmstart, uint16_t pcmstop, uint8_t * pcmData);

    void Fade();

private:
    int LoadPPC(const WCHAR * filename);
    int LoadPPCInternal(uint8_t * data, size_t size) noexcept;

    void FindFile(const WCHAR * filename, WCHAR * filePath, size_t size) const noexcept;

private:
    std::vector<std::wstring> _SearchPath;

    File * _File;

    opnaw_t * _OPNAW;
    ppz8_t * _PPZ8; 

    pps_t * _PPS;
    p86_t * _P86;

    bool _UsePPSForDrums;           // Use the PPS to play drum instruments for the K/R commands.
    bool _UseSSGForDrums;           // Use the SSG to play drum instruments for the K/R commands.

    uint32_t _PCMSampleRate;        // PCM output frequency (11k, 22k, 44k, 55k)
    uint32_t _PPZSampleRate;        // PPZ output frequency

    bool _UseInterpolation;

    bool _UseInterpolationP86;
    bool _UseInterpolationPPS;
    bool _UseInterpolationPPZ;

    bool _StopAfterFadeout;

    static const uint8_t _DummyRhythmData = 0xFF;

    uint8_t _Version;               // File version. 0x00 for standard .M or .M2 files targeting PC-98/PC-88/X68000 systems, Must be 0xFF for files targeting FM Towns hardware.

    state_t _State;
    driver_t _Driver;
    effect_t _SSGEffect;

    int32_t _TimerBTempo;           // Current value of TimerB (= ff_tempo during ff)

    channel_t _FMChannels[MaxFMChannels];
    channel_t _SSGChannels[MaxSSGChannels];
    channel_t _ADPCMChannel;
    channel_t _RhythmChannel;

    channel_t _FMExtensionChannels[MaxFMExtensionChannels];
    channel_t _SSGEffectChannel;

    channel_t _PPZ8Channels[MaxPPZ8Channels];

    channel_t _DummyChannel;

    int32_t _FMSlotKey1[3];
    int32_t _FMSlotKey2[3];

    static const size_t MaxFrames = 30000;

    frame16_t _SrcFrames[MaxFrames];
    frame32_t _DstFrames[MaxFrames];
    frame32_t _TmpFrames[MaxFrames];

    uint8_t _MData[64 * 1024];      // Contents of the .M or .M2 file
    uint8_t _VData[ 8 * 1024];
    uint8_t _EData[64 * 1024];

    sample_bank_t _SampleBank;

    std::wstring _PCMFileName;      // P86 or PPC
    std::wstring _PCMFilePath;

    std::wstring _PPSFileName;      // PPS
    std::wstring _PPSFilePath;

    std::wstring _PPZFileName[2];   // PVI or PPZ
    std::wstring _PPZFilePath[2];

    int32_t _SSGNoiseFrequency;

    uint32_t _RhythmMask;           // Rhythm sound source mask. Compatible with x8c/10h bit
    uint32_t _RhythmChannelMask;    // Bit mask: bit is set to 1 if the corresponding drum channel is playing.
    int32_t _RhythmVolume;          // Rhythm volume

    #pragma region Dynamic Settings

    uint8_t _Status1;               // Unused

    bool _InTimerAInterrupt;
    bool _InTimerBInterrupt;

    bool _IsPlaying;
    bool _IsUsingP86;

    frame16_t * _FramePtr;
    size_t _FramesToDo;

    int64_t _Position;              // Time from start of playing (in μs)
    int64_t _FadeOutPosition;       // SetFadeOutDurationHQ start time
    int32_t _Seed;                  // Random seed

    int32_t _OldSSGNoiseFrequency;
    int32_t _OldInstrumentNumber;

    #pragma endregion
};

#pragma warning(default: 4820) // x bytes padding added after last data member
