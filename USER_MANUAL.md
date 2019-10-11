# FREEDV GUI USER MANUAL

## Introduction

FreeDV GUI (or just FreeDV) is a GUI program for Linux, Windows, and
OSX for running FreeDV on a desktop PC or laptop.

This document is a work in progress.  Notes on new FreeDV features are
being added as they are developed.

## Getting Started

FreeDV GUI can be challenging to set up.  The easiest way is to find a
friend who has set up FreeDV and get them to help. Alternatively, this
section contains several tips to help you get started.

### Sound Card Configuration

For Receive only operation you just need one sound card, this is a
great way to get started.

For Tx/Rx Operation, you need two sound cards.  One connects to your
radio, and one for the operator.  The sound card connecting to the
radio can be a rig interface device like a Signalink, Rigblaster,
your radio's internal USB sound card, or a home brew rig interface.

The second sound card is often a set of USB headphones, or your
computer's internal sound card.

### Receive Only (One Sound Card)

Start with just a receive only station.  You just need the basic sound
hardware in your computer, for example a microphone/speaker on your
computer.

1. Open the *Tools - Audio Config* Dialog 
1. At the bottom select *Receive* Tab
1. In *From Radio To Computer* select your default sound input device (usually at the top)
1. In the *From Computer To Speaker/Headphone* window select your default sound output device (usually at the top)
1. At the bottom select *Transmit* Tab
1. In *From Microphone* window select *none*
1. In *To Radio* window select *none*
1. Press OK to close the dialog

When you press Start FreeDV will start decoding any incoming signals
on the microphone input, playing the decoded audio out of your
speaker.  If no valid FreeDV signals are received, no audio will be
played.

If you connect the microphone input on your computer to your radio
receiver, you can decode off air signals.  If you have a rig
interface, try configuring that as the *From Radio To Computer*
device, with your computer's sound card as the *From Computer To
Speaker/Headphone* device.

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

### Transmit/Receive (Two Sound Cards)

For Tx/Rx operation you need to configure two sound cards, by setting
up Tools - Audio Config *Transmit* and *Receive* Tabs.

When receiving, FreeDV off-air signals **from** your radio are decoded
by your computer and sent **to** your speaker/headphones, where you
can listen to them.

When transmitting, FreeDV takes your voice **from** the microphone,
and encodes it to a FreeDV signal in you computer which is sent **to**
your radio for transmission over the air.

Tab | Sound Device | Notes
--- | --- | ---
Receive Tab | From Radio To Computer | The off air FreeDV signal **from** your radio rig interface to your computer
Receive Tab | From Computer To Speaker/Headphone | The decoded audio from your computer to your Speaker/headphones
Transmit Tab | From Microphone to Computer | Your voice from the microphone to your computer
Transmit Tab | From Computer To Radio | The FreeDV signal from your computer sent **to** your radio rig interface for transmission

## Sound Card Levels

Sound card levels are generally adjusted in the computer's Control
Panel or Settings, or in some cases via controls on your rig interface
hardware or menus on your radio.

When FreeDV is running, you can observe the sound card signals in the
main window tabs (From Radio, From Mic, To Speaker).

1. On receive, FreeDV is not very sensitive to the **From Radio**
level, adjust so it is mid-range and not clipping.  FreeDV uses phase
shift keying (PSK) so is not sensitive to amplitude.

1. The transmit level from your computer to your radio is important.
On transmit, adjust your level so that the ALC is **just** being
nudged.  More **is not better** with the FreeDV transmit signal.
Overdriving your transmitter will lead to a distorted transit signal, and
a poor SNR at the receiver.  This is a very common problem.

1. Adjust the microphone audio so the peaks are not clipping, and the
average is about half the maximum.

## Audio Processing

FreeDV likes a clean path through you radio.  Turn all audio
processing **OFF** on transmit and receive:

+ On receive, DSP noise reduction should be off.

+ On transmit, speech compression should be off.

+ Keep the receive audio path as "flat" as possible, no special filters

+ FreeDV will not work any better if you band pass filter the off air
received signals.  It has it's own, very tight filters in the
demodulator.

## PTT Configuration

The Tools - PTT dialog supports three different ways to control PTT on
your radio:

+ VOX: sends a tone to the left channel of the Transmit/To Radio sound card
+ HamLib: support for many different radios via the HamLib library and a serial port
+ Serial Port: Direct access to the Serial port pins

Once you have configured PTT, try the **Test** button.

Serial PTT support is complex.  We get many reports that FreeDV 
PTT doesn't work on a particular radio, but may work fine with other
programs such as Fldigi.  This is often a mis-match between the serial
parameters Hamlib is using with FreeDV and your radio. For example you
may have changed the default serial rate on your radio. Carefully
check the serial parameters on your radio match those used by FreeDV
in the PTT Dialog.

