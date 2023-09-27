﻿
// Professional Music Driver [P.M.D.] version 4.8 Constant Tables / Programmed By M. Kajihara / Windows Converted by C60

#include <CppCoreCheck/Warnings.h>

#pragma warning(disable: 4625 4626 4711 5045 ALL_CPPCORECHECK_WARNINGS)

#include "table.h"

/******************************************************************************
; Part Table
******************************************************************************/
const int TrackTable[][3] = // for PMDB2
{
    //  Part number, Part B, Sound Source number
    {  0, 1, 0 }, //A
    {  1, 2, 0 }, //B
    {  2, 3, 0 }, //C

    {  3, 1, 1 }, //D
    {  4, 2, 1 }, //E
    {  5, 3, 1 }, //F

    {  6, 1, 2 }, //G
    {  7, 2, 2 }, //H
    {  8, 3, 2 }, //I

    {  9, 1, 3 }, //J

    { 10, 3, 4 }, //K

    { 11, 3, 0 }, //c2
    { 12, 3, 0 }, //c3
    { 13, 3, 0 }, //c4

    { -1, 0,-1 }, //Rhythm

    { 22, 3, 1 }, //Effect

    { 14, 0, 5 }, //PPZ1
    { 15, 1, 5 }, //PPZ2
    { 16, 2, 5 }, //PPZ3
    { 17, 3, 5 }, //PPZ4
    { 18, 4, 5 }, //PPZ5
    { 19, 5, 5 }, //PPZ6
    { 20, 6, 5 }, //PPZ7
    { 21, 7, 5 }  //PPZ8
};

/******************************************************************************
; 音階DATA(FM)
******************************************************************************/
const int fnum_data[] =
{
    0x026a, // C
    0x028f, // D-
    0x02b6, // D
    0x02df, // E-
    0x030b, // E
    0x0339, // F
    0x036a, // G-
    0x039e, // G
    0x03d5, // A-
    0x0410, // A
    0x044e, // B-
    0x048f, // B
    0x0ee8, // 0x0c
    0x0e12, // 0x0d
    0x0d48 // 0x0e
};

/******************************************************************************
; 音階DATA(PSG)
******************************************************************************/
const int psg_tune_data[] =
{
    0x0ee8, // C
    0x0e12, // D-
    0x0d48, // D
    0x0c89, // E-
    0x0bd5, // E
    0x0b2b, // F
    0x0a8a, // G-
    0x09f3, // G
    0x0964, // A-
    0x08dd, // A
    0x085e, // B-
    0x07e6, // B
    0x8080, // 0x0c
    0x8080, // 0x0d
    0xe0a0 // 0x0e
};

/******************************************************************************
; 音階DATA(ADPCM)
******************************************************************************/
const int pcm_tune_data[] =
{
    0x3132 * 2, // C
    0x3420 * 2, // C+
    0x373a * 2, // D
    0x3a83 * 2, // D+
    0x3dfe * 2, // E
    0x41af * 2, // F
    0x4597 * 2, // F+
    0x49bb * 2, // G
    0x4e1e * 2, // G+
    0x52c4 * 2, // A
    0x57b1 * 2, // A+
    0x5ce8 * 2, // B
    0x3e80,  // 0x0c
    0x3c73,  // 0x0d
    0x7400  // 0x0e
};

/******************************************************************************
; 音階DATA(PMD86)
******************************************************************************/
const uint32_t p86_tune_data[] =
{
    0xff002AB7, // o1c
    0xff002D41, // o1c+
    0xff002FF2, // o1d
    0xff0032CB, // o1d+
    0xff0035D1, // o1e
    0xff003904, // o1f
    0xff003C68, // o1f+
    0xff003FFF, // o1g
    0xff0043CE, // o1g+
    0xff0047D6, // o1a
    0xff004C1B, // o1a+
    0xff0050A2, // o1b
    0xff00556E, // o2c
    0xff005A82, // o2c+
    0xff005FE4, // o2d
    0xff006597, // o2d+
    0xff006BA2, // o2e
    0xff007209, // o2f
    0xff0078D0, // o2f+
    0xff007FFF, // o2g
    0xff00879C, // o2g+
    0xff008FAC, // o2a
    0xff009837, // o2a+
    0xff00A145, // o2b
    0xff00AADC, // o3c
    0xff00B504, // o3c+
    0xff00BFC8, // o3d
    0xff00CB2F, // o3d+
    0xff00D744, // o3e
    0xff00E412, // o3f
    0xff00F1A1, // o3f+
    0xff010000, // o3g
    0xff20CB6B, // o3g+
    0xff20D783, // o3a
    0xff20E454, // o3a+
    0xff20F1E7, // o3b
    0xff40AADC, // o4c
    0xff40B504, // o4c+
    0xff40BFC8, // o4d
    0xff40CB2F, // o4d+
    0xff40D744, // o4e
    0xff40E412, // o4f
    0xff40F1A1, // o4f+
    0xff410000, // o4g
    0xff60CB6B, // o4g+
    0xff60D783, // o4a
    0xff60E454, // o4a+
    0xff60F1E7, // o4b
    0xff80AADC, // o5c
    0xff80B504, // o5c+
    0xff80BFC8, // o5d
    0xff80CB2F, // o5d+
    0xff80D744, // o5e
    0xff80E412, // o5f
    0xff80F1A1, // o5f+
    0xff810000, // o5g
    0xffA0CB6B, // o5g+
    0xffA0D783, // o5a
    0xffA0E454, // o5a+
    0xffA0F1E7, // o5b
    0xffC0AADC, // o6c
    0xffC0B504, // o6c+
    0xffC0BFC8, // o6d
    0xffC0CB2F, // o6d+
    0xffC0D744, // o6e
    0xffC0E412, // o6f
    0xffC0F1A1, // o6f+
    0xffC10000, // o6g
    0xffE0CB6B, // o6g+
    0xffE0D783, // o6a
    0xffE0E454, // o6a+
    0xffE0F1E7, // o6b
    0xffE1004A, // o7c
    0xffE10F87, // o7c+
    0xffE11FAC, // o7d
    0xffE130C7, // o7d+
    0xffE142E7, // o7e

    // ここまで？

    0xffE1561C, // o7f
    0xffE16A72, // o7f+
    0xffE18000, // o7g
    0xffE196D6, // o7g+
    0xffE1AF06, // o7a
    0xffE1C8A8, // o7a+
    0xffE1E3CF, // o7b
    0xffE20094, // o8c
    0xffE21F0E, // o8c+
    0xffE23F59, // o8d
    0xffE2618F, // o8d+
    0xffE285CE, // o8e
    0xffE2AC38, // o8f
    0xffE2D4E5, // o8f+
    0xffE30000, // o8g
    0xffE32DAC, // o8g+
    0xffE35E0D, // o8a
    0xffE39150, // o8a+
    0xffE3C79E, // o8b
    0xff80463E,
    0xff407400,
    0xff11800C,
    0xff387606,
    0xff0D76A2
};


