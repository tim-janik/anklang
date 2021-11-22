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
