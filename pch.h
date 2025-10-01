
/** $VER: pch.h (2025.10.01) P. Stuer **/

#pragma once

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4100 4625 4626 4710 4711 4738 5045 ALL_CPPCORECHECK_WARNINGS)

#include <SDKDDKVer.h>

#define NOMINMAX

#include <WinSock2.h>
#include <Windows.h>
#include <winnls.h>

#define FOOBAR2000_TARGET_VERSION 82

#include <sdk/foobar2000-lite.h>

#include <ctype.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>

#include <cmath>
#include <algorithm>
