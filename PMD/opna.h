
// Based on PMDWin code by C60

#pragma once

#include "FileIO.h"

#include <ymfm_opn.h>

typedef int32_t Sample;
typedef int32_t ISample;
typedef int64_t emulated_time; // We use an int64_t as emulated time, as a 16.48 fixed point value
  
struct StereoSample
{
    Sample left;
    Sample right;
};

#pragma pack(push)
#pragma pack(2)
struct Stereo16bit
{
    short left;
    short right;
} ;
#pragma pack(pop)

#pragma warning(disable: 4265)
class OPNA : public ymfm::ymfm_interface
{
public:
   
    OPNA(IFileIO * fileio);
    ~OPNA();
    
    void setfileio(IFileIO* pfileio);
    
    bool Init(uint32_t c, uint32_t r, bool ip = false, const WCHAR * path = nullptr);
    bool SetRate(uint32_t r);
    bool SetRate(uint32_t c, uint32_t r, bool = false);
    bool LoadRhythmSamples(const WCHAR*);
    void Reset();
    
    void SetVolumeFM(int db);
    void SetVolumePSG(int db);
    void SetVolumeADPCM(int db);
    void SetVolumeRhythmTotal(int db);
    void SetVolumeRhythm(int index, int db);
    
    void SetReg(uint32_t addr, uint32_t data);
    uint32_t GetReg(uint32_t addr);
    
    uint32_t ReadStatus();
    uint32_t ReadStatusEx();
    
    bool Count(uint32_t us);
    uint32_t GetNextEvent();
    
    void Mix(Sample* buffer, int nsamples);
    
    static constexpr uint32_t DEFAULT_CLOCK = 3993600*2;

protected:
    IFileIO * _FileIO;
    
    // internal state
    ymfm::ym2608 m_chip;
    uint32_t m_clock;
    uint64_t m_clocks;
    typename ymfm::ym2608::output_data m_output;
    emulated_time m_step;
    emulated_time m_pos;
    
    uint32_t rate;                // FM 音源合成レート
    
    struct Rhythm
    {
        uint8_t    pan;    // ぱん
        int8_t    level;    // おんりょう
        int      volume;    // おんりょうせってい
        int16_t*  sample;    // さんぷる
        uint32_t  size;    // さいず
        uint32_t  pos;    // いち
        uint32_t  step;    // すてっぷち
        uint32_t  rate;    // さんぷるのれーと
    };
    
    bool _HasADPCMROM;
    
    static constexpr int32_t FM_TLBITS = 7;
    static constexpr int32_t FM_TLENTS = (1 << FM_TLBITS);
    static constexpr int32_t FM_TLPOS = (FM_TLENTS / 4);
    
    int32_t tltable[FM_TLENTS + FM_TLPOS];
    
    Rhythm  _Rhythm[6];
    int32_t  rhythmtvol;
    int8_t  rhythmtl;    // リズム全体の音量
    uint8_t  rhythmkey;    // リズムのキー
    
    // internal state
    std::vector<uint8_t> m_data[ymfm::ACCESS_CLASSES];
    
    emulated_time output_step;
    emulated_time output_pos;
    
    emulated_time timer_period[2];
    emulated_time timer_count[2];
    uint8_t reg27;
    
    void RhythmMix(Sample* buffer, uint32_t count);
    
    void StoreSample(Sample& dest, ISample data);
    
    // generate one output sample of output
    virtual void generate(emulated_time output_start, emulated_time output_step, int32_t* buffer);
    
    // write data to the ADPCM-A buffer
    void write_data(ymfm::access_class type, uint32_t base, uint32_t length, uint8_t const* src);
    
    // simple getters
    uint32_t sample_rate() const;
    
    // handle a read from the buffer
    virtual uint8_t ymfm_external_read(ymfm::access_class type, uint32_t offset) override;
    
    virtual void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data) override;
    
    virtual void ymfm_sync_mode_write(uint8_t data) override;
    
    virtual void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;
};

inline int Limit(int v, int max, int min)
{
    return ymfm::clamp(v, min, max);
}
