
/** $VER: Tables.h (2026.01.03) Professional Music Driver [P.M.D.] version 4.8 Constant Tables / Programmed by M. Kajihara / Windows converted by C60 **/

#pragma once

#include "OPNA.h"

#pragma warning(disable: 4820) // x bytes padding added after last data member

struct ssg_effect_t
{
    int Priority;
    const int * Data;
};

#pragma warning(default: 4820) // x bytes padding added after last data member

extern const int ChannelTable[][3];
extern const int FMScaleFactor[];
extern const int SSGScaleFactor[];
extern const int PCMScaleFactor[];
extern const uint32_t P86ScaleFactor[];
extern const int PPZScaleFactor[];
extern const int FMToneCarrier[];
extern const uint32_t SSGRhythmDefinitions[][3];

extern const ssg_effect_t SSGEffects[];
