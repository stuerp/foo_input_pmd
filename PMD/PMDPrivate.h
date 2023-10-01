
// Based on PMDWin code by C60

#pragma once

#define ERR_SUCCESS                 0

#define ERR_OPEN_FAILED             1
#define ERR_UNKNOWN_FORMAT          2
#define ERR_ALREADY_LOADED          3
#define ERR_OUT_OF_MEMORY           4

#define ERR_WRONG_PARTNO           32
#define ERR_NOT_MASKED             33 // The specified part is not masked.
#define ERR_EFFECT_USED            34 // The mask cannot be operated because it is being used by sound effects.
#define ERR_MUSIC_STOPPED          99 // You performed a mask operation while the song was stopped.

#define ERR_UNKNOWN               999

#define SOUND_55K           55555
#define SOUND_55K_2         55466
#define SOUND_48K           48000
#define SOUND_44K           44100
#define SOUND_22K           22050
#define SOUND_11K           11025

#define PPZ8_i0             44100
#define PPZ8_i1             33080
#define PPZ8_i2             22050
#define PPZ8_i3             16540
#define PPZ8_i4             11025
#define PPZ8_i5              8270
#define PPZ8_i6              5513
#define PPZ8_i7              4135

#define DEFAULT_REG_WAIT    15000
#define MAX_PCMDIR             64
#define MAX_MEMO             1024

#define OPNAClock   (3993600 * 2)

#define MaxFMTracks             6
#define MaxSSGTracks            3
#define MaxADPCMTracks          1
#define MaxOPNARhythmTracks     1
#define MaxExtTracks            3
#define MaxRhythmTracks         1
#define MaxEffectTracks         1
#define MaxPPZ8Tracks           8
#define MaxChannels               (MaxFMTracks + MaxSSGTracks + MaxADPCMTracks + MaxOPNARhythmTracks + MaxExtTracks + MaxRhythmTracks + MaxEffectTracks + MaxPPZ8Tracks)

struct Channel
{
    uint8_t * Data;
    uint8_t * LoopData;
    int Length;

    int qdat; // 1 gatetime (value calculated from q/Q value)

    uint32_t fnum;  // 2 Power BLOCK/FNUM
    int detune;  // 2 Detune

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

    int mdepth;  // 1 M depth

    int mdspd;  // 1 M speed
    int mdspd2;  // 1 M speed_2

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

    int volume;  // 1 VOLUME
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

    int fmpan;  // 1 FM Panning + AMD + PMD
    int psgpat;  // 1 SSG PATTERN [TONE/NOISE/MIX]
    int InstrumentNumber;
    int loopcheck; // 1 When the loop ends 1 When the loop ends 3
    int carrier; // 1 FM Carrier
    int slot1;  // 1 SLOT 1 ﾉ TL
    int slot3;  // 1 SLOT 3 ﾉ TL
    int slot2;  // 1 SLOT 2 ﾉ TL
    int slot4;  // 1 SLOT 4 ﾉ TL

    int SlotMask; // 1 FM slotmask
    int ToneMask; // 1 maskdata for FM tone definition
    int PartMask; // bit 0: Normal, bit 1: Sound effect, bit 2: For NECPCM
    int VolumeMask1; // Volume LFO mask
    int VolumeMask2; // Volume LFO mask

    // bit 3: none / bit 4: For PPZ/ADE / bit 5: s0 time / bit 6: m / bit 7: temporary
    int keyoff_flag;  // 1 Flag indicating whether keyoff has been performed
    int qdata;  // 1 value of q
    int qdatb;  // 1 value of q
    int hldelay; // 1 HardLFO delay
    int hldelay_c; // 1 HardLFO delay Counter

    int Tone;  // Scale data being played (0xFF = rest)
    int sdelay;  // 1 Slot delay
    int sdelay_c; // 1 Slot delay counter
    int sdelay_m; // 1 Slot delay Mask
    int alg_fb;  // 1 Tone alg/fb
    int keyon_flag; // 1 After processing new scale/rest data, inc
    int qdat2;  // 1 q minimum guaranteed value
    int onkai_def; // 1 Scale data being played (before modulation processing / ?fh: rest)
    int shift_def; // 1 Master modulation value
    int qdat3;  // 1 q Random
};

#pragma warning(disable: 4820) // x bytes padding added after last data member
struct State
{
    uint8_t * MData;            // Address of MML data + 1

