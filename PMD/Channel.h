#pragma once

struct Channel
{
    uint8_t * Data;
    uint8_t * LoopData;
    int Length;

    int qdat; // 1 gatetime (value calculated from q/Q value)

    uint32_t fnum;  // 2 Power BLOCK/FNUM
    int DetuneValue;

    int lfoswi;  // 1. LFOSW: bit 0: tone, bit 1: vol,  bit 2: same period, bit 3: portamento
    int extendmode; // 1. bit 1: Detune, bit 2: LFO, bit 3: Env Normal/Extend

    // LFO 1
    int lfodat;  // 2 LFO DATA

    int delay;  // 1 LFO [DELAY]
    int delay2;  // 1 [DELAY_2]

    int speed;  // 1 [SPEED]
    int speed2;  // 1 [SPEED_2]

    int step;  // 1 [STEP]
    int step2;  // 1 [STEP_2]

    int time;  // 1 [TIME]
    int time2;  // 1 [TIME_2]

    int MDepthSpeedA;
    int MDepthSpeedB;
    int MDepth;

    int lfo_wave; // 1 LFO waveform

    int mdc;  // 1 M depth Counter (Fluctuation value)
    int mdc2;  // 1 M depth Counter

    // LFO 2
    int _lfodat; // 2 LFO DATA

    int _delay;  // 1 LFO [DELAY]
    int _delay2; // 1  [DELAY_2]

    int _speed;  // 1  [SPEED]
    int _speed2; // 1  [SPEED_2]

    int _step;  // 1  [STEP]
    int _step2;  // 1  [STEP_2]

    int _time;  // 1  [TIME]
    int _time2;  // 1  [TIME_2]

    int _mdepth; // 1 M depth

    int _mdspd;  // 1 M speed
    int _mdspd2; // 1 M speed_2

    int _lfo_wave; // 1 LFO waveform

    int _mdc;  // 1 M depth Counter (Fluctuation value)
    int _mdc2;  // 1 M depth Counter

    int porta_num; // 2 ポルタメントの加減値（全体）
    int porta_num2; // 2 ポルタメントの加減値（一回）
    int porta_num3; // 2 ポルタメントの加減値（余り）

    int Volume;  // 1 VOLUME
    int shift;  // 1 ｵﾝｶｲ ｼﾌﾄ ﾉ ｱﾀｲ

    // bit 4: tone / bit 5: vol / bit 6: same period
    int volpush; // 1 Volume PUSHarea
    int envf;  // 1 SSG ENV. [START_FLAG] / -1でextend
    int eenv_count; // 1 ExtendSSGenv/No=0 AR=1 DR=2 SR=3 RR=4
    int eenv_ar; // 1  /AR  /旧pat
    int eenv_dr; // 1 /DR  /旧pv2
    int eenv_sr; // 1 /SR  /旧pr1
    int eenv_rr; // 1 /RR  /旧pr2
    int eenv_sl; // 1 /SL
    int eenv_al; // 1 /AL
    int eenv_arc; // 1 /ARのカウンタ /旧patb
    int eenv_drc; // 1 /DRのカウンタ
    int eenv_src; // 1 /SRのカウンタ /旧pr1b
    int eenv_rrc; // 1 /RRのカウンタ /旧pr2b
    int eenv_volume;  // 1 /Volume値(0?15)/旧penv

    int PanAndVolume;
    int psgpat;  // 1 SSG PATTERN [TONE/NOISE/MIX]
    int InstrumentNumber;
    int loopcheck; // 1 When the loop ends 1 When the loop ends 3
    int carrier; // 1 FM Carrier
    int slot1;  // 1 SLOT 1 ﾉ TL
    int slot3;  // 1 SLOT 3 ﾉ TL
    int slot2;  // 1 SLOT 2 ﾉ TL
    int slot4;  // 1 SLOT 4 ﾉ TL

    int FMSlotMask; // 1 FM slotmask
    int ToneMask; // 1 maskdata for FM tone definition
    int MuteMask; // bit 0: Normal, bit 1: Sound effect, bit 2: For NECPCM
    int VolumeMask1; // Volume LFO mask
    int VolumeMask2; // Volume LFO mask

    // bit 3: none / bit 4: For PPZ/ADE / bit 5: s0 time / bit 6: m / bit 7: temporary
    int KeyOffFlag;  // 1 Flag indicating whether keyoff has been performed
    int qdata;  // 1 value of q
    int qdatb;  // 1 value of q

    int HardwareLFODelay;
    int HardwareLFODelayCounter;

    int Tone;  // Scale data being played (0xFF = rest)
    int sdelay;  // 1 Slot delay
    int sdelay_c; // 1 Slot delay counter
    int sdelay_m; // 1 Slot delay Mask
    int alg_fb;  // 1 Tone alg/fb
    int KeyOnFlag; // 1 After processing new scale/rest data, inc
    int qdat2;  // 1 q minimum guaranteed value
    int DefaultTone; // 1 Scale data being played (before modulation processing / ?fh: rest)
    int shift_def; // 1 Master modulation value
    int qdat3;  // 1 q Random
};
