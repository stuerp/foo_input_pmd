﻿
/** $VER: OPNAW.h (2023.10.18) OPNA emulator (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <Windows.h>
#include <strsafe.h>

#include "OPNA.h"

#pragma warning(disable: 4355) // 'this' used in base member initializer list
OPNA::OPNA(File * file) :
    _File(file),

    _Instrument{},
    _InstrumentCounter(0),

    _MasterVolume(0),
    _InstrumentTL(0),
    _InstrumentMask(0),
    _HasADPCMROM(false),

    output_pos(0),
    timer_period{},
    timer_count{},
    reg27(0U),

    _Chip(*this),
    _Output(),

    _Pos(0),
    _Step(0),

    _TickCount(0U)
{
    SetOutputFrequency(DEFAULT_CLOCK, 8000U);

    // Create the table.
    for (int i = -FM_TLPOS; i < FM_TLENTS; ++i)
        tltable[i + FM_TLPOS] = (int32_t) (uint32_t(65536. * pow(2.0, i * -16. / (int) FM_TLENTS)) - 1);
}

OPNA::~OPNA()
{
    DeleteInstruments();
}

/// <summary>
/// Initializes the module.
/// </summary>
bool OPNA::Initialize(uint32_t clock, uint32_t outputFrequency, bool useInterpolation, const WCHAR * directoryPath)
{
    SetOutputFrequency(clock, outputFrequency, useInterpolation);

    _Pos  = 0;
    _Step = (emulated_time) (0x1000000000000ull / GetSampleRate());
    _TickCount = 0U;

    _Chip.reset();

    SetFMVolume(0);
    SetSSGVolume(0);
    SetADPCMVolume(0);
    SetRhythmVolume(0);

    for (int i = 0; i < _countof(_Instrument); ++i)
        SetInstrumentVolume(i, 0);

    LoadInstruments(directoryPath);

    return true;
}

/// <summary>
/// Sets the output frequency.
/// </summary>
void OPNA::SetOutputFrequency(uint32_t clockSpeed, uint32_t outputFrequency, bool) noexcept
{
    _ClockSpeed = clockSpeed;

    SetOutputFrequency(outputFrequency);
}

/// <summary>
/// Sets the output frequency.
/// </summary>
void OPNA::SetOutputFrequency(uint32_t outputFrequency) noexcept
{
    if (outputFrequency == 0)
        return;

    _OutputFrequency = outputFrequency;
    output_step = (emulated_time) (0x1000000000000ull / _OutputFrequency);
}

#pragma region(Volume)
/// <summary>
/// Sets the FM sound source volume, in dB.
/// </summary>
void OPNA::SetFMVolume(int dB)
{
    dB = (std::min)(dB, 20);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setfmvolume(Volume);
}

/// <summary>
/// Sets the SSG (Software-Controlled Sound Generator) sound source volume, in dB.
/// </summary>
void OPNA::SetSSGVolume(int dB)
{
    dB = (std::min)(dB, 20);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setpsgvolume(Volume);
}

/// <summary>
/// Sets the ADPCM sound source volume, in dB.
/// </summary>
void OPNA::SetADPCMVolume(int dB)
{
    dB = (std::min)(dB, 20);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setadpcmvolume(Volume);
}

/// <summary>
/// Sets the Rhythm sound source master volume, in dB.
/// </summary>
void OPNA::SetRhythmVolume(int dB)
{
    dB = (std::min)(dB, 20);

    _MasterVolume = -(dB * 2 / 3);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setrhythmvolume(Volume);
}

/// <summary>
/// Sets the volume of the specified instrument, in dB.
/// </summary>
void OPNA::SetInstrumentVolume(int index, int dB)
{
    dB = (std::min)(dB, 20);

    _Instrument[index].Volume = -(dB * 2 / 3);
}
#pragma endregion

#pragma region(Registers)
/// <summary>
/// Sets the value of a register.
/// </summary>
void OPNA::SetReg(uint32_t addr, uint32_t value)
{
    if ((0x10 <= addr) && (addr <= 0x1F) && !_HasADPCMROM)
    {
        // Use PPS WAV files to play percussion.
        switch (addr)
        {
            case 0x10: // DM / KEYON
                if (!(value & 0x80)) // Key On
                {
                    _InstrumentMask |= value & 0x3F;

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
                _InstrumentTL = (int8_t) (~value & 0x3F);
                break;

            case 0x18: // Bass Drum
            case 0x19: // Snare Drum
            case 0x1a: // Top Cymbal
            case 0x1b: // Hihat
            case 0x1c: // Tom-tom
            case 0x1d: // Rim shot
                _Instrument[addr & 7].Pan   = (value >> 6) & 0x03;
                _Instrument[addr & 7].Level = (int8_t) (~value & 0x1F);
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
#pragma endregion

#pragma region(Timer)
/// <summary>
/// Timer processing
/// </summary>
bool OPNA::Count(uint32_t tickCount)
{
    bool result = false;

    if (reg27 & 1)
    {
        if (timer_count[0] > 0)
            timer_count[0] -= ((emulated_time) tickCount << (48 - 6)) / (1000000 >> 6);
    }

    if (reg27 & 2)
    {
        if (timer_count[1] > 0)
            timer_count[1] -= ((emulated_time) tickCount << (48 - 6)) / (1000000 >> 6);
    }

    for (int i = 0; i < _countof(timer_count); ++i)
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
/// Gets the next timer tick.
/// </summary>
uint32_t OPNA::GetNextTick()
{
    if (timer_count[0] == 0 && timer_count[1] == 0)
        return 0;

    emulated_time result = INT64_MAX;

    for (int i = 0; i < _countof(timer_count); ++i)
    {
        if (timer_count[i] > 0)
            result = (std::min)(result, timer_count[i]);
    }

    return (uint32_t) ((result + ((emulated_time) 1 << 48) / 1000000) * (1000000 >> 6) >> (48 - 6));
}
#pragma endregion

#pragma region(Sample Mixing)
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
        MixRhythmSamples(sampleData, sampleCount);
}

/// <summary>
/// Mixes the rhythm instrument samples with the existing synthesized samples.
/// </summary>
void OPNA::MixRhythmSamples(Sample * sampleData, size_t sampleCount) noexcept
{
    if (!((_InstrumentMask & 0x3F) && _Instrument[0].Samples && (_MasterVolume < 128)))
        return;

    Sample * SampleDataEnd = sampleData + (sampleCount * 2);

    for (size_t i = 0; i < _countof(_Instrument); ++i)
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
                int32_t Sample = (Ins.Samples[Ins.Pos / 1024] * Vol) >> 12;

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
void OPNA::StoreSample(Sample & sampleData, int32_t sampleValue)
{
    if constexpr(sizeof(Sample) == 2)
        sampleData = (Sample) Limit(sampleData + sampleValue, 0x7fff, -0x8000);
    else
        sampleData += sampleValue;
}
#pragma endregion

#pragma region(Rhythm Instruments)
/// <summary>
/// Loads the samples for the rhythm instruments.
/// </summary>
bool OPNA::LoadInstruments(const WCHAR * directoryPath)
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
 
    static const WCHAR * InstrumentName[_countof(_Instrument)] =
    {
        L"bd", L"sd", L"top", L"hh", L"tom", L"rim",
    };

    DeleteInstruments(); 

    for (int i = 0; i < _countof(_Instrument); ++i)
    {
        FilePath[0] = '\0';

        WCHAR FileName[_MAX_PATH] = { 0 };

        ::StringCbPrintfW(FileName, _countof(FileName), L"2608_%s.wav", InstrumentName[i]);

        WAVEReader & wr = _Instrument[i].Wave;

        {
            CombinePath(FilePath, _countof(FilePath), directoryPath, FileName);

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
        _Instrument[i].Step    = wr.SampleRate() * 1024 / _OutputFrequency;
        _Instrument[i].Pos     = 0U;

        _InstrumentCounter++;
    }

    if (_InstrumentCounter != _countof(_Instrument))
    {
        DeleteInstruments(); 

        return false;
    }

    return true;
}

/// <summary>
/// Deletes the rhythm instrument samples.
/// </summary>
void OPNA::DeleteInstruments() noexcept
{
    for (int i = 0; i < _countof(_Instrument); ++i)
    {
        _Instrument[i].Wave.Reset();
        _Instrument[i].Samples = nullptr;
        _Instrument[i].Pos = ~0U;
    }

    _InstrumentCounter = 0;
}
#pragma endregion

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

// Writes data to the ADPCM-A buffer.
void OPNA::write_data(ymfm::access_class type, uint32_t base, uint32_t length, uint8_t const * src)
{
    uint32_t end = base + length;

    if (end > _Data[type].size())
        _Data[type].resize(end);

    ::memcpy(&_Data[type][base], src, length);
}

// Reads data from the ADPCM-A buffer (Callback).
uint8_t OPNA::ymfm_external_read(ymfm::access_class type, uint32_t offset)
{
    if (!_HasADPCMROM && (type == ymfm::ACCESS_ADPCM_A))
        return 0;

    auto & data = _Data[type];

    return (uint8_t) ((offset < data.size()) ? data[offset] : 0U);
}

// Writes data from the ADPCM-A buffer (Callback).
void OPNA::ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data)
{
    write_data(type, address, 1, &data);
}

// Clears the time (Callback).
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

// Sets the timer (Callback).
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
