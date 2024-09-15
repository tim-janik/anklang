Welcome to Anklang, a free and open-source digital audio synthesizer and music creation software.

## Anklang: Craft Your Audio, Live and Free

Anklang is a music synthesis and composition program designed to help you create and shape your sounds,
released under the Mozilla Public License 2.0 ([MPL-2.0 summary](https://choosealicense.com/licenses/mpl-2.0/)).
Join us in collaborating on Anklang, an experimental, digital audio workstation for live music composition and audio synthesis.
Join the journey, contribute, and help us improve it.

## Getting Started

Getting started with Anklang is easy.
Begin by downloading the latest version from our GitHub release page.
You can choose from a variety of binary packages for GNU/Linux, including AppImage and Debian packages.

[Anklang Releases · tim-janik/anklang](https://github.com/tim-janik/anklang/releases/)

If you want to try out the latest features and bug fixes, you can use our nightly release builds.
Keep in mind that these builds might be less stable than the regular release builds.

### Installing Anklang

#### Using the AppImage Package

1. Download the AppImage package from our GitHub release page.
2. Once downloaded, open the file manager and navigate to the `Downloads` folder.
3. Right-click on the AppImage file and select `Permissions` → `Execute` → `[x] Allow executing file as program`.
4. After that, the AppImage can be started directly from the file manager.

Alternatively, you can use the command line:

```sh
# Enable execution of the AppImage
chmod +x ~/Downloads/anklang-[VERSION]-x64.AppImage
# Run Anklang via self contained AppImage
~/Downloads/anklang-[VERSION]-x64.AppImage
```

#### Using the Debian Package

1. Install the Debian package using tools like `gnome-software` or via the command line:

```sh
# Install Anklang system wide
sudo apt install Downloads/anklang_[VERSION]_amd64.deb
# Start via system PATH
anklang
```

### First Run and Driver Setup

Once Anklang is started, drivers and other settings can be adjusted in the `Preferences`.
The `File Menu` in the upper left includes an item to open the `Preferences` dialog.

Depending on your system, a number of PCM and MIDI devices can be selected here.
The `PulseAudio Sound Server` or `Automatic driver selection` should work on most systems out of the box.

For systems with Jackd installed, the Jackd sound server needs to be running first:

```sh
# Suspend PulseAudio and run Jackd instead
# -r avoids realtime mode which requires special rights
# -d alsa uses the Jackd ALSA backend
pasuspender -- jackd -r -d alsa
```

The Jackd server should be running now, so in Anklang the "JACK Audio Device" can be selected.

To verify Anklang does indeed use Jackd, the setup can be inspected as follows:

```sh
# List active Jack ports
jack_lsp
# The list should include 'AnklangSynthEngine'

# Start GUI to control Jackd
qjackctl
# Click on the "Graph" button to show connections
# The graph should include 'AnklangSynthEngine'
```

### Next Steps

Now that you've installed and run Anklang for the first time, you can explore the various features and tutorials to help you get started with music synthesis and composition.

Refer to the following documentation sections for more information on using Anklang,
including tutorial material, Howto descriptions, Man pages, API reference, file formats and conventions,
and development details.

Contribute to Anklang by submitting pull requests or via the
[Ideas & Roadmap Discussions](https://github.com/tim-janik/anklang/issues/52)
in the Anklang issue tracker.


Happy creating with Anklang!
