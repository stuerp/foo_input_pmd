
// PMD driver (Based on PMDWin code by C60)

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4710 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "PMD.h"

#include "Utility.h"
#include "Table.h"

#include "OPNAW.h"
#include "PPZ.h"
#include "PPS.h"
#include "P86.h"

void PMD::EffectMain(Channel * channel, int al)
{
    if (_State.SSGEffectFlag)
        return;    //  効果音を使用しないモード

    if (_DriverState.UsePPS && (al & 0x80))
    {  // PPS を鳴らす
        if (_EffectState.effon >= 2)
            return;  // 通常効果音発音時は発声させない

        _SSGTrack[2].PartMask |= 0x02;

        _EffectState.effon = 1;        // 優先度１(ppsdrv)
        _EffectState.EffectNumber = al;      // Tone Number Setting (80H?)

        int bh = 0;
        int bl = 15;
        int ah = _EffectState.hosei_flag;

        if (ah & 1)
            bh = channel->detune % 256;    // BH = Detuneの下位 8bit

        if (ah & 2)
        {
            if (channel->volume < 15)
                bl = channel->volume;    // BL = volume値 (0?15)

            if (_State.FadeOutVolume)
                bl = (bl * (256 - _State.FadeOutVolume)) >> 8;
        }

        if (bl)
        {
            bl ^= 0x0f;
            ah = 1;
            al &= 0x7f;

            _PPS->Play(al, bh, bl);
        }
    }
    else
    {
        _EffectState.EffectNumber = al;

        if (_EffectState.effon <= SSGEffects[al].Priority)
        {
            if (_DriverState.UsePPS)
                _PPS->Stop();

            _SSGTrack[2].PartMask |= 0x02;

            efffor(SSGEffects[al].Data);

            _EffectState.effon = SSGEffects[al].Priority; // Set priority
        }
    }
}

//  SSG Drums & Sound Effects Routine (From WT298)
//  AL to sound effect No. Enter and CALL
//  If you have ppsdrv, run it
void PMD::effgo(Channel * channel, int al)
{
    if (_DriverState.UsePPS)
    {
        al |= 0x80;

        if (_EffectState.last_shot_data == al)
            _PPS->Stop();
        else
            _EffectState.last_shot_data = al;
    }

    _EffectState.hosei_flag = 3; // With pitch/volume correction (K part)

    EffectMain(channel, al);
}

// Command "n": SSG Sound Effect Playback
void PMD::eff_on2(Channel * channel, int al)
{
    _EffectState.hosei_flag = 1;        //  Only the pitch is corrected (n command)

    EffectMain(channel, al);
}

void PMD::effplay()
{
    if (--_EffectState.ToneSweepCounter)
        EffectSweep();
    else
        efffor(_EffectState.Address);
}

void PMD::efffor(const int * si)
{
    int al = *si++;

    if (al != -1)
    {
        _EffectState.ToneSweepCounter = al;

        int cl = *si;

        _OPNAW->SetReg(4, (uint32_t) (*si++)); // Set frequency

        int ch = *si;

        _OPNAW->SetReg(5, (uint32_t) (*si++)); // Set frequency

        _EffectState.ToneSweepFrequency = (ch << 8) + cl;

        _State.OldSSGNoiseFrequency = _EffectState.NoiseSweepFrequency = *si;

        _OPNAW->SetReg(6, (uint32_t) *si++); // ノイズ

        _OPNAW->SetReg(7, ((*si++ << 2) & 0x24) | (_OPNAW->GetReg(0x07) & 0xdb));

        _OPNAW->SetReg(10, (uint32_t) *si++); // ボリューム
        _OPNAW->SetReg(11, (uint32_t) *si++); // エンベロープ周波数
        _OPNAW->SetReg(12, (uint32_t) *si++);
        _OPNAW->SetReg(13, (uint32_t) *si++); // エンベロープPATTERN

        _EffectState.ToneSweepIncrement = *si++; // スイープ増分 (TONE)
        _EffectState.NoiseSweepIncrement = *si++; // スイープ増分 (NOISE)

        _EffectState.NoiseSweepCounter = _EffectState.NoiseSweepIncrement & 15;    // スイープカウント (NOISE)

        _EffectState.Address = (int *) si;
    }
    else
        EffectStop();
}

void PMD::EffectStop()
{
    if (_DriverState.UsePPS)
        _PPS->Stop();

    _OPNAW->SetReg(0x0a, 0x00);
    _OPNAW->SetReg(0x07, ((_OPNAW->GetReg(0x07)) & 0xdb) | 0x24);

    _EffectState.effon = 0;
    _EffectState.EffectNumber = -1;
}

void PMD::EffectSweep()
{
    _EffectState.ToneSweepFrequency += _EffectState.ToneSweepIncrement;

    _OPNAW->SetReg(4, (uint32_t) LOBYTE(_EffectState.ToneSweepFrequency));
    _OPNAW->SetReg(5, (uint32_t) HIBYTE(_EffectState.ToneSweepFrequency));

    if (_EffectState.NoiseSweepIncrement == 0)
        return; // No noise sweep

    if (--_EffectState.NoiseSweepCounter)
        return;

    int dl = _EffectState.NoiseSweepIncrement;

    _EffectState.NoiseSweepCounter = dl & 15;

    // used to be "dl / 16"
    // with negative value division is different from shifting right
    // division: usually truncated towards zero (mandatory since c99)
    //   same as x86 idiv
    // shift: usually arithmetic shift
    //   same as x86 sar

    _EffectState.NoiseSweepFrequency += dl >> 4;

    _OPNAW->SetReg(6, (uint32_t) _EffectState.NoiseSweepFrequency);

    _State.OldSSGNoiseFrequency = _EffectState.NoiseSweepFrequency;
}
