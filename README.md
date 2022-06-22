<!-- BADGES -->
[![License][mpl2-badge]][mpl2-url]
[![Contributors][contributors-badge]][contributors-url]
[![Commits][commits-badge]][commits-url]
[![Test Build][testing-badge]][testing-url] \
[![Version][version-badge]][version-url]
[![Downloads][downloads-badge]][downloads-url]
[![Watchers][watchers-badge]][watchers-url]
[![Issues][issues-badge]][issues-url]
[![Live Chat][irc-badge]][irc-url]
<!-- [![Stargazers][stars-badge]][stars-url] [![Forks][forks-badge]][forks-url] -->

<!-- HEADING -->
ANKLANG
=======

→ Audio Synthesizer and MIDI Composer ←

[Explore the manual](https://anklang.testbit.eu/pub/anklang/anklang-manual.html)
	·
[Join Chat](https://web.libera.chat/#Anklang)
	·
[Bugs & Features](https://github.com/tim-janik/anklang/issues)

<!-- ABOUT -->
## About the Anklang project

Anklang is a digital audio synthesis application for live creation and composition of music and other audio material.

The project is a revamp of several former audio projects by its two main authors
and aims to realize a coherent, solid amalgamation for composition and interactive
creation of synthesis music.

<!-- USAGE -->
## Usage

The project can be built on Linux by cloning the repository and running `make`.
However it is easier to download one of the self-contained AppImage release builds,
[mark it executable](https://discourse.appimage.org/t/how-to-run-an-appimage/80)
and run it.

<!-- ROADMAP -->
## Roadmap

☑  Implement the application core in C++17 and the GUI as a web front-end, utilizing web browsers or Electron.

☑  Implement separate audio synthesis threads with MIDI device support.

☑  Add devices for synthesis and effects.

☐  Support third-party plugins via [CLAP](https://github.com/free-audio/clap) [🗩 ](https://www.kvraudio.com/forum/viewtopic.php?t=574861), [LV2](https://en.wikipedia.org/wiki/LV2).

☐  Add standard audio library for various instrument types.

☐  Implement a quality MIDI file importer with mappings into the audio library.

<!-- LICENSE.txt -->
## License

This application including the audio engine are licensed under
[MPL-2.0](https://github.com/tim-janik/anklang/blob/trunk/LICENSE).

However, plugins that can be used with this application or may be downloaded
via extension packs, may fall under different licensing terms, such as
GPLv3 or proprietary licenses.

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[commits-badge]: https://img.shields.io/github/commit-activity/w/tim-janik/anklang?label=Commits&style=for-the-badge&color=green
[commits-url]: https://github.com/tim-janik/anklang/commits
[contributors-badge]: https://img.shields.io/github/contributors/tim-janik/anklang.svg?style=for-the-badge&color=green
[contributors-url]: https://github.com/tim-janik/anklang/graphs/contributors
[coverity-badge]: https://img.shields.io/coverity/scan/23262.svg?style=for-the-badge
[downloads-badge]: https://img.shields.io/github/downloads/tim-janik/anklang/total?style=for-the-badge&color=blue
[downloads-url]: https://github.com/tim-janik/anklang/releases
[drivers-badge]: https://img.shields.io/badge/Drivers-MIDI%20|%20ALSA%20|%20%20Pulse%20|%20Jack-999?style=for-the-badge
[fixme-badge]: https://img.shields.io/github/search/tim-janik/anklang/fixme?label=FIXME&style=for-the-badge
[forks-badge]: https://img.shields.io/github/forks/tim-janik/anklang.svg?style=for-the-badge
[forks-url]: https://github.com/tim-janik/anklang/network/members
[irc-badge]: https://img.shields.io/badge/Live%20Chat-Libera%20IRC-blueviolet?style=for-the-badge
[irc-url]: https://web.libera.chat/#Anklang
[issues-badge]: https://img.shields.io/github/issues-raw/tim-janik/anklang.svg?style=for-the-badge
[issues-url]: https://github.com/tim-janik/anklang/issues
[mpl2-badge]: https://img.shields.io/static/v1?label=License&message=MPL-2&color=9c0&style=for-the-badge
[mpl2-url]: https://github.com/tim-janik/anklang/blob/trunk/LICENSE
[packages-badge]: https://img.shields.io/badge/Packages-AppImage%20|%20deb-999?style=for-the-badge
[stars-badge]: https://img.shields.io/github/stars/tim-janik/anklang.svg?style=for-the-badge
[stars-url]: https://github.com/tim-janik/anklang/stargazers
[testing-badge]: https://img.shields.io/github/workflow/status/tim-janik/anklang/Testing?style=for-the-badge
[testing-url]: https://github.com/tim-janik/anklang/actions
[version-badge]: https://img.shields.io/github/v/release/tim-janik/anklang?label=version&style=for-the-badge
[version-url]: https://github.com/tim-janik/anklang/tags
[watchers-badge]: https://img.shields.io/github/watchers/tim-janik/anklang?style=for-the-badge
[watchers-url]: https://github.com/tim-janik/anklang/graphs/traffic
<!-- https://github.com/othneildrew/Best-README-Template -->
