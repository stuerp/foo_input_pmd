
/** $VER: Driver.cpp (2025.10.01) Driver (Based on PMDWin code by C60 / Masahiro Kajihara) **/

#include <pch.h>

#include "PMD.h"

void pmd_driver_t::DriverMain()
{
    int i;

    _Driver._LoopCheck = 0x03;

    if (_State.x68_flg == 0x00)
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

    _Driver._FMSelector = 0x000;

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
            PPZ8Main(&_PPZChannels[i]);
        }
    }

    if (_Driver._LoopCheck == 0x00)
        return;

    for (i = 0; i < MaxFMChannels; ++i)
    {
        if (_FMChannels[i]._LoopCheck != 0x03)
            _FMChannels[i]._LoopCheck = 0x00;
    }

    for (i = 0; i < MaxSSGChannels; ++i)
    {
        if (_SSGChannels[i]._LoopCheck != 0x03)
            _SSGChannels[i]._LoopCheck = 0x00;

        if (_FMExtensionChannels[i]._LoopCheck != 0x03)
            _FMExtensionChannels[i]._LoopCheck = 0x00;
    }

    if (_ADPCMChannel._LoopCheck != 0x03)
        _ADPCMChannel._LoopCheck = 0x00;

    if (_RhythmChannel._LoopCheck != 0x03)
        _RhythmChannel._LoopCheck = 0x00;

    if (_EffectChannel._LoopCheck != 0x03)
        _EffectChannel._LoopCheck = 0x00;

    for (i = 0; i < MaxPPZ8Channels; ++i)
    {
        if (_PPZChannels[i]._LoopCheck != 0x03)
            _PPZChannels[i]._LoopCheck = 0x00;
    }

    if (_Driver._LoopCheck != 0x03)
    {
        _State.LoopCount++;

        if (_State.LoopCount == 0xFF)
            _State.LoopCount = 1;
    }
    else
        _State.LoopCount = -1;
}

void pmd_driver_t::DriverStart()
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

    InitializeOPNA();
    InitializeTimers();

    _IsPlaying = true;
}

void pmd_driver_t::DriverStop()
{
    _Driver._Flags &= ~DriverStopRequested;

    _IsPlaying = false;

    _State.LoopCount = -1;
    _State.FadeOutSpeed = 0;
    _State._FadeOutVolume = 0xFF;

    Mute();
}
