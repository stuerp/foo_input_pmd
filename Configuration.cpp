
/** $VER: Configuration.cpp (2023.07.15) **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "framework.h"

#include "Configuration.h"

#pragma hdrstop

static const GUID CfgSamplesPathGUID = {0xce37a2ee,0x7527,0x4b30,{0x8b,0x42,0x7f,0x37,0xa9,0x1e,0x02,0xec}}; // {ce37a2ee-7527-4b30-8b42-7f37a91e02ec}
static const GUID CfgMaxLoopNumberGUID = {0xc85c30a0,0xc18d,0x4033,{0x82,0x35,0x1b,0xe7,0x1c,0x3e,0xaa,0x17}}; // {c85c30a0-c18d-4033-8235-1be71c3eaa17}
static const GUID CfgFadeOutDurationGUID = {0xdd824b3e,0x65ce,0x451d,{0x8e,0xbc,0xcd,0x6e,0x70,0xd4,0xab,0x5c}}; // {dd824b3e-65ce-451d-8ebc-cd6e70d4ab5c}

cfg_string CfgSamplesPath(CfgSamplesPathGUID, DefaultSamplesPath);
cfg_uint CfgMaxLoopNumber(CfgMaxLoopNumberGUID, DefaultMaxLoopNumber);
cfg_uint CfgFadeOutDuration(CfgFadeOutDurationGUID, DefaultFadeOutDuration);
