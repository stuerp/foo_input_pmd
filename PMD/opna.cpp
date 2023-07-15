
// Based on PMDWin code by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include <Windows.h>
#include <WCHAR.h>

#include <algorithm>
#include "opna.h"

#define LOG_WRITES (0)

OPNA::OPNA(IFileIO * fileIO) :
    m_chip(*this),
    m_clock(DEFAULT_CLOCK),
    m_clocks(0),
    m_output(),
    m_step(0),
    m_pos(0),
    rate(8000),
    _HasADPCMROM(false),
    _Rhythm{},
    rhythmtvol(0),
    rhythmtl(0),
    rhythmkey(0),
    output_step(0x1000000000000ull / rate),
    output_pos(0),
    timer_period{},
    timer_count{},
    reg27(0)
{
    _FileIO = fileIO;

    // Create the table.
    for (int i = -FM_TLPOS; i < FM_TLENTS; i++)
        tltable[i + FM_TLPOS] = uint32_t(65536. * pow(2.0, i * -16. / (int) FM_TLENTS)) - 1;
}

OPNA::~OPNA()
{
    for (int i = 0; i < 6; i++)
        delete[] _Rhythm[i].sample;
}

// File Stream 設定
void OPNA::setfileio(IFileIO * pfileio)
{
    _FileIO = pfileio;
}

// 初期化
bool OPNA::Init(uint32_t c, uint32_t r, bool ip, const WCHAR * path)
{
    m_clock = c;
    rate = r;
    m_clocks = 0;
    m_step = 0x1000000000000ull / m_chip.sample_rate(m_clock);
    m_pos = 0;

    LoadRhythmSamples(path);

    if (!SetRate(c, r, ip))
        return false;

    m_chip.reset();

    SetVolumeFM(0);
    SetVolumePSG(0);
    SetVolumeADPCM(0);
    SetVolumeRhythmTotal(0);
    for (int i = 0; i < 6; i++)
        SetVolumeRhythm(0, 0);

    return true;
}

// サンプリングレート変更
bool OPNA::SetRate(uint32_t r)
{
    rate = r;
    output_step = 0x1000000000000ull / rate;
    return true;
}

bool OPNA::SetRate(uint32_t c, uint32_t r, bool)
{
    SetRate(r);

    m_clock = c;
    rate = r;

    return true;
}

