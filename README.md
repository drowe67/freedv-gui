# Building FreeDV GUI

This document describes how to build the FreeDV GUI program for various operating systems.  FreeDV GUI is developed on Ubuntu Linux, and then cross compiled for Windows using Fedora Linux (Fedora has great cross compiling support) and Docker.

## Further Reading

  * http://freedv.org - introduction, documentation, downloads
  * [FreeDV GUI User Manual](USER_MANUAL.md)
  
## Installing prerequisites on Ubuntu Linux

  ```
  $ sudo apt install libspeexdsp-dev libsamplerate0-dev sox git \
  libwxgtk3.2-dev libhamlib-dev libasound2-dev libao-dev \
  libgsm1-dev libsndfile1-dev cmake module-assistant build-essential \
  autoconf automake libtool libebur128-dev
  $ git clone https://github.com/drowe67/freedv-gui.git
  $ cd freedv-gui

  (if using pipewire/PulseAudio -- recommended and the default) 
  $ sudo apt install libpulse-dev
  
  (if using PortAudio)
  $ sudo apt install portaudio19-dev
  ```

  (Depending on release you may need to use `libwxgtk3.0-gtk3-dev` instead of `libwxgtk3.2-dev`.)
  
## Installing prerequisites on Fedora Linux

  ```
  $ sudo dnf groupinstall "Development Tools"
  $ sudo dnf install cmake wxGTK3-devel libsamplerate-devel \
    libsndfile-devel speexdsp-devel hamlib-devel alsa-lib-devel libao-devel \
    gsm-devel gcc-c++ sox autoconf automake libtool libebur128-devel
  $ git clone https://github.com/drowe67/freedv-gui.git
  $ cd freedv-gui

  (if using pipewire/PulseAudio -- default and recommended)
  $ sudo dnf install pulseaudio-libs-devel

  (if using PortAudio)
  $ sudo dnf install portaudio-devel
  ```

## Running FreeDV on Linux

1. Install PyTorch, TorchAudio and matplotlib Python packages. Some distros have packages for one or more of these,
   but you can also use pip in a Python virtual environment (recommended to ensure the latest versions):

   ```
   $ cd freedv-gui
   $ python3 -m venv rade-venv
   $ . ./rade-venv/bin/activate
   (rade-venv) $ pip3 install torch --index-url https://download.pytorch.org/whl/cpu
   (rade-venv) $ pip3 install matplotlib
   ```

   *Note: you may need to install `python3-venv` or your distro's equivalent package in order to create Python virtual environments. Python 3.9+ is also required for PyTorch to work.*

2. Build FreeDV to make sure the correct dependencies are linked in (namely numpy):

   ```
   (rade-venv) $ pwd
   /home/<user>/freedv-gui
   (rade-venv) $ ./build_linux.sh
   ```

3. Make sure FreeDV can find the ML model:

   ```
   (rade-venv) $ pwd
   /home/<user>/freedv-gui
   (rade-venv) $ cd build_linux
   (rade-venv) $ ln -s $(pwd)/rade_src/model19_check3 model19_check3
   ```

4. Execute FreeDV:

   ```
   (rade-venv) $ pwd
   /home/<user>/freedv-gui/build_linux
   (rade-venv) $ export GDK_BACKEND=x11 # optional, see (*) below
   (rade-venv) $ PYTHONPATH="$(pwd)/rade_src:$PYTHONPATH" src/freedv
   ```

(*) If your Linux distribution and/or desktop environment uses Wayland, FreeDV will always open in the middle 
of the screen, regardless of where you positioned it before. You can avoid this by having FreeDV run as an 
X11 application instead using XWayland (`GDK_BACKEND=x11`).

Alternatively, you can use [this script](https://github.com/barjac/freedv-rade-build) developed by 
Barry Jackson G4MKT to automate the above steps. While the FreeDV project thanks him for his contribution
to helping Linux users more easily get on the air with FreeDV, the FreeDV development team will not provide 
support. All support inquiries regarding this script should be directed to the linked repo.

## Audio driver selection

By default, FreeDV uses the native audio APIs on certain platforms. These are as follows:

| Platform | Audio API |
|---|---|
| macOS | Core Audio |
| Linux | pipewire (via PulseAudio library) |
| Windows | WASAPI |

On platforms not listed above, PortAudio is used instead. PortAudio can also be explicitly selected by the
user by defining the environment variable `USE_NATIVE_AUDIO=0` before running the `build_*.sh` script
(or specifying `-DUSE_NATIVE_AUDIO=0` to `cmake`).

## Installing on Linux

You need to install the codec2 shared libraries, and freedv-gui:
  ```
  $ cd ~/freedv-gui/codec2/build_linux
  $ sudo make install
  $ cd ~/freedv-gui/build_linux
  $ sudo make install
  $ sudo ldconfig
  ```
 
## Testing

The ```wav``` directory contains test files of modulated audio that you can use to test FreeDV (see the [USER_MANUAL](USER_MANUAL.md)).

## Building for Windows

Windows releases are built using the LLVM version of MinGW. This allows
one to build FreeDV for ARM as well as for Intel Windows systems.

### Prerequisites

* CMake >= 3.25.0
* Linux (tested on Ubuntu 22.04)
    * *NOTE: This does not currently work on macOS due to CMake using incorrect library suffixes.*
* NSIS for generating the installer (for example, `sudo apt install nsis` on Ubuntu)

### Instructions

1. Download LLVM MinGW at https://github.com/mstorsjo/llvm-mingw/releases/.
2. Decompress into your preferred location. For example: `tar xvf llvm-mingw-20220906-ucrt-ubuntu-18.04-x86_64.tar.xz` (The exact filename here will depend on the file downloaded in step (1). Note that for best results, you should use a build containing "ucrt" in the file name corresponding to the platform which you're building the Windows binary from.)
3. Add LLVM MinGW to your PATH: `export PATH=/path/to/llvm-mingw-20220906-ucrt-ubuntu-18.04-x86_64/bin:$PATH`. (The folder containing the LLVM tools is typically named the same as the file downloaded in step (2) minus the extension.)
4. Create a build folder inside freedv-gui: `mkdir build_windows`
5. Run CMake to configure the FreeDV build: `cd build_windows && cmake -DCMAKE_TOOLCHAIN_FILE=${PWD}/../cross-compile/freedv-mingw-llvm-[architecture].cmake ..`
   * Valid architectures are: aarch64 (64 bit ARM), i686 (32 bit Intel/AMD), x86_64 (64 bit Intel/AMD)
6. Build FreeDV as normal: `make` (You can also add `-j[num]` to the end of this command to use multiple cores and shorten the build time.)
7. Create FreeDV installer: `make package`

## Building and installing on macOS

Using MacPorts, most of the appropriate dependencies can be installed by:

```
$ sudo port install automake git libtool sox +universal cmake wget pkgconf
```

and on Homebrew:

```
$ brew install automake libtool git sox cmake wget pkgconf
```

Once the dependencies are installed, you can then run the `build_osx.sh` script inside the source tree to build
FreeDV and associated libraries (codec2, hamlib). A FreeDV.app app bundle will be created inside the build_osx/src
folder which can be copied to your system's Applications folder.

*Note: for distribution, code signing is required. The following commands can be run to enable this:*

```
CODESIGN_IDENTITY=[identity in your keychain] UNIV_BUILD=1 ./build_osx.sh
cd build_osx
make release
```

