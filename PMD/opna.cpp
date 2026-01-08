
/** $VER: OPNA.cpp (2026.01.07) OPNA emulator (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#include <pch.h>

#include "OPNA.h"

#pragma warning(disable: 4355) // 'this' used in base member initializer list

opna_t::opna_t(File * file) :
    _File(file),

    _Instruments{},
    _InstrumentCount(0),

    _MasterVolume(0),
    _InstrumentTotalLevel(0),
    _InstrumentMask(0),
    _HasADPCMROM(false),

    _OutputPosition(0),
    _TimerPeriod{},
    _TimerCounter{},
    _Reg27(0U),

    _Chip(*this),
    _Output(),

    _Pos(0),
    _Step(0),

    _TickCount(0U)
{
    Initialize(DefaultClockSpeed, 8000U);

    // Create the table.
    for (int32_t i = -FM_TLPOS; i < FM_TLENTS; ++i)
        tltable[i + FM_TLPOS] = (int32_t) (uint32_t(65536.0 * std::pow(2.0, i * -16.0 / (int32_t) FM_TLENTS)) - 1);
}

opna_t::~opna_t()
{
    DeleteInstruments();
}

/// <summary>
/// Initializes the module.
/// </summary>
bool opna_t::Initialize(uint32_t clockSpeed, uint32_t sampleRate, const WCHAR * directoryPathDrums) noexcept
{
    Initialize(clockSpeed, sampleRate);

    _Pos  = 0;
    _Step = (emulated_time) (0x1000000000000ull / GetSampleRate());
    _TickCount = 0U;

    _Chip.reset();

    SetFMVolume(0);
    SetSSGVolume(0);
    SetADPCMVolume(0);
    SetRhythmVolume(0);

    for (int32_t i = 0; i < (int32_t) _countof(_Instruments); ++i)
        SetInstrumentVolume(i, 0);

    LoadInstruments(directoryPathDrums);

    return true;
}

/// <summary>
/// Initializes the module.
/// </summary>
void opna_t::Initialize(uint32_t clockSpeed, uint32_t sampleRate) noexcept
{
    _ClockSpeed = clockSpeed;

    if (sampleRate == 0)
        return;

    _SampleRate = sampleRate;
    _OutputStep = (emulated_time) (0x1000000000000ull / _SampleRate);
}

#pragma region Volume

/// <summary>
/// Sets the FM sound source volume, in dB.
/// </summary>
void opna_t::SetFMVolume(int dB)
{
    dB = std::min(dB, 20);

    int32_t Volume = (dB > -192) ? (int32_t) (65536.0 * std::pow(10.0, dB / 40.0)) : 0;

    _Chip.SetFMVolume(Volume); // Used to scale the volume before handing it off for output.
}

/// <summary>
/// Sets the SSG (Software-Controlled Sound Generator) sound source volume, in dB.
/// </summary>
void opna_t::SetSSGVolume(int dB)
{
    dB = std::min(dB, 20);

    int32_t Volume = (dB > -192) ? (int32_t) (65536.0 * std::pow(10.0, dB / 40.0)) : 0;

    _Chip.SetPSGVolume(Volume); // Used to scale the volume before handing it off for output.
}

/// <summary>
/// Sets the ADPCM sound source volume, in dB.
/// </summary>
void opna_t::SetADPCMVolume(int dB)
{
    dB = std::min(dB, 20);

    int32_t Volume = (dB > -192) ? (int32_t) (65536.0 * std::pow(10.0, dB / 40.0)) : 0;

    _Chip.SetADPCMVolume(Volume); // Used to scale the volume before handing it off for output.
}

/// <summary>
/// Sets the Rhythm sound source master volume, in dB.
/// </summary>
void opna_t::SetRhythmVolume(int dB)
{
    dB = std::min(dB, 20);

    _MasterVolume = -(dB * 2 / 3);

    int32_t Volume = (dB > -192) ? (int32_t) (65536.0 * std::pow(10.0, dB / 40.0)) : 0;

    _Chip.SetRhythmVolume(Volume); // Used to scale the volume before handing it off for output.
}

/// <summary>
/// Sets the volume of the specified instrument, in dB.
/// </summary>
void opna_t::SetInstrumentVolume(int index, int dB)
{
    dB = std::min(dB, 20);

    _Instruments[index].Volume = -(dB * 2 / 3);
}

#pragma endregion

#pragma region Registers

/// <summary>
/// Sets the value of a register.
/// </summary>
void opna_t::SetReg(uint32_t addr, uint32_t value)
{
    if ((0x10 <= addr) && (addr <= 0x1F) && !_HasADPCMROM)
    {
        // Use PPS WAV files to play percussion.
        switch (addr)
        {
            case 0x10: // DM / KEYON
            {
                if (!(value & 0x80)) // Key On
                {
                    _InstrumentMask |= value & 0x3F;

                    if (value & 0x01) _Instruments[0].Pos = 0;
                    if (value & 0x02) _Instruments[1].Pos = 0;
                    if (value & 0x04) _Instruments[2].Pos = 0;
                    if (value & 0x08) _Instruments[3].Pos = 0;
                    if (value & 0x10) _Instruments[4].Pos = 0;
                    if (value & 0x20) _Instruments[5].Pos = 0;
                }
                else
                    _InstrumentMask &= ~value;
                break;
            }

            case 0x11:
            {
                _InstrumentTotalLevel = (int8_t) (~value & 0x3F);
                break;
            }

            case 0x18: // Bass Drum
            case 0x19: // Snare Drum
            case 0x1A: // Top Cymbal
            case 0x1B: // Hihat
            case 0x1C: // Tom-tom
            case 0x1D: // Rim shot
            {
                _Instruments[addr & 7].Pan   = (value >> 6) & 0x03;
                _Instruments[addr & 7].Level = (int8_t) (~value & 0x1F);
                break;
            }
        }
    }
    else
    {
        const uint32_t Addr1 = 2 * ((addr >> 8) & 3);

        // Write to the chip.
        if (Addr1 != 0xFFFF)
        {
            const uint8_t Data1 = addr & 0xFF;
            const uint8_t Data2 = (uint8_t) value;

            _Chip.write(Addr1,     Data1);
            _Chip.write(Addr1 + 1, Data2);
        }
    }
}

/// <summary>
/// Gets the value of a register.
/// </summary>
uint32_t opna_t::GetReg(uint32_t addr)
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
#pragma endregion

#pragma region Timer processing

/// <summary>
/// Advances the timers until the next tick (in μs).
/// </summary>
bool opna_t::AdvanceTimers(uint32_t nextTick) noexcept
{
    bool Result = false;

    if (_Reg27 & 0x01)
    {
        if (_TimerCounter[0] > 0)
            _TimerCounter[0] -= ((emulated_time) nextTick << (48 - 6)) / (1'000'000 >> 6);
    }

    if (_Reg27 & 0x02)
    {
        if (_TimerCounter[1] > 0)
            _TimerCounter[1] -= ((emulated_time) nextTick << (48 - 6)) / (1'000'000 >> 6);
    }

    for (int32_t i = 0; i < (int32_t) _countof(_TimerCounter); ++i)
    {
        if ((_Reg27 & (4 << i)) && (_TimerCounter[i] < 0))
        {
            Result = true;

            do
            {
                _TimerCounter[i] += _TimerPeriod[i];
            }
            while (_TimerCounter[i] < 0);

            m_engine->engine_timer_expired((uint32_t) i);
        }
    }

    return Result;
}

/// <summary>
/// Gets the number of μs until the next timer tick occurs.
/// </summary>
uint32_t opna_t::GetNextTick() const noexcept
{
    if (_TimerCounter[0] == 0 && _TimerCounter[1] == 0)
        return 0;

    emulated_time Tick = INT64_MAX;

    if (_TimerCounter[0] > 0)
        Tick = std::min(Tick, _TimerCounter[0]);

    if (_TimerCounter[1] > 0)
        Tick = std::min(Tick, _TimerCounter[1]);

    return (uint32_t) (((Tick + ((emulated_time) 1 << 48) / 1'000'000) * (1'000'000 >> 6)) >> (48 - 6));

}

#pragma endregion

#pragma region Sample Mixing

/// <summary>
/// Synthesizes a buffer of rhythm samples.
/// </summary>
void opna_t::Mix(sample_t * sampleData, size_t sampleCount) noexcept
{
    sample_t * SampleData = sampleData;
    size_t SampleCount = sampleCount;

    while (SampleCount-- != 0)
    {
        int32_t Outputs[2] = { 0, 0 };

        // ymfm
        generate(_OutputPosition, _OutputStep, Outputs);
        _OutputPosition += _OutputStep;

        *SampleData++ += Outputs[0];
        *SampleData++ += Outputs[1];
    }

    if (!_HasADPCMROM)
        MixRhythmSamples(sampleData, sampleCount);
}

/// <summary>
/// Mixes the rhythm instrument samples with the existing synthesized samples.
/// </summary>
void opna_t::MixRhythmSamples(sample_t * sampleData, size_t sampleCount) noexcept
{
    if (!((_InstrumentMask & 0x3F) && _Instruments[0].Samples && (_MasterVolume < 128)))
        return;

    const sample_t * SampleDataEnd = sampleData + (sampleCount * 2);

    for (size_t i = 0; i < _countof(_Instruments); ++i)
    {
        Instrument & Ins = _Instruments[i];

        if ((_InstrumentMask & (1 << i)))
        {
            const int32_t dB = std::clamp(_InstrumentTotalLevel + _MasterVolume + Ins.Level + Ins.Volume, -31, 127);

            const int32_t Volume = tltable[FM_TLPOS + (dB << (FM_TLBITS - 7))] >> 4;
            const int32_t MaskL  = -((Ins.Pan >> 1) & 1);
            const int32_t MaskR  = - (Ins.Pan       & 1);

            for (sample_t * SampleData = sampleData; (SampleData < SampleDataEnd) && (Ins.Pos < Ins.Size); SampleData += 2)
            {
                const int32_t Sample = (Ins.Samples[Ins.Pos / 1024] * Volume) >> 12;

                StoreSample(SampleData[0], Sample & MaskL);
                StoreSample(SampleData[1], Sample & MaskR);

                Ins.Pos += Ins.Step;
            }
        }
    }
}

/// <summary>
/// Stores the sample.
/// </summary>
void opna_t::StoreSample(sample_t & sampleData, int32_t sampleValue)
{
    if constexpr(sizeof(sample_t) == 2)
        sampleData = (sample_t) std::clamp(sampleData + sampleValue, -0x8000, 0x7FFF);
    else
        sampleData += sampleValue;
}

#pragma endregion

#pragma region Rhythm Instruments

/// <summary>
/// Loads the samples for the rhythm instruments.
/// </summary>
bool opna_t::LoadInstruments(const WCHAR * directoryPath)
{
    _HasADPCMROM = false;

    WCHAR FilePath[_MAX_PATH] = { 0 };

    CombinePath(FilePath, _countof(FilePath), directoryPath, L"ym2608_adpcm_rom.bin");

    {
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
    }
 
    DeleteInstruments(); 

    for (auto & Instrument : _Instruments)
    {
        FilePath[0] = '\0';

        WCHAR FileName[_MAX_PATH] = { 0 };

        static const WCHAR * InstrumentName[_countof(_Instruments)] =
        {
            L"bd", L"sd", L"top", L"hh", L"tom", L"rim",
        };

        ::StringCbPrintfW(FileName, _countof(FileName), L"2608_%s.wav", InstrumentName[_InstrumentCount]);

        WAVEReader & wr = Instrument.Wave;

        {
            CombinePath(FilePath, _countof(FilePath), directoryPath, FileName);

            if (!wr.Open(FilePath))
            {
                if (_InstrumentCount != 5)
                    break;

                // Try to open an alternate file for the sixth instrument.
                CombinePath(FilePath, _countof(FilePath), directoryPath, L"2608_rym.wav");

                if (!wr.Open(FilePath))
                    break;
            }

            wr.Close();

            if (!(wr.Format() == 1) && (wr.ChannelCount() == 1) && (wr.SampleRate() == 44100) && (wr.BitsPerSample() == 16))
                break;
        }

        uint32_t SampleCount = wr.Size() / 2;

        Instrument.Samples = (const int16_t *) wr.Data();
        Instrument.Size    = SampleCount * 1024;
        Instrument.Step    = wr.SampleRate() * 1024 / _SampleRate;
        Instrument.Pos     = 0U;

        ++_InstrumentCount;
    }

    if (_InstrumentCount != _countof(_Instruments))
    {
        DeleteInstruments(); 

        return false;
    }

    return true;
}

/// <summary>
/// Deletes the rhythm instrument samples.
/// </summary>
void opna_t::DeleteInstruments() noexcept
{
    for (auto & Instrument : _Instruments)
    {
        Instrument.Wave.Reset();
        Instrument.Samples = nullptr;
        Instrument.Pos = ~0U;
    }

    _InstrumentCount = 0;
}

#pragma endregion

#pragma region ymfm_interface

/// <summary>
/// Generates one output sample.
/// </summary>
void opna_t::generate(emulated_time output_start, emulated_time, int32_t * buffer)
{
    // Generate at the appropriate sample rate.
    for (; _Pos <= output_start; _Pos += _Step)
        _Chip.generate(&_Output);

    // Add the final result to the buffer.
    const int32_t out0 = _Output.data[0];
    const int32_t out1 = _Output.data[1 % ymfm::ym2608::OUTPUTS];
    const int32_t out2 = _Output.data[2 % ymfm::ym2608::OUTPUTS];

    *buffer++ += out0 + out2;
    *buffer++ += out1 + out2;

    _TickCount++;
}

/// <summary>
/// Writes data to the ADPCM-A buffer. 
/// </summary>
void opna_t::write_data(ymfm::access_class type, uint32_t base, uint32_t length, uint8_t const * src)
{
    const uint32_t end = base + length;

    if (end > _Data[type].size())
        _Data[type].resize(end);

    ::memcpy(&_Data[type][base], src, length);
}

/// <summary>
/// Reads data from the ADPCM-A buffer (Callback).
/// </summary>
uint8_t opna_t::ymfm_external_read(ymfm::access_class type, uint32_t offset)
{
    if (!_HasADPCMROM && (type == ymfm::ACCESS_ADPCM_A))
        return 0;

    auto & data = _Data[type];

    return (uint8_t) ((offset < data.size()) ? data[offset] : 0U);
}

/// <summary>
/// Writes data to the ADPCM-A buffer (Callback). 
/// </summary>
void opna_t::ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data)
{
    write_data(type, address, 1, &data);
}

/// <summary>
/// Clears the timer. (Callback).
/// </summary>
void opna_t::ymfm_sync_mode_write(uint8_t value)
{
    _Reg27 = value;

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
    ymfm_interface::ymfm_sync_mode_write(_Reg27);
}

/// <summary>
/// Sets the timer (Callback).
/// </summary>
void opna_t::ymfm_set_timer(uint32_t timerIndex, int32_t duration_in_clocks)
{
    if (duration_in_clocks >= 0)
    {
        _TimerPeriod[timerIndex] = (((emulated_time) duration_in_clocks << 43) / _ClockSpeed) << 5;
        _TimerCounter[timerIndex] = _TimerPeriod[timerIndex];
    }
    else
    {
        _TimerPeriod[timerIndex] = 0;
        _TimerCounter[timerIndex] = 0;
    }
}

#pragma endregion
