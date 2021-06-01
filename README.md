<!-- BADGES -->
[![Issues][issues-badge]][issues-url]
[![License][mpl2-badge]][mpl2-url]
[![Plugins][lgpl-plugins-badge]][lgpl21-url]
[![Stargazers][stars-badge]][stars-url]
[![Forks][forks-badge]][forks-url]
[![Contributors][contributors-badge]][contributors-url]

<!-- HEADING -->
ANKLANG
=======

→ Audio Synthesizer and MIDI Composer ←

[Explore the manual](https://anklang.testbit.eu/pub/anklang/anklang-manual.html)
	·
[Join Chat](https://kiwiirc.com/nextclient/irc.libera.chat/#Anklang)
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

☐  Add devices for synthesis, filtering, envelopes, equalizers.

☐  Support third-party plugins through e.g. [LV2](https://en.wikipedia.org/wiki/LV2).

☐  Add standard audio library for various instrument types.

☐  Implement a quality MIDI file importer with mappings into the audio library.

<!-- LICENSE.txt -->
## License

Large parts of the application core are licensed under
[MPL-2.0](https://github.com/tim-janik/anklang/blob/trunk/misc/MPL-2.0.txt). \
Plugins are often licensed under
[LGPL-2.1+](https://github.com/tim-janik/anklang/blob/trunk/misc/LGPL-2.1.txt).

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-badge]: https://img.shields.io/github/contributors/tim-janik/anklang.svg?style=for-the-badge
[contributors-url]: https://github.com/tim-janik/anklang/graphs/contributors
[forks-badge]: https://img.shields.io/github/forks/tim-janik/anklang.svg?style=for-the-badge
[forks-url]: https://github.com/tim-janik/anklang/network/members
[issues-badge]: https://img.shields.io/github/issues/tim-janik/anklang.svg?style=for-the-badge
[issues-url]: https://github.com/tim-janik/anklang/issues
[mpl2-badge]: https://img.shields.io/static/v1?label=License&message=MPL-2&color=9c0&style=for-the-badge
[mpl2-url]: https://github.com/tim-janik/anklang/blob/trunk/misc/MPL-2.0.txt
[lgpl-plugins-badge]: https://img.shields.io/static/v1?label=Plugins&message=LGPL-2.1%2B&color=9c0&style=for-the-badge
[lgpl21-url]: https://github.com/tim-janik/anklang/blob/trunk/misc/LGPL-2.1.txt
[stars-badge]: https://img.shields.io/github/stars/tim-janik/anklang.svg?style=for-the-badge
[stars-url]: https://github.com/tim-janik/anklang/stargazers
<!-- https://github.com/othneildrew/Best-README-Template -->
