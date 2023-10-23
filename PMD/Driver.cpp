
// PMD driver (Based on PMDWin code by C60)

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4710 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "PMD.h"

void PMD::DriverMain()
{
    int i;

    _Driver.loop_work = 3;

    if (_State.x68_flg == 0)
    {
        for (i = 0; i < 3; ++i)
        {
            _Driver.CurrentChannel = i + 1;
            SSGMain(&_SSGChannel[i]);
        }
    }

    _Driver.FMSelector = 0x100;

    for (i = 0; i < 3; ++i)
    {
        _Driver.CurrentChannel = i + 1;
        FMMain(&_FMChannel[i + 3]);
    }

    _Driver.FMSelector = 0;

    for (i = 0; i < 3; ++i)
    {
        _Driver.CurrentChannel = i + 1;
        FMMain(&_FMChannel[i]);
    }

    for (i = 0; i < 3; ++i)
    {
        _Driver.CurrentChannel = 3;
        FMMain(&_FMExtensionChannel[i]);
    }

    if (_State.x68_flg == 0x00)
    {
        RhythmMain(&_RhythmChannel);

        if (_State.IsUsingP86)
            P86Main(&_ADPCMChannel);
        else
            ADPCMMain(&_ADPCMChannel);
    }

    if (_State.x68_flg != 0xFF)
    {
        for (i = 0; i < 8; ++i)
        {
            _Driver.CurrentChannel = i;
            PPZMain(&_PPZChannel[i]);
        }
    }

    if (_Driver.loop_work == 0)
        return;

    for (i = 0; i < 6; ++i)
    {
        if (_FMChannel[i].loopcheck != 3)
            _FMChannel[i].loopcheck = 0;
    }

    for (i = 0; i < 3; ++i)
    {
        if (_SSGChannel[i].loopcheck != 3)
            _SSGChannel[i].loopcheck = 0;

        if (_FMExtensionChannel[i].loopcheck != 3)
            _FMExtensionChannel[i].loopcheck = 0;
    }

    if (_ADPCMChannel.loopcheck != 3)
        _ADPCMChannel.loopcheck = 0;

    if (_RhythmChannel.loopcheck != 3)
        _RhythmChannel.loopcheck = 0;

    if (_EffectChannel.loopcheck != 3)
        _EffectChannel.loopcheck = 0;

    for (i = 0; i < MaxPPZChannels; ++i)
    {
        if (_PPZChannel[i].loopcheck != 3)
            _PPZChannel[i].loopcheck = 0;
    }

    if (_Driver.loop_work != 3)
    {
        _State.LoopCount++;

        if (_State.LoopCount == 0xFF)
            _State.LoopCount = 1;
    }
    else
        _State.LoopCount = -1;
}

void PMD::DriverStart()
{
    // Set Timer B = 0 and Timer Reset (to match the length of the song every time)
    _State.Tempo = 0;

    SetTimerBTempo();

    _OPNAW->SetReg(0x27, 0x00); // Timer reset (both timer A and B)

    _Driver.music_flag &= 0xFE;

    DriverStop();

    _SamplePtr = _SampleSrc;
    _SamplesToDo = 0;
    _Position = 0;

    InitializeState();
    InitializeChannels();

    InitializeOPN();
    InitializeInterrupt();

    _IsPlaying = true;
}

void PMD::DriverStop()
{
    _Driver.music_flag &= 0xFD;

    _IsPlaying = false;
    _State.LoopCount = -1;
    _State.FadeOutSpeed = 0;
    _State.FadeOutVolume = 0xFF;

    Silence();
}
