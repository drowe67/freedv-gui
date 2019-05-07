 # Building FreeDV GUI

This document describes how to build the FreeDV GUI program for
various operating systems.  FreeDV GUI is developed on Ubuntu Linux,
and then cross compiled for Windows using Fedora Linux (Fedora has great
cross compiling support).

# Further Reading

  * http://freedv.org - introduction, documentation, downloads
  * RELEASE_NOTES.txt - changes made to each version
  * USER_MANUAL.txt   - FreeDV GUI Manual

# Building and installing on Ubuntu Linux (16-18)
  ```
  $ sudo apt install libc6-i386 libspeexdsp-dev libsamplerate0-dev sox git \
  libwxgtk3.0-dev portaudio19-dev libhamlib-dev libasound2-dev libao-dev \
  libgsm1-dev libsndfile-dev
  $ ./build_linux.sh
  ```

# Building and installing on Fedora Linux
  ```
  $ sudo dnf groupinstall "Development Tools"
  $ sudo dnf install cmake wxGTK3-devel portaudio-devel libsamplerate-devel \
    libsndfile-devel speexdsp-devel hamlib-devel alsa-lib-devel libao-devel \
    gsm-devel
  $ ./build_linux.sh
  ```
  
# Run FreeDV:
   ```
   $ ./build_linux/src/freedv
   ```

The ```wav``` directory contains test files of modulated audio that
you can use to test FreeDV (see USER_MANUAL.txt)

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
    mingw{32,64}-portaudio mingw{32,64}-libsndfile mingw{32,64}-libsamplerate.noarch
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
  
## Building and installing on OSX

Please see README.osx

## Building and installing on FreeBSD

In ```build_linux.sh``` change the ```build_linux``` directory to build_freebsd
