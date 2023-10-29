
/** $VER: Configuration.h (2023.10.18) **/

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "framework.h"

#include "State.h"

#include <sdk/cfg_var.h>

enum PlaybackModes
{
    LoopNever,
    Loop,
    LoopWithFadeOut,
    LoopForever
};

#define DefaultSamplesPath          "."

#define DefaultPlaybackMode         PlaybackModes::LoopNever
#define DefaultLoopCount            2
#define DefaultFadeOutDuration      3000

#define DefaultSynthesisRate        FREQUENCY_55_5K

#define DefaultUsePPS               false
#define DefaultUseSSG            false

extern cfg_string CfgSamplesPath;

extern cfg_uint CfgPlaybackMode;
extern cfg_uint CfgLoopCount;
extern cfg_uint CfgFadeOutDuration;

extern cfg_uint CfgSynthesisRate;

extern cfg_bool CfgUsePPS;
extern cfg_bool CfgUseSSG;
