==================================
 Building and installing on Linux
==================================

To build codec2, the build-essential and cmake packages will be required at
a minimum. If they are not already installed, you can install them by typing

  $ sudo apt-get install build-essential cmake

To install the required development libraries instead of building them
statically:

Debian/Ubuntu:

  $ sudo apt-get install libwxgtk3.0-dev portaudio19-dev \
    libhamlib-dev libsamplerate-dev libasound2-dev libao-dev libgsm1-dev \
    libsndfile-dev

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

Builds static versions of wxWidgets, portaudio, codec2-dev, which are commonly
missing on many Linux systems, or of the wrong (older) version.

1/ Assumes static build of wxWidgets and the freedv-dev source is checked out into ~/freedv-dev:

  $ cd ~/freedv-dev
  $ mkdir build_linux
  $ cd build_linux
  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE ../
  $ make

2/ Then you can configure FreeDV using your local codec-dev, something like:

  $ cmake -DCMAKE_BUILD_TYPE=Debug -DBOOTSTRAP_WXWIDGETS=TRUE -DCODEC2_INCLUDE_DIRS=/path/to/codec2-dev/src -DCODEC2_LIBRARY=/path/to/codec2-dev/build_linux/src/libcodec2.so -DUSE_STATIC_CODEC2=FALSE -DUSE_STATIC_PORTAUDIO=TRUE ../

3/ OR build a local copy of codec2-dev:

  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE -DUSE_STATIC_CODEC2=TRUE -DUSE_STATIC_PORTAUDIO=TRUE  ../
  
4/ Build and run FreeDV:

   $ make
   $ ./src/freedv


Quickstart 2
------------

1/ Assuming you have all the dependant libraries:

  $ cd /path/to/freedv
  $ mkdir build_linux
  $ cd build_linux
  $ cmake ../ (defaults to /usr/local, use CMAKE_INSTALL_PREFIX to override)
  (if no errors)
  $ make
  (as root)
  # make install


=======================================================
 Building for Windows on Ubuntu Linux (Cross compiling)
=======================================================

1/ Install the cross compiling toolchain:

  $ sudo apt-get install mingw-w64

2/ Patch cmake using: http://www.cmake.org/gitweb?p=stage/cmake.git;a=patch;h=33286235048495ceafb636d549d9a4e8891967ae

3/ Checkout a fresh copy of codec2-dev and build for Windows, pointing to the generate_codebook built by a linux build of generate_codebook, using this cmake line

  $ cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/david/freedv-dev/cmake/Toolchain-Ubuntu-mingw32.cmake -DUNITTEST=FALSE -DGENERATE_CODEBOOK=/home/david/codec2-dev/build_linux/src/generate_codebook 

4/ Build WxWidgets

  $ cd /path/to/freedv-dev
  $ mkdir build_windows
  $ cd build_windows
  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE .. -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-Ubuntu-mingw32.cmake -DCMAKE_BUILD_TYPE=Debug
  $ make

5/ Download and install the Windows version of Hamlib:

  $ wget http://internode.dl.sourceforge.net/project/hamlib/hamlib/1.2.15.3/hamlib-win32-1.2.15.3.zip
  $ unzip hamlib-win32-1.2.15.3.zip

6/ Build All the libraries and FreeDV:

  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-Ubuntu-mingw32.cmake -DUSE_STATIC_PORTAUDIO=TRUE -DUSE_STATIC_SNDFILE=TRUE -DUSE_STATIC_SAMPLERATE=TRUE -DUSE_STATIC_CODEC2=FALSE -DCODEC2_INCLUDE_DIRS=/home/david/tmp/codec2-dev/src -DCODEC2_LIBRARY=/home/david/tmp/codec2-dev/build_windows/src/libcodec2.dll.a -DHAMLIB_INCLUDE_DIR=hamlib-win32-1.2.15.3/include -DHAMLIB_LIBRARY=hamlib-win32-1.2.15.3/lib/gcc/libhamlib.dll.a -DCMAKE_BUILD_TYPE=Debug ..
  $ make

7/ Test on Linux with "wine", this will tell you if any DLLs are missing:

  $ wine src/freedv.exe

8/ When moving to an actual Windows machine, I needed:

  /usr/lib/gcc/i686-w64-mingw32/4.8/libstdc++-6.dll
  /usr/lib/gcc/i686-w64-mingw32/4.8/libgcc_s_sjlj-1.dll
  /usr/i686-w64-mingw32/lib/libwinpthread-1.dll

  Wine seems to find these automagically, so I found them on my system by
  looking at ~/.wine/system.reg for PATH:

  [System\\CurrentControlSet\\Control\\Session Manager\\Environment] 1423800803
  "PATH"=str(2):"C:\\windows\\system32;C:\\windows;C:\\windows\\system32\\wbem;Z:\\usr\\i686-w64-mingw32\\lib;Z:\\usr\\lib\\gcc\\i686-w64-mingw32\\4.8"


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

=================
USER GUIDE NOTES
=================

TODO: Put this in a more usable form (maybe parse into HTML), video
tutorials etc...

1/ Error Histogram
------------------

Displays BER of each carrier when in "test frame"
mode.  As each QPSK carrier has 2 bits there are 2*Nc histogram
points.

Ideally all carriers will have about the same BER (+/- 20% after 5000
total bit errors).  However problems can occur with filtering in the
tx path.  If one carrier has less power, then it will have a higher
BER.  The errors in this carrier will tend to dominate overall
BER. For example if one carrier is attenuated due to SSB filter ripple
in the tx path then the BER on that carrier will be higher.  This is
bad news for DV.

Suggested usage: 

i) Transmit FreeDV in test frame mode.  Use a 2nd rx (or
get a friend) to monitor your rx signal with FreeDV in test frame
mode.  

ii) Adjust your rx SNR to get a BER of a few % (e.g. reduce tx
power, use a short antenna for the rx, point your beam away, adjust rx
RF gain).  

iii) Monitor the error histogram for a few minutes, until you
have say 5000 total bit errors.  You have a problem if the BER of any
carrier is more than 20% different from the rest.

A typical issue will be one carrier at 1.0, the others at 0.5,
indicating the poorer carrier BER is twice the larger.

2/ Voice Keyer
--------------

Puts FreeDV and your radio into transmit, reads a wave
file of your voice to call CQ, then switches to receive to see if
anyone is replying.  If you press space bar the voice keyer stops.  If
a signal with a valid sync is received for a few seconds the voice
keyer stops.  The Tools-PTT dialog can be used to select the wave
file, set the Rx delay, and number of times the tx/rx cycle repeats.

3/ UDP Control port
-------------------

FreeDV can be controlled via UDP from other programs on the same host
(127.0.0.1) using text strings.  The default port is 3000, and can be
set Via Tools-Options.

Currents commands:

  "set txtmsg XXX" - sets the text/callsign ID string to XXX
  "restore"        - Restores the FreeDV windowto full size if minimised
  "ptton"          - PTT on (transmit)
  "pttoff"         - PTT off (receive)

Hint: "netcat" can be used to test this feature.

4/ Full Duplex Testing with loopback
------------------------------------

FreeDV GUI can operate in full duplex mode which is useful for
development as only one PC is required.  Tx and Rx signals can be
looped back via an analog connection between the sound cards, or (on
Linux) using the Alsa loopback module:

$ sudo modprobe snd-aloop
$ ./freedv

In Tools - Audio Config - Receive Tab  - From Radio select -> Loopback: Loopback PCM (hw:1,0)
                        - Transmit Tab - To Radio select   -> Loopback: Loopback PCM (hw:1,1)