/******************************************************************************
; 音階DATA(PPZ)
******************************************************************************/
const int ppz_tune_data[] =
{
    0x8000, // C
    0x87a6, // C+
    0x8fb3, // D
    0x9838, // D+
    0xa146, // E
    0xaade, // F
    0xb4ff, // F+
    0xbfcc, // G
    0xcb34, // G+
    0xd747, // A
    0xe418, // A+
    0xf1a5, // B
    0x3e80, // 0x0c
    0x42de, // 0x0d
    0x7400 // 0x0e
};

/******************************************************************************
; ＦＭ音色のキャリアのテーブル
******************************************************************************/
const int carrier_table[] =
{
    0x80, 0x80, 0x80, 0x80, 0xa0, 0xe0, 0xe0, 0xf0,
    0xee, 0xee, 0xee, 0xee, 0xcc, 0x88, 0x88, 0x00
};

/******************************************************************************
; Rythm data
******************************************************************************/
const int rhydat[][3] =
{
    // PT, PAN/VOLUME, KEYON
    { 0x18, 0xdf, 0x01 }, // Bus
    { 0x19, 0xdf, 0x02 }, // Snare
    { 0x1c, 0x5f, 0x10 }, //タム [LOW]
    { 0x1c, 0xdf, 0x10 }, //タム [MID]
    { 0x1c, 0x9f, 0x10 }, // タム [HIGH]
    { 0x1d, 0xd3, 0x20 }, // Rim
    { 0x19, 0xdf, 0x02 }, // Clap
    { 0x1b, 0x9c, 0x88 }, // C Hi Hat
    { 0x1a, 0x9d, 0x04 }, // O Hi Hat
    { 0x1a, 0xdf, 0x04 }, // Cymbal
    { 0x1a, 0x5e, 0x04 }  // Ride Cymbal
};

/******************************************************************************
; PPZ8 用 PAN データ
******************************************************************************/
const int ppzpandata[] = { 0, 9, 1, 5 };

/******************************************************************************
; SSG Effect Table
******************************************************************************/

const int D_000[] = {  // Bass Drum                1990-06-22 05:47:11
      1,220,  5, 31, 54, 15,  0,  0,  0,127,  0,
      8,164,  6,  0, 62, 16,176,  4,  0,127,  0,
    -1
};

const int D_001[] = {  // Snare Drum               1990-06-22 05:48:06
     14,144,  1,  7, 54, 16,184, 11,  0, 93,-14,
    -1
};

const int D_002[] = {  // Low Tom                  1990-06-22 05:49:19
      2,188,  2,  0, 54, 15,  0,  0,  0,100,  0,
     14,132,  3,  0, 54, 16,196,  9,  0,100,  0,
    -1
};

const int D_003[] = {  // Middle Tom               1990-06-22 05:50:23
      2,244,  1,  5, 54, 15,  0,  0,  0, 60,  0,
     14,108,  2,  0, 54, 16,196,  9,  0, 60,  0,
    -1
};

const int D_004[] = {  // High Tom                 1990-06-22 05:51:13
      2, 44,  1,  0, 54, 15,  0,  0,  0, 50,  0,
     14,144,  1,  0, 54, 16,196,  9,  0, 50,  0,
    -1
};

const int D_005[] = {  // Rim Shot                 1990-06-22 05:51:57
      2, 55,  0,  0, 62, 16, 44,  1,  0,100,  0,
    -1
};
const int D_006[] = {  // Snare Drum 2             1990-06-22 05:52:36
     16,  0,  0, 15, 55, 16,184, 11,  0,  0,-15,
    -1
};

const int D_007[] = {  // Hi-Hat Close             1990-06-22 05:53:10
      6, 39,  0,  0, 54, 16,244,  1,  0,  0,  0,
    -1
};

const int D_008[] = {  // Hi-Hat Open              1990-06-22 05:53:40
     32, 39,  0,  0, 54, 16,136, 19,  0,  0,  0,
    -1
};

const int D_009[] = {  // Crush Cymbal             1990-06-22 05:54:11
     31, 40,  0, 31, 54, 16,136, 19,  0,  0,-15,
    -1
};

const int D_010[] = {  // Ride Cymbal              1990-06-22 05:54:38
     31, 30,  0,  0, 54, 16,136, 19,  0,  0,  0,
    -1
};

// Effect for 電撃MIX

const int DM_001[] = { // syuta                    1994-05-25 23:13:02
      3,221,  1, 15, 55, 16,232,  3,  0,  0,113,
      2,221,  1,  0, 55, 16,232,  3,  0,  0,  0,
    -1
};

const int DM_002[] = { // Au                       1994-05-25 23:13:07
      1, 44,  1,  0, 62, 16, 44,  1, 13,  0,  0,
      6, 44,  1,  0, 62, 16, 16, 39,  0, 80,  0,
    -1
};

const int DM_003[] = { // syuba                    1994-05-25 23:13:25
      4,221,  1,  0, 55, 14, 16, 39,  0,  0, 81,
      4,221,  1, 10, 55, 16,208,  7,  0,  0,-15,
    -1
};

const int DM_004[] = { // syu                      1994-05-25 23:17:51
      3,221,  1,  0, 55, 16,244,  1, 13,  0,  0,
      8,221,  1, 15, 55, 16,208,  7,  0,  0,  0,
    -1
};

const int DM_005[] = { // sya-                     1994-05-25 23:19:01
      3,221,  1, 10, 55, 16,100,  0, 13,  0,  0,
     16,221,  1,  5, 55, 16, 16, 39,  0,  0,  0,
    -1
};

const int DM_006[] = { // po                       1994-05-25 23:13:32
      2,144,  1,  0, 62, 16,244,  1,  0,  0,  0,
    -1
};

const int DM_007[] = { // tattu                    1994-05-25 23:13:37
      4,221,  1, 15, 55, 16,232,  3,  0,  0,  0,
    -1
};

const int DM_008[] = { // zusyau                   1994-05-25 23:13:42
      2,221,  1, 31, 55, 15, 16, 39,  0,  0,  0,
     12,221,  1,  0, 55, 16,136, 19,  0,  0, 17,
    -1
};

const int DM_009[] = { // piro                     1994-05-25 23:20:41
      2,144,  1,  0, 62, 16,232,  3,  0,  0,  0,
      2,200,  0,  0, 62, 16,232,  3,  0,  0,  0,
    -1
};

const int DM_010[] = { // piron                    1994-05-25 23:20:26
      4,144,  1,  0, 62, 16,208,  7,  0,  0,  0,
      8,200,  0,  0, 62, 16,184, 11,  0,  0,  0,
    -1
};

