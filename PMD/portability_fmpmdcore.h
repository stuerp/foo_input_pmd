
// Based on PMDWin code by C60

#pragma once

#include "opna.h"

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

struct StereoSample
{
    Sample left;
    Sample right;
};

#pragma pack(push)
#pragma pack(2)
struct Stereo16bit
{
    short left;
    short right;
} ;
#pragma pack(pop)
