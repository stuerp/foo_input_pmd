
/** $VER: Configuration.h (2023.07.15) **/

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "framework.h"

#include <sdk/cfg_var.h>

#define DefaultSamplesPath      "."
#define DefaultMaxLoopNumber    2
#define DefaultFadeOutDuration  10 * 1000

extern cfg_string CfgSamplesPath;
extern cfg_uint CfgMaxLoopNumber;
extern cfg_uint CfgFadeOutDuration;
