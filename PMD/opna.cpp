
// Based on PMDWin code by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <Windows.h>
#include <strsafe.h>

#include "OPNA.h"

#pragma warning(disable: 4355) // 'this' used in base member initializer list
OPNA::OPNA(File * file) :
    _File(file),

    _Chip(*this),
    _Output(),

    _ClockSpeed(DEFAULT_CLOCK),
    _SynthesisRate(8000U),

    _Step(0),
    _Pos(0),
    _TickCount(0U),

    _Instrument{},
    _MasterVolume(0),
    _InstrumentTL(0),
    _InstrumentMask(0),
    _HasADPCMROM(false),

    output_step((emulated_time) (0x1000000000000ull / _SynthesisRate)),
    output_pos(0),
    timer_period{},
    timer_count{},
    reg27(0U)
{
    // Create the table.
    for (int i = -FM_TLPOS; i < FM_TLENTS; ++i)
        tltable[i + FM_TLPOS] = (int32_t) (uint32_t(65536. * pow(2.0, i * -16. / (int) FM_TLENTS)) - 1);
}

OPNA::~OPNA()
{
    for (int i = 0; i < _countof(_Instrument); ++i)
    {
        _Instrument[i].Wave.Reset();
        _Instrument[i].Samples = nullptr;
    }
}

/// <summary>
/// Initializes the module.
/// </summary>
bool OPNA::Init(uint32_t clock, uint32_t synthesisRate, bool useInterpolation, const WCHAR * directoryPath)
{
    if (!SetRate(clock, synthesisRate, useInterpolation))
        return false;

    _Step = (emulated_time) (0x1000000000000ull / GetSampleRate());
    _Pos  = 0;
    _TickCount = 0U;

    _Chip.reset();

    SetFMVolume(0);
    SetPSGVolume(0);
    SetADPCMVolume(0);
    SetRhythmMasterVolume(0);

    for (int i = 0; i < _countof(_Instrument); i++)
        SetRhythmVolume(i, 0);

    LoadInstruments(directoryPath);

    return true;
}

/// <summary>
/// Sets the synthesis rate.
/// </summary>
bool OPNA::SetRate(uint32_t clockSpeed, uint32_t synthesisRate, bool)
{
    _ClockSpeed = clockSpeed;

    return SetRate(synthesisRate);
}

/// <summary>
/// Sets the synthesis rate.
/// </summary>
bool OPNA::SetRate(uint32_t synthesisRate)
{
    _SynthesisRate = synthesisRate;
    output_step = (emulated_time) (0x1000000000000ull / _SynthesisRate);

    return true;
}

/// <summary>
/// Loads the rythm samples.
/// </summary>
bool OPNA::LoadInstruments(const WCHAR * directoryPath)
{
    _HasADPCMROM = false;

    WCHAR FilePath[_MAX_PATH] = { 0 };

    CombinePath(FilePath, _countof(FilePath), directoryPath, L"ym2608_adpcm_rom.bin");

    int64_t FileSize = _File->GetFileSize(FilePath);

    if (FileSize > 0 && _File->Open(FilePath))
    {
        std::vector<uint8_t> temp((size_t) FileSize);

        _File->Read(temp.data(), (uint32_t) FileSize);
        _File->Close();

        write_data(ymfm::ACCESS_ADPCM_A, 0, (uint32_t) FileSize, temp.data());

        _HasADPCMROM = true;

        return true;
    }
 
    static const WCHAR * InstrumentName[_countof(_Instrument)] =
    {
        L"bd", L"sd", L"top", L"hh", L"tom", L"rim",
    };

    int i;

    for (i = 0; i < _countof(_Instrument); i++)
        _Instrument[i].Pos = ~0U;

    for (i = 0; i < _countof(_Instrument); i++)
    {
        FilePath[0] = '\0';

        ::StringCbPrintfW(FilePath, _countof(FilePath), L"%s2608_%s.wav", directoryPath, InstrumentName[i]);

        WAVEReader & wr = _Instrument[i].Wave;

        {
            wr.Reset();

            if (!wr.Open(FilePath))
            {
                if (i != 5)
                    break;

                CombinePath(FilePath, _countof(FilePath), directoryPath, L"2608_rym.wav");

                if (!wr.Open(FilePath))
                    break;
            }

            wr.Close();

            if (!(wr.Format() == 1) && (wr.ChannelCount() == 1) && (wr.SampleRate() == 44100) && (wr.BitsPerSample() == 16))
                break;
        }

        uint32_t SampleCount = wr.Size() / 2;

        _Instrument[i].Samples = (const int16_t *) wr.Data();
        _Instrument[i].Size    = SampleCount * 1024;
        _Instrument[i].Step    = wr.SampleRate() * 1024 / _SynthesisRate;
        _Instrument[i].Pos     = 0U;
    }

    if (i != _countof(_Instrument))
    {
        for (i = 0; i < _countof(_Instrument); i++)
        {
            _Instrument[i].Wave.Reset();
            _Instrument[i].Samples = nullptr;
        }

        return false;
    }

    return true;
}

