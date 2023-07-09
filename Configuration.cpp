
/** $VER: Configuration.cpp (2023.07.08) **/

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "Configuration.h"

#pragma hdrstop

static const GUID CfgSamplesPathGUID = {0xce37a2ee,0x7527,0x4b30,{0x8b,0x42,0x7f,0x37,0xa9,0x1e,0x02,0xec}}; // {ce37a2ee-7527-4b30-8b42-7f37a91e02ec}

cfg_string CfgSamplesPath(CfgSamplesPathGUID, ".");
