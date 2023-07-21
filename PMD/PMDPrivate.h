
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

#define nbufsample          30000
#define OPNAClock   (3993600 * 2)

#define NumOfFMPart             6
#define NumOfSSGPart            3
#define NumOfADPCMPart          1
#define NumOfOPNARhythmPart     1
#define NumOfExtPart            3
#define NumOfRhythmPart         1
#define NumOfEffPart            1
#define NumOfPPZ8Part           8
#define NumOfAllPart            (NumOfFMPart+NumOfSSGPart+NumOfADPCMPart+NumOfOPNARhythmPart+NumOfExtPart+NumOfRhythmPart+NumOfEffPart+NumOfPPZ8Part)

#pragma warning(disable: 4820) // x bytes padding added after last data member
struct PMDWORK
{
    int partb;  // 処理中パート番号
    int tieflag; // &のフラグ(1 : tie)
    int volpush_flag;  // 次の１音音量down用のflag(1 : voldown)
    int rhydmy;  // R part ダミー演奏データ
    int fmsel;  // FM 表(=0)か裏(=0x100)か flag
    int omote_key[3];  // FM keyondata表(=0)
    int ura_key[3]; // FM keyondata裏(=0x100)
    int loop_work; // Loop Work
    bool _UsePPS;  // ppsdrv を使用するか？flag(ユーザーが代入)
    int pcmrepeat1; // PCMのリピートアドレス1
    int pcmrepeat2; // PCMのリピートアドレス2
    int pcmrelease; // PCMのRelease開始アドレス
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

// Data area during performance
struct PartState
{
    uint8_t * address; // 2 ｴﾝｿｳﾁｭｳ ﾉ ｱﾄﾞﾚｽ
    uint8_t * partloop; // 2 ｴﾝｿｳ ｶﾞ ｵﾜｯﾀﾄｷ ﾉ ﾓﾄﾞﾘｻｷ

    int leng;  // 1 ﾉｺﾘ LENGTH
    int qdat;  // 1 gatetime (q/Q値を計算した値)
    uint32_t fnum;  // 2 ｴﾝｿｳﾁｭｳ ﾉ BLOCK/FNUM
    int detune;  // 2 ﾃﾞﾁｭｰﾝ
    int lfodat;  // 2 LFO DATA
    int porta_num; // 2 ポルタメントの加減値（全体）
    int porta_num2; // 2 ポルタメントの加減値（一回）
    int porta_num3; // 2 ポルタメントの加減値（余り）
    int volume;  // 1 VOLUME
    int shift;  // 1 ｵﾝｶｲ ｼﾌﾄ ﾉ ｱﾀｲ
    int delay;  // 1 LFO [DELAY]
    int speed;  // 1 [SPEED]
    int step;  // 1 [STEP]
    int time;  // 1 [TIME]
    int delay2;  // 1 [DELAY_2]
    int speed2;  // 1 [SPEED_2]
    int step2;  // 1 [STEP_2]
    int time2;  // 1 [TIME_2]
    int lfoswi;  // 1 LFOSW. B0/tone B1/vol B2/同期 B3/porta

    //          B4/tone B5/vol B6/同期
    int volpush; // 1 Volume PUSHarea
    int mdepth;  // 1 M depth
    int mdspd;  // 1 M speed
    int mdspd2;  // 1 M speed_2
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
    int extendmode; // 1 B1/Detune B2/LFO B3/Env Normal/Extend
    int fmpan;  // 1 FM Panning + AMD + PMD
    int psgpat;  // 1 PSG PATTERN [TONE/NOISE/MIX]
    int voicenum; // 1 音色番号
    int loopcheck; // 1 ループしたら１ 終了したら３
    int carrier; // 1 FM Carrier
    int slot1;  // 1 SLOT 1 ﾉ TL
    int slot3;  // 1 SLOT 3 ﾉ TL
    int slot2;  // 1 SLOT 2 ﾉ TL
    int slot4;  // 1 SLOT 4 ﾉ TL
    int slotmask; // 1 FM slotmask
    int neiromask; // 1 FM 音色定義用maskdata
    int lfo_wave; // 1 LFOの波形
    int partmask; // 1 PartMask b0:通常 b1:効果音 b2:NECPCM用