/// <summary>
/// Sets the FM volume.
/// </summary>
void OPNA::SetFMVolume(int dB)
{
    dB = (std::min)(dB, 20);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setfmvolume(Volume);
}

/// <summary>
/// Sets the PSG volume.
/// </summary>
void OPNA::SetPSGVolume(int dB)
{
    dB = (std::min)(dB, 20);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setpsgvolume(Volume);
}

/// <summary>
/// Sets the ADPCM volume.
/// </summary>
void OPNA::SetADPCMVolume(int dB)
{
    dB = (std::min)(dB, 20);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setadpcmvolume(Volume);
}

/// <summary>
/// Sets the Rhythm master volume.
/// </summary>
void OPNA::SetRhythmMasterVolume(int dB)
{
    dB = (std::min)(dB, 20);

    _MasterVolume = -(dB * 2 / 3);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setrhythmvolume(Volume);
}

/// <summary>
/// Sets the Rhythm volume of the specified instrument.
/// </summary>
void OPNA::SetRhythmVolume(int index, int dB)
{
    dB = (std::min)(dB, 20);

    _Instrument[index].Volume = -(dB * 2 / 3);
}

/// <summary>
/// Sets the value of a register.
/// </summary>
void OPNA::SetReg(uint32_t addr, uint32_t value)
{
    if ((addr >= 0x10) && (addr <= 0x1f) && !_HasADPCMROM)
    {
        // Use PPS WAV files to play percussion.
        switch (addr)
        {
            case 0x10: // DM / KEYON
                if (!(value & 0x80)) // Key On
                {
                    _InstrumentMask |= value & 0x3f;

                    if (value & 0x01) _Instrument[0].Pos = 0;
                    if (value & 0x02) _Instrument[1].Pos = 0;
                    if (value & 0x04) _Instrument[2].Pos = 0;
                    if (value & 0x08) _Instrument[3].Pos = 0;
                    if (value & 0x10) _Instrument[4].Pos = 0;
                    if (value & 0x20) _Instrument[5].Pos = 0;
                }
                else
                    _InstrumentMask &= ~value;
                break;

            case 0x11:
                _InstrumentTL = (int8_t) (~value & 63);
                break;

            case 0x18: // Bass Drum
            case 0x19: // Snare Drum
            case 0x1a: // Top Cymbal
            case 0x1b: // Hihat
            case 0x1c: // Tom-tom
            case 0x1d: // Rim shot
                _Instrument[addr & 7].Pan   = (value >> 6) & 3;
                _Instrument[addr & 7].Level = (int8_t) (~value & 31);
                break;
        }
    }
    else
    {
        uint32_t addr1 = 0 + 2 * ((addr >> 8) & 3);
        uint8_t data1 = addr & 0xff;

        uint32_t addr2 = addr1 + 1;
        uint8_t data2 = (uint8_t) value;

        // write to the chip
        if (addr1 != 0xffff)
        {
            _Chip.write(addr1, data1);
            _Chip.write(addr2, data2);
        }
    }
}

/// <summary>
/// Gets the value of a register.
/// </summary>
uint32_t OPNA::GetReg(uint32_t addr)
{
    uint32_t addr1 = 0 + 2 * ((addr >> 8) & 3);
    uint8_t data1 = addr & 0xff;

    uint32_t addr2 = addr1 + 1;
    uint8_t result = 0;

    if (addr1 != 0xffff)
    {
        _Chip.write(addr1, data1);

        result = _Chip.read(addr2);
    }
    else
        result = 1;

    return result;
}

/// <summary>
/// Timer processing
/// </summary>
bool OPNA::Count(uint32_t us)
{
    bool result = false;

    if (reg27 & 1)
    {
        if (timer_count[0] > 0)
            timer_count[0] -= ((emulated_time) us << (48 - 6)) / (1000000 >> 6);
    }

    if (reg27 & 2)
    {
        if (timer_count[1] > 0)
            timer_count[1] -= ((emulated_time) us << (48 - 6)) / (1000000 >> 6);
    }

    for (int i = 0; i < sizeof(timer_count) / sizeof(emulated_time); i++)
    {
        if ((reg27 & (4 << i)) && timer_count[i] < 0)
        {
            result = true;

            do
            {
                timer_count[i] += timer_period[i];
            }
            while (timer_count[i] < 0);

            m_engine->engine_timer_expired((uint32_t) i);
        }
    }

    return result;
}

/// <summary>
/// Gets the next event.
/// </summary>
uint32_t OPNA::GetNextEvent()
{
    if (timer_count[0] == 0 && timer_count[1] == 0)
        return 0;

    emulated_time result = INT64_MAX;

    for (int i = 0; i < sizeof(timer_count) / sizeof(emulated_time); i++)
    {
        if (timer_count[i] > 0)
            result = (std::min)(result, timer_count[i]);
    }

    return (uint32_t) ((result + ((emulated_time) 1 << 48) / 1000000) * (1000000 >> 6) >> (48 - 6));
}

