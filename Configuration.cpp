
/** $VER: Configuration.cpp (2023.09.27) **/

#include "pch.h"

#include "Configuration.h"

#pragma hdrstop

static const GUID CfgSamplesPathGUID = {0xce37a2ee,0x7527,0x4b30,{0x8b,0x42,0x7f,0x37,0xa9,0x1e,0x02,0xec}}; // {ce37a2ee-7527-4b30-8b42-7f37a91e02ec}

static const GUID CfgPlaybackModeGUID = {0x156f3382,0xe0b5,0x462c,{0xb6,0x56,0x16,0x95,0xc6,0x39,0x01,0x54}}; // {156f3382-e0b5-462c-b656-1695c6390154}
static const GUID CfgLoopCountGUID = {0xc85c30a0,0xc18d,0x4033,{0x82,0x35,0x1b,0xe7,0x1c,0x3e,0xaa,0x17}}; // {c85c30a0-c18d-4033-8235-1be71c3eaa17}
static const GUID CfgFadeOutDurationGUID = {0xdd824b3e,0x65ce,0x451d,{0x8e,0xbc,0xcd,0x6e,0x70,0xd4,0xab,0x5c}}; // {dd824b3e-65ce-451d-8ebc-cd6e70d4ab5c}

static const GUID CfgSynthesisRateGUID = {0x9dfe9ae3,0xd80c,0x48f8,{0xa9,0xf5,0x81,0x9d,0xf7,0x55,0xad,0xdc}}; // {9dfe9ae3-d80c-48f8-a9f5-819df755addc}

static const GUID CfgUsePPSGUID = {0x21f735d9,0x60da,0x4d0e,{0xbb,0x1b,0x53,0x70,0x01,0xf4,0x1e,0x89}}; // {21f735d9-60da-4d0e-bb1b-537001f41e89}
static const GUID CfgUseSSGGUID = {0xd1995716,0xc1db,0x4ee4,{0xad,0xdb,0xd0,0xe0,0xd3,0x35,0x00,0x48}}; // {d1995716-c1db-4ee4-addb-d0e0d3350048}

cfg_string CfgSamplesPath(CfgSamplesPathGUID, DefaultSamplesPath);

cfg_uint CfgPlaybackMode(CfgPlaybackModeGUID, DefaultPlaybackMode);
cfg_uint CfgLoopCount(CfgLoopCountGUID, DefaultLoopCount);
cfg_uint CfgFadeOutDuration(CfgFadeOutDurationGUID, DefaultFadeOutDuration);

cfg_uint CfgSynthesisRate(CfgSynthesisRateGUID, DefaultSynthesisRate);

cfg_bool CfgUsePPS(CfgUsePPSGUID, DefaultUsePPS);
cfg_bool CfgUseSSG(CfgUseSSGGUID, DefaultUseSSG);
