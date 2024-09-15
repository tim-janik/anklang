# Howto: Setting Up Audio Devices in Anklang on Linux

Anklang relies on a properly configured audio system to function correctly.
This guide will walk you through the steps to ensure your audio devices are set up and working with Anklang on Linux.

### Step 1: Verify Audio Device Drivers

Most Linux systems come with ALSA (Advanced Linux Sound Architecture) device drivers for audio devices.
Additionally, they usually run either [Pulseaudio](https://wiki.archlinux.org/title/PulseAudio) or [Pipewire](https://docs.pipewire.org/) as their sound system.
Make sure your audio devices are properly detected and configured on your system.
You can do this by:

* Configuring audio devices via GNOME ++"Settings"++ → ++"🎵 Sound"++ (if you're using GNOME)
* Running [`pavucontrol`(1)](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Users/Troubleshooting/#baselinetest) from the terminal to adjust audio settings

### Step 2: Adjust Audio Device Volume and Microphone Sensitivity

Use [`alsamixer`(1)](https://man.archlinux.org/man/alsamixer.1) from the terminal to adjust audio device volume and microphone sensitivity:

* Press ++"F6"++ to select the sound card
* Press ++"F5"++ to display all channels
* Use cursor keys to navigate and adjust volume
* Press ++"M"++ to mute/unmute channels

### Step 3: Configure Anklang Audio Preferences

Once your audio devices are confirmed to work correctly, start Anklang and open the ++"🖿"++ "File" menu. Select ++"Preferences"++ (or press ++"Ctrl-,"++) to access the preferences dialog. All detected devices and some special entries are in the  the "PCM driver" menu:

* ++"PulseAudio Sound Server"++ or a similar entry will appear if a user space sound system is detected.
* ++"Null PCM Driver"++ will simulate an audio device that cannot be listened to (useful for testing or development)
* ++"Automatic Driver Selection"++ will first look for a running user space sound system like PulseAudio/Pipewire and route sound through it. If that's not available, the first non-busy sound device is automatically selected (USB and onboard devices being preferred over HDMI devices)
* Individual devices can also be selected directly from the menu

### Step 4: Select and Test Your Audio Device

Choose your desired audio device from the "PCM driver" drop-down menu. Changing the audio device selection has an instant effect. To test, load a (demo) song and confirm playback works through the selected device. If you encounter issues, ensure your audio devices are properly configured in your Linux system (Step 1 and 2).

By following these steps, you should be able to successfully set up your audio devices in Anklang on Linux. If you still encounter problems or have questions, please don't hesitate to
[report them](https://github.com/tim-janik/anklang/issues) or [join the discussions](https://github.com/tim-janik/anklang/discussions).

Happy creating with Anklang!
