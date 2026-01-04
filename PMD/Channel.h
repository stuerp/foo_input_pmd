
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
    int _DetuneValue;

    int HardwareLFOModulationMode;  // bit 0: Tone, bit 1: Volume, bit 2: Same period, bit 3: Portamento
    int ExtendMode; // bit 1: Detune, bit 2: LFO, bit 3: Env Normal/Extend

    // LFO 1
    int LFO1Data;
    int LFO1Waveform;

    int LFO1Delay1, LFO1Delay2;
    int LFO1Speed1, LFO1Speed2;
    int LFO1Step1, LFO1Step2;
    int LFO1Time1, LFO1Time2;

    int LFO1MDepth;
    int LFO1MDepthSpeed1, LFO1MDepthSpeed2;
    int LFO1MDepthCount1, LFO1MDepthCount2;

    // LFO 2
    int LFO2Data;
    int LFO2Waveform;

    int LFO2Delay1, LFO2Delay2;
    int LFO2Speed1, LFO2Speed2;
    int LFO2Step1, LFO2Step2;
    int LFO2Time1, LFO2Time2;

    int LFO2MDepth;
    int LFO2MDepthSpeed1, LFO2MDepthSpeed2;
    int LFO2MDepthCount1, LFO2MDepthCount2;

    // Portamento
    int _Portamento;
    int PortamentoQuotient;
    int PortamentoRemainder;

    int _Volume;
    int Transposition1;
    int Transposition2;

    int VolumeBoost;    // bit 4: tone / bit 5: vol / bit 6: same period

    int SSGEnvelopFlag; // -1 to extend
    int ExtendedCount;  // None=0 AR=1 DR=2 SR=3 RR=4

    int AttackDuration;
    int DecayDepth;
    int SustainRate;
    int ReleaseRate;
    int SustainLevel;
    int AttackLevel;    // Specifies the level at which the attack starts.

    int ExtendedAttackDuration;
    int ExtendedDecayDepth;
    int ExtendedSustainRate;
    int ExtendedReleaseRate;
    int ExtendedAttackLevel;

    int _PanAndVolume;

    int FMCarrier;
    int FMOperator1;
    int FMOperator3;
    int FMOperator2;
    int FMOperator4;
    int _FMSlotMask;

    int SSGMask;         // Tone / Noise / Mix
    int InstrumentNumber;

    int _ToneMask;       // Maskdata for FM tone definition

    int PartMask;       // bit 0: Normal, bit 1: Sound effect, bit 2: For NECPCM
    int VolumeMask1;    // Volume LFO mask
    int VolumeMask2;    // Volume LFO mask

    // bit 3: none / bit 4: For PPZ/ADE / bit 5: s0 time / bit 6: m / bit 7: temporary
    int KeyOnFlag;      // Set after processing new scale/rest data, inc
    int KeyOffFlag;     // Flag indicating whether KeyOff has been performed

    int HardwareLFODelay;
    int HardwareLFODelayCounter;

    int Tone;           // High nibble = octave, low nibble = note, 0xFF = rest
    int DefaultTone;

    int SlotDelay;
    int SlotDelayCounter;
    int SlotDelayMask;

    int AlgorithmAndFeedbackLoops;

    int GateTime;       // Calculated from q/Q value

    int EarlyKeyOffTimeout;
    int EarlyKeyOffTimeoutPercentage;
    int EarlyKeyOffTimeoutRandomRange;
    int EarlyKeyOffTimeout2;
};

#pragma warning(default: 4820) // x bytes padding added after last data member
