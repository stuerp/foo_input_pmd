
/** $VER: Channel.h (2026.01.06) Represents a sound source channel (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#pragma once

#pragma warning(disable: 4820) // x bytes padding added after last data member

class channel_t
{
public:
    uint8_t * _Data;
    uint8_t * _LoopData;
    int32_t _Size;

    uint8_t _LoopCheck; // Used to check When a loop ends.

    uint32_t _Factor;    // Current playing BLOCK/FNUM

    int16_t _DetuneValue;

    int32_t _HardwareLFO;  // bit 0: Tone, bit 1: Volume, bit 2: Same period, bit 3: Portamento
    int32_t _HardwareLFODelay;
    int32_t _HardwareLFODelayCounter;

    int32_t _ExtendMode; // bit 1: Detune, bit 2: LFO, bit 3: Env Normal/Extend

    // LFO 1
    int32_t _LFO1Data;
    int32_t _LFO1Waveform;

    int32_t _LFO1DelayCounter, _LFO1Delay;
    int32_t _LFO1SpeedCounter, _LFO1Speed;
    int32_t _LFO1StepCounter, _LFO1Step;
    int32_t _LFO1TimeCounter, _LFO1Time;

    /* Temporal Change of Depth */
    int32_t _LFO1Depth;
    int32_t _LFO1DepthSpeedCounter1, _LFO1DepthSpeed1;
    int32_t _LFO1DepthSpeedCounter2, _LFO1DepthSpeed2;

    // LFO 2
    int32_t _LFO2Data;
    int32_t _LFO2Waveform;

    int32_t _LFO2DelayCounter, _LFO2Delay;
    int32_t _LFO2SpeedCounter, _LFO2Speed;
    int32_t _LFO2StepCounter, _LFO2Step;
    int32_t _LFO2TimeCounter, _LFO2Time;

    /* Temporal Change of Depth */
    int32_t _LFO2Depth;
    int32_t _LFO2DepthSpeedCounter1, _LFO2DepthSpeed1;
    int32_t _LFO2DepthSpeedCounter2, _LFO2DepthSpeed2;

    // Portamento
    int32_t _Portamento;
    int32_t _PortamentoQuotient;
    int32_t _PortamentoRemainder;

    int32_t _Volume;
    int32_t Transposition1;
    int32_t Transposition2;

    int32_t VolumeBoost;    // bit 4: tone / bit 5: vol / bit 6: same period

    int32_t _SSGEnvelopFlag; // -1 to extend
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

    int32_t _SSGMask;        // Tone / Noise / Mix
    int32_t InstrumentNumber;

    int32_t _ToneMask;      // Maskdata for FM tone definition

    int32_t _PartMask;      // bit 0: Normal, bit 1: Sound effect, bit 2: For NECPCM
    int32_t _LFO1Mask;      // Volume LFO mask
    int32_t _LFO2Mask;      // Volume LFO mask

    // bit 3: none / bit 4: For PPZ/ADE / bit 5: s0 time / bit 6: m / bit 7: temporary
    int32_t _KeyOnFlag;      // Set after processing new scale/rest data, inc
    int32_t _KeyOffFlag;     // Flag indicating whether KeyOff has been performed

    int32_t _Tone;           // High nibble = octave, low nibble = note, 0xFF = rest
    int32_t _DefaultTone;

    int32_t _SlotDelay;
    int32_t _FMSlotDelayCounter;
    int32_t _FMSlotDelayMask;

    int32_t AlgorithmAndFeedbackLoops;

    int32_t _GateTime;       // Calculated from q/Q value

    int32_t _EarlyKeyOffTimeout1;
    int32_t EarlyKeyOffTimeoutPercentage;
    int32_t EarlyKeyOffTimeoutRandomRange;
    int32_t _EarlyKeyOffTimeout2;
};

#pragma warning(default: 4820) // x bytes padding added after last data member