    //    b3:none b4:PPZ/ADE用 b5:s0時 b6:m b7:一時
    int keyoff_flag;  // 1 KeyoffしたかどうかのFlag
    int volmask; // 1 音量LFOのマスク
    int qdata;  // 1 qの値
    int qdatb;  // 1 Qの値
    int hldelay; // 1 HardLFO delay
    int hldelay_c; // 1 HardLFO delay Counter
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
    int _lfo_wave; // 1 LFOの波形
    int _volmask; // 1 音量LFOのマスク
    int mdc;  // 1 M depth Counter (変動値)
    int mdc2;  // 1 M depth Counter
    int _mdc;  // 1 M depth Counter (変動値)
    int _mdc2;  // 1 M depth Counter
    int onkai;  // 1 演奏中の音階データ (0ffh:rest)
    int sdelay;  // 1 Slot delay
    int sdelay_c; // 1 Slot delay counter
    int sdelay_m; // 1 Slot delay Mask
    int alg_fb;  // 1 音色のalg/fb
    int keyon_flag; // 1 新音階/休符データを処理したらinc
    int qdat2;  // 1 q 最低保証値
    int onkai_def; // 1 演奏中の音階データ (転調処理前 / ?fh:rest)
    int shift_def; // 1 マスター転調値
    int qdat3;  // 1 q Random
};

#pragma warning(disable: 4820) // x bytes padding added after last data member
struct OPEN_WORK
{
    PartState * Part[NumOfAllPart]; // パートワークのポインタ

    uint8_t * mmlbuf;  // Musicdataのaddress+1
    uint8_t * tondat;  // Voicedataのaddress
    uint8_t * efcdat;  // FM Effecdataのaddress
    uint8_t * prgdat_adr; // 曲データ中音色データ先頭番地
    uint16_t * radtbl;  // R part offset table 先頭番地
    uint8_t * rhyadr;  // R part 演奏中番地

    bool _IsPlaying; // True if the driver is playing
    bool _UseRhythmSoundSource;  // Play Rhythm sound source with K/Rpart flag

    int rhythmmask; // Rhythm音源のマスク x8c/10hのbitに対応

    int fm_voldown; // FM voldown 数値
    int ssg_voldown;  // PSG voldown 数値
    int pcm_voldown;  // ADPCM voldown 数値
    int rhythm_voldown;  // RHYTHM voldown 数値

    int prg_flg; // 曲データに音色が含まれているかflag
    int x68_flg; // OPM flag
    int status;  // status1

    int _LoopCount;

    int tempo_d; // tempo (TIMER-B)
    int tempo_d_push;  // tempo (TIMER-B) / 保存用

    int _FadeOutSpeed;  // Fadeout速度
    int _FadeOutVolume;  // Fadeout音量

    int syousetu_lng;  // 小節の長さ
    int opncount; // 最短音符カウンタ
    int effflag; // PSG効果音発声on/off flag(ユーザーが代入)
    int psnoi;  // PSG noise周波数
    int psnoi_last; // PSG noise周波数(最後に定義した数値)
    int pcmstart; // PCM音色のstart値
    int pcmstop; // PCM音色のstop値
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
    int syousetu; // 小節カウンタ
    int port22h; // OPN-PORT 22H に最後に出力した値(hlfo)
    int tempo_48; // 現在のテンポ(clock=48 tの値)
    int tempo_48_push;  // 現在のテンポ(同上/保存用)

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
    int _TimerBSpeed;  // Current value of TimerB (= ff_tempo during ff)

    uint32_t _OPNARate; // PCM output frequency (11k, 22k, 44k, 55k)
    uint32_t _PPZ8Rate; // PPZ output frequency

    bool fmcalc55k;   // Do 55kHz FM synthesis?
    bool ppz8ip;    // Complement with PPZ8?
    bool ppsip;    // Complement with PPS?
    bool p86ip;    // Complement with PPS?
    bool _UseP86;   // Are we using P86?

    int _FadeOutSpeedHQ; // Fadeout (High Sound Quality) speed (fadeout at > 0)

    WCHAR _PPCFileName[MAX_PATH];
    std::vector<std::wstring> _SearchPath;
};
#pragma warning(default: 4820) // x bytes padding added after last data member
