
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
#define MaxTracks               (MaxFMTracks + MaxSSGTracks + MaxADPCMTracks + MaxOPNARhythmTracks + MaxExtTracks + MaxRhythmTracks + MaxEffectTracks + MaxPPZ8Tracks)

#pragma warning(disable: 4820) // x bytes padding added after last data member
struct DriverState
{
    int _CurrentChannel;
    int tieflag; // &のフラグ(1 : tie)
    int volpush_flag;  // 次の１音音量down用のflag(1 : voldown)
    int rhydmy;  // R part ダミー演奏データ
    int fmsel;  // FM 表(=0)か裏(=0x100)か flag
    int omote_key[3];  // FM keyondata表(=0)
    int ura_key[3]; // FM keyondata裏(=0x100)
    int loop_work; // Loop Work
    bool _UsePPS;  // ppsdrv を使用するか？flag(ユーザーが代入)

    int PCMRepeat1;
    int PCMRepeat2;
    int PCMRelease;

    int _OldTimerATime;  // 一個前の割り込み時の_TimerATime値
    int music_flag; // B0:次でMSTART 1:次でMSTOP のFlag
    int slotdetune_flag; // FM3 Slot Detuneを使っているか
    int slot3_flag; // FM3 Slot毎 要効果音モードフラグ
    int fm3_alg_fb; // FM3chの最後に定義した音色のalg/fb
    int af_check; // FM3chのalg/fbを設定するかしないかflag
    int lfo_switch; // 局所LFOスイッチ
};
#pragma warning(default: 4820)

struct EffectState
{
    int * effadr; // effect address
    int eswthz;  // トーンスゥイープ周波数
    int eswtst;  // トーンスゥイープ増分
    int effcnt;  // effect count
    int eswnhz;  // ノイズスゥイープ周波数
    int eswnst;  // ノイズスゥイープ増分
    int eswnct;  // ノイズスゥイープカウント
    int effon;  // 効果音　発音中
    int psgefcnum; // 効果音番号
    int hosei_flag; // ppsdrv 音量/音程補正をするかどうか
    int last_shot_data;  // 最後に発音させたPPSDRV音色
};

