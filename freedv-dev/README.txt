==================================
 FreeDV GUI README.txt
==================================

This document describes how to build the FreeDV GUI program for
various operating systems.  See also:

  http://freedv.org - introduction, documentation, downloads
  RELEASE_NOTES.txt - changes made to each version
  USER_MANUAL.txt   - FreeDV GUI Manual

==================================
 Building and installing on Linux
==================================

First the basics:

  $ sudo apt-get install build-essential cmake subversion

To install the required development libraries instead of building them
statically:

Debian/Ubuntu:

  $ sudo apt-get install libwxgtk3.0-dev portaudio19-dev \
    libhamlib-dev libsamplerate-dev libasound2-dev libao-dev libgsm1-dev \
    libsndfile-dev libspeexdsp-dev

Fedora:

  $ sudo dnf install wxGTK3-devel portaudio-devel libsamplerate-devel \ 
    libsndfile-devel speexdsp-devel hamlib-devel alsa-lib-devel libao-devel \
    gsm-devel


RHEL/CentOS and derivitves (requires Fedora EPEL repository)

  $ sudo yum install wxGTK3-devel portaudio-devel libsamplerate-devel \ 
    libsndfile-devel speexdsp-devel hamlib-devel alsa-lib-devel libao-devel \
    gsm-devel


Quickstart 1
------------

Using a modern Linux, and the development library packages installed above:

  $ cd /path/to/freedv
  $ mkdir build_linux
  $ cd build_linux
  $ cmake ../ 
  $ make
  $ ./src/freedv

Quickstart 2
-------------

Using a local build of codec2-dev that you may be developing:

  $ cd /path/to/freedv
  $ mkdir build_linux
  $ cd build_linux
  $ cmake -DCMAKE_BUILD_TYPE=Debug -DCODEC2_BUILD_DIR=/path/to/codec2-dev/build_linux  ..
  $ make
  $ ./src/freedv

Quickstart 3
------------

Builds static versions of wxWidgets, portaudio, codec2-dev, which are commonly
missing on older Linux systems.

1/ Assumes static build of wxWidgets and the freedv-dev source is checked out into ~/freedv-dev:

  $ cd ~/freedv-dev
  $ mkdir build_linux
  $ cd build_linux
  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE ../
  $ make

2/ Then you can configure FreeDV using your local codec-dev, something like:

  $ cmake -DCMAKE_BUILD_TYPE=Debug -DBOOTSTRAP_WXWIDGETS=TRUE -DCODEC2_BUILD_DIR=/path/to/codec2-dev/build_linux -DUSE_STATIC_PORTAUDIO=TRUE ../

3/ OR checkout and build a copy of codec2-dev:

  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE -DUSE_STATIC_CODEC2=TRUE -DUSE_STATIC_PORTAUDIO=TRUE  ../
  
4/ Build and run FreeDV:

   $ make
   $ ./src/freedv

=======================================================
 Building for Windows on Fedora 28 (Cross compiling)
=======================================================

Install basic MinGW development packages:
  $ sudo dnf install mingw{32,64}-filesystem mingw{32,64}-binutils \
    mingw{32,64}-gcc mingw{32/64}-crt mingw{32,64}-headers

Install dependencies of FreeDV/Codec2:
  $ sudo dnf install mingw{32,64}-speex

Enable Freetel specific packages not currently in Fedora proper:
  $ sudo dnf copr enable hobbes1069/mingw
  $ sudo dnf install mingw{32,64}-wxWidgets3 mingw{32,64}-hamlib \
    mingw{32,64}-portaudio mingw{32,64}-libsndfile

Building FreeDV for 64 Bit windows:
  $ cd ~/freedv-dev
  $ mkdir build_windows && cd build_windows
  $ mingw64-cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_STATIC_SAMPLERATE=TRUE ..
  $ make
  $ make package
  
WANTED: Instructions for cross compiling on Ubuntu 17 or 18

====================================
 Building and installing on Windows
====================================

The windows build is similar to linux and follows the same basic workflow,
however, while codec2 and FreeDV (freedv) build well on windows, some of the
dependencies do not. For that reson current windows releases are cross-compiled
from linux.

Only MinGW is supported. While it is likely possible to perform a pure MinGW
build, installing MSYS2 will make your life easier.

CMake may not automatically detect that you're in the MSYS environment. If this
occurs you need to pass cmake the proper generator:

cmake -G"MSYS Makefiles" [other options] </path/to/source>

===============================
 Bootstrapping wxWidgets build
===============================

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
$ cmake -DBOOTSTRAP_WXWIDGETS=TRUE /path/to/freedv
$ make
(wxWidgets is downloaded and built)
$ cmake .
(wxWidgets build should be detected)
$ make
(if all goes well, as root)
$ make install

====================================
 Building and installing on OSX
====================================

Pls see README.osx

====================================
 Building and installing on FreeBSD
====================================

As per "Quickstart 2" above but change build_linux to build_freebsd

=======
Editing
=======

Please make sure your text editor does not insert tabs, and
used indents of 4 spaces.  The following .emacs code was used to
configure emacs:

(setq-default indent-tabs-mode nil)

(add-hook 'c-mode-common-hook
          (function (lambda ()
                      (setq c-basic-offset 4)
                      )))

===============
    TODO
===============

This software needs some maintenance and refactoring.  Patches
welcome!

[ ] Remove unnecessary PortAudio Wrapper layer
[ ] break fdmdv2_main.cpp into smaller files
[ ] rename src files from fdmdv2_ -> freedv_
[ ] profile code and explore optimisations