const int DM_011[] = { // pirorironn               1994-05-25 23:21:50
      3,144,  1,  0, 62, 16,208,  7,  0,  0,  0,
      3,100,  0,  0, 62, 16,208,  7,  0,  0,  0,
      3,200,  0,  0, 62, 16,208,  7,  0,  0,  0,
      3,144,  1,  0, 62, 16,208,  7,  0,  0,  0,
      8,100,  0,  0, 62, 16,184, 11,  0,  0,  0,
    -1
};

const int DM_012[] = { // buu                      1994-05-25 23:23:10
     16,208,  7,  0, 62, 15, 16, 39,  0,  0,  0,
    -1
};

const int DM_013[] = { // babon                    1994-05-25 23:15:40
      4,221,  1, 31, 55, 16,136, 19,  0,  0,  0,
      8,221,  1, 31, 54, 16,184, 11,  0,127,-15,
    -1
};

const int DM_014[] = { // basyu-                   1994-05-25 23:15:44
      4,221,  1, 25, 55, 16,208,  7,  0,  0,  0,
     32,221,  1, 20, 55, 16,112, 23,  0,  0, 19,
    -1
};

const int DM_015[] = { // poun                     1994-05-25 23:15:27
      6,200,  0,  0, 54, 16,136, 19,  0, 20,  0,
    -1
};

const int DM_016[] = { // pasyu                    1994-05-25 23:22:59
      4, 40,  0, 20, 54, 16, 16, 39,  0, 20,  0,
     16, 20,  0,  5, 54, 16,136, 19,  0,  0,  0,
    -1
};

const int DM_017[] = { // KON                      1994-05-25 23:16:07
      6, 88,  2,  0, 62, 16,232,  3,  0,  0,  0,
    -1
};

const int DM_018[] = { // dosun                    1994-05-25 23:23:57
      4,232,  3,  0, 62, 16, 16, 39,  0,127,  0,
     16,221,  1,  0, 54, 16, 16, 39,  0, 64,  0,
    -1
};

const int DM_019[] = { // zu                       1994-05-25 23:24:59
      4,232,  3, 31, 54, 15, 16, 39,  0,  0,  0,
    -1
};

const int DM_020[] = { // go                       1994-05-25 23:24:43
      4,255, 15, 31, 54, 15, 16, 39,  0,  0,  0,
    -1
};

const int DM_021[] = { // poyon                    1994-05-25 23:26:17
      4,221,  1,  0, 62, 16,232,  3,  0,-50,  0,
     16,242,  0,  0, 62, 16,112, 23,  0, -8,  0,
    -1
};

const int DM_022[] = { // katun                    1994-05-25 23:27:10
      4,100,  0,  0, 62, 16,244,  1,  0,  0,  0,
      4, 10,  0,  0, 54, 16,232,  3,  0,  0,  0,
    -1
};

const int DM_023[] = { // syupin                   1994-05-25 23:28:18
      8,221,  1,  5, 55, 16,244,  1, 13,  0,  0,
     24, 30,  0,  0, 54, 16, 16, 39,  0,  0,  0,
    -1
};

const int DM_024[] = { // 1UP                      1994-05-25 23:16:52
      4, 44,  1,  0, 62, 16,136, 19,  0,  0,  0,
      4,180,  0,  0, 62, 16,136, 19,  0,  0,  0,
      4,200,  0,  0, 62, 16,136, 19,  0,  0,  0,
     24,150,  0,  0, 62, 16,136, 19,  0,  0,  0,
    -1
};

const int DM_025[] = { // PI                       1994-05-25 23:16:35
      3,238,  0,  0, 62, 14,208,  7,  0,  0,  0,
    -1
};

const int DM_026[] = { // pikon                    1994-05-25 23:29:19
      4,200,  0,  0, 62, 16,136, 19,  0,  0,  0,
     16,100,  0,  0, 62, 16,136, 19,  0,  0,  0,
    -1
};

const int DM_027[] = { // pyuu                     1994-05-25 23:30:33
     16,  0,  0,  0, 54, 16,244,  1, 13,  1, 17,
     16, 16,  0, 16, 54, 16,124, 21,  0,  1, 17,
    -1
};

const int DM_028[] = { // PI                       1994-05-25 23:16:24
      1,200,  0,  0, 62, 14,232,  3,  0,  0,  0,
    -1
};

const int DM_029[] = { // click                    1994-05-25 23:14:24
      2,200,  0,  0, 62, 16, 32,  3,  0,  0,  0,
      2,100,  0,  0, 62, 16, 32,  3,  0,  0,  0,
      2, 50,  0,  0, 62, 16, 32,  3,  0,  0,  0,
      2, 25,  0,  0, 62, 16, 32,  3,  0,  0,  0,
    -1
};


// Effect for Ｒｕｓｔｙ

const int RS_006[] = { // batan                    1993-01-08 01:44:30
      2,221,  1, 31, 55, 16,232,  3,  0,  0,  0,
      6,221,  1, 10, 55, 16,208,  7,  0,  0, 17,
    -1
};

const int RS_007[] = { // dodonn                   1993-01-08 01:39:10
      4,232,  3, 15, 54, 16, 16, 39,  0,127,  0,
     16,244,  1,  5, 54, 16,136, 19,  0,127,-13,
    -1
};

const int RS_009[] = { // kisya-                   1993-01-08 01:39:47
      4, 40,  0, 20, 54, 16, 16, 39,  0, 20,  0,
     24, 20,  0,  5, 54, 16, 16, 39,  0,  1,  0,
    -1
};

const int RS_010[] = { // bofu                     1993-01-08 01:45:38
      4,232,  3,  0, 54, 15, 16, 39,  0,127,  0,
     32, 10,  0, 10, 55, 16,112, 23,  0,  0,-13,
    -1
};

const int RS_011[] = { // gogogogo--               1993-06-29 12:27:41
     96,255, 15, 31, 54, 16, 96,234,  0,  0,  0,
    -1
};

const int RS_012[] = { // karakara                 1993-06-29 12:16:36
     64, 10,  0,  0, 54, 16, 32, 78,  0,  0,-127,
    -1
};

const int RS_013[] = { // buonn                    1993-01-08 01:47:56
      8,208,  7,  0, 62, 16,144,  1, 13,  0,  0,
      8,208,  7,  0, 62, 16,208,  7,  0,  0,  0,
    -1
};

const int RS_015[] = { // tyattu                   1993-01-08 01:49:27
      4, 20,  0,  8, 54, 16,184, 11,  0,  0,-31,
    -1
};

const int RS_018[] = { // zu                       1993-01-08 01:51:05
      4,208,  7, 30, 54, 16,160, 15,  0,  0,  0,
    -1
};

const int RS_019[] = { // saaaa                    1993-06-29 12:28:05
     60,221,  1,  4, 55, 10, 16, 39,  0,  0,  0,
    -1
};

const int RS_020[] = { // za                       1993-01-08 01:52:35
      6,221,  1, 16, 55, 16,136, 19,  0,  0,  0,
    -1
};

