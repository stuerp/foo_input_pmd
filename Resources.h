
/** $VER: Resources.h (2023.07.17) P. Stuer **/

#pragma once

#define TOSTRING_IMPL(x) #x
#define TOSTRING(x) TOSTRING_IMPL(x)

/** Component specific **/

#define STR_COMPONENT_NAME          "PMD Decoder"
#define STR_COMPONENT_VERSION       TOSTRING(NUM_FILE_MAJOR) "." TOSTRING(NUM_FILE_MINOR) "." TOSTRING(NUM_FILE_PATCH) "." TOSTRING(NUM_FILE_PRERELEASE)
#define STR_COMPONENT_BASENAME      "foo_input_pmd"
#define STR_COMPONENT_FILENAME      STR_COMPONENT_BASENAME ".dll"
#define STR_COMPONENT_COPYRIGHT     "Copyright (c) 2023. All rights reserved."
#define STR_COMPONENT_COMMENTS      "Written by P. Stuer"
#define STR_COMPONENT_DESCRIPTION   "Adds playback of Professional Music Driver (PMD) files to foobar2000"

/** Generic **/

#define STR_COMPANY_NAME        TEXT("")
#define STR_INTERNAL_NAME       TEXT(STR_COMPONENT_NAME)
#define STR_COMMENTS            TEXT(STR_COMPONENT_COMMENTS)
#define STR_COPYRIGHT           TEXT(STR_COMPONENT_COPYRIGHT)

#define NUM_FILE_MAJOR          0
#define NUM_FILE_MINOR          3
#define NUM_FILE_PATCH          0
#define NUM_FILE_PRERELEASE     0

#define STR_FILE_NAME           TEXT(STR_COMPONENT_FILENAME)
#define STR_FILE_VERSION        TOSTRING(NUM_FILE_MAJOR) TEXT(".") TOSTRING(NUM_FILE_MINOR) TEXT(".") TOSTRING(NUM_FILE_PATCH) TEXT(".") TOSTRING(NUM_FILE_PRERELEASE)
#define STR_FILE_DESCRIPTION    TEXT(STR_COMPONENT_DESCRIPTION)

#define NUM_PRODUCT_MAJOR       0
#define NUM_PRODUCT_MINOR       3
#define NUM_PRODUCT_PATCH       0
#define NUM_PRODUCT_PRERELEASE  0

#define STR_PRODUCT_NAME        STR_INTERNAL_NAME
#define STR_PRODUCT_VERSION     TOSTRING(NUM_PRODUCT_MAJOR) TEXT(".") TOSTRING(NUM_PRODUCT_MINOR) TEXT(".") TOSTRING(NUM_PRODUCT_PATCH) TEXT(".") TOSTRING(NUM_PRODUCT_PRERELEASE)

#define STR_ABOUT_NAME          STR_INTERNAL_NAME
#define STR_ABOUT_WEB           TEXT("https://github.com/stuerp/") STR_COMPONENT_BASENAME
#define STR_ABOUT_EMAIL         TEXT("mailto:peter.stuer@outlook.com")

/** Preferences **/

#define IDD_PREFERENCES             1000
#define IDD_PREFERENCES_NAME        STR_COMPONENT_NAME

#define IDC_SAMPLES_PATH            IDD_PREFERENCES + 1
#define IDC_SAMPLES_PATH_SELECT     IDC_SAMPLES_PATH + 1

#define IDC_LOOP_MODE               IDC_SAMPLES_PATH_SELECT + 1
#define IDC_LOOP_COUNT              IDC_LOOP_MODE + 1
#define IDC_FADE_OUT_PERIOD         IDC_LOOP_COUNT + 1

#define IDC_SAMPLE_RATE             IDC_FADE_OUT_PERIOD + 1

#pragma region("Layout")

#define W_A00   332 // Dialog width as set by foobar2000, in dialog units
#define H_A00   288 // Dialog height as set by foobar2000, in dialog units

#define H_LB     8  // Label height
#define H_BT    14  // Button height
#define H_EB    14  // Edit box height
#define H_CB    14  // Combo box height

#pragma region("Samples path")
// Label
#define W_A01    106
#define H_A01    H_LB
#define X_A01    7
#define Y_A01    7 + 2

// Button
#define W_A03    16
#define H_A03    H_BT
#define X_A03    W_A00 - 7 - W_A03
#define Y_A03    Y_A01 - 2

// EditBox
#define W_A02    W_A00 - 7 - W_A01 - 3- 3 - W_A03 - 7
#define H_A02    H_EB
#define X_A02    X_A01 + W_A01 + 3
#define Y_A02    Y_A01 - 2
#pragma endregion

#pragma region("Looping")
// Groupbox
#define W_A04    W_A00 - 7 - 7
#define X_A04    7
#define Y_A04    Y_A02 + H_A02 + 4

    // Label: Playback
    #define W_A05    40
    #define H_A05    H_LB
    #define X_A05    X_A04 + 5
    #define Y_A05    Y_A04 + 11 + 2

    // Combobox: Playback
    #define W_A06    100
    #define H_A06    H_CB
    #define X_A06    X_A05 + W_A05 + 3
    #define Y_A06    Y_A05 - 2

    // Label: Loop count
    #define W_A07    58
    #define H_A07    H_LB
    #define X_A07    X_A06 + W_A06 + 4
    #define Y_A07    Y_A04 + 11 + 2

    // Edit box: Loop count
    #define W_A08    50
    #define H_A08    H_EB
    #define X_A08    X_A07 + W_A07 + 3
    #define Y_A08    Y_A07 - 2

    // Label: Fade out period
    #define W_A09    58
    #define H_A09    H_LB
    #define X_A09    X_A07
    #define Y_A09    Y_A08 + H_A08 + 4 + 2

    // Edit box: Fade out period
    #define W_A10    50
    #define H_A10    H_EB
    #define X_A10    X_A09 + W_A09 + 3
    #define Y_A10    Y_A09 - 2

    // Label: ms
    #define W_A11    10
    #define H_A11    H_LB
    #define X_A11    X_A10 + W_A10 + 3
    #define Y_A11    Y_A10 + 2

#define H_A04    11 + H_A08 + 4 + H_A10 + 4
#pragma endregion

#pragma region("Sample rate")
// Label; Playback
#define W_A12   46
#define H_A12   H_LB
#define X_A12   7
#define Y_A12   Y_A04 + H_A04 + 4 + 2

// Combobox: Playback
#define W_A13   60
#define H_A13   H_CB
#define X_A13   X_A12 + W_A12 + 3
#define Y_A13   Y_A12 - 2

// Label: Hz
#define W_A14    10
#define H_A14    H_LB
#define X_A14    X_A13 + W_A13 + 3
#define Y_A14    Y_A13 + 2
#pragma endregion

#pragma endregion