/// <summary>
/// Synthesizes a buffer of rhythm samples.
/// </summary>
void OPNA::Mix(Sample * sampleData, size_t sampleCount) noexcept
{
    Sample * SampleData = sampleData;
    size_t SampleCount = sampleCount;

    while (SampleCount-- != 0)
    {
        int32_t Outputs[2] = { 0, 0 };

        // ymfm
        generate(output_pos, output_step, Outputs);
        output_pos += output_step;

        *SampleData++ += Outputs[0];
        *SampleData++ += Outputs[1];
    }

    if (!_HasADPCMROM)
        RhythmMix(sampleData, sampleCount);
}

/// <summary>
/// Synthesizes a buffer of rhythm samples using WAV.
/// </summary>
void OPNA::RhythmMix(Sample * sampleData, size_t sampleCount) noexcept
{
    if (_Instrument[0].Samples && (_MasterVolume < 128) && (_InstrumentMask & 0x3f))
    {
        Sample * SampleDataEnd = sampleData + (sampleCount * 2);

        for (size_t i = 0; i < _countof(_Instrument); i++)
        {
            Instrument & Ins = _Instrument[i];

            if ((_InstrumentMask & (1 << i)))
            {
                int dB = Limit(_InstrumentTL + _MasterVolume + Ins.Level + Ins.Volume, 127, -31);

                int Vol   = tltable[FM_TLPOS + (dB << (FM_TLBITS - 7))] >> 4;
                int MaskL = -((Ins.Pan >> 1) & 1);
                int MaskR = - (Ins.Pan       & 1);

                for (Sample * SampleData = sampleData; (SampleData < SampleDataEnd) && (Ins.Pos < Ins.Size); SampleData += 2)
                {
                    int Sample = (Ins.Samples[Ins.Pos / 1024] * Vol) >> 12;

                    Ins.Pos += Ins.Step;

                    StoreSample(SampleData[0], Sample & MaskL);
                    StoreSample(SampleData[1], Sample & MaskR);
                }
            }
        }
    }
}

/// <summary>
/// Stores the sample.
/// </summary>
void OPNA::StoreSample(Sample & dest, int32_t data)
{
    if constexpr(sizeof(Sample) == 2)
        dest = (Sample) Limit(dest + data, 0x7fff, -0x8000);
    else
        dest += data;
}

#pragma region(ymfm_interface)
/// <summary>
/// Generate one output sample of output..
/// </summary>
void OPNA::generate(emulated_time output_start, emulated_time, int32_t * buffer)
{
    // Generate at the appropriate sample rate
    for (; _Pos <= output_start; _Pos += _Step)
        _Chip.generate(&_Output);

    // Add the final result to the buffer
    int32_t out0 = _Output.data[0];
    int32_t out1 = _Output.data[1 % ymfm::ym2608::OUTPUTS];
    int32_t out2 = _Output.data[2 % ymfm::ym2608::OUTPUTS];

    *buffer++ += out0 + out2;
    *buffer++ += out1 + out2;

    _TickCount++;
}

// Write data to the ADPCM-A buffer
void OPNA::write_data(ymfm::access_class type, uint32_t base, uint32_t length, uint8_t const * src)
{
    uint32_t end = base + length;

    if (end > m_data[type].size())
        m_data[type].resize(end);

    ::memcpy(&m_data[type][base], src, length);
}

// callback : handle read from the buffer
uint8_t OPNA::ymfm_external_read(ymfm::access_class type, uint32_t offset)
{
    if (!_HasADPCMROM && (type == ymfm::ACCESS_ADPCM_A))
        return 0;

    auto & data = m_data[type];

    return (uint8_t) ((offset < data.size()) ? data[offset] : 0U);
}

// callback : handle write to the buffer
void OPNA::ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data)
{
    write_data(type, address, 1, &data);
}

// callback : clear timer
void OPNA::ymfm_sync_mode_write(uint8_t data)
{
    reg27 = data;

/* Unused for now
    if (reg27 & 0x10)
    {
        timer_count[0] = timer_period[0];
        reg27 &= ~0x10;
    }

    if (reg27 & 0x20)
    {
        timer_count[1] = timer_period[1];
        reg27 &= ~0x20;
    }
*/
    ymfm_interface::ymfm_sync_mode_write(reg27);
}

// callback : set timer
void OPNA::ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks)
{
    if (duration_in_clocks >= 0)
    {
        timer_period[tnum] = (((emulated_time) duration_in_clocks << 43) / _ClockSpeed) << 5;
        timer_count[tnum] = timer_period[tnum];
    }
    else
    {
        timer_period[tnum] = 0;
        timer_count[tnum] = 0;
    }
}
#pragma endregion
