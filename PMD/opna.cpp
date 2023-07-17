
// Based on PMDWin code by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <Windows.h>
#include <strsafe.h>

#include "OPNA.h"

OPNA::OPNA(File * file) :
    _File(file),

    _Chip(*this),
    _Output(),
    _TickCount(0U),

    _ClockSpeed(DEFAULT_CLOCK),
    _SynthesisRate(8000U),

    _Step(0),
    _Pos(0),

    _Rhythm{},
    _RhythmTotalVolume(0),
    rhythmtl(0),
    rhythmkey(0),
    _HasADPCMROM(false),

    output_step(0x1000000000000ull / _SynthesisRate),
    output_pos(0),
    timer_period{},
    timer_count{},
    reg27(0U)
{
    // Create the table.
    for (int i = -FM_TLPOS; i < FM_TLENTS; i++)
        tltable[i + FM_TLPOS] = uint32_t(65536. * pow(2.0, i * -16. / (int) FM_TLENTS)) - 1;
}

OPNA::~OPNA()
{
    for (int i = 0; i < 6; i++)
        delete[] _Rhythm[i].Sample;
}

bool OPNA::Init(uint32_t clock, uint32_t synthesisRate, bool useInterpolation, const WCHAR * directoryPath)
{
    if (!SetRate(clock, synthesisRate, useInterpolation))
        return false;

    _Step = 0x1000000000000ull / GetSampleRate();
    _Pos  = 0;
    _TickCount = 0U;

    _Chip.reset();

    SetVolumeFM(0);
    SetVolumePSG(0);
    SetVolumeADPCM(0);
    SetVolumeRhythmTotal(0);

    for (int i = 0; i < 6; i++)
        SetVolumeRhythm(0, 0);

    LoadRhythmSamples(directoryPath);

    return true;
}

/// <summary>
/// Sets the synthesis rate.
/// </summary>
bool OPNA::SetRate(uint32_t clockSpeed, uint32_t rate, bool)
{
    _ClockSpeed = clockSpeed;

    SetRate(rate);

    return true;
}

/// <summary>
/// Sets the synthesis rate.
/// </summary>
bool OPNA::SetRate(uint32_t synthesisRate)
{
    _SynthesisRate = synthesisRate;
    output_step = 0x1000000000000ull / _SynthesisRate;

    return true;
}

