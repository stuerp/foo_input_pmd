
// Based on PMDWin code by C60

#pragma once

#include "File.h"
#include "WAVEReader.h"

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
/// <summary>
/// Implements a FM Sound Source module, a six-channel FM synthesis sound system, based on the YM2203.
/// </summary>
class OPNA : public ymfm::ymfm_interface
{
public:
    OPNA(File * file);
    ~OPNA();
    
    bool Init(uint32_t c, uint32_t r, bool ip = false, const WCHAR * directoryPath = nullptr);
    bool SetRate(uint32_t r);
    bool SetRate(uint32_t c, uint32_t r, bool = false);
    bool LoadInstruments(const WCHAR *);

    void SetFMVolume(int dB);
    void SetPSGVolume(int dB);
    void SetADPCMVolume(int dB);
    void SetOverallRhythmVolume(int dB);
    void SetRhythmVolume(int index, int dB);
    
    void SetReg(uint32_t addr, uint32_t data);
    uint32_t GetReg(uint32_t addr);
    
    void Reset() { _Chip.reset(); }
    uint32_t ReadStatus() { return _Chip.read_status(); }       // Reads the status register.
    uint32_t ReadStatusEx() { return _Chip.read_status_hi(); }  // Reads the status register (extended addressing).

    bool Count(uint32_t us);
    uint32_t GetNextEvent();
    
    void Mix(Sample * sampleData, int sampleCount);
    
    static constexpr uint32_t DEFAULT_CLOCK = 3993600 * 2;

protected:
    void RhythmMix(Sample * sampleData, uint32_t sampleCount);
    void StoreSample(Sample & dest, ISample data);

    uint32_t GetSampleRate() const { return _Chip.sample_rate(_ClockSpeed); }

protected:
    File * _File;

    // Internal state
    ymfm::ym2608 _Chip;
    typename ymfm::ym2608::output_data _Output;

    uint32_t _ClockSpeed;
    uint32_t _SynthesisRate;

    emulated_time _Step;
    emulated_time _Pos;
    uint64_t _TickCount;

    static constexpr int32_t FM_TLBITS = 7;
    static constexpr int32_t FM_TLENTS = (1 << FM_TLBITS);
    static constexpr int32_t FM_TLPOS = (FM_TLENTS / 4);

    int32_t tltable[FM_TLENTS + FM_TLPOS];

    struct Instrument
    {
        WAVEReader Wave;

        const int16_t * Sample;
        uint32_t Size;

        uint32_t Step;
        uint32_t Pos;

        int32_t Volume;
        uint8_t Pan;
        int8_t Level;
    };
    
    Instrument _Instrument[6];

    int32_t _InstrumentVolume;

    int8_t _InstrumentTL;
    uint8_t _InstrumentKey;
    
    bool _HasADPCMROM;

protected:
    #pragma region(ymfm_interface)
    virtual void generate(emulated_time output_start, emulated_time output_step, int32_t * buffer);
    
    void write_data(ymfm::access_class type, uint32_t base, uint32_t length, uint8_t const* src);
    
    virtual uint8_t ymfm_external_read(ymfm::access_class type, uint32_t offset) override;
    
    virtual void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data) override;
    
    virtual void ymfm_sync_mode_write(uint8_t data) override;
    
    virtual void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;
    #pragma endregion

protected:
    #pragma region(ymfm_interface)
    std::vector<uint8_t> m_data[ymfm::ACCESS_CLASSES];
    
    emulated_time output_step;
    emulated_time output_pos;
    
    emulated_time timer_period[2];
    emulated_time timer_count[2];

    uint8_t reg27;
    #pragma endregion
};

inline int Limit(int v, int max, int min)
{
    return ymfm::clamp(v, min, max);
}