### HamLib

Hamlib comes with a default serial rate for each radio.  If your radio
has a different serial rate change the Serial Rate drop down box to
match your radio.

When **Test** is pressed, the "Serial Params" field is populated and
displayed.  This will help track down any mis-matches between Hamlib
and your radio.

If you are really stuck, download Hamlib and test your radio's PTT
using the command line ```rigctl``` program.

### Changing COM Port On Windows

If you change the COM port of a USB-Serial device in Device Manager,
please unplug and plug back in the USB device.  Windows/FreeDV won't
recognise the device on the new COM Port until it has been
unplugged/plugged.

## Common Problems

### Overdriving Transmit Level

This is a very common problem for first time FreeDV users.  Adjust
your transmit levels so the ALC is just being nudged.  For a 100W
PEP radio, you average power should be 20W.

More power is not better with FreeDV.  An overdriven signal will have
poor SNR at the receiver.

### I can't set up FreeDV, especially the Sound Cards

This can be challenging the first time around:

1. Try a receive only (one audio card) set up first.

1. Ask someone who already runs FreeDV for help.

1. If you don't know anyone local, ask for help on the digital voice
mailing list.  Be specific about the hardware you have and the exact
nature of your problem.

### I need help with my radio or rig interface

There are many radios, many computers, and many sound cards.  It is
impossible to test them all. Many radios have intricate menus with
custom settings.  It is unreasonable to expect the authors of FreeDV to
have special knowledge of your exact hardware.

However someone may have worked through the same problem as you.  Ask
on the digital voice mailing list.

### Can't hear anything on receive

Many FreeDV modes will not play any audio if there is no valid signal.
You may also have squelch set too high.  In some modes the **Analog**
button will let you hear the received signal from the SSB radio.

Try the Test Wave Files above to get a feel for what a FreeDV signal
looks and sounds like.

### Trouble getting Sync with 700D

You need to be within +/- 20 Hz on the transmit signal.  It helps if
both the tx and rx stations tune to known, exact frequencies such as
exactly 7.177MHz.  On channels with fast fading sync may take a few
seconds.

### PTT doesn't work.  It works with Fldigi and other Hamlib applications.

Many people struggle with initial PTT setup:

1. Read the PTT Configuration section above.

1. Try the Tools - PTT Test function.

2. Check you rig serial settings.  Did you change them from defaults
for another program?

3. Ask someone who already uses FreeDV to help.

4. Contact the digital voice mailing list.  Be specific about your
hardware, what you have tried, and the exact nature of the problem.

### FreeDV 2020 mode is greyed out

