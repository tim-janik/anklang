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
