# FREEDV GUI USER MANUAL

## Introduction

FreeDV GUI (or just FreeDV) is a GUI program for Linux, Windows, and
OSX for running FreeDV on a desktop PC or laptop.

This document is a work in progress.  Notes on new FreeDV features are
being added as they are developed.

## References 

http://freedv.org

## Quick Start Guide

TBC Chris

## Getting Started

FreeDV GUI can be challenging to set up.  The easiest way is to find a
friend who has set up FreeDV and get them to help. Alternatively, this
section contains several tips to help you get started.

### Receive Only Mode

Start with just a receive only station.  You just need the basic sound
hardware in your computer.

1. Open the *Tools - Audio Config* Dialog 
1. At the bottom select *Receive* Tab
1. In *From Radio* select your default sound input device (usually at the top)
1. In the *To Speaker/Headphone* window select your default sound output device (usually at the top)
1. At the bottom select *Transmit* Tab
1. In *From Microphone* window *none*
1. In *To Radio* window *none*
1. Press OK to close the dialog

When you press Start FreeDV will start decoding any incoming signals,
playing the decoded audio out of your default sound device.  If no
valid FreeDV signals are received, no audio will be played.

If you don't have anyone to transmit FreeDV signals to you, try the
test wave files in the next section.

### Test Wave Files

