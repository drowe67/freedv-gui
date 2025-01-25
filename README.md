 # Building FreeDV GUI

This document describes how to build the FreeDV GUI program for various operating systems.  FreeDV GUI is developed on Ubuntu Linux, and then cross compiled for Windows using Fedora Linux (Fedora has great cross compiling support) and Docker.

## Further Reading

  * http://freedv.org - introduction, documentation, downloads
  * [FreeDV GUI User Manual](USER_MANUAL.md)
  
## Building on Ubuntu Linux

  ```
  $ sudo apt install libspeexdsp-dev libsamplerate0-dev sox git \
  libwxgtk3.2-dev libhamlib-dev libasound2-dev libao-dev \
  libgsm1-dev libsndfile1-dev cmake module-assistant build-essential
  $ git clone https://github.com/drowe67/freedv-gui.git
  $ cd freedv-gui

  (if using pipewire/PulseAudio -- recommended and the default) 
  $ sudo apt install libpulse-dev
  $ ./build_linux.sh
  
  (if using PortAudio)
  $ sudo apt install portaudio19-dev
  $ ./build_linux.sh portaudio
  ```

  (Depending on release you may need to use `libwxgtk3.0-gtk3-dev` instead of `libwxgtk3.2-dev`.)
  
  Then run with:
  ```
  $ ./build_linux/src/freedv
  ```
  
  Note this builds all libraries locally, nothing is installed on your machine.  ```make install``` is not required.

## Building on Fedora Linux
  ```
  $ sudo dnf groupinstall "Development Tools"
  $ sudo dnf install cmake wxGTK3-devel libsamplerate-devel \
    libsndfile-devel speexdsp-devel hamlib-devel alsa-lib-devel libao-devel \
    gsm-devel gcc-c++ sox
  $ git clone https://github.com/drowe67/freedv-gui.git
  $ cd freedv-gui

  (if using pipewire/PulseAudio -- default and recommended)
  $ sudo dnf install pulseaudio-libs-devel
  $ ./build_linux.sh

  (if using PortAudio)
  $ sudo dnf install portaudio-devel
  $ ./build_linux.sh portaudio
  ```

  Then run with:

  ```
  $ ./build_linux/src/freedv
  ```

## Running RADE mode on Linux

RADE is a new FreeDV mode that uses machine learning to improve voice quality and decoding ability.
We are currently focused on Windows and macOS for initial development, but it is possible to run on 
Linux by following these steps:

1. Install PyTorch, TorchAudio and matplotlib Python packages. Some distros have packages for one or more of these,
   but you can also use pip in a Python virtual environment (recommended to ensure the latest versions):

   ```
   $ cd freedv-gui
   $ python3 -m venv rade-venv
   $ . ./rade-venv/bin/activate
   (rade-venv) $ pip3 install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
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
   (rade-venv) $ PYTHONPATH="$(pwd)/rade_src:$PYTHONPATH" src/freedv
   ```

Alternatively, you can use [this script](https://github.com/barjac/freedv-rade-build) developed by 
Barry Jackson G4MKT to automate the above steps. While the FreeDV project thanks him for his contribution
to helping Linux users more easily get on the air with FreeDV, the FreeDV development team will not provide 
support. All support inquiries regarding this script should be directed to the linked repo.

## Building without LPCNet

In preparation for possible future deprecation of FreeDV 2020 and 2020B modes, it is
possible to build without requiring the [LPCNet](https://github.com/drowe67/LPCNet.git) library.
To do this, pass `LPCNET_DISABLE=1` as an environment variable to the build script, i.e.

```
$ LPCNET_DISABLE=1 ./build_linux.sh
```

or alternatively, do not pass in `LPCNET_BUILD_DIR` to `cmake` if manually executing the build.
This also has the side effect of disabling 2020 and 2020B in the user interface, preventing either 
from being selected.

*Note: if you don't already have Codec2 installed on your machine, you will need to pass `-DBOOTSTRAP_LPCNET=1`
to `cmake` in order for LPCNet to also be built.*

## Installing on Linux

You need to install the codec2 and lpcnetfreedv shared libraries, and freedv-gui:
  ```
  $ cd ~/freedv-gui/codec2/build_linux
  $ sudo make install
  $ cd ~/freedv-gui/LPCNet/build_linux
  $ sudo make install
  $ cd ~/freedv-gui/build_linux
  $ sudo make install
  $ sudo ldconfig
  ```
 
## Testing

The ```wav``` directory contains test files of modulated audio that you can use to test FreeDV (see the [USER_MANUAL](USER_MANUAL.md)).

## Building for Windows

Windows releases are built using the LLVM version of MinGW. This allows
one to build FreeDV for ARM as well as for Intel Windows systems, including support
for 2020 mode (on systems fast enough to acceptably decode it).

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

### Known Issues

* NSIS-related issues:
    * ARM installers will not properly register in Windows despite installing properly. You can still run the application manually by navigating to C:\Program Files\FreeDV \[version\]\ using File Explorer and double-clicking on `freedv.exe`.

## Building and installing on macOS

Using MacPorts, most of the appropriate dependencies can be installed by:

```
$ sudo port install automake git libtool sox +universal cmake
```

and on Homebrew:

```
$ brew install automake libtool git sox cmake
```

Once the dependencies are installed, you can then run the `build_osx.sh` script inside the source tree to build
FreeDV and associated libraries (codec2, LPCNet, hamlib). A FreeDV.app app bundle will be created inside the build_osx/src
folder which can be copied to your system's Applications folder.