/// <summary>
/// Loads the rythm samples.
/// </summary>
bool OPNA::LoadRhythmSamples(const WCHAR * directoryPath)
{
    _HasADPCMROM = false;

    WCHAR FilePath[_MAX_PATH] = { 0 };

    CombinePath(FilePath, _countof(FilePath), directoryPath, L"ym2608_adpcm_rom.bin");

    int64_t FileSize = _File->GetFileSize(FilePath);

    if (FileSize > 0 && _File->Open(FilePath))
    {
        std::vector<uint8_t> temp(FileSize);

        _File->Read(temp.data(), FileSize);
        _File->Close();

        write_data(ymfm::ACCESS_ADPCM_A, 0, (uint32_t) FileSize, temp.data());

        _HasADPCMROM = true;
    }
    else
    {
        static const WCHAR * InstrumentName[6] =
        {
            L"bd", L"sd", L"top", L"hh", L"tom", L"rim",
        };

        int i;

        for (i = 0; i < 6; i++)
            _Rhythm[i].Pos = ~0U;

        for (i = 0; i < 6; i++)
        {
            FilePath[0] = '\0';

            ::StringCbPrintfW(FilePath, _countof(FilePath), L"%s2608_%s.wav", directoryPath, InstrumentName[i]);

            if (!_File->Open(FilePath))
            {
                if (i != 5)
                    break;

                CombinePath(FilePath, _countof(FilePath), directoryPath, L"2608_rym.wav");

                if (!_File->Open(FilePath))
                    break;
            }

            struct
            {
                uint32_t chunksize;
                uint16_t tag;
                uint16_t nch;
                uint32_t rate;
                uint32_t avgbytes;
                uint16_t align;
                uint16_t bps;
            } whdr;

            _File->Seek(0x10, File::SeekBegin);

            _File->Read(&whdr.chunksize, sizeof(whdr.chunksize));
            _File->Read(&whdr.tag, sizeof(whdr.tag));
            _File->Read(&whdr.nch, sizeof(whdr.nch));
            _File->Read(&whdr.rate, sizeof(whdr.rate));
            _File->Read(&whdr.avgbytes, sizeof(whdr.avgbytes));
            _File->Read(&whdr.align, sizeof(whdr.align));
            _File->Read(&whdr.bps, sizeof(whdr.bps));

            uint8_t subchunkname[4];

            uint32_t SampleCount = 0;

            do
            {
                _File->Seek(SampleCount, File::SeekCurrent);
                _File->Read(&subchunkname, 4);
                _File->Read(&SampleCount, 4);
            }
            while (::memcmp("data", subchunkname, 4));

            SampleCount /= 2;

            if (SampleCount >= 0x100000 || whdr.tag != 1 || whdr.nch != 1)
                break;

            SampleCount = (uint32_t) (std::max)((int32_t) SampleCount, (int32_t) ((1 << 31) / 1024));

            delete _Rhythm[i].Sample;

            _Rhythm[i].Sample = new int16_t[SampleCount];

            if (_Rhythm[i].Sample == nullptr)
                break;

            _File->Read(_Rhythm[i].Sample, SampleCount * 2);

            _Rhythm[i].Rate = whdr.rate;
            _Rhythm[i].Step = _Rhythm[i].Rate * 1024 / _SynthesisRate;
            _Rhythm[i].Pos =
            _Rhythm[i].Size = SampleCount * 1024;

            _File->Close();
        }

        if (i != 6)
        {
            for (i = 0; i < 6; i++)
            {
                delete[] _Rhythm[i].Sample;
                _Rhythm[i].Sample = nullptr;
            }

            return false;
        }
    }

    return true;
}

/// <summary>
/// Sets the FM volume
/// </summary>
void OPNA::SetVolumeFM(int dB)
{
    dB = (std::min)(dB, 20);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setfmvolume(Volume);
}

void OPNA::SetVolumePSG(int dB)
{
    dB = (std::min)(dB, 20);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setpsgvolume(Volume);
}

void OPNA::SetVolumeADPCM(int dB)
{
    dB = (std::min)(dB, 20);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setadpcmvolume(Volume);
}

void OPNA::SetVolumeRhythmTotal(int dB)
{
    dB = (std::min)(dB, 20);

    _RhythmTotalVolume = -(dB * 2 / 3);

    int32_t Volume = (dB > -192) ? int(65536.0 * ::pow(10.0, dB / 40.0)) : 0;

    _Chip.setrhythmvolume(Volume);
}

void OPNA::SetVolumeRhythm(int index, int dB)
{
    dB = (std::min)(dB, 20);

    _Rhythm[index].Volume = -(dB * 2 / 3);
}

// Set data in register array
void OPNA::SetReg(uint32_t addr, uint32_t data)
{
    if ((addr >= 0x10) && (addr <= 0x1f) && !_HasADPCMROM)
    {
        // Use WAV files to play percussion.
        switch (addr)
        {
            case 0x10: // DM / KEYON
                if (!(data & 0x80)) // Key On
                {
                    rhythmkey |= data & 0x3f;

                    if (data & 0x01) _Rhythm[0].Pos = 0;
                    if (data & 0x02) _Rhythm[1].Pos = 0;
                    if (data & 0x04) _Rhythm[2].Pos = 0;
                    if (data & 0x08) _Rhythm[3].Pos = 0;
                    if (data & 0x10) _Rhythm[4].Pos = 0;
                    if (data & 0x20) _Rhythm[5].Pos = 0;
                }
                else
                    rhythmkey &= ~data; // Dump
                break;

            case 0x11:
                rhythmtl = ~data & 63;
                break;

            case 0x18: // Bass Drum
            case 0x19: // Snare Drum
            case 0x1a: // Top Cymbal
            case 0x1b: // Hihat
            case 0x1c: // Tom-tom
            case 0x1d: // Rim shot
                _Rhythm[addr & 7].Pan   = (data >> 6) & 3;
                _Rhythm[addr & 7].Level = ~data & 31;
                break;
        }
    }
    else
    {
        uint32_t addr1 = 0 + 2 * ((addr >> 8) & 3);
        uint8_t data1 = addr & 0xff;
        uint32_t addr2 = addr1 + 1;
        uint8_t data2 = data;

        // write to the chip
        if (addr1 != 0xffff)
        {
            _Chip.write(addr1, data1);
            _Chip.write(addr2, data2);
        }
    }
}

