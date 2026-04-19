<!-- BADGES -->
[![Version][version-badge]][version-url]
[![License][mpl2-badge]][mpl2-url]
[![Test Build][testing-badge]][testing-url]
[![Downloads][downloads-badge]][downloads-url]
[![Live Chat][irc-badge]][irc-url]
<!-- [![Stargazers][stars-badge]][stars-url] [![Forks][forks-badge]][forks-url] -->

<!-- HEADING -->
ANKLANG
=======

→ Audio Synthesizer and MIDI Composer ←

[Documentation](https://tim-janik.github.io/anklang/) ·
[Github](https://github.com/tim-janik/anklang/) ·
[Issues](https://github.com/tim-janik/anklang/issues) ·
[IRC](https://web.libera.chat/#Anklang)

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
Roadmap Discussions: Feedback & Ideas: [#52](https://github.com/tim-janik/anklang/issues/52)

Phase 1: Foundation & Architecture (Completed)

☑  Application Core in C++20, with WebUI (HTML, CSS, JS based front-end GUI)

☑  Electron based WebUI & Vite-based UI build system

☑  Migrate Vue2 → Vue3; migrate Vue3 → Lit

☑  Use Hybrid Architecture (C++23 Backend → Lit Frontend)

☑  Reactive TypeScript IPC bindings

☑  Migrate Lit Frontend → SolidJS (partial)

☑  Replace old internal DSP engine with Tracktion Engine (regressions)

☑  Tracktion Engine integration as primary audio/MIDI framework (partial)

Phase 2: Engine Migration (Current Focus)

☐  Project Save/Load integration with Tracktion Engine, self-contained file copies

☐  Migration of Project, Track, and Clip models to Tracktion architecture

☐  Implement test sound generation and playback

☐  Add JACK audio support via trkn

☐  Adaptation of synthesis devices & effects (BlepSynth LiquidSFZ Freeverb Saturation)

☐  Implement file selection UI for liquidsfz

☐  Add effects (and more devices): Chorus Delays Distortions Reverberation FluidSynth

☐  Adjust Settings dialog to new engine

☑  Real-time audio/synthesis monitoring (uses outdated term "Telemetry")

☐  Revamp Audio Device selection for new engine

Phase 3: Core DAW Features

☑  Pianoroll editing and timeline navigation

☐  UI Polish (Electron lifecycle, focus trapping, and JS → TS migration)

☐  Implement device editor (add/remove/trkn-devices)

☐  Device management and property UIs

☐  MIDI note scripting

☐  MIDI file importer for multiple tracks

Phase 4: Production Suite

☐  Advanced Clip Launcher (play order, duration configuration)

☐  Full Mixer (Solo, Mute, Pan, Volume), stereo bus routing, per-drum effects

☐  Automation lanes with real-time event recording and modulation

☐  Per-device modulators / LFOs / envelopes / step sequencers

☐  Effect chains and preset management with macro parameters

☐  Add sample downloader and sound library integration

☐  Add standard audio library for various instrument types

☐  Convenient audio track support (WAV via drag-and-drop)

☐  Map MIDI file imports to the audio library

Phase 5: Plugin Ecosystem & Beyond

☐  Third-party plugin hosting (VST2, VST3)

☐  Support third-party plugins via [LV2](https://en.wikipedia.org/wiki/LV2)

☐  Support third-party plugins via [CLAP](https://github.com/free-audio/clap) [🗩 ](https://www.kvraudio.com/forum/viewtopic.php?t=574861)

☐  AI-assisted composition and generative synthesis tools

☐  Advanced spectral synthesis and granular sampling


<!-- DEVELOPMENT -->
## Development

### Project Structure
- `ase/` - C++23 backend sources (`*.c`, `*.h`, `*.cc`, `*.hh`)
- `ase/api.hh` - Public API for backend <-> frontend IPC
- `trkn/` - Vendor sources (tracktion_engine, JUCE)
- `ui/` - Web UI (Vite, Tailwind, SolidJS)
- `jsonipc/` - IPC for JSON messages between backend and browser

### Building & Testing
- Build: `make`
- Run all tests: `make check`
- Run specific test: `make check-<test_name>` (e.g., `make check-string_funcs`)
- Direct test execution: `out/lib/AnklangSynthEngine --test <test_name>`
- List available tests: `out/lib/AnklangSynthEngine --list-tests`

### Test Guidelines
- New code requires proper tests
- Avoid tests for what the type system already guarantees
- Only use methods available on the interface (extend `*.hh` files if needed)


<!-- LICENSE.txt -->
## License

This application including the audio engine are licensed under
[MPL-2.0](https://github.com/tim-janik/anklang/blob/trunk/LICENSE).

However, plugins that can be used with this application or may be downloaded
via extension packs, may fall under different licensing terms, such as
GPLv3 or proprietary licenses.

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=tim-janik/Anklang&type=Timeline)](https://star-history.com/#tim-janik/Anklang)


<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[commits-badge]: https://img.shields.io/github/commit-activity/w/tim-janik/anklang?label=Commits&style=for-the-badge&color=green
[commits-url]: https://github.com/tim-janik/anklang/commits
[contributors-badge]: https://img.shields.io/github/contributors/tim-janik/anklang.svg?style=for-the-badge&color=green
[contributors-url]: https://github.com/tim-janik/anklang/graphs/contributors
[coverity-badge]: https://img.shields.io/coverity/scan/23262.svg?style=for-the-badge
[downloads-badge]: https://img.shields.io/github/downloads/tim-janik/anklang/total?style=for-the-badge
[downloads-url]: https://github.com/tim-janik/anklang/releases
[drivers-badge]: https://img.shields.io/badge/Drivers-MIDI%20|%20ALSA%20|%20%20Pulse%20|%20Jack-999?style=for-the-badge
[fix\me-badge]: https://img.shields.io/github/search/tim-janik/anklang/fix%6De?label=FIX%4DE&style=for-the-badge
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
[testing-badge]: https://img.shields.io/github/actions/workflow/status/tim-janik/anklang/testing.yml?style=for-the-badge
[testing-url]: https://github.com/tim-janik/anklang/actions
[version-badge]: https://img.shields.io/github/v/release/tim-janik/anklang?label=version&style=for-the-badge&color=blue
[version-url]: https://github.com/tim-janik/anklang/releases/latest
[watchers-badge]: https://img.shields.io/github/watchers/tim-janik/anklang?style=for-the-badge
[watchers-url]: https://github.com/tim-janik/anklang/graphs/traffic
<!-- https://github.com/othneildrew/Best-README-Template -->