const int RS_021[] = { // TYARIN                   1993-06-29 12:29:19
      4, 40,  0,  0, 54, 15, 16, 39,  0,  0,  0,
      8, 30,  0,  0, 62, 16,208,  7,  0,  0,  0,
    -1
};

const int RS_022[] = { // SYUWAWA                  1993-06-29 12:35:38
     48,100,  0,  0, 55, 16,136, 19, 13, -1, 33,
     12, 50,  0,  0, 55, 13,136, 19,  0,  0, 33,
     12,221,  1,  0, 55, 12, 16, 39,  0,  0, 33,
     12,221,  1,  0, 55, 11, 16, 39,  0,  0, 33,
     12,221,  1,  0, 55, 10, 16, 39,  0,  0, 33,
     12,221,  1,  0, 55,  9, 16, 39,  0,  0, 33,
    -1
};

const int RS_024[] = { // PIN                      1993-06-29 12:36:42
      6,100,  0,  0, 62, 16,232,  3,  0,  0,  0,
    -1
};

const int RS_026[] = { // KAMINARI                 1993-06-29 12:42:57
      4, 23,  0, 31, 55, 16,208,  7,  0,  0,  0,
     64, 15,  0, 31, 55, 16,152, 58,  0,  0,  0,
    -1
};

const int RS_027[] = { // PI                       1993-06-29 12:44:03
      3,238,  0,  0, 62, 14,208,  7,  0,  0,  0,
    -1
};

const int RS_028[] = { // KEIKOKU                  1993-06-29 12:46:13
      7, 44,  1,  0, 62, 16,160, 15,  0,  0,  0,
      7, 44,  1,  0, 62, 16,208,  7,  0,  0,  0,
     48, 44,  1,  0, 62, 16, 16, 39,  0,  0,  0,
    -1
};

const int RS_029[] = { // ETC 1                    1993-06-29 12:46:54
     96,208,  7,  0, 62, 16, 16, 39,  0,-20,  0,
    -1
};

const int RS_030[] = { // BUFOFOFO                 1993-06-29 12:58:12
      8,208,  7,  0, 62, 16, 16, 39,  0,-80,  0,
      8,  8,  7,  0, 62, 16, 16, 39,  0,-80,  0,
      8, 64,  6,  0, 62, 16, 16, 39,  0,-80,  0,
     48,120,  5,  0, 62, 16, 16, 39,  0,-70,  0,
    -1
};

const int RS_031[] = { // ETC 3                    1993-06-29 12:49:32
      8,232,  3,  0, 62, 16, 16, 39,  0, 80,  0,
      8,176,  4,  0, 62, 16, 16, 39,  0, 80,  0,
      8, 20,  5,  0, 62, 16, 16, 39,  0, 80,  0,
     48,120,  5,  0, 62, 16, 16, 39,  0, 80,  0,
    -1
};

const int RS_032[] = { // ETC 4                    1993-06-29 12:50:11
     96,  0,  0,  0, 62, 16, 16, 39,  0,-128,  0,
    -1
};

const int RS_033[] = { // HADE BOMB                1993-06-29 12:52:06
      4,100,  0, 31, 54, 16,208,  7,  0,127,  0,
     32,  0,  0, 31, 54, 16, 16, 39,  0,127,-127,
    -1
};

const int RS_035[] = { // JARARAN                  1993-06-29 13:02:17
      2,244,  1, 20, 54, 16, 16, 39,  0, -4,  0,
      2,144,  1, 15, 54, 16, 16, 39,  0, -4, 65,
      2, 44,  1, 10, 62, 16, 16, 39,  0, -4, 65,
      2,200,  0,  5, 54, 16, 16, 39,  0, -4, 65,
     16,150,  0,  0, 62, 16,184, 11,  0,  0,  0,
    -1
};

// Effect for ポゼッショナー

const int PO_011[] = { // Rain fall                1990-06-22 05:55:43
    254,221,  1,  3, 55, 10, 16, 39,  0,  0,  0,
    -1
};

const int PO_012[] = { // Spinner                  1990-06-22 05:57:18
     24,140,  0,  0, 62, 16, 88, 27,  0, 14,  0,
    -1
};

const int PO_013[] = { // Kaminari                 1990-06-22 05:59:16
     48,160, 15, 31, 54, 16, 16, 39,  0,  0,  1,
    -1
};

const int PO_014[] = { // Sairen                   1990-06-22 06:00:45
     31,100,  0,  0, 62, 16, 88, 27,  0, -1,  0,
    -1
};

const int PO_015[] = { // Door Shut                1990-06-22 06:03:28
      6,221,  1,  8, 55, 16,184, 11,  0,  0,-15,
      8,144,  1,  0, 54, 16,144,  1, 13,-40,  0,
    -1
};

const int PO_016[] = { // Kiteki                   1990-06-22 06:05:23
     96,160, 15,  0, 62, 16, 48,117,  0,  0,  0,
    -1
};

const int PO_017[] = { // Ship Bomb                1990-06-22 06:06:54
      4,221,  1, 31, 55, 16,208,  7,  0,  0,  0,
     64,221,  1, 20, 55, 16, 16, 39,  0,  0,  0,
    -1
};

const int PO_018[] = { // Spinner 2                1990-06-22 06:08:08
     64,120,  0,  0, 54, 16, 16, 39,  0,  2,  0,
    -1
};

const int PO_019[] = { // Helli                    1990-06-22 06:09:58
      4,221,  1,  4, 55, 16,208,  7,  0,  0,  0,
      4,221,  1,  0, 55, 16,208,  7,  0,  0,  0,
      4,221,  1,  4, 55, 16,232,  3,  0,  0,  0,
      4,221,  1,  0, 55, 16,232,  3,  0,  0,  0,
    -1
};

const int PO_020[] = { // Kinzoku Sagyou           1990-06-22 07:23:41
     16, 30,  0,  5, 54, 16,160, 15,  0,  0,  0,
    -1
};

const int PO_021[] = { // Kaze (DAME)              1990-06-22 06:13:46
     16,220,  5,  0, 62, 15, 16, 39,  0,  0,  0,
      8,220,  5,  0, 62, 15, 16, 39,  0,-10,  0,
     48,140,  5,  0, 62, 16, 16, 39,  0, 10,  0,
    -1
};

const int PO_022[] = { // Taikushitu Soto          1990-06-22 06:15:55
      4,160, 15, 31, 54, 16,184, 11,  0,  0,  0,
     24,184, 11,  8, 54, 16,136, 19,  0, 40, 20,
    -1
};

const int PO_023[] = { // Punch                    1990-06-22 06:17:13
      4,160, 15, 31, 54, 16,208,  7,  0, 10,  0,
      8,221,  1, 28, 54, 16,208,  7,  0,127,  0,
    -1
};

const int PO_024[] = { // Shower                   1990-06-22 06:19:08
    254,  0,  0,  0, 55, 10,  0,  0,  0,  0,  0,
    -1
};

