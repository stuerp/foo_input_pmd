
/** $VER: Resources.h (2023.07.16) P. Stuer **/

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
#define NUM_FILE_MINOR          2
#define NUM_FILE_PATCH          0
#define NUM_FILE_PRERELEASE     0

#define STR_FILE_NAME           TEXT(STR_COMPONENT_FILENAME)
#define STR_FILE_VERSION        TOSTRING(NUM_FILE_MAJOR) TEXT(".") TOSTRING(NUM_FILE_MINOR) TEXT(".") TOSTRING(NUM_FILE_PATCH) TEXT(".") TOSTRING(NUM_FILE_PRERELEASE)
#define STR_FILE_DESCRIPTION    TEXT(STR_COMPONENT_DESCRIPTION)

#define NUM_PRODUCT_MAJOR       0
#define NUM_PRODUCT_MINOR       2
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

#pragma region("Layout")

#define W_A0    332 // Dialog width as set by foobar2000, in dialog units
#define H_A0    288 // Dialog height as set by foobar2000, in dialog units

#pragma region("Samples path")
// Label
#define X_A1    7
#define Y_A1    7
#define W_A1    106
#define H_A1    8

// Button
#define W_A3    16
#define H_A3    14
#define X_A3    W_A0 - 7 - W_A3
#define Y_A3    Y_A2

// EditBox
#define X_A2    X_A1 + W_A1 + 3
#define Y_A2    Y_A1
#define W_A2    W_A0 - 7 - W_A1 - 3 - W_A3 - 3 - 7
#define H_A2    14
#pragma endregion

#pragma endregion
