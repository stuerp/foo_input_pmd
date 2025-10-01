
/** $VER: OPNAW.h (2025.10.01) OPNA emulator (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#pragma once

#include "File.h"
#include "WAVEReader.h"

#include <ymfm_opn.h>

typedef int32_t Sample;
typedef int64_t emulated_time; // We use an int64_t as emulated time, as a 16.48 fixed point value
  
struct Stereo32bit
{
    int32_t Left;
    int32_t Right;
};

#pragma pack(push)
#pragma pack(2)
struct Stereo16bit
{
    int16_t Left;
    int16_t Right;
} ;
#pragma pack(pop)

#pragma warning(disable: 4265)
/// <summary>
/// Implements a YM2608, aka OPNA, is a sixteen-channel sound chip developed by Yamaha.
/// It's a member of Yamaha's OPN family of FM synthesis chips, and the successor to the YM2203. It was notably used in NEC's PC-8801/PC-9801 series computers.
/// </summary>
class OPNA : public ymfm::ymfm_interface
{
public:
    OPNA(File * file);
    ~OPNA();
    
    bool Initialize(uint32_t clock, uint32_t outputFrequency, const WCHAR * directoryPath) noexcept;
    void Initialize(uint32_t clock, uint32_t outputFrequency) noexcept;

    bool LoadInstruments(const WCHAR *);
    bool HasADPCMROM() const noexcept { return _HasADPCMROM; }
    bool HasPercussionSamples() const noexcept { return _InstrumentCount == _countof(_Instruments); }

    void SetFMVolume(int dB);
    void SetSSGVolume(int dB);
    void SetADPCMVolume(int dB);
    void SetRhythmVolume(int dB);
    void SetInstrumentVolume(int index, int dB);

    void SetReg(uint32_t addr, uint32_t value);
    uint32_t GetReg(uint32_t addr);
    
    void Reset() { _Chip.reset(); }
    uint32_t ReadStatus() { return _Chip.read_status(); }       // Reads the status register.
    uint32_t ReadStatusEx() { return _Chip.read_status_hi(); }  // Reads the status register (extended addressing).

    bool AdvanceTimers(uint32_t tickCount) noexcept;
    uint32_t GetNextTick() const noexcept;
    
    void Mix(Sample * sampleData, size_t sampleCount) noexcept;
    
    static constexpr uint32_t DefaultClockSpeed = 3993600 * 2;

private:
    void MixRhythmSamples(Sample * sampleData, size_t sampleCount) noexcept;
    void StoreSample(Sample & sample, int32_t data);

    uint32_t GetSampleRate() const { return _Chip.sample_rate(_ClockSpeed); }

protected:
    File * _File;

    // Internal state
    static constexpr int32_t FM_TLBITS = 7;
    static constexpr int32_t FM_TLENTS = (1 << FM_TLBITS);
    static constexpr int32_t FM_TLPOS = (FM_TLENTS / 4);

    int32_t tltable[FM_TLENTS + FM_TLPOS];

    struct Instrument
    {
        WAVEReader Wave;

        const int16_t * Samples;
        uint32_t Size;

        uint32_t Step;
        uint32_t Pos;

        int32_t Volume;
        uint8_t Pan;
        int8_t Level;
    };
    
    Instrument _Instruments[6];
    uint32_t _InstrumentCount;

    int32_t _MasterVolume;

    int8_t _InstrumentTL;
    uint8_t _InstrumentMask; // 1 bit per percussion instrument
    
    bool _HasADPCMROM;

protected:
    #pragma region YMFM Interface

    virtual void generate(emulated_time output_start, emulated_time output_step, int32_t * buffer);
    
    void write_data(ymfm::access_class type, uint32_t base, uint32_t length, uint8_t const* src);
    
    virtual uint8_t ymfm_external_read(ymfm::access_class type, uint32_t offset) override;
    
    virtual void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data) override;
    
    virtual void ymfm_sync_mode_write(uint8_t data) override;
    
    virtual void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;

    #pragma endregion

protected:
    #pragma region YMFM Interface

    emulated_time _OutputStep;
    emulated_time _OutputPosition;
    
    // Timer A and B
    emulated_time _TimerPeriod[2];
    emulated_time _TimerCounter[2];

    uint8_t _Reg27;

    #pragma endregion

private:
    void DeleteInstruments() noexcept;

private:
    #pragma region YMFM Interface

    std::vector<uint8_t> _Data[ymfm::ACCESS_CLASSES];

    ymfm::ym2608 _Chip;
    typename ymfm::ym2608::output_data _Output;

    uint32_t _ClockSpeed;
    uint32_t _SampleRate;

    emulated_time _Pos;
    emulated_time _Step;
    uint64_t _TickCount;

    #pragma endregion
};
