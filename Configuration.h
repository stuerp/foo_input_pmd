
/** $VER: Configuration.h (2023.07.17) **/

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "framework.h"

#include <sdk/cfg_var.h>

#define DefaultSamplesPath      "."

#define DefaultPlaybackMode     0
#define DefaultMaxLoopNumber    2
#define DefaultFadeOutDuration  10 * 1000

#define DefaultSynthesisRate    44100

extern cfg_string CfgSamplesPath;

extern cfg_uint CfgPlaybackMode;
extern cfg_uint CfgMaxLoopNumber;
extern cfg_uint CfgFadeOutDuration;

extern cfg_uint CfgSynthesisRate;
