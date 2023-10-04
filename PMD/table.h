﻿
// Professional Music Driver [P.M.D.] version 4.8 Constant Tables / Programmed by M. Kajihara / Windows converted by C60

#pragma once

#include "OPNA.h"

#pragma warning(disable: 4820) // x bytes padding added after last data member
struct SSGEffect
{
    int Priority;
    const int * Data;
};
#pragma warning(default: 4820) // x bytes padding added after last data member

extern const int ChannelTable[][3];
extern const int fnum_data[];
extern const int psg_tune_data[];
extern const int pcm_tune_data[];
extern const uint32_t p86_tune_data[];
extern const int ppz_tune_data[];
extern const int carrier_table[];
extern const int rhydat[][3];

extern const SSGEffect SSGEffects[];