const int PO_025[] = { // Shokki                   1990-06-22 06:22:14
      6, 31,  0,  4, 54, 16,232,  3,  0,  0,  0,
      8, 30,  0,  0, 54, 16,232,  3,  0,  0,  0,
    -1
};

const int PO_026[] = { // Tobikomi                 1990-06-22 06:24:09
      8,220,  5, 25, 54, 16,184, 11,  0,127,  0,
     48,221,  1, 10, 55, 16, 64, 31,  0,  0, 18,
    -1
};

const int PO_027[] = { // Air Fukidasi             1990-06-22 06:25:35
      4,208,  7,  0, 55, 16,208,  7,  0,  0,  0,
     48,221,  1,  4, 55, 16, 16, 39,  0,  0, 20,
    -1
};

const int PO_028[] = { // Heavy Door Open          1990-06-22 07:23:33
     48,208,  7, 31, 54, 16,152, 58,  0, -5,  0,
    -1
};

const int PO_029[] = { // Car Door Shut            1990-06-22 07:23:30
     16,232,  3, 31, 54, 16,184, 11,  0,127,  0,
    -1
};

const int PO_030[] = { // Car Come'in              1990-06-22 06:30:31
      4,160, 15, 31, 54, 15, 16, 39,  0,  0,  0,
     96,160, 15, 28, 54, 16, 32, 78,  0,  0,  0,
    -1
};

const int PO_031[] = { // Ice Hikkaki              1990-06-22 06:31:26
      2, 10,  0,  0, 54, 16,244,  1,  0,  0,  0,
      2, 20,  0,  0, 54, 16,244,  1,  0,  0,  0,
    -1
};

const int PO_032[] = { // Ship Crush Down          1990-06-22 07:23:23
     64,160, 15, 20, 54, 16, 48,117,  0,  1, 22,
    192,221,  1, 31, 55, 16, 48,117,  0,  0,  0,
    -1
};

const int PO_033[] = { // Toraware                 1990-06-22 06:35:02
     32,232,  3,  0, 54, 16, 64, 31,  0,  0,  0,
    -1
};

const int PO_034[] = { // Sekizou Break            1990-06-22 06:36:14
      4,221,  1, 31, 55, 15, 16, 39,  0,  0,  0,
     64,221,  1, 10, 55, 16, 16, 39,  0,  0, 18,
    -1
};

const int PO_035[] = { // Blaster Shot             1990-06-22 06:37:55
      4,221,  1, 31, 55, 16,184, 11,  0,  0,  0,
      4,160, 15, 20, 54, 16,184, 11,  0, 20,  0,
     64,  0,  0,  4, 54, 16, 16, 39,  0,  1, 20,
    -1
};

const int PO_036[] = { // Seifuku Yabuki           1990-06-22 06:39:58
     16,221,  1,  4, 55, 14, 16, 39,  0,  0,  0,
    -1
};

const int PO_037[] = { // Miminari                 1990-06-22 06:42:13
      4,  8,  0,  0, 62, 16,  1,  0,  8,  0,  0,
     64,  0,  0,  0, 62, 16, 64, 31,  0,  1,  0,
    -1
};

const int PO_038[] = { // Sekizou Ayasige          1990-06-22 06:44:23
     40,160, 15,  0, 62, 16,232,253,  0,-10,  0,
     50, 16, 14,  0, 62, 16, 32, 78,  0, 10,  0,
    -1
};

const int PO_039[] = { // Voiler & Engine          1990-06-22 07:23:14
     60,221,  1, 30, 55, 14, 48,117,  0,  0,-14,
     16,184, 11,  2, 55, 16,112, 23,  0, 40, 17,
    -1
};

const int PO_040[] = { // Suimen                   1990-06-22 06:47:58
      4, 10,  0,  4, 54, 16,232,  3,  0,  0,  0,
      6,221,  1,  0, 55, 16,208,  7,  0,  0,  0,
    -1
};

const int PO_041[] = { // Kika                     1990-06-22 06:48:29
     64,221,  1,  0, 55, 16, 16, 39,  0,  0, 20,
    -1
};

const int PO_042[] = { // Change Kakyuu            1990-06-22 06:50:00
     48,232,  3,  0, 62, 16, 16, 39,  0, 10,  0,
    -1
};

const int PO_043[] = { // Change Blue              1990-06-22 06:51:47
     15,100,  0,  0, 62, 16,232,  3, 14, -4,  0,
    -1
};

const int PO_044[] = { // Youma Funsyutu           1990-06-22 06:54:06
      6,221,  1,  4, 55, 16,208,  7,  0,  0,  0,
      4,100,  0,  4, 54, 16,160, 15,  0,-20,  0,
     64,221,  1,  8, 55, 16, 64, 31,  0,  0,-10,
    -1
};

const int PO_045[] = { // Kekkai                   1990-06-22 07:23:06
    128,232,  3, 31, 54, 16, 48,117,  0,  1,-14,
    -1
};

const int PO_046[] = { // Gosintou 1               1990-06-22 06:56:47
      4, 20,  0,  0, 54, 16,232,  3,  0,  0,  0,
    -1
};

const int PO_047[] = { // Gosintou 2               1990-06-22 06:58:25
      8,208,  7,  0, 62, 16,232,  3, 13,-10,  0,
     64,208,  7,  0, 62, 16, 16, 39,  0,  2,  0,
    -1
};

const int PO_048[] = { // Gosintou 3               1990-06-22 07:00:22
      8,221,  1,  0, 55, 16, 32,  3, 13,  0, 17,
     16,221,  1,  0, 55, 16,208,  7,  0,  0, 17,
    -1
};

const int PO_049[] = { // Hand Blaster             1990-06-22 07:01:53
      4,160, 15, 31, 54, 16,184, 11,  0,  0,  0,
      4, 40,  0,  0, 54, 16,232,  3,  0,-10,  0,
     64,221,  1,  0, 55, 16, 16, 39,  0,  0, 18,
    -1
};

const int PO_050[] = { // Magic                    1990-06-22 07:04:00
      4, 32,  0,  0, 62, 16,208,  7,  0,  0,  0,
     24, 32,  0,  0, 54, 16, 64, 31,  0, -1,  0,
     90,160, 15, 31, 54, 16, 48,117,  0,-40,-12,
    -1
};

const int PO_051[] = { // Grabiton 1               1990-06-22 07:04:41
      4,221,  1, 31, 55, 16, 16, 39,  0,  0,  0,
     31,221,  1,  0, 55, 16, 16, 39,  0,  0, 17,
    -1
};

const int PO_052[] = { // Grabiton 2               1990-06-22 07:05:10
    128,160, 15, 31, 54, 16, 48,117,  0,  0,  0,
    -1
};

const int PO_053[] = { // Attack Kakyuu            1990-06-22 07:06:38
      4,160, 15, 31, 54, 16, 16, 39,  0,  0,  0,
     16,221,  1,  0, 55, 16,112, 23,  0,  0,  0,
    -1
};

