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

1/ Using a modern Linux, and the development library packages
   installed above:

  $ cd /path/to/freedv
  $ mkdir build_linux
  $ cd build_linux
  $ cmake ../ 
  $ make
  $ ./src/freedv

Quickstart 2
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

  $ cmake -DCMAKE_BUILD_TYPE=Debug -DBOOTSTRAP_WXWIDGETS=TRUE -DCODEC2_INCLUDE_DIRS=/path/to/codec2-dev/src -DCODEC2_LIBRARY=/path/to/codec2-dev/build_linux/src/libcodec2.so -DUSE_STATIC_CODEC2=FALSE -DUSE_STATIC_PORTAUDIO=TRUE ../

3/ OR build a local copy of codec2-dev:

  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE -DUSE_STATIC_CODEC2=TRUE -DUSE_STATIC_PORTAUDIO=TRUE  ../
  
4/ Build and run FreeDV:

   $ make
   $ ./src/freedv

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

====================================
 Building an Ubuntu 16.04 Package 
====================================

Notes by David R, July 2017.  Work is progress....

Method 1
--------

1/ Set up instructions here:

  http://packaging.ubuntu.com/html/

  Section 2 has the setup, Section 6 is a good exercise to get started.  I followed
  steps in Section 6 below without understanding all of them just yet....

2/ Create a source tarball, e.g.for 1.2.2:

  $ svn export http://svn.code.sf.net/p/freetel/code/freedv/tags/1.2.2 freedv-1.2.2; tar czf freedv-1.2.2.tgz freedv-1.2.2

3/ use magic Ubuntu bzr add-on tools:

  $ bzr dh-make freedv 1.2.2 freedv-1.2.2.tgz
  (Press s for single)
  $ cd freedv
  $ bzr add debian/source/format
  $ bzr commit -m "Initial commit of Debian packaging."
  $ bzr builddeb -- -us -uc
  $ cd ..; ls *.deb
  $ sudo dpkg --install freedv_1.2.2-1_i386.deb
  $ freedv
  $ sudo apt-get remove freedv

Method 2
--------

More streamlined version (e.g. for test package based on SVN version 3255):

  (Edit debian files in your version of freedv-dev)
  $ cd tmp
  $ svn export ~/freedv-dev freedv-dev; tar czf freedv-dev.tgz freedv-dev; rm -Rf freedv-dev
  $ bzr dh-make freedv-dev 3255 freedv-dev.tgz
  (Press s for single)
  $ cd freedv-dev
  $ bzr builddeb -- -us -uc
  
Method 3
--------

Using Stuart Longlands debian files from 2015

  (Edit debian files in your version of freedv-dev)
  $ cd freedv-dev
  $ dpkg-buildpackage -uc -us

====================
FreeDV GUI TODO List
====================

[ ] Ubuntu packaging
[ ] default sound card in/out setting for rx out of the box
[ ] When application close on windows while "Start" down sometimes crashes
    + Also on Linux it reports an unterminated thread when exiting
[ ] Tool-Audio Config Dialog sound device names truncated on Windows
[ ] Serialport::closeport() on Linux takes about 1 second 
    + delays 'Stop' on main window test on Tools-PTT Test
[ ] Voice keyer file name at bottom on main screen truncated
    + need a bigger field
[ ] Start/Stop file rec/playback, work out a better UI, 
    maybe buttons on front page
[ ] feature for evaluating yr own sound quality
    + trap bad mic response/levels
    + zero in on different sound quality from different users
[ ] feeding audio over UDP say from from gqrx
    + could also be used to netcat stored files
[ ] refactoring
    [ ] fdmdv2_main.cpp is way too long
    [ ] rename fdmdv2*.cpp -> freedv*.cpp
    [ ] dlg_ptt uses ComPortsDlg name internally, rename PttDlg or similar
[ ] Add RSID
    + use case, when would it be used?
[ ] clean up dialogs
    + were based on auto generation code
    + must be an easier/clearer way to write them

