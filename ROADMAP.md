# Anklang Roadmap

Roadmap Discussions: Feedback & Ideas: [#52](https://github.com/tim-janik/anklang/issues/52)

## Architecture Modernization & Engine Transition [0.4.0]

☑  Application Core in C++20, with WebUI (HTML, CSS, JS based front-end GUI)

☑  Electron based WebUI & Vite-based UI build system

☑  Migrate Vue2 → Vue3; migrate Vue3 → Lit

☑  Use Hybrid Architecture (C++23 Backend → Lit Frontend)

☑  Reactive TypeScript IPC bindings

☑  Migrate Lit Frontend → SolidJS (partial)

☑  Replace old internal DSP engine with Tracktion Engine (regressions)

☑  Tracktion Engine integration as primary audio/MIDI framework (partial)

## Engine Migration (Current Focus) [0.5.0]

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

## Core DAW Features [0.6.0]

☑  Pianoroll editing and timeline navigation

☐  UI Polish (Electron lifecycle, focus trapping, and JS → TS migration)

☐  Implement device editor (add/remove/trkn-devices)

☐  Device management and property UIs

☐  MIDI note scripting

☐  MIDI file importer for multiple tracks

## Production Suite [1.0.0]

☐  Advanced Clip Launcher (play order, duration configuration)

☐  Full Mixer (Solo, Mute, Pan, Volume), stereo bus routing, per-drum effects

☐  Automation lanes with real-time event recording and modulation

☐  Per-device modulators / LFOs / envelopes / step sequencers

☐  Effect chains and preset management with macro parameters

☐  Add sample downloader and sound library integration

☐  Add standard audio library for various instrument types

☐  Convenient audio track support (WAV via drag-and-drop)

☐  Map MIDI file imports to the audio library

## Plugin Ecosystem & Beyond [1.x]

☐  Third-party plugin hosting (VST2, VST3)

☐  Support third-party plugins via [LV2](https://en.wikipedia.org/wiki/LV2)

☐  Support third-party plugins via [CLAP](https://github.com/free-audio/clap) [🗩 ](https://www.kvraudio.com/forum/viewtopic.php?t=574861)

☐  AI-assisted composition and generative synthesis tools

☐  Advanced spectral synthesis and granular sampling