const int PO_054[] = { // Attack Blue(TEKITOU)     1990-06-22 07:08:33
      6,100,  0,  0, 54, 16,244,  1, 13, -5,  0,
     16, 70,  0,  0, 54, 16,112, 23,  0,127,  0,
    -1
};

const int PO_055[] = { // Attack Red               1990-06-22 07:10:10
     20,184, 11,  0, 54, 14, 16, 39,  0,-100,  0,
     16,232,  3,  0, 54, 16,112, 23,  0,100,  0,
    -1
};

const int PO_056[] = { // Attack White             1990-06-22 07:11:16
      4,  0,  0,  4, 54, 16, 16, 39,  0,127,-15,
     16,  0,  0,  0, 54, 16,112, 23,  0, 10, 17,
    -1
};

const int PO_057[] = { // Attack Black             1990-06-22 07:22:10
      4,200,  0,  4, 54, 16,208,  7,  0,127, 17,
     10,  0,  0,  0, 54, 16, 88,  2, 13,  1,  0,
     24, 10,  0,  0, 54, 16,112, 23,  0,  5, 17,
    -1
};

const int PO_058[] = { // Attack Last              1990-06-22 07:22:14
     20, 60,  0,  4, 54, 14, 16, 39,  0, -1,  0,
     20, 40,  0,  0, 54, 14, 16, 39,  0,  1,113,
     20, 60,  0, 10, 54, 16,112, 23,  0,  1, 20,
    -1
};

const int PO_059[] = { // Damage 1                 1990-06-22 07:17:32
      4,221,  1, 31, 54, 16,184, 11,  0,127,  0,
     16,221,  1,  0, 55, 16,112, 23,  0,  0, 33,
    -1
};

const int PO_060[] = { // Damage 2                 1990-06-22 07:19:18
      8,232,  3, 31, 54, 14, 16, 39,  0,100,  0,
      8,120,  5, 31, 54, 15, 16, 39,  0,-100,113,
     16, 88,  2, 31, 54, 16,112, 23,  0,127,-15,
    -1
};

const int PO_061[] = { // Attack                   1990-06-22 07:22:55
      8,  0,  0, 31, 54, 16,184, 11,  0,100,  0,
     24,221,  1,  0, 55, 16, 16, 39,  0,  0, 17,
    -1
};

// Effect for ＮＡＤＩＡ

const int ND_000[] = { // MAP                      1992-01-27 17:32:40
     48,221,  1,  0, 62, 16, 16, 39,  0, -1,  0,
    -1
};

const int ND_001[] = { // SONAR                    1992-01-27 17:33:23
    192,200,  0,  0, 62, 16, 64,156,  0,  0,  0,
    -1
};

const int ND_002[] = { // KOUKOU                   1992-01-27 17:57:44
    254,221,  1,  8, 55, 12, 16, 39,  0,  0,  0,
    -1
};

const int ND_003[] = { // MEGIDO                   1992-01-27 17:35:47
    192,200,  0,  0, 54, 16, 16, 39, 13, -1,  0,
      6,221,  1,  0, 54, 16, 16, 39,  0,127,  0,
    192,221,  1,  0, 55, 16, 96,234,  0,  0, -8,
    -1
};

const int ND_004[] = { // JINARI                   1992-01-27 17:36:37
    254,221,  1, 31, 54, 14, 16, 39,  0,-128,113,
    -1
};

const int ND_005[] = { // SWITCH                   1992-01-27 17:37:21
      6,221,  1, 15, 55, 16,208,  7,  0,  0,  0,
      6, 20,  0,  0, 54, 16,160, 15,  0,  0,  0,
    -1
};

const int ND_006[] = { // DOSYUUNN                 1992-01-27 17:38:01
      6,221,  1,  0, 54, 16, 16, 39,  0,127,  0,
    192,221,  1,  0, 55, 16, 96,234,  0,  0, 24,
    -1
};

const int ND_007[] = { // GYUOON                   1992-01-27 17:39:09
    192,232,  3, 31, 54, 16, 96,234,  0, -4,  0,
    -1
};

const int ND_008[] = { // PIPIPIPI                 1992-01-27 17:40:16
     64,150,  0,  0, 62, 16,176,  4,  8,  0,  0,
    -1
};

const int ND_009[] = { // SYUBATTU                 1992-01-27 17:41:16
     12,221,  1,  0, 55, 16,232,  3, 13,  0, 20,
     24,221,  1, 15, 55, 16, 64, 31,  0,  0,  0,
    -1
};

const int ND_010[] = { // BEAM UNARI               1992-01-27 17:42:05
    254, 25,  0,  0, 54, 14, 16, 39,  0,  0,-111,
    -1
};

const int ND_011[] = { // BEAM KAKUSAN             1992-01-27 17:43:07
      6,221,  1, 15, 55, 16,160, 15,  0,  0,  0,
    192,208,  7,  0, 54, 16, 96,234,  0, -8,  0,
    -1
};

const int ND_012[] = { // ORGAN                    1992-01-27 18:01:45
     48,221,  1,  0, 62, 14, 16, 39,  0,  0,  0,
    -1
};

const int ND_013[] = { // PANEL                    1992-01-27 17:57:15
      6,221,  1,  4, 55, 16,160, 15,  0,  0,  0,
      6,221,  1,  4, 55, 16,160, 15,  0,  0,  0,
      6,221,  1,  4, 55, 16,160, 15,  0,  0,  0,
      6,221,  1,  4, 55, 16,160, 15,  0,  0,  0,
     24, 20,  0, 10, 54, 16, 16, 39,  0,  0,  0,
    -1
};

const int ND_014[] = { // DRILL                    1992-01-27 17:45:25
    254,160, 15, 31, 54, 15, 16, 39,  0,  0,  0,
    -1
};

const int ND_015[] = { // PRAZMA                   1992-01-27 17:45:59
      6, 20,  0, 15, 55, 16,112, 23,  0,  0,  0,
      6, 20,  0,  0, 54, 16, 16, 39,  0,  0,  0,
    -1
};

const int ND_016[] = { // BABEL                    1992-01-27 17:46:34
    254,160, 15,  0, 62, 16, 16, 39, 14,  0,  0,
    -1
};

const int ND_017[] = { // ELEVETOR                 1992-01-27 17:47:27
     12,233,  1,  0, 54, 14, 16, 39,  0, -1,  0,
    254,221,  1,  0, 54, 14, 16, 39,  0,  0,  0,
    -1
};

const int ND_018[] = { // MEGIDO HASSYA            1992-01-27 17:48:04
    254,160, 15, 15, 54, 15, 16, 39, 13,  0,  0,
    -1
};

const int ND_019[] = { // DAIBAKUHATU              1992-01-27 18:28:56
     12,221,  1, 31, 54, 16, 16, 39,  0,127,  0,
    144,  0,  0,  0, 54, 16, 96,234,  0,127, 24,
    192,160, 15, 31, 54, 16, 80, 70, 14,  0,  0,
    -1
};

