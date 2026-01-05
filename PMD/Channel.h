
/** $VER: Channel.h (2026.01.04) Represents a sound source channel (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#pragma once

#pragma warning(disable: 4820) // x bytes padding added after last data member

class channel_t
{
public:
    uint8_t * _Data;
    uint8_t * _LoopData;
    int32_t _Size;

    uint8_t _LoopCheck; // Used to check When a loop ends.

    uint32_t Factor;    // Current playing BLOCK/FNUM

    int16_t _DetuneValue;

    int32_t _HardwareLFO;  // bit 0: Tone, bit 1: Volume, bit 2: Same period, bit 3: Portamento
    int32_t _ExtendMode; // bit 1: Detune, bit 2: LFO, bit 3: Env Normal/Extend

    // LFO 1
    int32_t _LFO1Data;
    int32_t _LFO1Waveform;

    int32_t _LFO1Delay1, _LFO1Delay2;
    int32_t _LFO1Speed1, _LFO1Speed2;
    int32_t _LFO1Step1, _LFO1Step2;
    int32_t _LFO1Time1, _LFO1Time2;

    int32_t _LFO1MDepth;
    int32_t _LFO1MDepthSpeed1, _LFO1MDepthSpeed2;
    int32_t _LFO1MDepthCount1, _LFO1MDepthCount2;

    // LFO 2
    int32_t LFO2Data;
    int32_t LFO2Waveform;

    int32_t LFO2Delay1, LFO2Delay2;
    int32_t LFO2Speed1, LFO2Speed2;
    int32_t LFO2Step1, LFO2Step2;
    int32_t LFO2Time1, LFO2Time2;

    int32_t LFO2MDepth;
    int32_t LFO2MDepthSpeed1, LFO2MDepthSpeed2;
    int32_t LFO2MDepthCount1, LFO2MDepthCount2;

    // Portamento
    int32_t _Portamento;
    int32_t PortamentoQuotient;
    int32_t PortamentoRemainder;

    int32_t _Volume;
    int32_t Transposition1;
    int32_t Transposition2;

    int32_t VolumeBoost;    // bit 4: tone / bit 5: vol / bit 6: same period

    int32_t SSGEnvelopFlag; // -1 to extend
    int32_t ExtendedCount;  // None=0 AR=1 DR=2 SR=3 RR=4

    int32_t AttackDuration;
    int32_t DecayDepth;
    int32_t SustainRate;
    int32_t ReleaseRate;
    int32_t SustainLevel;
    int32_t AttackLevel;    // Specifies the level at which the attack starts.

    int32_t ExtendedAttackDuration;
    int32_t ExtendedDecayDepth;
    int32_t ExtendedSustainRate;
    int32_t ExtendedReleaseRate;
    int32_t ExtendedAttackLevel;

    int32_t _PanAndVolume;

    int32_t FMCarrier;
    int32_t FMOperator1;
    int32_t FMOperator3;
    int32_t FMOperator2;
    int32_t FMOperator4;
    int32_t _FMSlotMask;

    int32_t SSGMask;         // Tone / Noise / Mix
    int32_t InstrumentNumber;

    int32_t _ToneMask;       // Maskdata for FM tone definition

    int32_t PartMask;       // bit 0: Normal, bit 1: Sound effect, bit 2: For NECPCM
    int32_t _LFO1Mask;    // Volume LFO mask
    int32_t _LFO2Mask;    // Volume LFO mask

    // bit 3: none / bit 4: For PPZ/ADE / bit 5: s0 time / bit 6: m / bit 7: temporary
    int32_t KeyOnFlag;      // Set after processing new scale/rest data, inc
    int32_t KeyOffFlag;     // Flag indicating whether KeyOff has been performed

    int32_t _HardwareLFODelay;
    int32_t HardwareLFODelayCounter;

    int32_t Tone;           // High nibble = octave, low nibble = note, 0xFF = rest
    int32_t DefaultTone;

    int32_t SlotDelay;
    int32_t SlotDelayCounter;
    int32_t SlotDelayMask;

    int32_t AlgorithmAndFeedbackLoops;

    int32_t GateTime;       // Calculated from q/Q value

    int32_t EarlyKeyOffTimeout;
    int32_t EarlyKeyOffTimeoutPercentage;
    int32_t EarlyKeyOffTimeoutRandomRange;
    int32_t EarlyKeyOffTimeout2;
};

#pragma warning(default: 4820) // x bytes padding added after last data member
