# Program Installation

## Download and Startup

Ankang can be downloaded as binary package for GNU/Linux from
its Github release page:

https://github.com/tim-janik/anklang/releases/

To test bleeding edge versions, "Nightly" builds are also available,
but mileage may very with these. In particular stability or file format
compatibility can be affected in the nightly versions.

The Debian package (`.deb`) can be installed with tools like `gnome-software`
or via the command line:

```sh
# Install Anklang system wide
sudo apt install Downloads/anklang_[VERSION]_amd64.deb
# Start via system PATH
anklang
```

The AppImage package can be executed without an installation step.
Once downloaded, it needs permissions for execution and can be run
right away. This is done for instance as follow:

- Once downloaded, open ~/Downloads/ for instance via web browser → 'Show in Folder'
- Right click on the AppImage to open the file manager context menu
- Select 'Permission' → 'Execute' → [x] Allow executing file as program
- After that, the AppImage can be started directly from the file manager

Of course, the same can also be done via the command line:

```sh
# Enable execution of the AppImage
chmod +x ~/Downloads/anklang-[VERSION]-x64.AppImage
# Run Anklang via self contained AppImage
~/Downloads/anklang-[VERSION]-x64.AppImage
```

## Driver Setup

Once Anklang is started, drivers and other settings can be adjusted
in the Preferences. The 'File Menu' in the upper left includes an
item to open the 'Preferences' dialog.

Depending on the system, a number of PCM and MIDI devices can be selected
here. The "PulseAudio Sound Server" or "Automatic driver selection" should
work on most systems out of the box.
On systems with Jackd installed, the Jackd sound server needs to be
running first:

```sh
# Suspend PulseAudio and run Jackd instead
# -r avoids realtime mode which requires special rights
# -d alsa uses the Jackd ALSA backend
pasuspender -- jackd -r -d alsa
```

The Jackd server should be running now, so in Anklang the
"JACK Audio Device" can be selected.
To verify Anklang does indeed use Jackd, the setup can be
inspected as follows:

```sh
# List active Jack ports
jack_lsp
# The list should include 'AnklangSynthEngine'

# Start GUI to control Jackd
qjackctl
# Click on the "Graph" button to show connections
# The graph should include 'AnklangSynthEngine'
```