You must have a modern CPU with AVX support to run FreeDV 2020.  If 
you do not have AVX the FreeDV 2020 mode button will be greyed out.
A Microsoft utlity called [coreinfo](https://docs.microsoft.com/en-us/sysinternals/downloads/coreinfo) 
can be also used to determine if your CPU supports AVX.  A * means 
you have AVX, a - means no AVX:
```
AES             -       Supports AES extensions
AVX             *       Supports AVX intruction extensions
FMA             -       Supports FMA extensions using YMM state``
```

### I installed a new version and FreeDV stopped working

You may need to clean out the previous configuration.  Try
Tools-Restore Defaults.


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

## FreeDV Modes

The following table is a guide to the different modes, using
analog SSB and Skype as anchors for a rough guide to audio quality:

Mode | Min SNR | Fading | Latency | Speech Bandwidth | Speech Quality
--- | :---: | :---: | :---: | :---: | :---:
SSB | 0 | 8/10 | low | 2600 | 5/10
1600 | 4 | 3/10 | low | 4000 | 4/10
700C | 2  | 6/10 | low |  4000 | 3/10
700D | -2 | 7/10 | high | 4000 | 3/10
2020 | 4  | 5/10 | high | 8000 | 7/10
Skype | - |- | medium | 8000 | 8/10

The Min SNR is roughly the SNR where you cannot converse without
repeating yourself.  The numbers above are on channels without fading
(AWGN channels like VHF radio).  For fading channels the minimum SNR
is a few dB higher. The Fading column shows how robust the mode is to
HF Fading channels, higher is more robust.

The more advanced 700D and 2020 modes have a high latency due to the
use of large Forward Error Correction (FEC) codes.  They buffer many
frames of speech, which combined with PC sound card buffering results
in end-end latencies of 1-2 seconds.  They may take a few seconds to
sync at the start of an over, especially in fading channels.

### FreeDV 700D

In mid 2018 FreeDV 700D was released, with a new OFDM modem, powerful
Forward Error Correction (FEC) and optional interleaving.  It uses the
same 700 bit/s speech codec at 700C. It operates at SNRs as low as
-2dB, and has good HF channel performance.  It is around 10dB better
than FreeDV 1600 on fading channels, and is competitive with SSB at
low SNRs.  The FEC provides some protection from urban HF noise.

FreeDV 700D is sensitive to tuning.  To obtain sync you must be within
+/- 60Hz of the transmit frequency.  This is straightforward with
modern radios which are generally accurate to +/-1 Hz, but requires
skill and practice when used with older, VFO based radios.

The rest of this section describes features and options specific to
FreeDV 700D.

Main GUI Page:

1. Separate indication of Modem and (for 700D) Interleaver Sync. The
number on the Interleaver Sync indicator is the interleaver size in
160ms frames. This is usually set to 1.
     
1. ReSync button breaks 700D sync and forces it to try again.  Useful
if 700D gets a false sync in low SNR channels.

Tools - Options dialog:

1. Clipping: For 700C and 700D reduces the Peak/Average Power Ratio
(PAPR) (also known as Crest Factor) from 12dB to 8dB by clipping the
Tx signal.  This will add a little noise to the Tx spectrum and Rx
Scatter diagram, but MAY enable you to drive your Power Amplifier
harder.  Use with caution to avoid overloading your Power Amplifier.

1. Tx Band Pass Filter: limits the transmit bandwidth to about 1000
Hz.  Usually left on.

1. 700D Interleaver: The interleaver averages out errors over several
frames, which improves performance for fast fading channels, and
channels with burst errors.  A 16 frame interleaver will improve
performance by 4dB.  However interleaving adds delay, and delays sync.
Both the tx and rx must have the same interleaver setting.  For
example a setting of 2 means we average errors over 2 160ms frames,
and introduces 2x160=320ms delay in both the Tx and Rx (640ms total).
The interleaver is usually set to 1.

1. 700D Manual Unsync: Sync must be broken manually (ReSync button)
when this option is selected.  Disables automatic falling out of
sync. Experimental features that may be useful for ensuring 700D stays
in sync during long fades, to avoid long resync delays with the
interleaver.

### FreeDV 2020

FreeDV 2020 was released in mid 2019.  It uses an experimental codec
based on the LPCNet neural net (deep learning) synthesis engine
developed by Jean-Marc Valin.  It offers 8 kHz audio bandwidth in an
RF bandwidth of just 1600 Hz.  FreeDV 2020 employs the same OFDM modem
and FEC as 700D.

The purpose of FreeDV 2020 is to test neural net speech coding over HF
radio.  It is highly experimental, and possibly the first use of
neural net vocoders in a real world, over the air system.

FreeDV 2020 is designed for slow fading HF channels with a SNR of 10dB
or better.  It is not designed for fast fading or very low SNRs like
700D.  It is designed to be a high quality alternative to SSB in
channels where SSB is already an "arm-chair" copy.  On an AWGN (non
fading channel), it will deliver reasonable speech quality down to 2dB
SNR.

You may encounter the following limitations with FreeDV 2020:

1. Some voices may sound very rough.  In early testing
about 90% of speakers tested work well.
1. Like 700D, you must tune within -/+ 60Hz for FreeDV 2020 to sync.
1. With significant fading, sync may take a few seconds.
1. There is a 2 second end-end latency.  You are welcome to try tuning
   this (Tools - Options - FIFO size, also see Sound Card Debug
   section below).
1. It requires a modern (post 2010) Intel CPU with AVX support.  If you 
   don't have AVX the FreeDV 2020 mode button will be grayed out. 

### Horus Binary Mode

Horus Binary mode (HorusB) High Altitude Balloon (HAB) telemetry using
the same FSK modem as 2400A/B and 800XA.

Connect your UHF SSB radio to FreeDV, and it will output telemetry
messages to the UDP port specified on Tools-Options "UDP Messages".
For Project Horus work, the port 55690 is used. Check the "Enable UDP
messages" checkbox.

You can test Horus telemetry decodes by "Playing" [this](http://rowetel.com/downloads/horus/4fsk_binary_100Rb_8khzfs.wav) test file using Tools - Start/Stop Play File - from Radio.  On Linux, you can
monitoring the messages using netcat:
```
  $ nc -ul 55690 
```
At the bottom of Tools-Options, the "APiVerbose" check box enables
printing of verbose API debug messages to the console, which will also
work in Windows if Tools-Options "Windows Debug Console" is checked.

A Python script is required to upload the telemetry messages to the HabHub
server, please see https://github.com/projecthorus/horusbinary#usage---via-freedv

## Tools - Filter

This section describes features on Tools-Filter.  

Control | Description
 --- | --- |
Noise Supression | Enable noise supression, dereverberation, AGC of mic signal using the Speex pre-processor
700C/700D Auto EQ | Automatic equalisation for FreeDV 700C and FreeDV 700D Codec input audio

Auto EQ (Automatic Equalisation) adjusts the input speech spectrum to best fit the speech codec. It can remove annoying bass artefacts and make the codec speech easier to understand.

[Blog Post on Auto EQ Part 1](http://www.rowetel.com/?p=6778)
[Blog Post on Auto EQ Part 2](http://www.rowetel.com/?p=6860)

## Tools - Options

This section describes features on Tools-Options.  Many of these features are also described in other parts of this manual.

### FreeDV 700 Options

Control | Description
 --- | --- |
Clipping | Hard clipping of transmit waveform to increase the average power, at the expense of some distortion
700C Diversity Combine | Combining of two sets of 700C carriers for better fading channel performance
700D Interleaver | How many 700D frames to Interleave, larger leads to better fading channel performance but more latency
700D Tx Band Pass Filter | Reduces 700D TX spectrum bandwidth
700D Manual Unsync | Forces 700D to remain in sync, and not drop sync automatically

### OFDM Modem Phase Estimator Options

These options apply to the FreeDV 700D and 2020 modes that use the OFDM modem:

1. The High Bandwidth option gives better performance on channels where the phase changes quickly, for example fast fading HF channels, and the Es'Hail 2 satellite. When unchecked, the phase estimator bandwidth is automatically selected.  It starts off high to enable fast sync, then switches to low bandwidth to optimise performance for low SNR HF channels.

1. The DPSK (differential PSK) checkbox has a similar effect - better performance on High SNR channels where the phase changes rapidly.  This option converts the OFDM modem to use differential PSK, rather than coherent PSK.  DPSK is used by earlier FreeDV modes such as FreeDV 1600.  It affects the Tx and Rx side, so both sides must select DPSK.

If you problems with 700D or 2020 sync even though you have a strong signal - try these option.

## Advanced/Developer Features

### Stats Window

Located on the lower left hand side of the main screen.

Term | Notes
--- | --- |
Bits | Number of bits demodulated
Errs | Number of bit errors detected
Resyncs | Number of times the demodulator has resynced
ClkOff | Estimated sample clock offset in parts per million
FreqOff | Estimated frequency offset in Hz
Sync | Sync metric (OFDM modes like 700D and 2020)
Var | Speech encoder distortion for 700C/700D (see Auto EQ)

The sample clock offset is the estimated difference between the
modulator (tx) and demodulator (rx) sample clocks.  For example if the
transmit station sound card is sampling at 44000 Hz, and the receive
station sound card 44001 Hz, the sample clock offset would be
((44000-44001)/44000)*1E6 = 22.7 ppm.

### Timing Delta Tab

This indicates the symbol timing estimate of the demodulator, in the
range of +/- 0.5 of a symbol.  With off air signals, this will have a
saw tooth appearance, as the demod tracks the modulator sample clock.
The steeper the slope, the greater the sample clock offset.

[FreeDV 1600 Sample Clock Offset Bug](http://www.rowetel.com/?p=6041)

[Testing a FDMDV Modem](http://www.rowetel.com/?p=2433)

### UDP Messages

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

### Sound Card Debug

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
     
### Test Frame Histogram

This feature was developed for testing FreeDV 700C.  Select the Test
Frame Histogram tab on Front Page

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

### Full Duplex Testing with loopback

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

## Glossary

Term | Notes
--- | ---
AWGN | Additive White Gaussian Noise - a channel with just noise and no fading (like VHF)
FEC | Forward Error Correction.  Extra bits to we send to protect the speech codec bits
LDPC | Low Density Parity Check Codes, a powerful family of FEC codes

## Release Notes

### V1.4 June-October 2019

1. FreeDV 2020, Project Horus Binary Modes.
1. [Improved OFDM Modem Acquisition](http://www.rowetel.com/?p=6824), this will improve sync time on FreeDV 700D and 2020 on HF fading channels, and can also handle +/- 60 Hz frequency offsets when tuning.
1. Fixed FreeDV 700C frequency offset bug fix, was losing sync at certain frequency offsets.
1. Wide bandwidth phase estimation and DPSK for OFDM modes (700D/2020) for fast fading/QO-100 channels (Tools-Options)
1. Better speech quality on FreeDV 700C/700D with Auto equaliser (Tools-Filter)

### V1.3 May 2018

* FreeDV 700D

## References 

* http://freedv.org
* [Digitalvoice mailing list](https://groups.google.com/forum/#!forum/digitalvoice)