struct Track
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
    int speed;  // 1 [SPEED]
    int step;  // 1 [STEP]
    int time;  // 1 [TIME]
    int delay2;  // 1 [DELAY_2]
    int speed2;  // 1 [SPEED_2]
    int step2;  // 1 [STEP_2]
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
    int _speed;  // 1  [SPEED]
    int _step;  // 1  [STEP]
    int _time;  // 1  [TIME]
    int _delay2; // 1  [DELAY_2]
    int _speed2; // 1  [SPEED_2]
    int _step2;  // 1  [STEP_2]
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
    int envf;  // 1 PSG ENV. [START_FLAG] / -1でextend
    int eenv_count; // 1 ExtendPSGenv/No=0 AR=1 DR=2 SR=3 RR=4
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
    int psgpat;  // 1 PSG PATTERN [TONE/NOISE/MIX]
    int _SampleNumber; // 1 Tone number
    int loopcheck; // 1 When the loop ends 1 When the loop ends 3
    int carrier; // 1 FM Carrier
    int slot1;  // 1 SLOT 1 ﾉ TL
    int slot3;  // 1 SLOT 3 ﾉ TL
    int slot2;  // 1 SLOT 2 ﾉ TL
    int slot4;  // 1 SLOT 4 ﾉ TL

    int _SlotMask; // 1 FM slotmask
    int _ToneMask; // 1 maskdata for FM tone definition
    int _PartMask; // bit 0: Normal, bit 1: Sound effect, bit 2: For NECPCM
    int _VolumeMask1; // Volume LFO mask
    int _VolumeMask2; // Volume LFO mask

    // bit 3: none / bit 4: For PPZ/ADE / bit 5: s0 time / bit 6: m / bit 7: temporary
    int keyoff_flag;  // 1 Flag indicating whether keyoff has been performed
    int qdata;  // 1 value of q
    int qdatb;  // 1 value of q
    int hldelay; // 1 HardLFO delay
    int hldelay_c; // 1 HardLFO delay Counter

    int onkai;  // Scale data being played (0xFF = rest)
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
    Track * _Track[MaxTracks];

    uint8_t * _MData;  // Address of MML data + 1
    uint8_t * _VData;  // Voice data
    uint8_t * _EData;  // FM Effect data
    uint8_t * prgdat_adr; // Start address of tone data in song data

    uint16_t * _RythmAddressTable;  // R part offset table. Start address
    uint8_t * rhyadr;  // R part Current address

    bool _UseSSG;  // Play Rhythm Sound Source with K/Rpart flag

    bool _UseFM55kHzSynthesis;
    bool _UseInterpolationPPZ8;
    bool _UseInterpolationPPS;
    bool _UseInterpolationP86;

    int rhythmmask; // Rhythm Sound Source mask. Compatible with x8c/10h bit

    int fm_voldown; // FM voldown 数値
    int ssg_voldown;  // PSG voldown 数値
    int pcm_voldown;  // ADPCM voldown 数値
    int rhythm_voldown;  // RHYTHM voldown 数値

    int prg_flg; // Whether the song data contains a tone flag
    int x68_flg; // OPM flag
    int status;  // status1

    int _LoopCount;

    int tempo_d; // Tempo (TIMER-B)
    int tempo_d_push;  // Tempo (TIMER-B) / for saving

    int _FadeOutSpeed;  // Fadeout速度
    int _FadeOutVolume;  // Fadeout音量

    int _BarLength;  // Bar length
    int _OpsCounter; // Shortest note counter
    int effflag; // PSG sound effect on/off flag (substituted by user)
    int _PSGNoiseFrequency;  // PSG noise frequency
    int _PSGNoiseFrequencyLast; // PSG noise frequency (last defined value)

    int PCMStart;
    int PCMStop;

    int rshot_dat; // リズム音源 shot flag
    int rdat[6]; // リズム音源 音量/パンデータ
    int rhyvol;  // リズムトータルレベル
    int kshot_dat; // ＳＳＧリズム shot flag
    int fade_stop_flag;  // Fadeout後 MSTOPするかどうかのフラグ
    int pcm_gs_flag;  // ADPCM使用 許可フラグ (0で許可)

    int slot_detune1;  // FM3 Slot Detune値 slot1
    int slot_detune2;  // FM3 Slot Detune値 slot2
    int slot_detune3;  // FM3 Slot Detune値 slot3
    int slot_detune4;  // FM3 Slot Detune値 slot4

    int fadeout_flag;  // When calling Fade from inside 1
    int revpan;  // PCM86逆相flag
    int pcm86_vol; // PCM86の音量をSPBに合わせるか?
    int _BarCounter;
    int port22h; // OPN-PORT 22H に最後に出力した値(hlfo)

    int tempo_48; // Current tempo (value of clock = 48 t)
    int tempo_48_push;  // Current tempo (same as above / for saving)

    int _fm_voldown;  // FM voldown 数値 (保存用)
    int _ssg_voldown;  // PSG voldown 数値 (保存用)
    int _pcm_voldown;  // PCM voldown 数値 (保存用)
    int _rhythm_voldown; // RHYTHM voldown 数値 (保存用)
    int _pcm86_vol; // PCM86の音量をSPBに合わせるか? (保存用)

    int rshot_bd; // リズム音源 shot inc flag (BD)
    int rshot_sd; // リズム音源 shot inc flag (SD)
    int rshot_sym; // リズム音源 shot inc flag (CYM)
    int rshot_hh; // リズム音源 shot inc flag (HH)
    int rshot_tom; // リズム音源 shot inc flag (TOM)
    int rshot_rim; // リズム音源 shot inc flag (RIM)
    int rdump_bd; // リズム音源 dump inc flag (BD)
    int rdump_sd; // リズム音源 dump inc flag (SD)
    int rdump_sym; // リズム音源 dump inc flag (CYM)
    int rdump_hh; // リズム音源 dump inc flag (HH)
    int rdump_tom; // リズム音源 dump inc flag (TOM)
    int rdump_rim; // リズム音源 dump inc flag (RIM)

    uint32_t ch3mode; // ch3 Mode
    int ppz_voldown;  // PPZ8 voldown 数値
    int _ppz_voldown;  // PPZ8 voldown 数値 (保存用)

    bool _IsTimerABusy;
    int _TimerATime;

    bool _IsTimerBBusy;
    int _TimerBTempo;  // Current value of TimerB (= ff_tempo during ff)

    uint32_t _OPNARate; // PCM output frequency (11k, 22k, 44k, 55k)
    uint32_t _PPZ8Rate; // PPZ output frequency

    bool _IsPlaying; // True if the driver is playing
    bool _IsUsingP86;

    int _FadeOutSpeedHQ; // Fadeout (High Sound Quality) speed (fadeout at > 0)

    WCHAR _PPCFileName[MAX_PATH];
    std::vector<std::wstring> _SearchPath;
};
#pragma warning(default: 4820) // x bytes padding added after last data member
