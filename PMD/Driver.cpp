
/** $VER: Driver.cpp (2025.10.01) Driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#include <pch.h>

#include "PMD.h"

void PMD::DriverMain()
{
    int i;

    _Driver._LoopWork = 3;

    if (_State.x68_flg == 0)
    {
        for (i = 0; i < 3; ++i)
        {
            _Driver._CurrentChannel = i + 1;
            SSGMain(&_SSGChannels[i]);
        }
    }

    _Driver._FMSelector = 0x100;

    // Process FM channel 4, 5 and 6.
    for (i = 0; i < 3; ++i)
    {
        _Driver._CurrentChannel = i + 1;
        FMMain(&_FMChannels[i + 3]);
    }

    _Driver._FMSelector = 0;

    // Process FM channel 1, 2 and 3.
    for (i = 0; i < 3; ++i)
    {
        _Driver._CurrentChannel = i + 1;

        FMMain(&_FMChannels[i]);
    }

    // Process FM extension channel 1, 2 and 3.
    for (i = 0; i < 3; ++i)
    {
        _Driver._CurrentChannel = 3;
        FMMain(&_FMExtensionChannels[i]);
    }

    if (_State.x68_flg == 0x00)
    {
        RhythmMain(&_RhythmChannel);

        if (_IsUsingP86)
            P86Main(&_ADPCMChannel);
        else
            ADPCMMain(&_ADPCMChannel);
    }

    if (_State.x68_flg != 0xFF)
    {
        for (i = 0; i < 8; ++i)
        {
            _Driver._CurrentChannel = i;
            PPZMain(&_PPZChannels[i]);
        }
    }

    if (_Driver._LoopWork == 0)
        return;

    for (i = 0; i < MaxFMChannels; ++i)
    {
        if (_FMChannels[i].loopcheck != 3)
            _FMChannels[i].loopcheck = 0;
    }

    for (i = 0; i < MaxSSGChannels; ++i)
    {
        if (_SSGChannels[i].loopcheck != 3)
            _SSGChannels[i].loopcheck = 0;

        if (_FMExtensionChannels[i].loopcheck != 3)
            _FMExtensionChannels[i].loopcheck = 0;
    }

    if (_ADPCMChannel.loopcheck != 3)
        _ADPCMChannel.loopcheck = 0;

    if (_RhythmChannel.loopcheck != 3)
        _RhythmChannel.loopcheck = 0;

    if (_EffectChannel.loopcheck != 3)
        _EffectChannel.loopcheck = 0;

    for (i = 0; i < MaxPPZChannels; ++i)
    {
        if (_PPZChannels[i].loopcheck != 3)
            _PPZChannels[i].loopcheck = 0;
    }

    if (_Driver._LoopWork != 3)
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

    _OPNAW->SetReg(0x27, 0x00); // Reset timer A and B.

    _Driver._Flags &= ~DriverStartRequested;

    DriverStop();

    _FramePtr = _SrcFrames;
    _FramesToDo = 0;
    _Position = 0;

    InitializeState();
    InitializeChannels();

    InitializeOPN();
    InitializeInterrupt();

    _IsPlaying = true;
}

void PMD::DriverStop()
{
    _Driver._Flags &= ~DriverStopRequested;

    _IsPlaying = false;

    _State.LoopCount = -1;
    _State.FadeOutSpeed = 0;
    _State._FadeOutVolume = 0xFF;

    Mute();
}