const int ND_020[] = { // NAMI                     1992-01-27 17:50:59
    254,221,  1,  0, 55, 16, 16, 39, 14,  0,  0,
    -1
};

const int ND_021[] = { // DOOOONN                  1992-01-27 17:51:39
     96,208,  7,  0, 54, 16, 16, 39,  0, 40,  0,
    -1
};

const int ND_022[] = { // DOGA                     1992-01-27 17:52:18
      6,221,  1, 31, 54, 16, 16, 39,  0,127,  0,
     12,221,  1,  0, 55, 16,160, 15,  0,  0,  0,
    -1
};

const int ND_023[] = { // PISI                     1992-01-27 17:52:53
      6, 20,  0, 31, 54, 16, 16, 39,  0,  0,  0,
     24, 20,  0,  0, 54, 16, 16, 39,  0,  0,  0,
    -1
};

const int ND_024[] = { // BLUE WATER               1992-01-27 17:53:15
    254, 15,  0,  0, 62, 14, 16, 39,  0,  0,  0,
    -1
};

const int ND_025[] = { // HOWAWAN                  1992-01-27 17:56:51
     12,144,  1,  0, 62, 16,100,  0, 13, -2,  0,
     12,134,  1,  0, 62, 16,100,  0, 13, -2,  0,
     12,124,  1,  0, 62, 16,100,  0, 13, -2,  0,
     12,114,  1,  0, 62, 16,100,  0, 13, -2,  0,
     48, 90,  1,  0, 62, 16, 16, 39,  0, -2,  0,
    -1
};

const int ND_026[] = { // ZUGAN                    1992-01-27 17:19:49
      6,221,  1, 31, 55, 16,160, 15,  0,  0,  0,
     64,221,  1, 24, 55, 16, 32, 78,  0,  0,  0,
    -1
};

const int ND_027[] = { // DAAANN                   1992-01-27 17:20:28
     48,221,  1, 31, 55, 16,152, 58,  0,  0,  0,
    -1
};

const int ND_028[] = { // DOGOOOONN                1992-01-27 17:21:14
      6,221,  1,  1, 54, 16, 16, 39,  0,127,  0,
    192,221,  1, 31, 55, 16, 96,234,  0,  0,  0,
    -1
};

const int ND_029[] = { // GASYA                    1992-01-27 17:22:08
      3,221,  1, 15, 55, 16,208,  7,  0,  0,  0,
     12,221,  1,  1, 55, 16,160, 15,  0,  0,  0,
    -1
};

const int ND_030[] = { // BASYUSYUSYU              1992-01-27 17:22:52
      3,221,  1, 15, 55, 15, 16, 39,  0,  0,  0,
    192,221,  1, 31, 55, 16, 96,234,  0,  0,113,
    -1
};

const int ND_031[] = { // DOSYUSYUSYU              1992-01-27 17:24:31
    192,  0,  0,  0, 54, 16, 96,234,  0,-128, 17,
    -1
};

const int ND_032[] = { // SYUSYUUUUNN              1992-01-27 17:25:34
     12,221,  1,  0, 55, 15, 16, 39,  0,  0,113,
     32,221,  1,  0, 55, 16, 32, 78,  0,  0, 17,
    -1
};

const int ND_033[] = { // BASYANN - HYURURURU      1992-01-27 18:00:33
      6,221,  1, 31, 55, 16, 16, 39,  0,  0,-63,
     32,221,  1,  4, 55, 16, 16, 39,  0,  0,  0,
    192,  0,  0,  0, 54, 16, 96,234,  0,  1,  0,
    -1
};

const int ND_034[] = { // ZYURUZYURU               1992-01-27 17:27:38
    192,221,  1,  0, 55, 16, 96,234,  0,  0,113,
    -1
};

const int ND_035[] = { // ZUGOGOGOGO               1992-01-27 17:29:07
      6,221,  1, 15, 55, 16, 16, 39,  0,  0,  0,
      6,221,  1, 31, 55, 16, 16, 39,  0,  0,-15,
      6,221,  1, 31, 55, 16, 16, 39,  0,  0,-15,
      6,221,  1, 31, 55, 16, 16, 39,  0,  0,-15,
    192,221,  1, 31, 55, 16, 96,234,  0,  0, -8,
    -1
};

const int ND_036[] = { // ZUGOOOONN                1992-01-27 17:29:50
      6,221,  1, 15, 55, 16, 16, 39,  0,  0,  0,
    192,221,  1, 31, 55, 16, 48,117,  0,  0,  0,
    -1
};

const int ND_037[] = { // BI--                     1992-01-27 17:59:08
     48, 40,  0,  0, 62, 16,100,  0,  8,  0,  0,
    -1
};

const int ND_038[] = { // BASYUSYUUU               1992-01-27 17:30:38
     48,221,  1,  0, 55, 16, 16, 39,  0,  0,-111,
    -1
};

const int ND_039[] = { // BISYU                    1992-01-27 17:31:52
      6,232,  3, 15, 54, 16, 16, 39,  0,127,  0,
     24,221,  1,  0, 55, 16, 16, 39,  0,  0,  0,
    -1
};

