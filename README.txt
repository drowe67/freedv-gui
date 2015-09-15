==================================
 Building and installing on Linux
==================================

To build codec2, the build-essential and cmake packages will be required.
If they are not already installed, you can install them by typing

  $ sudo apt-get install build-essential cmake

Quickstart 1
-----------

Builds static versions of wxWidgets, portaudio, codec2-dev, which are commonly
missing on many Linux systems, or of the wrong (older) version.

1/ Assuming the freedv-dev source is checked out into ~/freedv-dev:

  $ sudo apt-get install libgtk2.0-dev libhamlib-dev libsamplerate-dev libasound2-dev libao-dev libgsm1-dev libsndfile-dev
  $ cd freedv-dev
  $ mkdir build_linux
  $ cd build_linux
  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE ~/freedv-dev ../
  $ make

2/ Then you can configure FreeDV using your local codec-dev, something like:

  $ cmake -DCMAKE_BUILD_TYPE=Debug -DBOOTSTRAP_WXWIDGETS=TRUE -DCODEC2_INCLUDE_DIRS=/path/to/codec2-dev/src -DCODEC2_LIBRARY=/path/to/codec2-dev/build_linux/src/libcodec2.so -DUSE_STATIC_CODEC2=FALSE -DUSE_STATIC_PORTAUDIO=TRUE -DUSE_STATIC_SOX=TRUE ../

3/ OR build a local copy of codec2-dev:

  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE -DUSE_STATIC_CODEC2=TRUE -DUSE_STATIC_PORTAUDIO=TRUE -DUSE_STATIC_SOX=TRUE ../
  
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
  $ make install


=======================================================
 Building for Windows on Ubuntu Linux (Cross compiling)
=======================================================

1/ Install the cross compiling toolchain:

  $ sudo apt-get install mingw-w64

2/ Patch cmake using: http://www.cmake.org/gitweb?p=stage/cmake.git;a=patch;h=33286235048495ceafb636d549d9a4e8891967ae

3/ Checkout a fresh copy of codec2-dev and build for Windows, pointing to the generate_codebook built by a linux build of generate_codebook, using this cmake line

  $ cmake .. -DCMAKE_TOOLCHAIN_FILE=../freedv-dev/cmake/Toolchain-Ubuntu-mingw32.cmake -DUNITTEST=FALSE -DGENERATE_CODEBOOK=/home/david/codec2-dev/build_linux/src/generate_codebook 

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

  $ cmake -DBOOTSTRAP_WXWIDGETS=TRUE -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-Ubuntu-mingw32.cmake -DUSE_STATIC_PORTAUDIO=TRUE -DUSE_STATIC_SNDFILE=TRUE -DUSE_STATIC_SAMPLERATE=TRUE -DUSE_STATIC_SOX=TRUE -DUSE_STATIC_CODEC2=FALSE -DCODEC2_INCLUDE_DIRS=/home/david/tmp/codec2-dev/src -DCODEC2_LIBRARY=/home/david/tmp/codec2-dev/build_windows/src/libcodec2.dll.a -DHAMLIB_INCLUDE_DIR=hamlib-win32-1.2.15.3/include -DHAMLIB_LIBRARY=hamlib-win32-1.2.15.3/lib/gcc/libhamlib.dll.a -DCMAKE_BUILD_TYPE=Debug ..
  $ make

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

====
TODO
====

[ ] Open R&D questions
    + Goal is to develop an open source DV mode that performs comparably to SSB
    [ ] Does 700 perform OK next to SSB?
        + approx same tx pk level (hard to measure exactly)
        + try some low SNR channels
        + try some fast fading/nasty channels
    [ ] Is 700 speech quality acceptable?

[X] test frames 
    [X] freedv API support
    [X] BER displayed on GUI for 700 and 1600
    [X] plot error patterns for 700 and 1600
        + callback for error patterns, or poll via stats interface
    [X] plot error histograms for 700 and 1600
        + map bit error to carrier, have done this in tcohpsk?
        + how to reset histogram?  On error reset?
        + histogram screen ... new code?
        + test with filter

[X] Bugs
    [X] resync issue
    [X] equalise power on 700 and 1600
    [X] research real and complex PAPR
    [X] waterfall and spectrum in analog mode
    [X] The waterfall in analog mode appears to quit working sometimes?

    [X] On TX, intermittently PTT will cause signal to be heard in speakers.  Toggle PTT or 
        Stop/Start toggle and then starts working.
    [X] Squelch control on 1600 mode will not open up squelch to 0 (appears to be around 2 dB)
    [X] space bar keys PTT when entering text info box
    [X] checksum based txt reception
        + only print if valid
    [X] IC7200 audio breakup
    [ ] short varicode doesn't work
        + #ifdef-ed out for now
        + cld be broken in freedv_api
    [X] On 700 audio sounds tinny and clicky when out of sync compared to 1600 why?
        + clue: only when analog not pressed
        + this was 7.5 to 8kHz interpolator bug
    [X] spectrum and waterfall scale changes when analog pressed
    [X] ocassional test frames error counter goes crazy
    [ ] old Waterfall AGC
    [ ] 700 syncs up to 1000Hz sine waves
        + shouldn't trigger sync logic, will be a problem with carriers
    [ ] "clip" led, encourage people to adjust gain to hit that occ when speaking
    [ ] Win32 record from radio time
    [ ] Stuttering with squelch off
        + Rpeorted by Mark VK5QI, can we repeat?

[ ] FreeDV 700 improvements
    [ ] bpf filter after clipping to remove clicks
        [ ] tcohpsk first, measure PAPR, impl loss
    [ ] error masking
        [ ] C version
        [ ] training off air?  Switchable?
        [ ] excitation params
        [ ] training
    [ ] plotting other demod stats like ch ampl and phase ests
    [ ] profile with perf, different libresample routine
    [ ] check for occassional freedv 700 loss of sync
        + scatter seems to jump
    [ ] switchable diversity (narrowband) option
        + measure difference on a few channels
        + blog
    [ ] slow mode at half speed rather than repeating, or tx twice and sum
    [ ] Can freedv remove ambient rf with a simple second Rx?
    [ ] presence, posting to web site
    [ ] explore relaxing sync 700 thresh
    [ ] longer record files
    [ ] two vertical lines in the waterfall representing the band limits of the 700B mode

[X] win32
    [X] X-compile works
    [X] basic installer
    [X] Win32 installer
        + Richard has taken care of this

[ ] Small fixes
    [X] Playfile bug
    [X] running again
    [X] bump ver number
    [X] long varicode default
    [X] option to _not_ require checksum, on by default
    [X] default squelch 2dB
    [X] scatter diagram tweaks
        + e.g. meaningful plots on fading channels in real time 
        [X] agc with hysteresis
            + changed to log steps
        [X] longer persistance
            + changed to 6 seconds
        [X] diversity addtions on 700
            + still not real obvious on plot
            + might be useful to make this switchable
    [X] scatter diagram different colours/carrier
    [X] remember what mode you were in
    [ ] cmd line file decode
    [ ] Waterfall direction
    [ ] documentation or use, walk through, you tube, blog posts

[ ] Web support for Presence/spotting hooks

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