In the
[wav](https://github.com/drowe67/freedv-gui/tree/master/wav)
directory are audio files containing off-air FreeDV modem
signals. There is one for each FreeDV mode.  Select a FreeDV mode and
press Start.  Choose a file using "Tools - Start/Stop Play File From
Radio".  You should hear decoded FreeDV speech.

These files will give you a feel for what FreeDV signals sound like,
and basic operation of the FreeDV software.

## Release Notes

Version | Date | Notes
--- | --- | ---
V1.4 | June 2019 | Project Horus Binary Mode, FreeDV 2020
V1.3 | May 2018 | FreeDV 700D

## Horus Binary Mode

Horus Binary mode (HorusB) High Altitude Balloon (HAB) telemetry using
the same FSK modem as used for 2400A/B and 800XA.

Connect your UHF SSB radio to FreeDV, and it will output telemetry
messages to the UDP port specified on Tools-Options "UDP Messages".
For Project Horus work, the port 55690 is used. Check the "Enable UDP
messages" checkbox.

You can test Horus telemetry decodes by "Playing" [this](http://rowetel.com/downloads/horus/4fsk_binary_100Rb_8khzfs.wav) test file.

using Tools - Start/Stop Play File - from Radio.  On Linux, you can
monitoring the messages using netcat:
```
  $ nc -ul 55690 
```
At the bottom of Tools-Options, the "APiVerbose" check box enables
printing of verbose API debug messages to the console, which will also
work in Windows if Tools-Options "Windows Debug Console" is checked.

A Python script is required to upload the telemetry messages to the HabHub
server, please see https://github.com/projecthorus/horusbinary#usage---via-freedv

## FreeDV 700D

In 2018 FreeDV 700D was released, with a new OFDM modem, powerful
Forward Error Correction (FEC) and optional interleaving.  It uses the
same 700 bit/s speech codec at 700C. It operates error free at SNRs as
low as -2dB, and has good HF channel performance.  It is around 10dB
better than FreeDV 1600 on fading channels, and is competitive with
SSB at low SNRs.

Main GUI Page:

1. Separate indication of Modem and (for 700D) Interleaver Sync. The
number On the Interleaver Sync indicator is the interleaver size in
160ms frames.
     
1. ReSync button breaks 700D sync and forces it to try again.  Useful
if 700D gets a false sync in low SNR channels.

Tools - Options dialog:

1. 700D Interleaver: The interleaver averages out errors over several
frames, which improves performance for fast fading channels, and
channels with burst errors.  A 16 frame interleaver will improve
performance by 4dB.  However interleaving adds delay, and delays sync.
Both the tx and rx must have the same interleaver setting.  For
example a setting of 2 means we average errors over 2 160ms frames,
and introduces 2x160=320ms delay in both the Tx and Rx (640ms total).

1. 700D Manual Unsync: Sync must be broken manually (ReSync button)
when this option is selected.  Disables automatic falling out of
sync. May be useful for ensuring 700D stays in sync during long fades,
to avoid long resync delays with the interleaver.

1. Clipping: For 700C and 700D reduces the Peak/Average Power Ratio
(PAPR) (also known as Crest Factor) from 12dB to 8dB by clipping the
Tx signal.  This will add a little noise to the Tx spectrum and Rx
Scatter diagram, but MAY enable you to drive your Power Amplifier
harder.  Use with caution to avoid overloading your Power Amplifier.
     
## Sound Card Debug Features

These features were added for FreeDV 700D, to help diagnose sound card
issues during development.

Tools - Options dialog:

Debug FIFO and PortAudio counters: used for debugging audio
problems on 700D.  During beta testing there were problems with break
up in the 700D Tx and Rx audio on Windows.

The PortAudio counters (PortAudio1 and PortAudio2) should not be
incremented when running in Tx or Rx, as this indicates samples are
being lost by the sound driver which will lead to sync problems.

The Fifo counter outempty1 counter should not be incremented during
Tx, as this indicates FreeDV is not supplying samples fast enough to
the PortAudio drivers.  The results will be resyncs at the receiver.

Check these counters by pressing Start, then Reset them and observe
the counters for 30 seconds.
     
If the PortAudio counters are incrementing on receive try:

  1. Adjusting framesPerBuffer; try 0, 128, 256, 512, 1024.

  1. Shut down other applications that might be using audio, such as
  Skype or your web browser.

  1. A different sound card rate such as 44.1kHz instead of 48kHz.

  If the outempty1 counter is incrementing on transmit try increasing
  the FifoSize.
     
  The txThreadPriority checkbox reduces the priority of the main txRx
  thread in FreeDV which may help the sound driver thread process
  samples.

  The txRxDumpTiming check box dumps timing information to a console
  that is used for debugging the rx break up problem on 700D.  Each
  number is how many ms the txRxThread took to run.

  The txRxDumpTiming check box dumps the number of samples free in the
  tx FIFO sending samples to the Tx.  If this hits zero, your tx audio
  will break up and the rx will lose sync.  Tx audio break up will
  also occur if you see "outfifo1" being incremented on the "Fifo"
  line during tx.  Try increasing the FifoSize.
     
## UDP Messages

When FreeDV syncs on a received signal for 5 seconds, it will send a
"rx sync" UDP message to a Port on your machine (localhost).  An
external program or script listening on this port can then take some
action, for example send "spotting" information to a web server or
send an email your phone.

Enable UDP messages on Tools-Options, and test using the "Test"
button.
     
On Linux you can test reception of messages using netcat:
```
  $ nc -ul 3000
```  
An sample script to email you on FreeDV sync: https://svn.code.sf.net/p/freetel/code/freedv-dev/src/send_email_on_sync.py

Usage for Gmail:
```
  $ python send_email_on_sync.py --listen_port 3000 --smtp_server smtp.gmail.com --smtp_port 587 your@gmail.com your_pass
```

## PTT Configuration

Tools-PTT Dialog

Hamlib comes with a default serial rate for each radio.  If your radio
has a different serial rate change the Serial Rate drop down box to
match your radio.

When "Test" is pressed, the "Serial Params" field is populated and
displayed.  This will help track down any mis-matches between Hamlib
and your radio.

Serial PTT support is complex.  We get many reports that FreeDV Hamlib
PTT doesn't work on a particular radio, but may work fine with other
programs such as Fldigi.  This is often a mis-match between the
serial parameters Hamlib is using with FreeDV and your radio. For
example you may have changed the default serial rate on your
radio. Carefully check the serial parameters on your radio match those
used by FreeDV in the PTT Dialog.

If you are really stuck, download Hamlib and test your radio's PTT
using the command line ```rigctl``` program.

### Changing COM Port On Windows

If you change the COM port of a USB-Serial device in Device Manager,
please unplug and plug back in the USB device.  Windows/FreeDV won't
recognise the device on the new COM Port until it has been
unplugged/plugged.

## Voice Keyer 

Voice Keyer Button on Front Page, and Options-PTT dialog.

Puts FreeDV and your radio into transmit, reads a wave file of your
voice to call CQ, then switches to receive to see if anyone is
replying.  If you press space bar the voice keyer stops.  If a signal
with a valid sync is received for a few seconds the voice keyer stops.

The Options-PTT dialog can be used to select the wave file, set the Rx
delay, and number of times the tx/rx cycle repeats.

The wave file for the voice keyer should be in 8kHz mono 16 bit sample
form.  Use a free application such as Audacity to convert a file you
have recorded to this format.

## Test Frame Histogram

Test Frame Histogram tab on Front Page

Displays BER of each carrier when in "test frame" mode.  As each QPSK
carrier has 2 bits there are 2*Nc histogram points.

Ideally all carriers will have about the same BER (+/- 20% after 5000
total bit errors).  However problems can occur with filtering in the
tx path.  If one carrier has less power, then it will have a higher
BER.  The errors in this carrier will tend to dominate overall
BER. For example if one carrier is attenuated due to SSB filter ripple
in the tx path then the BER on that carrier will be higher.  This is
bad news for DV.

Suggested usage: 

1. Transmit FreeDV in test frame mode.  Use a 2nd rx (or
get a friend) to monitor your rx signal with FreeDV in test frame
mode.  

1.  Adjust your rx SNR to get a BER of a few % (e.g. reduce tx
power, use a short antenna for the rx, point your beam away, adjust rx
RF gain).  

1. Monitor the error histogram for a few minutes, until you have say
5000 total bit errors.  You have a problem if the BER of any carrier
is more than 20% different from the rest.

1. A typical issue will be one carrier at 1.0, the others at 0.5,
indicating the poorer carrier BER is twice the larger.

## Full Duplex Testing with loopback

Tools - Options - Half Duplex check box

FreeDV GUI can operate in full duplex mode which is useful for
development of listening to your own FreeDV signal as only one PC is
required.  Normal operation is half duplex.

Tx and Rx signals can be looped back via an analog connection between
the sound cards.

On Linux, using the Alsa loopback module:
```
  $ sudo modprobe snd-aloop
  $ ./freedv

  In Tools - Audio Config - Receive Tab  - From Radio select -> Loopback: Loopback PCM (hw:1,0)
                          - Transmit Tab - To Radio select   -> Loopback: Loopback PCM (hw:1,1)
```