// Get register
uint32_t OPNA::GetReg(uint32_t addr)
{
    uint32_t addr1 = 0 + 2 * ((addr >> 8) & 3);
    uint8_t data1 = addr & 0xff;

    uint32_t addr2 = addr1 + 1;
    uint8_t result = 0;

    // write to the chip
    if (addr1 != 0xffff)
    {
        _Chip.write(addr1, data1);

        result = _Chip.read(addr2);
    }
    else
    {
        result = 1;
    }

    return result;
}

// Timer time processing
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

            m_engine->engine_timer_expired(i);
        }
    }

    return result;
}

// Find the time until the next timer fires.
uint32_t OPNA::GetNextEvent()
{
    if (timer_count[0] == 0 && timer_count[1] == 0)
        return 0;

    emulated_time result = INT64_MAX;

    for (int i = 0; i < sizeof(timer_count) / sizeof(emulated_time); i++)
    {
        if (timer_count[i] > 0)
            result = (std::min) (result, timer_count[i]);
    }

    return (result + ((emulated_time) 1 << 48) / 1000000) * (1000000 >> 6) >> (48 - 6);
}

/// <summary>
/// Synthesize a rhythm sample.
/// </summary>
void OPNA::Mix(Sample * sampleData, int sampleCount)
{
    Sample * SampleData = sampleData;
    int SampleCount = sampleCount;

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
/// Synthesize a rhythm sample using WAV.
/// </summary>
void OPNA::RhythmMix(Sample * sampleData, uint32_t sampleCount)
{
    if ((_RhythmTotalVolume < 128) && _Rhythm[0].Sample && (rhythmkey & 0x3f))
    {
        Sample * SampleDataEnd = sampleData + (sampleCount * 2);

        for (int i = 0; i < 6; i++)
        {
            Rhythm & r = _Rhythm[i];

            if ((rhythmkey & (1 << i)) /* //@ && r.level < 128 */)
            {
                int dB = Limit(rhythmtl + _RhythmTotalVolume + r.Level + r.Volume, 127, -31);

                int Vol   = tltable[FM_TLPOS + (dB << (FM_TLBITS - 7))] >> 4;
                int MaskL = -((r.Pan >> 1) & 1);
                int MaskR = - (r.Pan       & 1);

                for (Sample * SampleData = sampleData; (SampleData < SampleDataEnd) && (r.Pos < r.Size); SampleData += 2)
                {
                    int Sample = (r.Sample[r.Pos / 1024] * Vol) >> 12;

                    r.Pos += r.Step;

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
void OPNA::StoreSample(Sample & dest, ISample data)
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
//  uint32_t addr1 = 0xffff, addr2 = 0xffff;
//  uint8_t data1 = 0, data2 = 0;

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
    if (!_HasADPCMROM && type == ymfm::ACCESS_ADPCM_A)
        return 0;

    auto & data = m_data[type];

    return (offset < data.size()) ? data[offset] : 0;
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

    /* //@ とりあえず無効化
    if (reg27 & 0x10) {
        timer_count[0] = timer_period[0];
        reg27 &= ~0x10;
    }
    if (reg27 & 0x20) {
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
