
/** $VER: Configuration.cpp (2023.07.15) **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "framework.h"

#include "Configuration.h"

#pragma hdrstop

static const GUID CfgSamplesPathGUID = {0xce37a2ee,0x7527,0x4b30,{0x8b,0x42,0x7f,0x37,0xa9,0x1e,0x02,0xec}}; // {ce37a2ee-7527-4b30-8b42-7f37a91e02ec}
static const GUID CfgMaxLoopNumberGUID = {0xc85c30a0,0xc18d,0x4033,{0x82,0x35,0x1b,0xe7,0x1c,0x3e,0xaa,0x17}}; // {c85c30a0-c18d-4033-8235-1be71c3eaa17}

cfg_string CfgSamplesPath(CfgSamplesPathGUID, ".");
cfg_uint CfgMaxLoopNumber(CfgMaxLoopNumberGUID, 2);