/// <summary>
/// Loads the rythm samples.
/// </summary>
bool OPNA::LoadRhythmSamples(const WCHAR * path)
{
    _HasADPCMROM = false;

    FilePath filepath;
    WCHAR buf[_MAX_PATH] = { 0 };

    filepath.Makepath_dir_filename(buf, path, L"ym2608_adpcm_rom.bin");

    int64_t FileSize = _FileIO->GetFileSize(buf);

    if (FileSize > 0 && _FileIO->Open(buf, FileIO::flags_readonly))
    {
        std::vector<uint8_t> temp(FileSize);

        _FileIO->Read(temp.data(), FileSize);
        _FileIO->Close();

        write_data(ymfm::ACCESS_ADPCM_A, 0, (uint32_t) FileSize, temp.data());

        _HasADPCMROM = true;
    }
    else
    {
        static const WCHAR * InstrumentName[6] =
        {
            L"BD", L"SD", L"TOP", L"HH", L"TOM", L"RIM",
        };

        int i;

        for (i = 0; i < 6; i++)
            _Rhythm[i].pos = ~0;

        for (i = 0; i < 6; i++)
        {
            FilePath filepath;
            uint32_t fsize = 0;
            WCHAR buf[_MAX_PATH] = { 0 };

            if (path)
                filepath.Strncpy(buf, path, _MAX_PATH);

            filepath.Strncat(buf, L"2608_", _MAX_PATH);
            filepath.Strncat(buf, InstrumentName[i], _MAX_PATH);
            filepath.Strncat(buf, L".WAV", _MAX_PATH);

            if (!_FileIO->Open(buf, FileIO::flags_readonly))
            {
                if (i != 5)
                    break;

                if (path)
                    filepath.Strncpy(buf, path, _MAX_PATH);

                filepath.Strncpy(buf, L"2608_RYM.WAV", _MAX_PATH);

                if (!_FileIO->Open(buf, FileIO::flags_readonly))
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

            _FileIO->Seek(0x10, FileIO::seekmethod_begin);
            _FileIO->Read(&whdr.chunksize, sizeof(whdr.chunksize));
            _FileIO->Read(&whdr.tag, sizeof(whdr.tag));
            _FileIO->Read(&whdr.nch, sizeof(whdr.nch));
            _FileIO->Read(&whdr.rate, sizeof(whdr.rate));
            _FileIO->Read(&whdr.avgbytes, sizeof(whdr.avgbytes));
            _FileIO->Read(&whdr.align, sizeof(whdr.align));
            _FileIO->Read(&whdr.bps, sizeof(whdr.bps));

            uint8_t subchunkname[4];

            do
            {
                _FileIO->Seek(fsize, FileIO::seekmethod_current);
                _FileIO->Read(&subchunkname, 4);
                _FileIO->Read(&fsize, 4);
            }
            while (::memcmp("data", subchunkname, 4));

            fsize /= 2;

            if (fsize >= 0x100000 || whdr.tag != 1 || whdr.nch != 1)
                break;

            fsize = (uint32_t) (std::max)((int32_t) fsize, (int32_t) ((1 << 31) / 1024));

            delete _Rhythm[i].sample;
            _Rhythm[i].sample = new int16_t[fsize];
            if (!_Rhythm[i].sample)
                break;

            _FileIO->Read(_Rhythm[i].sample, fsize * 2);

            _Rhythm[i].rate = whdr.rate;
            _Rhythm[i].step = _Rhythm[i].rate * 1024 / rate;
            _Rhythm[i].pos = _Rhythm[i].size = fsize * 1024;

            _FileIO->Close();
        }

        if (i != 6)
        {
            for (i = 0; i < 6; i++)
            {
                delete[] _Rhythm[i].sample;
                _Rhythm[i].sample = 0;
            }

            return false;
        }
    }

    return true;
}

// リセット
void OPNA::Reset()
{
    m_chip.reset();
}

// 音量設定
void OPNA::SetVolumeFM(int db)
{
    int32_t volume;

    db = (std::min) (db, 20);
    if (db > -192)
        volume = int(65536.0 * pow(10.0, db / 40.0));
    else
        volume = 0;

    m_chip.setfmvolume(volume);
}

void OPNA::SetVolumePSG(int db)
{
    int32_t volume;

    db = (std::min) (db, 20);
    if (db > -192)
        volume = int(65536.0 * pow(10.0, db / 40.0));
    else
        volume = 0;

    m_chip.setpsgvolume(volume);
}

void OPNA::SetVolumeADPCM(int db)
{
    int32_t volume;

    db = (std::min) (db, 20);
    if (db > -192)
        volume = int(65536.0 * pow(10.0, db / 40.0));
    else
        volume = 0;

    m_chip.setadpcmvolume(volume);
}

void OPNA::SetVolumeRhythmTotal(int db)
{
    int32_t volume;

    db = (std::min) (db, 20);
    rhythmtvol = -(db * 2 / 3);

    if (db > -192)
        volume = int(65536.0 * pow(10.0, db / 40.0));
    else
        volume = 0;

    m_chip.setrhythmvolume(volume);
}

void OPNA::SetVolumeRhythm(int index, int db)
{
    db = (std::min) (db, 20);
    _Rhythm[index].volume = -(db * 2 / 3);
}

// Set data in register array
void OPNA::SetReg(uint32_t addr, uint32_t data)
{
    if (addr >= 0x10 && addr <= 0x1f && !_HasADPCMROM)
    {
        // rhythmをwavで鳴らす時

        switch (addr)
        {
            case 0x10:      // DM/KEYON
                if (!(data & 0x80))  // KEY ON
                {
                    rhythmkey |= data & 0x3f;
                    if (data & 0x01) _Rhythm[0].pos = 0;
                    if (data & 0x02) _Rhythm[1].pos = 0;
                    if (data & 0x04) _Rhythm[2].pos = 0;
                    if (data & 0x08) _Rhythm[3].pos = 0;
                    if (data & 0x10) _Rhythm[4].pos = 0;
                    if (data & 0x20) _Rhythm[5].pos = 0;
                }
                else
                {          // DUMP
                    rhythmkey &= ~data;
                }
                break;

            case 0x11:
                rhythmtl = ~data & 63;
                break;

            case 0x18:     // Bass Drum
            case 0x19:    // Snare Drum
            case 0x1a:    // Top Cymbal
            case 0x1b:    // Hihat
            case 0x1c:    // Tom-tom
            case 0x1d:    // Rim shot
                _Rhythm[addr & 7].pan = (data >> 6) & 3;
                _Rhythm[addr & 7].level = ~data & 31;
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
            if (LOG_WRITES)
                printf("Write %10.5f: %03X=%02X\n", double(m_clocks) / double(m_chip.sample_rate(m_clock)), data1, data2);
            m_chip.write(addr1, data1);
            m_chip.write(addr2, data2);
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
        m_chip.write(addr1, data1);
        result = m_chip.read(addr2);
        if (LOG_WRITES)
            printf("Read  %10.5f: %03X=%02X\n", double(m_clocks) / double(m_chip.sample_rate(m_clock)), data1, result);

    }
    else
    {
        result = 1;
    }

    return result;
}

// Load the status
uint32_t OPNA::ReadStatus()
{
    return m_chip.read_status();
}

// 拡張ステータスを読み込む
uint32_t OPNA::ReadStatusEx()
{
    return m_chip.read_status_hi();
}

// タイマー時間処理
bool OPNA::Count(uint32_t us)
{
    bool result = false;

    if (reg27 & 1)
    {
        if (timer_count[0] > 0)
        {
            timer_count[0] -= ((emulated_time) us << (48 - 6)) / (1000000 >> 6);
        }
    }

    if (reg27 & 2)
    {
        if (timer_count[1] > 0)
        {
            timer_count[1] -= ((emulated_time) us << (48 - 6)) / (1000000 >> 6);
        }
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

// Synthesis
// buffer    Synthesis target
// nsamples  Number of synthetic samples
void OPNA::Mix(Sample * buffer, int nsamples)
{
    Sample * buffer2 = buffer;
    int nsamples2 = nsamples;

    while (nsamples2-- != 0)
    {
        int32_t outputs[2] = { 0, 0 };

        generate(output_pos, output_step, outputs);
        output_pos += output_step;

        *buffer2 += outputs[0];
        buffer2++;

        *buffer2 += outputs[1];
        buffer2++;
    }

    if (!_HasADPCMROM)
        RhythmMix(buffer, nsamples);
}

// generate one output sample of output
void OPNA::generate(emulated_time output_start, emulated_time output_step, int32_t * buffer)
{
    uint32_t addr1 = 0xffff, addr2 = 0xffff;
    uint8_t data1 = 0, data2 = 0;

    // generate at the appropriate sample rate
    for (; m_pos <= output_start; m_pos += m_step)
    {
        m_chip.generate(&m_output);
    }

    // add the final result to the buffer

    int32_t out0 = m_output.data[0];
    int32_t out1 = m_output.data[1 % ymfm::ym2608::OUTPUTS];
    int32_t out2 = m_output.data[2 % ymfm::ym2608::OUTPUTS];
    *buffer++ += out0 + out2;
    *buffer++ += out1 + out2;

    m_clocks++;
}

// データコピー
void OPNA::StoreSample(Sample & dest, ISample data)
{
    if (sizeof(Sample) == 2)
        dest = (Sample) Limit(dest + data, 0x7fff, -0x8000);
    else
        dest += data;
}

// リズム合成(wav)
void OPNA::RhythmMix(Sample * buffer, uint32_t count)
{
    if (rhythmtvol < 128 && _Rhythm[0].sample && (rhythmkey & 0x3f))
    {
        Sample * limit = buffer + count * 2;

        for (int i = 0; i < 6; i++)
        {
            Rhythm & r = _Rhythm[i];

            if ((rhythmkey & (1 << i)) /* //@ && r.level < 128 */)
            {
                int db = Limit(rhythmtl + rhythmtvol + r.level + r.volume, 127, -31);
                int vol = tltable[FM_TLPOS + (db << (FM_TLBITS - 7))] >> 4;
                int maskl = -((r.pan >> 1) & 1);
                int maskr = -(r.pan & 1);

                for (Sample * dest = buffer; dest < limit && r.pos < r.size; dest += 2)
                {
                    int sample = (r.sample[r.pos / 1024] * vol) >> 12;

                    r.pos += r.step;

                    StoreSample(dest[0], sample & maskl);
                    StoreSample(dest[1], sample & maskr);
                }
            }
        }
    }
}

// write data to the ADPCM-A buffer
void OPNA::write_data(ymfm::access_class type, uint32_t base, uint32_t length, uint8_t const * src)
{
    uint32_t end = base + length;
    if (end > m_data[type].size())
        m_data[type].resize(end);
    memcpy(&m_data[type][base], src, length);
}

// Get sample rate
uint32_t OPNA::sample_rate() const
{
    return m_chip.sample_rate(m_clock);
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
        timer_period[tnum] = (((emulated_time) duration_in_clocks << 43) / m_clock) << 5;
        timer_count[tnum] = timer_period[tnum];
    }
    else
    {
        timer_period[tnum] = 0;
        timer_count[tnum] = 0;
    }
}
