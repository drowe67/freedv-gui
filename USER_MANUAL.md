# FREEDV GUI USER MANUAL

## Introduction

FreeDV GUI (or just FreeDV) is a GUI program for Linux, Windows, and
OSX for running FreeDV on a desktop PC or laptop.

This is a live document.  Notes on new FreeDV features are being added as they are developed.

## Getting Started

FreeDV GUI can be challenging to set up.  The easiest way is to find a
friend who has set up FreeDV and get them to help. Alternatively, this
section contains several tips to help you get started.

### Sound Card Configuration

For Receive only operation you just need one sound card; this is a
great way to get started.

For Tx/Rx operation you need two sound cards.  One connects to your
radio, and one for the operator.  The sound card connecting to the
radio can be a rig interface device like a Signalink, Rigblaster,
your radio's internal USB sound card, or a home brew rig interface.

The second sound card is often a set of USB headphones or your
computer's internal sound card.

### Receive Only (One Sound Card)

Start with just a receive only station.  You just need the basic sound
hardware in your computer, for example a microphone/speaker on your
computer.

1. Open the *Tools - Audio Config* Dialog
1. At the bottom select *Receive* Tab
1. In *Input To Computer From Radio* select your default sound input device (usually at the top)
1. In the *Output From Computer To Speaker/Headphones* window select your default sound output device (usually at the top)
1. At the bottom select *Transmit* Tab
1. In *Input From Microphone To Computer* window select *none*
1. In *Output From Computer To Radio* window select *none*
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
and for the basic operation of the FreeDV software.

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
Receive Tab | Input To Computer From Radio | The off air FreeDV signal **from** your radio rig interface to your computer
Receive Tab | Output From Computer To Speaker | The decoded audio from your computer to your Speaker
Transmit Tab | Input From Microphone To Computer | Your voice from the microphone to your computer
Transmit Tab | Output From Computer To Radio | The FreeDV signal from your computer sent **to** your rig interface for Tx

### Changing Audio Devices

If you change audio devices (e.g. add or remove sound cards, USB hardware), it's a good idea to check the Tools/Audio Config dialog before pressing **Start**, to verify the audio devices are as expected. This is particularly important if any audio devices e.g. Headsets, USB Sound Cards, or Virtual Cables have been disconnected since the last time FreeDV was used.

Hitting **Refresh** in the lower left hand corner of the Tools/Audio Config will normally update the audio devices list. Keeping a screen shot of a known working configuration will be useful for new users. Unexpected audio configuration changes may also occur following a Windows updates.

Another solution is to re-start FreeDV and check Tools/Audio Config again after changing any audio hardware.

If you change/remove USB audio devices without refreshing Tools/Audio Config, FreeDV may crash.

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

1. FreeDV 700D and 700E can drive your transmitter at an average power of 40% of it's peak power rating.  For example 40W RMS for a 100W PEP radio. Make sure your transmitter can handle continuous power output at these levels, and reduce the power if necessary.

1. Adjust the microphone audio so the peaks are not clipping, and the
average is about half the maximum.

## Audio Processing

FreeDV likes a clean path through your radio.  Turn all audio
processing **OFF** on transmit and receive:

+ On receive, DSP noise reduction should be off.

+ On transmit, speech compression should be off.

+ Keep the receive audio path as "flat" as possible, no special filters.

+ FreeDV will not work any better if you band pass filter the off air
received signals.  It has its own, very tight filters in the
demodulator.

## PTT Configuration

The Tools - PTT dialog supports three different ways to control PTT on
your radio:

+ VOX: sends a tone to the left channel of the Transmit/To Radio sound card
+ HamLib: support for many different radios via the HamLib library and a serial port
+ Serial Port: direct access to the serial port pins

Once you have configured PTT, try the **Test** button.

Serial PTT support is complex.  We get many reports that FreeDV
PTT doesn't work on a particular radio, but may work fine with other
programs such as Fldigi.  This is often a mis-match between the serial
parameters Hamlib is using with FreeDV and your radio. For example you
may have changed the default serial rate on your radio. Carefully
check the serial parameters on your radio match those used by FreeDV
in the PTT Dialog.

