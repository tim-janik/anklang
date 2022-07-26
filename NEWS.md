## Anklang 0.1.0

#### System Requirements
* Linux - Ubuntu 20.04 is needed to run the Anklang AppImage

#### Hardware Support
* Build and package a second sound engine binary with AVX & FMA optimizations.

#### Documentation
* Extended documentation in many places.
* Improved copyright listing of all source files involved.
* Provide user documentation as anklang-manual.pdf.
* Provide developer documentation as anklang-internals.pdf.

#### User Interface
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

#### Synthesis
* Support single clip looping (very rudimentary), to be extended later.
* Add Gtk+-2 dynlib to provide a wrapper window for plugin UIs.
* Add support for CLAP-1.0.2 plugin loading and processing, the following
  CLAP extensions are currently implemented:
  LOG, GUI, TIMER_SUPPORT, THREAD_CHECK, AUDIO_PORTS, PARAMS, STATE,
  POSIX_FD_SUPPORT, AUDIO_PORTS_CONFIG, AUDIO_PORTS, NOTE_PORTS.

#### Internals
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

#### Other
* License clarification, the project uses MPL-2.0.
* Improved reproducible dockerized builds.
* Fixed dependencies of the Debian packages. #3


## Anklang 0.0.1-alpha1

#### System Requirements
* Linux - Ubuntu 20.04 is needed to run the Anklang AppImage

#### Packaging
* Build AppImage binaries with linuxdeploy, appimage-runtime-zstd
* Add build script for Debian package creation

#### Hardware Support
* Add ALSA MIDI and ALSA PCM drivers
* Support playback of MIDI input events
* Support ALSA device selection via UI preferences
* Prioritize drivers based on headset/USB/HDMI/Duplex properties

#### Documentation
* Add manpage (markdown), build documentation with pandoc2
* Add Anklang manual (markdown), support Latex based PDF builds
* Extract Javascript documentation with jsdoc
* Allow MathJax in the Anklang manual
* Add comprehensive 'copyright' file generation

#### User Interface
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

#### Synthesis
* Use seperate thread for digital synthesis
* Integrate resampler2 code from Stefan Westerfeld
* Integrate blepsynth module from Stefan Westerfeld
* Add support for synchronized multi-track playback

#### Interprocess Communication
* Provide updated JsonIpc layer for remote calls and event delivery
* Send realtime status updates as int/float blobs via IPC (termed 'telemtry')
* Support realtime UI updates via IPC by receiving telemetry at up to 60Hz

#### Build System
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