const EFFTBL efftbl[] =
{
    { 1, D_000  }, //   0  BDRM
    { 1, D_001  }, //   1  SIMONDS
    { 1, D_002  }, //   2  SIMONDSTAML
    { 1, D_003  }, //   3  SIMONDSTAMM
    { 1, D_004  }, //   4  SIMONDSTAMH
    { 1, D_005  }, //   5  RIMSHOTT
    { 1, D_006  }, //   6  CPSIMONDSSD2
    { 1, D_007  }, //   7  CLOSEHT
    { 1, D_008  }, //   8  OPENHT
    { 1, D_009  }, //   9  CRUSHCYMBA
    { 1, D_010  }, //  10  RDCYN
    { 2, DM_001 }, //  11  syuta
    { 2, DM_002 }, //  12  Au
    { 2, DM_003 }, //  13  syuba
    { 2, DM_004 }, //  14  syu
    { 2, DM_005 }, //  15  sya-
    { 2, DM_006 }, //  16  po
    { 2, DM_007 }, //  17  tattu
    { 2, DM_008 }, //  18  zusyau
    { 2, DM_009 }, //  19  piro
    { 2, DM_010 }, //  20  piron
    { 2, DM_011 }, //  21  pirorironn
    { 2, DM_012 }, //  22  buu
    { 2, DM_013 }, //  23  babon
    { 2, DM_014 }, //  24  basyu-
    { 2, DM_015 }, //  25  poun
    { 2, DM_016 }, //  26  pasyu
    { 2, DM_017 }, //  27  KON
    { 2, DM_018 }, //  28  dosun
    { 2, DM_019 }, //  29  zu
    { 2, DM_020 }, //  30  go
    { 2, DM_021 }, //  31  poyon
    { 2, DM_022 }, //  32  katun
    { 2, DM_023 }, //  33  syupin
    { 2, DM_024 }, //  34  1UP
    { 2, DM_025 }, //  35  PI
    { 2, DM_026 }, //  36  pikon
    { 2, DM_027 }, //  37  pyuu
    { 2, DM_028 }, //  38  PI
    { 2, DM_029 }, //  39  click
    { 2, RS_006 }, //  40  batan
    { 2, RS_007 }, //  41  dodonn
    { 2, RS_009 }, //  42  kisya-
    { 2, RS_010 }, //  43  bofu
    { 2, RS_011 }, //  44  gogogogo--
    { 2, RS_012 }, //  45  karakara
    { 2, RS_013 }, //  46  buonn
    { 2, RS_015 }, //  47  tyattu
    { 2, RS_018 }, //  48  zu
    { 2, RS_019 }, //  49  saaaa
    { 2, RS_020 }, //  50  za
    { 2, RS_021 }, //  51  TYARIN
    { 2, RS_022 }, //  52  SYUWAWA
    { 2, RS_024 }, //  53  PIN
    { 2, RS_026 }, //  54  KAMINARI
    { 2, RS_027 }, //  55  PI
    { 2, RS_028 }, //  56  KEIKOKU
    { 2, RS_029 }, //  57  ETC 1
    { 2, RS_030 }, //  58  BUFOFOFO
    { 2, RS_031 }, //  59  ETC 3
    { 2, RS_032 }, //  60  ETC 4
    { 2, RS_033 }, //  61  HADE BOMB
    { 2, RS_035 }, //  62  JARARAN
    { 2, PO_011 }, //  63  Rain fall
    { 2, PO_012 }, //  64  Spinner
    { 2, PO_013 }, //  65  Kaminari
    { 2, PO_014 }, //  66  Sairen
    { 2, PO_015 }, //  67  Door Shut
    { 2, PO_016 }, //  68  Kiteki
    { 2, PO_017 }, //  69  Ship Bomb
    { 2, PO_018 }, //  70  Spinner 2
    { 2, PO_019 }, //  71  Helli
    { 2, PO_020 }, //  72  Kinzoku Sagyou
    { 2, PO_021 }, //  73  Kaze (DAME)
    { 2, PO_022 }, //  74  Taikushitu Soto
    { 2, PO_023 }, //  75  Punch
    { 2, PO_024 }, //  76  Shower
    { 2, PO_025 }, //  77  Shokki
    { 2, PO_026 }, //  78  Tobikomi
    { 2, PO_027 }, //  79  Air Fukidasi
    { 2, PO_028 }, //  80  Heavy Door Open
    { 2, PO_029 }, //  81  Car Door Shut
    { 2, PO_030 }, //  82  Car Come'in
    { 2, PO_031 }, //  83  Ice Hikkaki
    { 2, PO_032 }, //  84  Ship Crush Down
    { 2, PO_033 }, //  85  Toraware
    { 2, PO_034 }, //  86  Sekizou Break
    { 2, PO_035 }, //  87  Blaster Shot
    { 2, PO_036 }, //  88  Seifuku Yabuki
    { 2, PO_037 }, //  89  Miminari
    { 2, PO_038 }, //  90  Sekizou Ayasige
    { 2, PO_039 }, //  91  Voiler & Engine
    { 2, PO_040 }, //  92  Suimen
    { 2, PO_041 }, //  93  Kika
    { 2, PO_042 }, //  94  Change Kakyuu
    { 2, PO_043 }, //  95  Change Blue
    { 2, PO_044 }, //  96  Youma Funsyutu
    { 2, PO_045 }, //  97  Kekkai
    { 2, PO_046 }, //  98  Gosintou 1
    { 2, PO_047 }, //  99  Gosintou 2
    { 2, PO_048 }, // 100  Gosintou 3
    { 2, PO_049 }, // 101  Hand Blaster
    { 2, PO_050 }, // 102  Magic
    { 2, PO_051 }, // 103  Grabiton 1
    { 2, PO_052 }, // 104  Grabiton 2
    { 2, PO_053 }, // 105  Attack Kakyuu
    { 2, PO_054 }, // 106  Attack Blue(TEKITOU)
    { 2, PO_055 }, // 107  Attack Red
    { 2, PO_056 }, // 108  Attack White
    { 2, PO_057 }, // 109  Attack Black
    { 2, PO_058 }, // 110  Attack Last
    { 2, PO_059 }, // 111  Damage 1
    { 2, PO_060 }, // 112  Damage 2
    { 2, PO_061 }, // 113  Attack
    { 2, ND_000 }, // 114  MAP
    { 2, ND_001 }, // 115  SONAR
    { 2, ND_002 }, // 116  KOUKOU
    { 2, ND_003 }, // 117  MEGIDO
    { 2, ND_004 }, // 118  JINARI
    { 2, ND_005 }, // 119  SWITCH
    { 2, ND_006 }, // 120  DOSYUUNN
    { 2, ND_007 }, // 121  GYUOON
    { 2, ND_008 }, // 122  PIPIPIPI
    { 2, ND_009 }, // 123  SYUBATTU
    { 2, ND_010 }, // 124  BEAM UNARI
    { 2, ND_011 }, // 125  BEAM KAKUSAN
    { 2, ND_012 }, // 126  ORGAN
    { 2, ND_013 }, // 127  PANEL
    { 2, ND_014 }, // 128  DRILL
    { 2, ND_015 }, // 129  PRAZMA
    { 2, ND_016 }, // 130  BABEL
    { 2, ND_017 }, // 131  ELEVETOR
    { 2, ND_018 }, // 132  MEGIDO HASSYA
    { 2, ND_019 }, // 133  DAIBAKUHATU
    { 2, ND_020 }, // 134  NAMI
    { 2, ND_021 }, // 135  DOOOONN
    { 2, ND_022 }, // 136  DOGA
    { 2, ND_023 }, // 137  PISI
    { 2, ND_024 }, // 138  BLUE WATER
    { 2, ND_025 }, // 139  HOWAWAN
    { 2, ND_026 }, // 140  ZUGAN
    { 2, ND_027 }, // 141  DAAANN
    { 2, ND_028 }, // 142  DOGOOOONN
    { 2, ND_029 }, // 143  GASYA
    { 2, ND_030 }, // 144  BASYUSYUSYU
    { 2, ND_031 }, // 145  DOSYUSYUSYU
    { 2, ND_032 }, // 146  SYUSYUUUUNN
    { 2, ND_033 }, // 147  BASYANN - HYURURURU
    { 2, ND_034 }, // 148  ZYURUZYURU
    { 2, ND_035 }, // 149  ZUGOGOGOGO
    { 2, ND_036 }, // 150  ZUGOOOONN
    { 2, ND_037 }, // 151  BI--
    { 2, ND_038 }, // 152  BASYUSYUUU
    { 2, ND_039 }, // 153  BISYU
};
