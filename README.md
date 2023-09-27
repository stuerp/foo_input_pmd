
# foo_input_pmd

[foo_input_pmd](https://github.com/stuerp/foo_input_pmd/releases) is a [foobar2000](https://www.foobar2000.org/) component that adds playback of Professional Music Driver (PMD) files to foobar2000.

Professional Music Driver (PMD) is a music driver developed by Masahiro Kajihara (KAJA) which utilizes MML (Music Macro Language) to create music files for most Japanese computers of the 80s and early 90s.

PMD can be used to make music for the PC-98, PC-88, X68000, and FM Towns. It's the most used tool to make music for the PC-x801 series, notable examples are Touhou Project and Grounseed.

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

#### Drum Samples

You can specify the directory that contains the [YM2608 (OPNA)](https://en.wikipedia.org/wiki/Yamaha_YM2608) drum samples in the preferences.

The sample files should meet the following conditions:

* RIFF WAVE PCM Format (1), 1 channel, Sample Rate 44100Hz, 16 bits per sample
* Filenames: 2608_bd.wav, 2608_sd.wav, 2608_top.wav, 2608_hh.wav, 2608_tom.wav and 2608_rim.wav or 2608_rym.wav.

A "ym2608_adpcm_rom.bin" ROM file in the same directory takes precedence over the WAV sample files and will be used when found.

### Tags

The following meta data tags are available:

| Name           | Value                           |
| -------------- | ------------------------------- |
| title          | Title of the track              |
| artist         | Arranger specified by the track |
| composer       | Composer specified by the track |
| memo           | Memo specified by the track     |

The following info tags are available:

| Name        | Value                              |
| ----------- | ---------------------------------- |
| loop_length | Length of loop (in ms), if defined |

The following info tags are available while playing a track:

| Name           | Value                      |
| -------------- | -------------------------- |
| synthesis_rate | Synthesis rate in Hz       |
| loop_number    | Number of the current loop |

## Developing

To build the code you need:

* [Microsoft Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/downloads/) or later
* [foobar2000 SDK](https://www.foobar2000.org/SDK) 2023-05-10
* [Windows Template Library (WTL)](https://github.com/Win32-WTL/WTL) 10.0.10320

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

v0.4.0, 2023-09-xx, *""*

* Added: Checkboxes to enable or disable the use of PPS and the SSG (Software-controlled Sound Generator).
* Fixed: Some settings weren't reset by the Reset button.
* Builds with foobar2000 SDK 2023-09-06.

v0.3.0, 2023-07-22, *"What's your preference?"*

* Improved: Added all configuration parameters to the Preferences page.
* Improved: Several performance improvements.
* Fixed: Wrong specification of source buffer size (again).

v0.2.0, 2023-07-16, *"That's a wrap"*

* Added: pmd_loop_length info tag.
* Added: Seeking to a position in the current song.
* Added: Loop playing with or without fading.
* Improved: Loading and scanning a file.
* Improved: Removed dependency on PMDWin.dll.
* Fixed: Files with Unicode filenames failed to load.
* Fixed: Wrong specification of source buffer size.

v0.1.0, 2023-07-09, *"Scratchin' the itch"*

* Initial release.

## Acknowledgements / Credits

* Peter Pawlowski for the [foobar2000](https://www.foobar2000.org/) audio player. ![foobar2000](https://www.foobar2000.org/button-small.png)
* C60 for [PMDWin](http://c60.la.coocan.jp/) a library to render PMD files to PCM.
  * The PMD driver is a heavily modified version of [PMDWin](http://c60.la.coocan.jp/) 0.52.
* [Aaron Giles](https://github.com/aaronsgiles) for [ymfm](https://github.com/aaronsgiles/ymfm.git).

## Reference Material

### foobar2000
  * [foobar2000 Development](https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Development:Overview)

### Windows User Interface
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
  * [pmdmini](https://github.com/mistydemeo/pmdmini)
  * [FMP/PMD plugin for KbMediaPlayer 1.0r6](https://www.purose.net/befis/download/kmp/)
  * [Kaja Tools](http://www5.airnet.ne.jp/kajapon/tool.html)

* Music Macro Language
  * [PMD MML Command Manual](https://pigu-a.github.io/pmddocs/pmdmml.htm)
  * [Video Game Music Preservation](http://www.vgmpf.com/Wiki/index.php/Music_Macro_Language)
  * [Pedipanol](https://mml-guide.readthedocs.io/pmd/intro/)

* Songs
  * [Hoot Archive](http://hoot.joshw.info/pc98/)
  * [Modland FTP server](https://www.exotica.org.uk/wiki/Modland)
  * [Touhou 7: Perfect Cherry Blossom](https://www.youtube.com/watch?v=7k8BBweVxcw). Check the notes for links to the .M and .MML files.
  * [Zun](http://www16.big.or.jp/~zun/html/pmd.html)

* Various
  * [RetroPie BIOS Collection](https://github.com/archtaurus/RetroPieBIOS)

## Links

* Home page: [https://github.com/stuerp/foo_input_pmd](https://github.com/stuerp/foo_input_pmd)
* Repository: [https://github.com/stuerp/foo_input_pmd.git](https://github.com/stuerp/foo_input_pmd.git)
* Issue tracker: [https://github.com/stuerp/foo_input_pmd/issues](https://github.com/stuerp/foo_input_pmd/issues)

## License

![License: MIT](https://img.shields.io/badge/license-MIT-yellow.svg)