Also see [Common Problems](#common-problems) section of this manual.

### HamLib

Hamlib comes with a default serial rate for each radio.  If your radio
has a different serial rate change the Serial Rate drop down box to
match your radio.

When **Test** is pressed, the "Serial Params" field is populated and
displayed.  This will help track down any mismatches between Hamlib
and your radio.

If you are really stuck, download Hamlib and test your radio's PTT
using the command line ```rigctl``` program.

#### Icom Radio Configuration

If using an Icom radio, Hamlib will use the radio's default CI-V address
when connecting. If this has been changed, you can specify the correct
address in the "Radio Address" field (valid values are 00 through FF
in hexadecimal). 

Note that "00" is the "wildcard" CI-V address. Your radio must have the 
"CI-V Transceive" option enabled in order for it to respond to commands
to that address. Otherwise, FreeDV must be configured to use the same
CI-V address as configured in the radio. For best results, ensure that
there are no other Icom/CI-V capable devices in the chain if 
"00"/"CI-V Transceive" is used.

### PSK Reporter (Experimental)

FreeDV has the ability to send FreeDV signal reports to [PSK Reporter](https://pskreporter.info/)
by enabling the option in Tools->Options and specifying your callsign and grid square. When enabled, this causes
FreeDV to disable the free form **Txt Msg** field and only transmit the **Callsign** field.

FreeDV validates the received information before submitting a position report to PSK Reporter. This is to ensure that FreeDV does not report invalid callsigns to the service (e.g. ones that don't exist or that correspond to real non-FreeDV users). However, all received text will display in the main window even if it has errors.

Reports sent to PSK Reporter will display using the mode "FREEDV" for ease of filtering. The user's 
current mode (e.g. 700D, 1600, etc.) will also appear in the "Using" field when hovering over or 
clicking on a reception report.

Note that Hamlib must be enabled so PSK Reporter can read your radio's frequency. A message will appear on pushing Start if this is not the case.

### Changing COM Port On Windows

If you change the COM port of a USB-Serial device in Device Manager,
please unplug and plug back in the USB device.  Windows/FreeDV won't
recognise the device on the new COM Port until it has been
unplugged/plugged.

### USB or LSB?

On bands below 10 MHz, LSB is used for FreeDV.  On 10MHz and above, USB is used. After much debate, the FreeDV community has adopted the same conventions as SSB, based on the reasoning that FreeDV is a voice mode.

As an aid to the above, FreeDV will show the current mode on the bottom of the window upon pressing the Start button if Hamlib is enabled and your radio supports retrieving frequency and mode information over CAT. If your radio is using an unexpected mode (e.g. LSB on 20 meters), it will display that mode on the bottom of the window next to the Clear button in red letters. When a session is not active, Hamlib isn't enabled, or if your radio doesn't support retrieving frequency and mode over CAT, it will remain grayed out with "unk" displaying instead of the mode (for "unknown").

## Common Problems

### Overdriving Transmit Level

This is a very common problem for first time FreeDV users.  Adjust your transmit levels so the ALC is just being nudged. More power is not better with FreeDV.  An overdriven signal will have poor SNR at the receiver.  For FreeDV 700D/700E operation with the clipper, make sure your transmitter can sustain high average power levels without damage (e.g. 40W RMS on a 100W PEP radio).

### I can't set up FreeDV, especially the Sound Cards

This can be challenging the first time around:

1. Try a receive only (one audio card) set up first.

1. Ask someone who already runs FreeDV for help.

1. If you don't know anyone local, ask for help on the digital voice
mailing list.  Be specific about the hardware you have and the exact
nature of your problem.

### Hamlib does not work with my Icom radio

The most common issue with Icom radios is that the CI-V address configured
in FreeDV does not match the address configured in the radio. Ensure that
the CI-V address in both FreeDV and on the radio are the same. If "00" is
used on the FreeDV side, ensure that the "CI-V Transceive" option is enabled
on the radio or else the radio will not respond to requests directed to that
address.

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

You need to be within +/- 60 Hz on the transmit signal.  It helps if
both the Tx and Rx stations tune to known, exact frequencies such as
exactly 7.177MHz.  On channels with fast fading sync may take a few
seconds.

### PTT doesn't work.  It works with Fldigi and other Hamlib applications.

Many people struggle with initial PTT setup:

1. Read the PTT Configuration section above.

1. Try the Tools - PTT Test function.

1. Check your rig serial settings.  Did you change them from defaults
for another program?

1. Linux version: do you have permissions for the serial port?  Are you a member
of the ```dialout``` group?

1. Ask someone who already uses FreeDV to help.

1. Contact the digital voice mailing list.  Be specific about your
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
FMA             -       Supports FMA extensions using YMM state
```

On Linux, you can check for `avx` in the **flags** section of `/proc/cpuinfo`
or the output of the `lscpu` command:
```
lscpu | grep -o "avx[^ ]*"
```
will display `avx` (or `avx2`) if your CPU supports the instructions.

### FreeDV 2020 mode is slow on ARM Macs

Preliminary testing on ARM Macs has shown that NEON optimizations in LPCNet are
sufficient to allow 2020 to be whitelisted on those machines. However, this is
definitely experimental. If you are experiencing issues with 2020 mode on these
Macs, please let the development team know so that further investigation can be done.

### I installed a new version and FreeDV stopped working

You may need to clean out the previous configuration.  Try
Tools-Restore Defaults.

### FreeDV crashes when I press Start

Have you removed/changed USB audio devices? If you remove/change USB audio devices without pressing Tools/Audio Config, FreeDV may crash.  See Changing Audio Devices above.

### FreeDV can't be opened on OSX because the developer cannot be verified

From January 2020 Apple is enforcing notarization for all OSX applications.  The FreeDV developers do not wish to operate within the Apple ecosystem due to the cost/intrusiveness of this requirement.

![notarization](contrib/osx_notarization1.png)

Security & Privacy shows the Open Anyway option for FreeDV:

![notarization](contrib/osx_notarization2.png)

![notarization](contrib/osx_notarization3.png)

Or you can use command line options:

```
xattr -d com.apple.quarantine FreeDV.app
```
or
```
xattr -d -r com.apple.quarantine FreeDV.app
```

## Voice Keyer

The Voice Keyer Button on the front page, and the Options-PTT dialog
puts FreeDV and your radio into transmit, reads a wave file of your
voice to call CQ, and then switches to receive to see if anyone is
replying.  If you press the space bar the voice keyer stops.  If a signal
with a valid sync is received for a few seconds the voice keyer stops.

The Options-PTT dialog can be used to select the wave file, set the Rx
delay, and number of times the tx/rx cycle repeats.

The wave file for the voice keyer should be in 8kHz mono 16 bit sample
form (16 kHz for 2020).  Use a free application such as Audacity to convert a file you
have recorded to this format.

## FreeDV Modes

The following table is a guide to the different modes, using
analog SSB and Skype as anchors for a rough guide to audio quality:

Mode | Min SNR | Fading | Latency | Speech Bandwidth | Speech Quality
--- | :---: | :---: | :---: | :---: | :---:
SSB | 0 | 8/10 | low | 2600 | 5/10
1600 | 4 | 3/10 | low | 4000 | 4/10
700C | 2  | 6/10 | low |  4000 | 3/10
700D | -2 | 4/10 | high | 4000 | 3/10
700E | 1 | 7/10 | medium | 4000 | 3/10
2020 | 4  | 4/10 | high | 8000 | 7/10
Skype | - |- | medium | 8000 | 8/10

The Min SNR is roughly the SNR where you cannot converse without
repeating yourself.  The numbers above are on channels without fading
(AWGN channels like VHF radio).  For fading channels the minimum SNR
is a few dB higher. The Fading column shows how robust the mode is to
HF Fading channels, higher is more robust.

The more advanced 700D and 2020 modes have a high latency due to the
use of large Forward Error Correction (FEC) codes.  They buffer many
frames of speech, which combined with PC sound card buffering results
in end-to-end latencies of 1-2 seconds.  They may take a few seconds to
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

### FreeDV 700E

FreeDV 700E was developed in December 2020 using lessons learned from on air operation of 700C and 700D.  A variant of 700D, it uses a shorter frame size (80ms) to reduce latency and sync time.  It is optimised for fast fading channels channels with up to 4Hz Doppler spread and 6ms delay spread.  FreeDV 7000E uses the same 700 bit/s codec as FreeDV 700C and 700D.  It requires about 3dB more power than 700D, but can operate reliably on fast fading channels.

The 700E release also includes optional compression (clipping) of the 700D an 700E transmit waveforms to reduce the Peak to Average Power Ratio to about 4dB.  For example a 100W PEP transmitter can be driven to about 40W RMS.  This is an improvement of 6dB over previous releases of FreeDV 700D. Before enabling the clipper make sure your transmitter is capable of handling sustained high average power without damage.  

Clipping can be enabled via Tools-Options.

On good channels with high SNR clipping may actually reduce the SNR of the received signal.  This is intentional - we are adding some pre-distortion in order to increase the RMS power.  Forward error correction (FEC) will clean up any errors introduced by clipping, and on poor channels the benefits of increased signal power outweigh the slight reduction in SNR on good channels.

### FreeDV 2020

FreeDV 2020 was developed in 2019.  It uses an experimental codec
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
channels where SSB is already an "arm-chair" copy.  On an AWGN (non-
fading channel), it will deliver reasonable speech quality down to 2dB
SNR.

FreeDV 2020 Tips:

1. It requires a modern (post 2010) Intel CPU with AVX support.  If you
   don't have AVX the FreeDV 2020 mode button will be grayed out.
1. Some voices may sound very rough.  In early testing
   about 90% of speakers tested work well.
1. Like 700D, you must tune within -/+ 60Hz for FreeDV 2020 to sync.
1. With significant fading, sync may take a few seconds.
1. There is a 2 second end-to-end latency.  You are welcome to try tuning
   this (Tools - Options - FIFO size, also see Sound Card Debug
   section below).
1. The voice keyer file must be 16 kHz mono 16 bit sample format.

## Tools - Filter

This section describes features on Tools-Filter.  

Control | Description
 --- | --- |
Noise Supression | Enable noise supression, dereverberation, AGC of mic signal using the Speex pre-processor
700C/700D Auto EQ | Automatic equalisation for FreeDV 700C and FreeDV 700D Codec input audio

Auto EQ (Automatic Equalisation) adjusts the input speech spectrum to best fit the speech codec. It can remove annoying bass artefacts and make the codec speech easier to understand.

* [Blog Post on Auto EQ Part 1](http://www.rowetel.com/?p=6778)
* [Blog Post on Auto EQ Part 2](http://www.rowetel.com/?p=6860)

## Tools - Options

This section describes features on Tools-Options.  Many of these features are also described in other parts of this manual.

### FreeDV 700 C/D/E Options

Control | Description
 --- | --- |
Clipping | Increases the average power. Ensure your transmitter can handle high RMS powers before using!
700C Diversity Combine | Combining of two sets of 700C carriers for better fading channel performance
700D Tx Band Pass Filter | Reduces 700D TX spectrum bandwidth
700D Manual Unsync | Forces 700D to remain in sync, and not drop sync automatically

### OFDM Modem Phase Estimator Options

These options apply to the FreeDV 700D and 2020 modes that use the OFDM modem:

1. The High Bandwidth option gives better performance on channels where the phase changes quickly, for example fast fading HF channels and the Es'Hail 2 satellite. When unchecked, the phase estimator bandwidth is automatically selected.  It starts off high to enable fast sync, then switches to low bandwidth to optimise performance for low SNR HF channels.

1. The DPSK (differential PSK) checkbox has a similar effect - better performance on High SNR channels where the phase changes rapidly.  This option converts the OFDM modem to use differential PSK rather than coherent PSK.  DPSK is used by earlier FreeDV modes such as FreeDV 1600.  It affects the Tx and Rx side, so both sides must select DPSK.

If you have problems with 700D or 2020 sync even though you have a strong signal - try these option.

## Helping Improve FreeDV

If you have an interesting test case, for example:

1. FreeDV working poorly with a particular person or microphone.
1. Poor over the air performance on a fast fading channel.
1. Problems with sync on strong signals.
1. A comparison with SSB.

Please send the developers an off air recording of the signal.  FreeDV can record files from your radio using Tools-Record File from Radio.  A recording of 30 to 60 seconds is most useful.

With a recording we can reproduce your exact problem.  If we can reproduce it we can fix it. Recordings are much more useful than anecdotes or subjective reports like "FreeDV doesn't work", "SSB is better", or "On 23 December it didn't work well on grid location XYZ".  With subjective reports problems are impossible to reproduce, cannot be fixed, and you are unlikely to get the attention of the developers.

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
transmit station sound card is sampling at 44000 Hz and the receive
station sound card 44001 Hz, the sample clock offset would be
((44000-44001)/44000)*1E6 = 22.7 ppm.

### Timing Delta Tab

This indicates the symbol timing estimate of the demodulator, in the
range of +/- 0.5 of a symbol.  With off air signals this will have a
saw tooth appearance, as the demod tracks the modulator sample clock.
The steeper the slope, the greater the sample clock offset.

* [FreeDV 1600 Sample Clock Offset Bug](http://www.rowetel.com/?p=6041)
* [Testing a FDMDV Modem](http://www.rowetel.com/?p=2433)

### UDP Messages

When FreeDV syncs on a received signal for 5 seconds, it will send a
"rx sync" UDP message to a port on your machine (localhost).  An
external program or script listening on this port can then take some
action, for example send "spotting" information to a web server or
send an email your phone.

Enable UDP messages on Tools-Options, and test using the "Test"
button.

On Linux you can test reception of messages using netcat:
```
  $ nc -ul 3000
```  
A sample script to email you on FreeDV sync: [send_email_on_sync.py](src/send_email_on_sync.py)

Usage for Gmail:
```
python send_email_on_sync.py --listen_port 3000 --smtp_server smtp.gmail.com \
--smtp_port 587 your@gmail.com your_pass
```

### Sound Card Debug

These features were added for FreeDV 700D, to help diagnose sound card
issues during development.

Tools - Options dialog:

Debug FIFO and PortAudio counters: used for debugging audio
problems on 700D.  During beta testing there were problems with break
up in the 700D Tx and Rx audio on Windows.

The PortAudio counters (PortAudio1 and PortAudio2) should not
increment when running in Tx or Rx, as this indicates samples are
being lost by the sound driver which will lead to sync problems.

The Fifo counter outempty1 counter should not increment during
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
total bit errors), however problems can occur with filtering in the
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

1. A typical issue will be one carrier at 1.0 and the others at 0.5,
indicating the poorer carrier BER is twice the larger.

### Full Duplex Testing with loopback

Tools - Options - Half Duplex check box

FreeDV GUI can operate in full duplex mode which is useful for
development or listening to your own FreeDV signal as only one PC is
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

## Tips

1. The space-bar can be used to toggle PTT.
1. You can left click on the main window to adjust tuning, the vertical red line on the frequency scale will show the current centre frequency.  FreeDV will automatically track any drift once it syncs.

## Converting this document to PDF

For the Linux inclined:
```
$ pandoc USER_MANUAL.md -o USER_MANUAL.pdf "-fmarkdown-implicit_figures -o" \
--from=markdown -V geometry:margin=.4in --toc --highlight-style=espresso
```

## Glossary

Term | Notes
--- | ---
AWGN | Additive White Gaussian Noise - a channel with just noise and no fading (like VHF)
FEC | Forward Error Correction - extra bits to we send to protect the speech codec bits
LDPC | Low Density Parity Check Codes - a family of powerful FEC codes

## Release Notes

### V1.5.0 December 2020

1. FreeDV 700E, better performance than 700D on fast fading channels
1. FreeDV 700D/700E clipper to increase average transmit power by 6dB

### V1.4.3 August 2020

1. Maintenance Release (no major new features)
1. Changes to support wxWidgets 3.1 (but Windows versions built against wxWidgets 3.0)
1. Under the hood - OFDM modem has been refactored, shouldn't affect freedv-gui operation

### V1.4.2 July 2020

1. Maintenance Release (no major new features)
1. Improved squelch/audio pass through on 700D/2020/2400B
1. Under the hood - Codec2 library has been refactored, shouldn't affect freedv-gui operation
1. Removed Project Horus support (now being maintained outside of Codec2/FreeDV)

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
* [FreeDV Technology Overview](https://github.com/drowe67/codec2/blob/master/README_freedv.md)
* [Digitalvoice mailing list](https://groups.google.com/forum/#!forum/digitalvoice)
