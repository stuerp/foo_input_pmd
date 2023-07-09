
# foo_input_pmd

[foo_input_pmd](https://github.com/stuerp/foo_input_pmd/releases) is a [foobar2000](https://www.foobar2000.org/) component that adds playback of Professional Music Driver (PMD) files to foobar2000.

Professional Music Driver (PMD) is a music driver developed by Masahiro Kajihara which utilizes MML (Music Macro Language) to create music files for most Japanese computers of the 80s and early 90s.

![Screenshot](/Resources/Screenshot.png?raw=true "Screenshot")

## Features

* Decodes Professional Music Driver (.m, .m2) files.
* Supports dark mode.

## Requirements

* Tested on Microsoft Windows 10 and later.
* [foobar2000](https://www.foobar2000.org/download) v1.6.16 or later (32 or 64-bit). ![foobar2000](https://www.foobar2000.org/button-small.png)

## Getting started

### Installation

* Double-click `foo_input_pmd.fbk2-component`.

or

* Import `foo_input_pmd.fbk2-component` into foobar2000 using the "*File / Preferences / Components / Install...*" menu item.

### Configuration

You can specify the directory that contains the [YM2608 (OPNA)](https://en.wikipedia.org/wiki/Yamaha_YM2608) drum samples in the preferences.

### Tags

The following info tags are available:

| Name           | Value       |
| -------------- | ----------- |
| samplerate     |       44100 |
| channels       |           2 |
| bitspersample  |          16 |
| encoding       | synthesized |
| bitrate        |   1411 kpbs |

The following meta data tags are available:

| Name           | Value                           |
| -------------- | ------------------------------- |
| title          | Title of the track              |
| artist         | Arranger specified by the track |
| pmd_composer   | Composer specified by the track |
| pmd_memo       | Memo specified by the track     |

## Developing

To build the code you need:

* [Microsoft Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/downloads/) or later
* [foobar2000 SDK](https://www.foobar2000.org/SDK) 2023-05-10
* [Windows Template Library (WTL)](https://github.com/Win32-WTL/WTL) 10.0.10320

The following library is included in the code:

* [pmdwin](http://c60.la.coocan.jp/) 0.52

To create the deployment package you need:

* [PowerShell 7.2](https://github.com/PowerShell/PowerShell) or later

### Setup

Create the following directory structure:

    3rdParty
        WTL10_10320
    bin
        x86
    foo_input_pmd
    out
    sdk

* `3rdParty/WTL10_10320` contains WTL 10.0.10320.
* `bin` contains a portable version of foobar2000 64-bit for debugging purposes.
* `bin/x86` contains a portable version of foobar2000 32-bit for debugging purposes.
* `foo_input_pmd` contains the [Git](https://github.com/stuerp/foo_input_pmd) repository.
* `out` receives a deployable version of the component.
* `sdk` contains the foobar2000 SDK.

### Building

Open `foo_input_pmd.sln` with Visual Studio and build the solution.

### Packaging

To create the component first build the x86 configuration and next the x64 configuration.

## Contributing

If you'd like to contribute, please fork the repository and use a feature
branch. Pull requests are warmly welcome.

## Change Log

v0.1.0, 2023-07-09, *"Scratchin' the itch"*

* Initial release.

## Acknowledgements / Credits

* Peter Pawlowski for the [foobar2000](https://www.foobar2000.org/) audio player. ![foobar2000](https://www.foobar2000.org/button-small.png)
* C60 for [PMDWin](http://c60.la.coocan.jp/) a library to render PMD files to PCM.
* [Aaron Giles](https://github.com/aaronsgiles) for [ymfm](https://github.com/aaronsgiles/ymfm.git).

## Reference Material

* foobar2000
  * [foobar2000 Development](https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Development:Overview)

* Windows User Interface
  * [Desktop App User Interface](https://learn.microsoft.com/en-us/windows/win32/windows-application-ui-development)
  * [Windows User Experience Interaction Guidelines](https://learn.microsoft.com/en-us/windows/win32/uxguide/guidelines)
  * [Windows Controls](https://learn.microsoft.com/en-us/windows/win32/controls/window-controls)
  * [Control Library](https://learn.microsoft.com/en-us/windows/win32/controls/individual-control-info)
  * [Resource-Definition Statements](https://learn.microsoft.com/en-us/windows/win32/menurc/resource-definition-statements)
  * [Visuals, Layout](https://learn.microsoft.com/en-us/windows/win32/uxguide/vis-layout)

* Professional Music Driver
  * [Help Solve the File Format Problem](http://justsolve.archiveteam.org/wiki/Professional_Music_Driver_PMD)
  * [PMD Documentation](https://pigu-a.github.io/pmddocs/)
  * [pmdmini](https://github.com/gzaffin/pmdmini)
  * [FMP/PMD plugin for KbMediaPlayer 1.0r6](https://www.purose.net/befis/download/kmp/)

* Music Macro Language
  * http://www.vgmpf.com/Wiki/index.php/Music_Macro_Language

* Various
  * [RetroPie BIOS Collection](https://github.com/archtaurus/RetroPieBIOS)
  * [Touhou 7: Perfect Cherry Blossom](https://www.youtube.com/watch?v=7k8BBweVxcw). Check the notes for links to the .M and .MML files.

## Links

* Home page: https://github.com/stuerp/foo_input_pmd
* Repository: https://github.com/stuerp/foo_input_pmd.git
* Issue tracker: https://github.com/stuerp/foo_input_pmd/issues

## License

![License: MIT](https://img.shields.io/badge/license-MIT-yellow.svg)
