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
