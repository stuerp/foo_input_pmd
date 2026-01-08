
// $VER: Utility.cpp (2026.01.07) Based on PMDWin code by C60

#include <pch.h>

#include "Utility.h"

// Returns true if the byte is the first byte of a multi-byte character.
bool IsMBBLead(uint8_t c)
{
    return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
}

//  Removes cursor movement escape sequences. Not supported.
char * RemoveEscapeSequences(char * dst, const char * src)
{
    const uint8_t * p = (const uint8_t *) src;
    char * Dst = dst;

    while (*p != '\0')
    {
        if (*p == 0x1b)
        {   // Skip the escape sequence.
            if (*(p + 1) == '[')
            {
                p += 2;

                while (*p && (toupper(*p) < 'A' || toupper(*p) > 'Z'))
                {
                    if (IsMBBLead(*p))
                    {
                        p += 2;
                        continue;
                    }

                    p++;
                }

                p++;
            }
            else
            if (*(p + 1) == '=')
                p += 4;
            else
            if (*(p + 1) == ')' || *(p + 1) == '!')
                p += 3;
            else
                p += 2;
        }
        else
            *Dst++ = (char) *p++;
    }

    *Dst = '\0';

    return dst;
}

// Converts 2-byte full-width (zen-kaku) Kanji characters to half-width (han-kaku) characters.
char * Zen2ToHan(char * dst, const char * src)
{
    #pragma warning(disable: 4566)
    static const wchar_t * CodeTable[] =
    {
        L"!",    // 8540
        L"\"",   // 8541
        L"#",    // 8542
        L"$",    // 8543
        L"%",    // 8544
        L"&",    // 8545
        L"'",    // 8546
        L"(",    // 8547
        L")",    // 8548
        L"*",    // 8549
        L"+",    // 854a
        L",",    // 854b
        L"-",    // 854c
        L".",    // 854d
        L"/",    // 854e
        L"0",    // 854f

        L"1",    // 8550
        L"2",    // 8551
        L"3",    // 8552
        L"4",    // 8553
        L"5",    // 8554
        L"6",    // 8555
        L"7",    // 8556
        L"8",    // 8557
        L"9",    // 8558
        L":",    // 8559
        L";",    // 855a
        L"<",    // 855b
        L"=",    // 855c
        L">",    // 855d
        L"?",    // 855e
        L"@",    // 855f

        L"A",    // 8560
        L"B",    // 8561
        L"C",    // 8562
        L"D",    // 8563
        L"E",    // 8564
        L"F",    // 8565
        L"G",    // 8566
        L"H",    // 8567
        L"I",    // 8568
        L"J",    // 8569
        L"K",    // 856a
        L"L",    // 856b
        L"M",    // 856c
        L"N",    // 856d
        L"O",    // 856e
        L"P",    // 856f

        L"Q",    // 8570
        L"R",    // 8571
        L"S",    // 8572
        L"T",    // 8573
        L"U",    // 8574
        L"V",    // 8575
        L"W",    // 8576
        L"X",    // 8577
        L"Y",    // 8578
        L"Z",    // 8579
        L"[",    // 857a
        L"\\",   // 857b
        L"]",    // 857c
        L"^",    // 857d
        L"_",    // 857e
        L"",     // 857f

        L"`",    // 8580
        L"a",    // 8581
        L"b",    // 8582
        L"c",    // 8583
        L"d",    // 8584
        L"e",    // 8585
        L"f",    // 8586
        L"g",    // 8587
        L"h",    // 8588
        L"i",    // 8589
        L"j",    // 858a
        L"k",    // 858b
        L"l",    // 858c
        L"m",    // 858d
        L"n",    // 858e
        L"o",    // 858f

        L"p",    // 8590
        L"q",    // 8591
        L"r",    // 8592
        L"s",    // 8593
        L"t",    // 8594
        L"u",    // 8595
        L"v",    // 8596
        L"w",    // 8597
        L"x",    // 8598
        L"y",    // 8599
        L"z",    // 859a
        L"{",    // 859b
        L"|",    // 859c
        L"}",    // 859d
        L"\x85\x9e",  // 859e
        L"｡",    // 859f

        L"｢",    // 85a0
        L"｣",    // 85a1
        L"､",    // 85a2
        L"･",    // 85a3
        L"ｦ",    // 85a4
        L"ｧ",    // 85a5
        L"ｨ",    // 85a6
        L"ｩ",    // 85a7
        L"ｪ",    // 85a8
        L"ｫ",    // 85a9
        L"ｬ",    // 85aa
        L"ｭ",    // 85ab
        L"ｮ",    // 85ac
        L"ｯ",    // 85ad
        L"ｰ",    // 85ae
        L"ｱ",    // 85af

        L"ｲ",    // 85b0
        L"ｳ",    // 85b1
        L"ｴ",    // 85b2
        L"ｵ",    // 85b3
        L"ｶ",    // 85b4
        L"ｷ",    // 85b5
        L"ｸ",    // 85b6
        L"ｹ",    // 85b7
        L"ｺ",    // 85b8
        L"ｻ",    // 85b9
        L"ｼ",    // 85ba
        L"ｽ",    // 85bb
        L"ｾ",    // 85bc
        L"ｿ",    // 85bd
        L"ﾀ",    // 85be
        L"ﾁ",    // 85bf

        L"ﾂ",    // 85c0
        L"ﾃ",    // 85c1
        L"ﾄ",    // 85c2
        L"ﾅ",    // 85c3
        L"ﾆ",    // 85c4
        L"ﾇ",    // 85c5
        L"ﾈ",    // 85c6
        L"ﾉ",    // 85c7
        L"ﾊ",    // 85c8
        L"ﾋ",    // 85c9
        L"ﾌ",    // 85ca
        L"ﾍ",    // 85cb
        L"ﾎ",    // 85cc
        L"ﾏ",    // 85cd
        L"ﾐ",    // 85ce
        L"ﾑ",    // 85cf

        L"ﾒ",    // 85d0
        L"ﾓ",    // 85d1
        L"ﾔ",    // 85d2
        L"ﾕ",    // 85d3
        L"ﾖ",    // 85d4
        L"ﾗ",    // 85d5
        L"ﾘ",    // 85d6
        L"ﾙ",    // 85d7
        L"ﾚ",    // 85d8
        L"ﾛ",    // 85d9
        L"ﾜ",    // 85da
        L"ﾝ",    // 85db
        L"ﾞ",    // 85dc
        L"ﾟ",    // 85dd
        L"\x85\xde",  // 85de
        L"\x85\xdf",  // 85df
        L"ﾜ",    // 85e0
        L"ｶ",    // 85e1
        L"ｹ",    // 85e2
        L"ｳﾞ",    // 85e3
        L"ｶﾞ",    // 85e4
        L"ｷﾞ",    // 85e5
        L"ｸﾞ",    // 85e6
        L"ｹﾞ",    // 85e7
        L"ｺﾞ",    // 85e8
        L"ｻﾞ",    // 85e9
        L"ｼﾞ",    // 85ea
        L"ｽﾞ",    // 85eb
        L"ｾﾞ",    // 85ec
        L"ｿﾞ",    // 85ed
        L"ﾀﾞ",    // 85ee
        L"ﾁﾞ",    // 85ef

        L"ﾂﾞ",    // 85f0
        L"ﾃﾞ",    // 85f1
        L"ﾄﾞ",    // 85f2
        L"ﾊﾞ",    // 85f3
        L"ﾊﾟ",    // 85f4
        L"ﾋﾞ",    // 85f5
        L"ﾋﾟ",    // 85f6
        L"ﾌﾞ",    // 85f7
        L"ﾌﾟ",    // 85f8
        L"ﾍﾞ",    // 85f9
        L"ﾍﾟ",    // 85fa
        L"ﾎﾞ",    // 85fb
        L"ﾎﾟ"     // 85fc
    };

    const size_t Max = ::strlen(src) + 2;

    char * Zen = (char *) ::malloc(Max);

    if (Zen == nullptr)
        return nullptr;

    ::strcpy_s(Zen, Max, src);
    Zen[::strlen(Zen) + 1] = '\0'; // 2 consecutive \0 bytes.

    uint8_t * p = (uint8_t *) Zen;
    uint8_t * q = (uint8_t *) dst;

    do
    {
        if (IsMBBLead(*p))
        {
            // Kanji 1st byte
            if ((p[0] == 0x85) && (p[1] >= 0x40) && (p[1] <= 0xFC))
            {
                const wchar_t * w = CodeTable[p[1] - 0x40]; // 2-byte half-width

                size_t l = ::wcslen(w);

                ::memcpy(q, w, l * sizeof(wchar_t));

                p += 2;
                q += l;
            }
            else
            {
                *q++ = *p++;
                *q++ = *p++;
            }
        }
        else
            *q++ = *p++;
    }
    while (*p != '\0');

    *q++ = '\0';
    *q++ = '\0';

    ::free(Zen);

    return dst;
}
