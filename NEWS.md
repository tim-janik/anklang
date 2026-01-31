## Anklang NEXT

* Switched to C++23, improved various build system automations.

## Anklang 0.3.0

**Full Changelog**: [https://github.com/tim-janik/anklang/compare/v0.2.0...v0.3.0](https://github.com/tim-janik/anklang/compare/v0.2.0...v0.3.0)

### Hardware and System Requirements
* Linux - the Anklang deb and AppImage are based on Ubuntu 20.04.
* Packaged sound engine binaries support SSE-only or AVX+FMA optimizations.

### Documentation
* Latest API reference documentation is auto generated: https://tim-janik.github.io/docs/anklang/index.html

### Fixed Bugs

- Choices are not rendered properly in device view [\#59](https://github.com/tim-janik/anklang/issues/59)
- Focus, shadow DOM, submenus, buttons and input elements [\#55](https://github.com/tim-janik/anklang/issues/55)
- Unify and fixate srcdir and builddir [\#54](https://github.com/tim-janik/anklang/issues/54)
- UI websocket death on invalid UTF-8 [\#49](https://github.com/tim-janik/anklang/issues/49)
- Insertion order doesn't work, every device is inserted at end [\#46](https://github.com/tim-janik/anklang/issues/46)
- Inserting Reverb after BlepSynth has no effect [\#24](https://github.com/tim-janik/anklang/issues/24)

### Closed Issues

- Build breaks: cp: ./ls-tree.lst: No such file or directory [\#18](https://github.com/tim-janik/anklang/issues/18)
- Freeverb Default Settings [\#23](https://github.com/tim-janik/anklang/issues/23)
- LV2: currently we need a X11 display since the Gtk Thread is unconditionally started [\#35](https://github.com/tim-janik/anklang/issues/35)
- LV2: unique device name generation duplicated with CLAP [\#34](https://github.com/tim-janik/anklang/issues/34)
- loop player [\#39](https://github.com/tim-janik/anklang/issues/39)
- Negative frame offsets in MidiEvent [\#26](https://github.com/tim-janik/anklang/issues/26)
- Shebangs in \*.sh files are wrong [\#17](https://github.com/tim-janik/anklang/issues/17)

### Merged Pull Requests

- Ase/combo.cc: fix combo insertion order for devices inserted at end [\#47](https://github.com/tim-janik/anklang/pull/47) ([swesterfeld](https://github.com/swesterfeld))
- DEVICES: blepsynth/blepsynth.cc: introduce flexible ADSR model [\#6](https://github.com/tim-janik/anklang/pull/6) ([swesterfeld](https://github.com/swesterfeld))
- DEVICES: blepsynth: remove design/test tools \(moved to dsp-research repo\) [\#30](https://github.com/tim-janik/anklang/pull/30) ([swesterfeld](https://github.com/swesterfeld))
- Fix typo in ch-development.md [\#58](https://github.com/tim-janik/anklang/pull/58) ([ritschwumm](https://github.com/ritschwumm))
- Freeverb merge dry/wet into mix parameter [\#45](https://github.com/tim-janik/anklang/pull/45) ([swesterfeld](https://github.com/swesterfeld))
- Midi events uint offset [\#43](https://github.com/tim-janik/anklang/pull/43) ([swesterfeld](https://github.com/swesterfeld))

### Synthesis Devices
* Fixed race condition in LiquidSFZ device loader thread. [swesterfeld](https://github.com/swesterfeld)
* Improved Freeverb device with sample accurate parameter changes, mix optimization, and smoothing for automation. [swesterfeld](https://github.com/swesterfeld)
* Added support for String properties in AudioProcessor.
* Fixed midi event frame offsets to be unsigned integers, resolving issue #26. [[swesterfeld](https://github.com/swesterfeld)
* Integrated swesterfeld/liquidsfz submodule for SFZ support in ASE. [swesterfeld](https://github.com/swesterfeld)
* Moved blepsynth design/test tools to the dsp-research repository. [swesterfeld](https://github.com/swesterfeld)
* Improved sample accuracy in Blepsynth synth device, fixed bugs. [swesterfeld](https://github.com/swesterfeld)
* Added Saturation device with parameter smoothing and performance optimizations. [swesterfeld](https://github.com/swesterfeld)
* Introduced flexible ADSR model for volume envelope in Blepsynth device. [swesterfeld](https://github.com/swesterfeld)
* Added Sallen-Key Filter to Blepsynth device, improved LadderVCF filter. [swesterfeld](https://github.com/swesterfeld)

### ASE
* New CLI options -M and -P to override audio and midi drivers.
* Added debug/info/error logging to a log file.
* Support gzip file compression for http transfers, pre-compressed all source maps.
* Added Member<> template to expose C++ member field as API to JavaScript.
* Default tempo set to 120 BPM, playback starts with transport tempo.
* Fixed loading from non-anklang directories.

### User Interface
* Introduced a project state grep dialog accessible via Alt+F12.
* Consolidated UI components in various places and removed redundant code.
* Replacing error confirmation dialogs with notice popups.
* Simplified and converted most UI components to LitElement components.
* Updated UI components to use a new tree browser component.
* Added support for file selection in device UIs.
* Introduced JavaScript Signal polyfill to simplify for dependency tracking.
* Fixed JavaScript focus handling, error formatting, and type checking.
* Added Tailwind CSS for styling, removed normalize.scss.
* Fixed piano roll drawing order, shifted cursor movement, and other UI fixes. [swesterfeld](https://github.com/swesterfeld)
* Improved UTF-8 support for legacy filenames, including mapping to private use area.
* Fixed mouse wheel handling in modern Chrome and Firefox browsers.
* Switched file browser to use `<grid/>` component for faster rendering.

### Testing
* CI, docker, Dockerfile fixups and improvements.
* Introduced a new test for track renaming in the X11 UI test suite.
* Enhanced the x11test-v to run silently by default, with optional verbose output.
* Added support for replay of multiple JSON files in X11 tests, upgraded electron and puppeteer dependencies.
* Parallelized automated test suite runs.

## Miscellaneous
* Various code cleanups, package dependency and include fixes.
* Switched to C++20, improved build system for better cross-platform support.
* Fixed WebSocket errors and improved callback handling in the API server.
* Switched to using git submodules for external dependencies.
* All public domain sources are dedicated via the Unlicense.
* Remove icon-gen, sharp dependencies, generate UI icons using mogrify.

### Contributors
* [@ritschwumm](https://github.com/ritschwumm) made his first contribution in https://github.com/tim-janik/anklang/pull/58
* [@swesterfeld](https://github.com/swesterfeld) continued his excellent device work.


## Anklang 0.2.0

### Hardware and System Requirements
* Linux - the Anklang deb and AppImage are based on Ubuntu 20.04.
* Packaged sound engine binaries support SSE-only or AVX+FMA optimizations.

### Documentation
* Integrated documentation from JS components into the user manual and API reference.
* Various improvements to the documentation and architecture descriptions, as well as manual refinements were made.
* Added continuous API reference documentation generation to the CI: https://tim-janik.github.io/docs/anklang/index.html
* Integrated JavaScript docs into Poxy docs and search (API reference).
* Added dedicated documentation sections for the ClipList, PartList and PianoRoll.
* Integrate PDF manual builds using TeX, automated via CI.
* Added integration for JsDoc and TypeScript annotations.

### Audio Synthesis
* Added support for clap_event_transport_t, fixes #5. [stw]
* Implemented non-linear mapping for the BlepSynth ADSR times. [stw]
* Fix BlepSynth cutoff frequency modulation for its filter, pass frequencies in Hz. [stw]
* Implemented the CLAP draft extension for file references.
* Incorporated Freeverb by Jezar at Dreampoint with damping mode fixes.
* Added a Jack PCM driver based on Stefan Westerfelds code and howto.

### ASE
* Automatically stop audio playback with the new `-t time` command line argument.
* Added `--class-tree` to print out the class inheritance tree.
* Added `-o audiofile` support for capturing output into WAV, OPUS and FLAC files.
* Added `--blake3 FILE` to test Blake3 hashing.
* Added SortedVector to unify several vector sorting code paths.
* Fix potential undefined behavior in the Pcg32Rng code, when left shifting a 32bit value by 32 bits. [stw]
* Added various code cleanups and fixed imports.
* Adjusted main loop PollFD handling to avoid engine hangs after UI exit.
* Added Loft, a lock- and obstruction-free concurrent memory allocator.
* Added a very fast multiply-with-carry PRNG, with a period of 2^255.
* Fixed atexit memory leaks, proper handling of nullptrs and compiler sanitizers.
* Implemented single Project/Engine relation and new API for activating and deactivating projects.
* Properly implemented garbage collection of remote JSONIPC handles.
* Improved performance by prefaulting heap and stack pages and malloc tuning.
* Enabled link time optimization for production builds.
* Implemented low-latency scheduling for the synthesis engine via sched_* or RtKit.
* Implemented remote reference counting for the JSONIPC API.
* Introduced automatic backup creation when saving project files.
* Improved serialization for projects, allowing dedicated resource file inclusion.
* Integrated building of releases with SSE + FMA support into ASE.
* Updated the code to compile cleanly with g++-12, libstdc++-13 and clang++-15.

### User Interface
* Added support for context help via F1 key in Lit components (handling shadowRoots)
* Fix flashing of unstyled content in Chrome and layout artefacts during context menu popups.
* Adapted mouse wheel normalization and sensitivity to modern browsers.
* Improved tooltips and change notification handling on properties.
* Added support for moving and copying (with CTRL key) piano roll notes using the mouse. #16 [stw]
* Support resizing multiple selected notes at once by dragging with the mouse in piano roll view. #15 [stw]
* Unified all (non-Vue) CSS styles into a single build and simplified its structure.
* Updated all icon sets and UI fonts.
* Implemented caching for static assets like stylesheets and invalidation handling via content hashes.
* Added a new warm grey palette based on ZCAM Jz (lightness) steps, which allows subtler color gradients to enhance overall appearance.
* Added the latest modern-normalize.css style resets to improve consistency across browsers.
* Added TypeScript annotations and type checking to improve UI JavaScript code quality.
* Added support for state toggling using LEFT/RIGHT/UP/DOWN keys to Switch inputs.
* Switched the main font to the variable font variant of InterDisplay.
* Implemented simple and fast SCSS extraction of JavaScript css`` literals.
* Moves SCSS snippet processing to postcss at build time.
* Implemented a reliable reactive wrapper that keeps Lit components and C++ components in sync.
* Added tracklist scrolling and improved tracklist styling.
* Added sound level meter rendering from telemetry data in the Track View component.
* Introduced caching of (remote) properties across several different web components.
* Improved piano roll layout by moving to HTML grid.
* Rewrote and extended all piano roll note editing tools.
* Reimplemented the Knob component to use PNG sprites instead of SVG layers.
* Added support for Zoom In/Out/Reset functionality in the menubar.
* Improved support for click-drag menu item selection in various menus, including track view, piano roll and device panel.
* Fixed Vue 3.2.28 breaking $forceUpdate() calls before mounting.
* Several web components were ported from Vue to Lit to simplify code and better efficiency.
* Eliminated shadowRoot uses for most components, saving resources.

### Packaging
* Added fix so all AppImage executable properly operate $ORIGIN relative.
* Fixed missing dependencies in AppImage builds so all needed libraries are included.
* Improved version handling and added missing Git info to distribution tarballs.
* Streamlined asset builds and other technical aspects of the release process.
* Ensure release assets are always built in the same reproducible environment.
* Started regular Nightly releases via the CI toolchain on every significant trunk merge commit.

### Testing
* Introduced end-to-end testing using X11 with headless runs.
* Added replay functionality via Puppeteer of DevTools event recordings.
* Added clang-tidy to the CI rules to improve code quality
* Added regular Arch Linux test builds.


## Anklang 0.1.0

### System Requirements
* Linux - Ubuntu 20.04 is needed to run the Anklang AppImage

### Hardware Support
* Build and package a second sound engine binary with AVX & FMA optimizations.

### Documentation
* Extended documentation in many places.
* Improved copyright listing of all source files involved.
* Provide user documentation as anklang-manual.pdf.
* Provide developer documentation as anklang-internals.pdf.

### User Interface
* Improve UI responsiveness when handling async API calls.
* Support proper note selection sets in the piano roll.
* Introduced Undo/Redo stack for piano roll changes.
* Use batch processing to responsively handle thousands of notes.
* Support shortcut editing for piano roll functions.
* Added Cut/Copy/Paste to piano roll.
* Added play position indicator to piano roll.
* Tool selection in piano roils now works on hover.
* Notes moved in the piano roll now properly bounce against edges.
* Selection in the piano roll now supports SHIFT and CONTROL.
* Clips can now store notes with velocity=0.
* Migrated CSS processing to postcss.
* Fix file path handling for project load and save.
* Shortend nicknames are now auto-derived for external plugins.
* Support loading of command line files in Anklang.
* Add MIME support for starting Anklang for *.anklang files.

### Synthesis
* Support single clip looping (very rudimentary), to be extended later.
* Add Gtk+-2 dynlib to provide a wrapper window for plugin UIs.
* Add support for CLAP-1.0.2 plugin loading and processing, the following
  CLAP extensions are currently implemented:
  LOG, GUI, TIMER_SUPPORT, THREAD_CHECK, AUDIO_PORTS, PARAMS, STATE,
  POSIX_FD_SUPPORT, AUDIO_PORTS_CONFIG, AUDIO_PORTS, NOTE_PORTS.

### Internals
* Provide infrastructure for future piano roll scripting.
* Support lean UI component implementations with lit.js.
* Use ZCAM color model to design/saturate/etc colors of the UI.
* Updated various third party components.
* Use Electron-18.3.5 as basis for the UI.
* Use adaptive ZSTD compression for project storage.
* Use fast ZSTD compression for binary snapshots in  Undo/Redo steps.
* Support sound engine blocks up to 2k.
* Adjust block sizes to reduce PulseAudio overhead.
* Keys matching in ASE_DEBUG is now case insensitive.
* Anklang can now be started with '--dev' to force open DevTools.

### Other
* License clarification, the project uses MPL-2.0.
* Improved reproducible dockerized builds.
* Fixed dependencies of the Debian packages. #3


## Anklang 0.0.1-alpha1

### System Requirements
* Linux - Ubuntu 20.04 is needed to run the Anklang AppImage

### Packaging
* Build AppImage binaries with linuxdeploy, appimage-runtime-zstd
* Add build script for Debian package creation

### Hardware Support
* Add ALSA MIDI and ALSA PCM drivers
* Support playback of MIDI input events
* Support ALSA device selection via UI preferences
* Prioritize drivers based on headset/USB/HDMI/Duplex properties

### Documentation
* Add manpage (markdown), build documentation with pandoc2
* Add Anklang manual (markdown), support Latex based PDF builds
* Extract Javascript documentation with jsdoc
* Allow MathJax in the Anklang manual
* Add comprehensive 'copyright' file generation

### User Interface
* Support Web browser based UI (Firefox, Chrome)
* Suppport Electron based UI, test via 'make run'
* Add Json, ZIP and zstd based storage of Anklang projects
* Add UI support for modal dialogs with focus capturing
* Add UI for file load and save dialogs in the browser
* Support the XDG directory specification
* Add an Anklang logo favicon
* Add support for fly-over popups to alert users
* Support UI tooltips and hints in the status bar
* Add Vue3 and Electron based UI, use scss for styles
* Work around Chrome's movementX/Y devicePixelRatio bug Chrome#1092358

### Synthesis
* Use seperate thread for digital synthesis
* Integrate resampler2 code from Stefan Westerfeld
* Integrate blepsynth module from Stefan Westerfeld
* Add support for synchronized multi-track playback

### Interprocess Communication
* Provide updated JsonIpc layer for remote calls and event delivery
* Send realtime status updates as int/float blobs via IPC (termed 'telemtry')
* Support realtime UI updates via IPC by receiving telemetry at up to 60Hz

### Build System
* Support automated Github CI via docker based builds
* Support Javascript and C++ linting
* Notify #Anklang on libera.chat about CI build results
* Integrate Inter font, ForkAwesome, material-icons and anklangicons-201123
* Integrate external sources from websocketpp, rapidjson, minizip-ng
* Add robust support for multiple targets for one Make rule
* Support live editing and reload via 'make serve'
* Employ fast build GNU Make based build setup


## Anklang 0.0.0:

Initial code import from Tim and Stefan