    uint8_t * VData;            // Voice data
    uint8_t * EData;            // FM Effect data

    uint8_t * RhythmData;
    uint8_t * ToneData;         // Tone data, if any

    uint8_t DummyRhythmData;

    uint16_t * RhythmDataTable; // Rhythm Data table

    bool UseRhythm;             // Use Rhythm sound source with K/R part.
    bool UseFM55kHzSynthesis;
    bool UseInterpolationPPZ8;
    bool UseInterpolationPPS;
    bool UseInterpolationP86;

    Channel * Channel[MaxChannels];

    int RhythmMask;             // Rhythm sound source mask. Compatible with x8c/10h bit
    int RhythmVolume;           // Rhythm volume

    int fm_voldown;
    int _fm_voldown;

    int ssg_voldown;
    int _ssg_voldown;

    int pcm_voldown;
    int _pcm_voldown;

    int rhythm_voldown;
    int _rhythm_voldown;

    int pcm86_vol; // Should the volume of PCM86 be adjusted to SPB?
    int _pcm86_vol; // Should the volume of PCM86 be adjusted to SPB? (For storage)

    int ppz_voldown; // PPZ8 voldown numerical value
    int _ppz_voldown; // PPZ8 voldown numerical value (for storage)

    // MData characteristics
    uint8_t x68_flg;    // OPM flag

    int status;

    int LoopCount;

    int FadeOutSpeed;
    int FadeOutVolume;

    int BarLength;  // Bar length
    int OpsCounter; // Shortest note counter

    int SSGEffectFlag; // SSG sound effect on/off flag (substituted by user)
    int SSGNoiseFrequency;
    int OldSSGNoiseFrequency;

    int PCMStart;
    int PCMStop;

    int tempo_d; // Tempo (TIMER-B)
    int tempo_d_push;  // Tempo (TIMER-B) / for saving

    int tempo_48; // Current tempo (value of clock = 48 t)
    int tempo_48_push;  // Current tempo (same as above / for saving)

    int kshot_dat; // SSG rhythm shot flag
    int fade_stop_flag;  // Flag for whether to MSTOP after Fadeout
    int pcm_gs_flag;  // ADPCM use permission flag (0 allows)

    int slot_detune1;  // FM3 Slot Detune値 slot1
    int slot_detune2;  // FM3 Slot Detune値 slot2
    int slot_detune3;  // FM3 Slot Detune値 slot3
    int slot_detune4;  // FM3 Slot Detune値 slot4

    int fadeout_flag;  // When calling Fade from inside 1
    int revpan;  // PCM86 reverse phase flag
    int BarCounter;
    int port22h; // Last value output to OPN-PORT 22H (hlfo)

    int rshot_dat; // RSS shot flag
    int rdat[6]; // RSS volume/pan data
    int rshot_bd; // RSS shot inc flag (BD)
    int rshot_sd; // RSS shot inc flag (SD)
    int rshot_sym; // RSS shot inc flag (CYM)
    int rshot_hh; // RSS shot inc flag (HH)
    int rshot_tom; // RSS shot inc flag (TOM)
    int rshot_rim; // RSS shot inc flag (RIM)
    int rdump_bd; // RSS dump inc flag (BD)
    int rdump_sd; // RSS dump inc flag (SD)
    int rdump_sym; // RSS dump inc flag (CYM)
    int rdump_hh; // RSS dump inc flag (HH)
    int rdump_tom; // RSS dump inc flag (TOM)
    int rdump_rim; // RSS dump inc flag (RIM)

    uint32_t ch3mode; // ch3 Mode

    bool IsTimerABusy;
    int TimerATime;

    bool IsTimerBBusy;
    int TimerBTempo;  // Current value of TimerB (= ff_tempo during ff)

    uint32_t OPNARate; // PCM output frequency (11k, 22k, 44k, 55k)
    uint32_t PPZ8Rate; // PPZ output frequency

    bool IsPlaying; // True if the driver is playing
    bool IsUsingP86;

    int FadeOutSpeedHQ; // Fadeout (High Sound Quality) speed (fadeout at > 0)

    WCHAR PPCFileName[MAX_PATH];
    std::vector<std::wstring> SearchPath;
};
#pragma warning(default: 4820) // x bytes padding added after last data member
