# FreeDV GUI README.txt

This document describes how to build the FreeDV GUI program for
various operating systems.  See also:

  * http://freedv.org - introduction, documentation, downloads
  * RELEASE_NOTES.txt - changes made to each version
  * USER_MANUAL.txt   - FreeDV GUI Manual

# Building and installing on Ubuntu Linux (16-18)
  ```
  $ sudo apt install libc6-i386 libspeexdsp-dev libsamplerate0-dev sox git \
  libwxgtk3.0-dev portaudio19-dev libhamlib-dev libasound2-dev libao-dev \
  libgsm1-dev libsndfile-dev

  $ ./build_ubuntu.sh
  ```

# Building and installing on Fedora Linux
  ```
  $ sudo dnf groupinstall "Development Tools"
  $ sudo dnf install icmake wxGTK3-devel portaudio-devel libsamplerate-devel \
    libsndfile-devel speexdsp-devel hamlib-devel alsa-lib-devel libao-devel \
    gsm-devel
  $ ./build_ubuntu.sh
  ```
  
# Run FreeDV:
   ```
   $ ./build_linux/src/freedv
   ```

## Building for Windows on Fedora (Cross compiling)

Install basic MinGW development packages:
```
  $ sudo dnf install mingw{32,64}-filesystem mingw{32,64}-binutils \
    mingw{32,64}-gcc mingw{32,64}-crt mingw{32,64}-headers mingw32-nsis
```

Install dependencies of FreeDV/Codec2:
```
  $ sudo dnf install mingw{32,64}-speex
```

Enable Freetel specific packages not currently in Fedora proper:
```
  $ sudo dnf install dnf-plugins-core
  $ sudo dnf copr enable hobbes1069/mingw
  $ sudo dnf install mingw{32,64}-wxWidgets3 mingw{32,64}-hamlib \
    mingw{32,64}-portaudio mingw{32,64}-libsndfile
```

Bootstrap codec2 and LPCNet:
This assumes all git checkouts are from your home directory.
```
  $ git clone https://github.com/drowe67/codec2.git
  $ cd codec2 && mkdir build_win && cd build_win
  $ mingw64-cmake ../
  $ make
  $ cd
  $ git clone https://github.com/drowe67/LPCNet.git
  $ cd LPCNet && mkdir build_win && cd build_win
  $ mingw64-cmake ../ -DCODEC2_BUILD_DIR=~/codec2/build_win
  $ make
  $ cd ~/codec2/build_win
  $ mingw64-cmake ../ -DLPCNET_BUILD_DIR=~/LPCNet/build_win
  $ make
```

Building FreeDV for 64 Bit windows:
```
  $ cd
  $ git clone https://github.com/drowe67/freedv-gui.git
  $ cd freedv-gui && mkdir build_wins && cd build_win
  $ mingw64-cmake ../ -DCODEC2_BUILD_DIR=~/codec2/build_win -D LPCNET_BUILD_DIR=~/LPCNet/build_win
  $ make
  $ make package
```
  
## Building and installing on Windows

The windows build is similar to linux and follows the same basic workflow,
however, while codec2 and FreeDV (freedv) build well on windows, some of the
dependencies do not. For that reson current windows releases are cross-compiled
from linux.

Only MinGW is supported. While it is likely possible to perform a pure MinGW
build, installing MSYS2 will make your life easier.

CMake may not automatically detect that you're in the MSYS environment. If this
occurs you need to pass cmake the proper generator:
```
  $ cmake -G"MSYS Makefiles" [other options] </path/to/source>
```

## Bootstrapping wxWidgets build

If wxWidgets (>= 3.0) is not available then one option is to have CMake boot-
strap the build for FreeDV.

This is required because the tool wx-config is used to get the correct compiler
and linker flags of the wxWidgets components needed by FreeDV. Since this is
normally done at configure time, not during "make", it is not possible for CMake
or have this information prior to building wxWidgets.

In order to work around this issue you can "bootstrap" the wxWidgets build using
the CMake option, "BOOTSTRAP_WXWIDGETS". wxWidgets will be built using static
libraries.

NOTE: This forces "USE_STATIC_WXWIDGETS" to be true internally regarless of the
value set manually.

(from any directory, but empty directory outside of the source is prefered.)
```
$ cmake -DBOOTSTRAP_WXWIDGETS=TRUE /path/to/freedv
$ make
(wxWidgets is downloaded and built)
$ cmake .
(wxWidgets build should be detected)
$ make
(if all goes well, as root)
$ make install
```

## Building and installing on OSX

Please see README.osx

## Building and installing on FreeBSD

In ```build_ubuntu.sh``` change build_linux to build_freebsd

## TODO

This software needs some maintenance and refactoring.  Patches
welcome!

- [ ] Remove unnecessary PortAudio Wrapper layer
- [ ] break fdmdv2_main.cpp into smaller files
- [ ] rename src files from fdmdv2_ -> freedv_
- [ ] profile code and explore optimisations
- [ ] The FreeDV waterfall doesn't pick up fast fading as well as
    some other SDRS, would be good to review the code and
    adjust
- [ ] Align license text in source file with LGPL
- [ ] GTK warning on fedora 28 when vert size of window so small we lose
      PTT button
- [ ] Play a file when we get sync, like "alarm"
