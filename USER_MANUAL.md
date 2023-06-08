# Introduction

FreeDV GUI (or just FreeDV) is a GUI program for Linux, Windows, and
OSX for running FreeDV on a desktop PC or laptop.

This is a live document.  Notes on new FreeDV features are being added as they are developed. 

# Getting Started

This section contains instructions to help you get started.

## Easy Setup

Upon starting FreeDV for the first time, the Easy Setup dialog will appear. This
is a streamlined setup process for FreeDV optimized for hardware commonly used
by amateur radio operators and is divided into three sections:

1. Sound card configuration,
2. CAT/PTT control, and
3. Reporting.

These sections are shown below:

![Easy Setup dialog](contrib/easy_setup.png)

Note that you can always return to this dialog by going to *Tools - Easy Setup*.

### Sound Card Configuration

To configure your sound card(s) using Easy Setup, simply select the sound device 
associated with your radio and the microphone and speaker devices you wish to use
to hear decoded audio as well as to transmit audio. If you're setting up a receive-only
station, you can choose "None" for the transmit audio device.

Additionally, if you are using a Flex 6000 series radio on the Windows platform, 
FreeDV will automatically select the DAX TX sound device. It is necessary only to 
select the correct "slice" for the radio sound device and the two devices to use for 
analog receive and transmit (e.g. your computer's built in microphone and speaker devices).

Note that some configurations (for example, SDR setups involving multiple radio sound
devices) may not be able to be configured with Easy Setup. For those, you can choose
the "Advanced" button and proceed to "Advanced Setup" below.

### CAT/PTT control

Easy Setup supports three methods of radio control:

1. No radio control (e.g. using a VOX audio device such as SignaLink),
2. Hamlib CAT control, and
3. Serial port PTT control.

Simply select the option that matches your radio setup and the required fields will
appear. For Hamlib, these are typically the type of radio you're using as well as the
serial port it's connected to (or TCP/IP hostname:port). Serial port PTT control requires 
the serial port your radio is using as well as whether your radio's PTT is triggered
via the RTS or DTR pin (and the required polarity for either).

If required, the "Advanced" button in this section will allow you to configure PTT input
(e.g. for a footswitch) and additional VOX related options. The "Test" button will
emit a constant carrier on the selected radio sound device as well as enable PTT to allow
you to adjust your radio sound levels (see "Sound Card Levels" below).

### Reporting

While not required, it is recommended to enable reporting so that others
can see who is currently receiving them. Both sides of a contact must have this enabled
in order for this feature to work. To configure reporting, simply enable
the feature here and enter your callsign and current grid square.

For more information about the reporting feature, see the "FreeDV Reporting" section below.

## Advanced Setup

### Sound Card Configuration

#### Receive Only (One Sound Card)

For this setup, you just need the basic sound hardware in your computer, 
for example a microphone/speaker on your computer.

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

#### Transmit/Receive (Two Sound Cards)

For Tx/Rx operation you need to configure two sound cards, by setting
up Tools - Audio Config *Transmit* and *Receive* Tabs.

When receiving, FreeDV off-air signals **from** your radio are decoded
by your computer and sent **to** your speaker/headphones, where you
can listen to them.

When transmitting, FreeDV takes your voice **from** the microphone,
and encodes it to a FreeDV signal in you computer which is sent **to**
your radio for transmission over the air.

Tab | Sound Device | Notes
--------------- | ---------------------------------------------- | ----------------------------------------------
Receive Tab | Input To Computer From Radio | The off air FreeDV signal **from** your radio rig interface to your computer
Receive Tab | Output From Computer To Speaker/Headphones | The decoded audio from your computer to your Speaker/Headphones
Transmit Tab | Input From Microphone To Computer | Your voice from the microphone to your computer
Transmit Tab | Output From Computer To Radio | The FreeDV signal from your computer sent **to** your rig interface for Tx

#### Changing Audio Devices

If you change audio devices (e.g. add or remove sound cards, USB hardware), it's a good idea to check the Tools/Audio Config dialog before pressing **Start**, to verify the audio devices are as expected. This is particularly important if any audio devices e.g. Headsets, USB Sound Cards, or Virtual Cables have been disconnected since the last time FreeDV was used.

Hitting **Refresh** in the lower left hand corner of the Tools/Audio Config will normally update the audio devices list. Keeping a screen shot of a known working configuration will be useful for new users. Unexpected audio configuration changes may also occur following a Windows updates.

Another solution is to re-start FreeDV and check Tools/Audio Config again after changing any audio hardware.

### PTT Configuration

The Tools - PTT dialog supports three different ways to control PTT on
your radio:

+ VOX: sends a tone to the left channel of the Transmit/To Radio sound card
+ Hamlib: support for many different radios via the Hamlib library and a serial port (or via TCP/IP for some devices, e.g. SDRs or FLrig/rigctld).
+ Serial Port: direct access to the serial port pins

You may also optionally configure a second serial port for PTT input.
This can be useful for interfacing devices like foot switches to 
FreeDV. If configured, FreeDV will switch into transmit mode (including
sending the needed Hamlib or serial commands to initiate PTT) when it
detects the configured signal.

Once you have configured PTT, try the **Test** button.

Serial PTT support is complex.  We get many reports that FreeDV
PTT doesn't work on a particular radio, but may work fine with other
programs such as Fldigi.  This is often a mismatch between the serial
parameters Hamlib is using with FreeDV and your radio. For example you
may have changed the default serial rate on your radio. Carefully
check the serial parameters on your radio match those used by FreeDV
in the PTT Dialog.

Also see [Common Problems](#common-problems) section of this manual.

#### HamLib

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

#### Changing COM Port On Windows

If you change the COM port of a USB-Serial device in Device Manager,
please unplug and plug back in the USB device.  Windows/FreeDV won't
recognise the device on the new COM Port until it has been
unplugged/plugged.

## Test Wave Files

In the installation are audio files containing off-air FreeDV modem signals. 
There is one file per FreeDV mode and are in the following locations depending 
on platform:

| Platform | Typical Location                                             |
|----------|--------------------------------------------------------------|
| Windows  | C:\\Program Files\\FreeDV [version]\\share\\freedv-gui\\wav  |
| Linux    | /usr/share/freedv-gui/wav or /usr/local/share/freedv-gui/wav |
| macOS    | See https://github.com/drowe67/freedv-gui/tree/master/wav    |

To play these files, first select a FreeDV mode and press Start.  Then 
choose a file using "Tools - Start/Stop Play File From Radio".  You should 
then hear decoded FreeDV speech.

These files will give you a feel for what FreeDV signals sound like,
and for the basic operation of the FreeDV software.

## Sound Card Levels

Sound card levels are generally adjusted in the computer's Control
Panel or Settings, or in some cases via controls on your rig interface
hardware or menus on your radio. In-app adjustments can also be done
by using the 'TX Level' slider at the bottom of the main screen; anything
below 0 dB attenuates the transmit signal.

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

1. FreeDV 700D and 700E can drive your transmitter at an average power of 40% of its peak power rating.  For example 40W RMS for a 100W PEP radio. Make sure your transmitter can handle continuous power output at these levels, and reduce the power if necessary.

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

## USB or LSB?

On bands below 10 MHz, LSB is used for FreeDV.  On 10MHz and above, USB is used. After much debate, the FreeDV community has adopted the same conventions as SSB, based on the reasoning that FreeDV is a voice mode.

As an aid to the above, FreeDV will show the current mode on the bottom of the window upon pressing the Start button if Hamlib is enabled and your radio supports retrieving frequency and mode information over CAT. If your radio is using an unexpected mode (e.g. LSB on 20 meters), it will display that mode on the bottom of the window next to the Clear button in red letters. When a session is not active, Hamlib isn't enabled, or if your radio doesn't support retrieving frequency and mode over CAT, it will remain grayed out with "unk" displaying instead of the mode (for "unknown").

# Voice Keyer

The Voice Keyer Button on the front page puts FreeDV and your radio into 
transmit, reads a wave file of your voice to call CQ, and then switches to 
receive to see if anyone is replying.  If you press the space bar or click
the PTT button, the voice keyer stops.  If a signal with a valid sync is 
received for a few seconds the voice keyer also stops.

The Audio tab inside Tools-Options can be used to select the wave file, set 
the Rx delay, and number of times the tx/rx cycle repeats.

# Quick Record

To quickly record incoming signals from the radio, a 'Record' button is provided
in the main window. Clicking this button will create a file beginning with the
name "FreeDV_FromRadio" and containing the current date and time. Clicking 'Record'
again will stop recording.

The Audio tab inside Tools-Options allows control of where these recordings are
saved. By default, this is inside the current user's Documents folder.

# Multiple Configurations

By default, FreeDV uses the following locations to store configuration:

* Linux: ~/.FreeDV 
* macOS: ~/Library/Preferences/FreeDV\ Preferences
* Windows: Registry (HKEY\_CURRENT\_USER\\SOFTWARE\\CODEC2-Project\\FreeDV)

If you'd like to store the configuration in another location (or store multiple configurations),
FreeDV accepts the -f (or --config) command line arguments to provide an alternate location. An
absolute path is recommended here; however, if only a relative path is provided, it will be relative
to the following locations:

* Linux: ~/
* macOS: ~/Library/Preferences/
* Windows: C:\\Users\\[username]\\AppData\\Roaming

## Executing FreeDV With a Different Configuration (Windows)

On Windows, you can create shortcuts to FreeDV with different file names for the "-f" command line
option as described above. To create a shortcut, right-click on the Desktop or in File Explorer and 
choose New->Shortcut. Click on Browse and navigate to one of the following paths:

* C:\\Program Files\\FreeDV [version]\\bin\\freedv.exe
* C:\\Program Files (x86)\\FreeDV [version]\\bin\\freedv.exe (if the 32 bit version is installed on a 64 bit machine)

Click Next and give the shortcut a unique description (e.g. "FreeDV IC-7300"). Then push Finish to create the shortcut.

Once the shortcut has been created, right-click it and choose Properties. Find the Shortcut tab in the resulting dialog
box and add "-f" followed by the desired filename to the end of the text in the Target field. Do not add any other
quote marks.

For example, to use a file called IC7300.conf stored in the Hamradio directory on the C drive the Target field should 
appear as follows:

"C:\\Program Files\\FreeDV [version]\\bin\\freedv.exe" -f C:\\Hamradio\\IC7300.conf

# FreeDV Reporting

FreeDV has the ability to send FreeDV signal reports to various online spotting services
by enabling the option in Tools-Options (in the Reporting tab) and specifying your callsign 
and Maidenhead grid square. When enabled, this causes FreeDV to disable the free form **Txt Msg** 
field and only transmit the **Callsign** field. As this uses a different encoding format 
from the free-form text field, both sides of the contact must have this enabled for the 
contact to be reported.

FreeDV validates the received information before submitting a position report. This 
is to ensure that FreeDV does not report invalid callsigns to the service (e.g. ones that don't exist 
or that correspond to real non-FreeDV users). However, if the reporting function is disabled,
all received text will display in the main window even if it has errors.

The following services are currently supported and can be individually enabled or disabled
along with the reporting feature as a whole:

* [PSK Reporter](https://pskreporter.info/) (using the "FREEDV" mode)
* [FreeDV Reporter](https://freedv-reporter.k6aq.net/)

The frequency that FreeDV reports is set by changing the "Report Frequency" drop down box in the main window. This 
is in kilohertz (kHz) and will turn red if the entered value is invalid. If Hamlib support is also enabled, 
this frequency will automatically remain in sync with the current VFO on the radio (i.e. if the frequency is changed
in the application, the radio will also change its frequency).

FreeDV will also show the callsigns of previously received signals. To view those, click on the arrow
next to the last received callsign at the bottom of the window. These are in descending order by time
of receipt (i.e. the most recently received callsign will appear at the top of the list).

# Multiple Mode Support

FreeDV can simultaneously decode the following modes when selected prior to pushing "Start":

* 2020/2020B
* 700C/D/E
* 1600

In addition, FreeDV can allow the user to switch between the above modes for transmit without having to push "Stop" first. 
These features can be enabled by going to Tools->Options->Modem and checking the "Simultaneously Decode All HF Modes" option. Note that
this may consume significant additional CPU resources, which can cause decode problems. In addition, these features are automatically
disabled if 800XA or 2400B are selected before pushing "Start" due to the significant additional CPU resources required to decode these
modes.

By default, FreeDV will use as many threads/cores in parallel as required to decode all supported HF modes. On some slower systems, it may be
necessary to enable the "Use single thread for multiple RX operation" option as well. This results in FreeDV decoding each mode in series
and additionally short circuits the list of modes to be checked when in sync.

Additionally, the squelch setting with simultaneous decode enabled is relative to the mode that supports the weakest signals 
(currently 700D).  The squelch for other modes will be set to a value higher than the slider (which is calculated by adding the 
difference between the "Min SNR" of 700D and the mode in question; see "FreeDV Modes" below). For example, the squelch for 700E
when the squelch slider is set to -2.0 becomes 1.0dB. This is designed to reduce undesired pops and clicks due to false decodes.

# FreeDV Modes

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

## FreeDV 700D

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

## FreeDV 700E

FreeDV 700E was developed in December 2020 using lessons learned from on air operation of 700C and 700D.  A variant of 700D, it uses a shorter frame size (80ms) to reduce latency and sync time.  It is optimised for fast fading channels channels with up to 4Hz Doppler spread and 6ms delay spread.  FreeDV 7000E uses the same 700 bit/s codec as FreeDV 700C and 700D.  It requires about 3dB more power than 700D, but can operate reliably on fast fading channels.

The 700E release also includes optional compression (clipping) of the 700D and 700E transmit waveforms to reduce the Peak to Average Power Ratio to about 4dB.  For example a 100W PEP transmitter can be driven to about 40W RMS.  This is an improvement of 6dB over previous releases of FreeDV 700D. Before enabling the clipper make sure your transmitter is capable of handling sustained high average power without damage.  

Clipping can be enabled via Tools-Options.

On good channels with high SNR clipping may actually reduce the SNR of the received signal.  This is intentional - we are adding some pre-distortion in order to increase the RMS power.  Forward error correction (FEC) will clean up any errors introduced by clipping, and on poor channels the benefits of increased signal power outweigh the slight reduction in SNR on good channels.

## FreeDV 2020

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

## FreeDV 2020B

Experimental mode developed in February 2022.  The goal of this mode is to improve the performance of FreeDV 2020 over HF channels.

Here are the three main innovations, and the theoretical improvements:

1. Compression (clipping) of the 2020x modem waveforms has been added, which is worth about 4dB. This should also improve the original FreeDV 2020 mode.  The Clipping checkbox is located on Tools-Options-Modem.  As per the other warnings in this manual please make sure you transmitter can handle the higher RMS power.
1. 2020B is like 700E to 700D - it works with fast fading but requires a few more dB of SNR. This will make it usable in European Winter (or over the South Pole Argentina to Australia) type channels - if you have enough SNR. The challenge with this mode is squeezing all the information we need (enough pilots symbols for fast fading, LPCNet, FEC bits) into a 2100 Hz channel - we are pushing up again the edges of many SSB filters. It also uses unequal FEC, just the most important 11 bits are protected.

This modes is under development and may change at any time.  If you experience comparability issues with another operator - check your Git Hash values on the Help-about menu to ensure you are running the same versions of LPCNet and codec2.

It is recommended that multi-rx be disabled when using 2020B. This mode is not supported by multi-rx, you will need to manually coordinate the mode with other stations.

# Tools Menu

## Tools - Filter

This section describes features on Tools-Filter.  

Control | Description
 -------------------------- | ------------------------------------------------------------------------ |
Noise Suppression | Enable noise suppression, dereverberation, AGC of mic signal using the Speex pre-processor
700C/700D Auto EQ | Automatic equalisation for FreeDV 700C and FreeDV 700D Codec input audio

Auto EQ (Automatic Equalisation) adjusts the input speech spectrum to best fit the speech codec. It can remove annoying bass artefacts and make the codec speech easier to understand.

* [Blog Post on Auto EQ Part 1](http://www.rowetel.com/?p=6778)
* [Blog Post on Auto EQ Part 2](http://www.rowetel.com/?p=6860)

## Tools - Options

### Modem Options

Control | Description
 ------------------------------ | ----------------------------------------------------------------------------- |
Clipping | Increases the average power. Ensure your transmitter can handle high RMS powers before using!
700C Diversity Combine | Combining of two sets of 700C carriers for better fading channel performance
TX Band Pass Filter | Reduces TX spectrum bandwidth

# Helping Improve FreeDV

If you have an interesting test case, for example:

1. FreeDV working poorly with a particular person or microphone.
1. Poor over the air performance on a fast fading channel.
1. Problems with sync on strong signals.
1. A comparison with SSB.

Please send the developers an off air recording of the signal.  FreeDV can record files from your radio using Tools-Record File from Radio.  A recording of 30 to 60 seconds is most useful.

With a recording we can reproduce your exact problem.  If we can reproduce it we can fix it. Recordings are much more useful than anecdotes or subjective reports like "FreeDV doesn't work", "SSB is better", or "On 23 December it didn't work well on grid location XYZ".  With subjective reports problems are impossible to reproduce, cannot be fixed, and you are unlikely to get the attention of the developers.

# Multiple Panes in GUI window

It is possible to have multiple panes opened within the GUI window for example, to observe both the Waterfall and Spectrum Tabs. New panes may be added above, below, left or right of existing panes.

A new visible pane is created by hovering the cursor over the required Tab, click and hold the left mouse button and drag the Tab to the required position and releasing the mouse button. If currently two panes are stacked vertically a third pane may be added either beside either pane or to the left/right of both panes.  If the Tab is required adjacent to both panes then it must be dragged to the left/right of the junction of the existing Tabs.

As the Tab is dragged into position a faint blue/grey image will show the position to be occupied by the pane. Panes may be relocated back to the menu bar by a similar process.

Tabs can be resized as required by hovering the cursor over the border and clicking and holding the left mouse button and dragging to required size.

The layout is not saved when the program is exited and must be recreated next time the program is started

![Multiple Panes](contrib/multiple_panes.png)

# Advanced/Developer Features

## Stats Window

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

## Timing Delta Tab

This indicates the symbol timing estimate of the demodulator, in the
range of +/- 0.5 of a symbol.  With off air signals this will have a
sawtooth appearance, as the demod tracks the modulator sample clock.
The steeper the slope, the greater the sample clock offset.

* [FreeDV 1600 Sample Clock Offset Bug](http://www.rowetel.com/?p=6041)
* [Testing a FDMDV Modem](http://www.rowetel.com/?p=2433)

## UDP Messages

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

## Sound Card Debug

These features were added for FreeDV 700D, to help diagnose sound card
issues during development.

### Tools - Options dialog:

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

## Test Frame Histogram

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

## Full Duplex Testing with loopback

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

# Tips

1. The space bar can be used to toggle PTT.
1. You can left click on the main window to adjust tuning, the vertical red line on the frequency scale will show the current centre frequency.  FreeDV will automatically track any drift once it syncs.

# Common Problems

## FreeDV Sets Radio To Wrong Mode

By default, FreeDV attempts to set the radio's mode to DIGU/USB-D (or LSB equivalent for 40 meters and below). Some radios
do not support data modes and only have USB and LSB. For these, you can go to Tools->Options->Rig Control and check the
"Use USB/LSB instead of DIGU/DIGL" option. This will cause FreeDV to use the standard USB and LSB modes for rig control instead.

Note that for best results, your radio should have all processing disabled if you're using the standard USB/LSB modes. This
disabling of processing typically takes place when using data mode.

## Overdriving Transmit Level

This is a very common problem for first time FreeDV users.  Adjust your transmit levels so the ALC is just being nudged. More power is not better with FreeDV.  An overdriven signal will have poor SNR at the receiver.  For FreeDV 700D/700E operation with the clipper, make sure your transmitter can sustain high average power levels without damage (e.g. 40W RMS on a 100W PEP radio).

## I can't set up FreeDV, especially the Sound Cards

This can be challenging the first time around:

1. Try a receive only (one audio card) set up first.

1. Ask someone who already runs FreeDV for help.

1. If you don't know anyone local, ask for help on the digital voice
mailing list.  Be specific about the hardware you have and the exact
nature of your problem.

## Hamlib does not work with my Icom radio

The most common issue with Icom radios is that the CI-V address configured
in FreeDV does not match the address configured in the radio. Ensure that
the CI-V address in both FreeDV and on the radio are the same. If "00" is
used on the FreeDV side, ensure that the "CI-V Transceive" option is enabled
on the radio or else the radio will not respond to requests directed to that
address.

On newer radios (e.g. 7300, 7610), you may also need to set "CI-V USB Echo Back" 
to ON as this may be set to OFF by default.

## I need help with my radio or rig interface

There are many radios, many computers, and many sound cards.  It is
impossible to test them all. Many radios have intricate menus with
custom settings.  It is unreasonable to expect the authors of FreeDV to
have special knowledge of your exact hardware.

However someone may have worked through the same problem as you.  Ask
on the digital voice mailing list.

## Can't hear anything on receive

Many FreeDV modes will not play any audio if there is no valid signal.
You may also have squelch set too high.  In some modes the **Analog**
button will let you hear the received signal from the SSB radio.

Try the Test Wave Files above to get a feel for what a FreeDV signal
looks and sounds like.

## The signal is strong but FreeDV won't get sync and decode

Do you have the correct sideband? See USB or LSB section.

Is it a FreeDV signal?  SSTV uses similar frequencies. To understand what FreeDV sounds like, see the Test Wave Files section.

## Trouble getting Sync with 700D

You need to be within +/- 60 Hz on the transmit signal.  It helps if
both the Tx and Rx stations tune to known, exact frequencies such as
exactly 7.177MHz.  On channels with fast fading sync may take a few
seconds.

## PTT doesn't work.  It works with Fldigi and other Hamlib applications.

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

## I'm on Windows and serial port PTT doesn't work with my USB to serial adapter.

Please verify that you are running the correct drivers for the USB to serial adapter
that you're using. Information and download links for the drivers used by the most
common devices can be found [here](https://www.miklor.com/COM/UV_Drivers.php). 

While it is preferred to use devices that use authorized/original versions of the
various USB to serial chipsets, it is possible to use some cloned devices with 
older drivers. When doing this, you may also need to force Windows to use an older 
version of a driver instead of automatically updating the driver on reboot. See
[here](https://wethegeek.com/how-to-disable-automatic-driver-updates-in-windows-10/)
for instructions on doing so in Windows 10. For Windows 8:

1. Search for "Change device" in the Windows 8 Start menu.
1. Click on where it says "Change device installation settings".
1. Select the "No, let me choose what to do" option.
1. Check the "automatically get the device app" option, then click Save changes to save the settings you just chose.

## FreeDV 2020 mode is greyed out

In order to use FreeDV 2020 mode, you must have one of the following:

1. An Intel based CPU with AVX support. A Microsoft utility called [coreinfo](https://docs.microsoft.com/en-us/sysinternals/downloads/coreinfo)
can be used to determine if your CPU supports AVX.  A * means you have 
AVX, a - means no AVX:

```
AES             -       Supports AES extensions
AVX             *       Supports AVX instruction extensions
FMA             -       Supports FMA extensions using YMM state
```

On Linux, you can check for `avx` in the **flags** section of `/proc/cpuinfo`
or the output of the `lscpu` command:
```
lscpu | grep -o "avx[^ ]*"
```
will display `avx` (or `avx2`) if your CPU supports the instructions.

2. A Mac with an ARM processor (e.g. 2020 Mac Mini or later).

If your system does not meet either (1) or (2), the 2020 option will be grayed out.

## FreeDV 2020 mode is slow on ARM Macs

Preliminary testing on ARM Macs has shown that NEON optimizations in LPCNet are
sufficient to allow 2020 to be whitelisted on those machines. However, this is
definitely experimental. If you are experiencing issues with 2020 mode on these
Macs, please let the development team know so that further investigation can be done.

## I installed a new version and FreeDV stopped working

You may need to clean out the previous configuration.  Try Tools - Restore Defaults.  Set up your sound cards again with Tools - Audio Config.

## FreeDV crashes when I press Start

Have you removed/changed USB audio devices? If you remove/change USB audio devices without pressing Tools - Audio Config, FreeDV may crash.  See Changing Audio Devices above.

## FreeDV can't be opened on OSX because the developer cannot be verified

From January 2020 Apple is enforcing notarization for all OSX applications.  The FreeDV developers do not wish to operate within the Apple ecosystem due to the cost/intrusiveness of this requirement.

![Notarization Error](contrib/osx_notarization1.png)

Security & Privacy shows the Open Anyway option for FreeDV:

![Security and Privacy](contrib/osx_notarization2.png)

![Open FreeDV](contrib/osx_notarization3.png)

Or you can use command line options:

```
xattr -d com.apple.quarantine FreeDV.app
```
or
```
xattr -d -r com.apple.quarantine FreeDV.app
```

# Converting this document to PDF

For the Linux inclined:
```
$ pandoc USER_MANUAL.md -o USER_MANUAL.pdf "-fmarkdown-implicit_figures -o" \
--from=markdown -V geometry:margin=.4in --toc --highlight-style=espresso
```

# Glossary

Term | Notes
------- | ---------------------------------------------------------------------------------------------
AWGN | Additive White Gaussian Noise - a channel with just noise and no fading (like VHF)
FEC | Forward Error Correction - extra bits to we send to protect the speech codec bits
LDPC | Low Density Parity Check Codes - a family of powerful FEC codes

# Release Notes

## TBD TBD 2023

1. Bugfixes:
    * Add missed captures for pavucontrol related handlers. (PR #420)
    * Fix issue causing intermittent failures to report the current frequency to FreeDV Reporter. (PR #421)
    * Set initial audio device count on startup. (PR #422)

## V1.8.10.1 June 2023

1. Bugfixes:
    * Fix bug with FreeDV Reporter going out of sync with radio. (PR #408)
    * Allow frequency to be changed even if mode change fails. (PR #408)
    * Only change mode, not frequency, when going in/out of Analog mode. (PR #414)
    * Fix crash when repeatedly switching in and out of Analog mode. (PR #413)

## V1.8.10 June 2023

1. Build system:
    * Minimum required Codec2 version bumped up to 1.1.0. (PR #383)
    * Disable libusb support for Hamlib on all platforms, not just Windows. (PR #387)
    * Build Hamlib as a dynamic library on Windows and macOS. (PR #395)
2. Bugfixes:
    * Fix incorrect assertion causing crash on certain systems without a microphone. (PR #384)
    * Shrink sliders so that the Filter window can fit on a 720p display. (PR #386, #396)
    * Hamlib: use RIG_VFO_CURR if explicit VFO doesn't work. (PR #385, #400)
    * Fix various misspellings in codebase. (PR #392)
    * Prevent Start/Stop button from being pressed twice in a row during shutdown. (PR #399)
3. Enhancements:
    * Add last received SNR to callsign list. (PR #389, #391)
    * Add support for FreeDV Reporter web-based tool. (PR #390, #402, #404)
    * Defer sound device checking until Start is pushed. (PR #393)
    * Add ability for Hamlib to use RTS/DTR instead of CAT for PTT. (PR #394)
    * Automatically change radio frequency if the reporting frequency changes. (PR #405)

## V1.8.9 April 2023

1. Enhancements:
    * Add 20% buffer for systems that are marginally able to decode 2020. (PR #355)
    * Enable RTS and DTR for PTT input to provide a voltage source for some footswitches. (PR #354)
    * Show previously received callsigns in main window. (PR #362, #378)
    * Add Record button to the main window to easily allow recording of the incoming signal. (PR #369)
    * Open GitHub releases page if Check For Updates is selected. (PR #382)
2. Bugfixes:
    * Fix typo preventing use of Easy Setup when not having a radio configured. (PR #359)
    * Fix issue preventing Yaesu sound devices from appearing in Easy Setup. (PR #371)
    * Fix crash on Windows after resizing the window to hide the waterfall. (PR #366, #375)
    * Use /dev/cu.* instead of /dev/tty.* on macOS. (PR #377)
    * Hamlib: avoid use of rig_get_vfo() for radios with only one VFO. (PR #376)
    * Prevent status bar text from truncating unless required. (PR #379)
    * Prevent devices from rearranging if one disappears. (PR #381)
    * Remove record completion popups to align with file playback behavior. (PR #380)
3. Build system:
    * GitHub action now uses LLVM MinGW for pull requests. (PR #360)
    * Update Speex/Hamlib build code to avoid unnecessary rebuilds. (PR #361)
    * Upgrade Hamlib to version 4.5.5. (PR #361)
    * Fix typo preventing proper naming of installers for test builds. (PR #363)
    * macOS builds should also not use Hamlib master. (PR #364)
4. Cleanup:
    * Remove 'Split' button from UI. (PR #372)
    * Remove dead code for RX/TX loopback buttons. (PR #372)

## V1.8.8.1 March 2023

1. Bugfixes:
    * Downgrade hamlib for Windows and macOS due to PTT and CAT control bugs on various radios. (PR #357)

## V1.8.8 March 2023

1. Bugfixes:
    * Resolve compile failure in EasySetupDialog on openSUSE. (PR #344)
    * Prevent Mode box from auto-resizing to avoid unexpected movement of other controls. (PR #347)
2. Build system:
    * CPack: Properly handle the case where FREEDV_HASH doesn't exist. (PR #345)
3. Enhancements:
    * Show friendlier error if serial ports can't be opened. (PR #348)
    * Use same VFO retrieval mechanism for PTT as with frequency sync. (PR #350)
    * Tweak PSK Reporter handling to report received callsigns more quickly. (PR #352)
    
## V1.8.7 January 2023

1. Code Cleanup:
    * Remove "force sync" option from Tools->Options (PR #332)
2. Enhancements:
    * Add "Easy Setup" dialog to simplify first time setup. (PR #189)
3. Bugfixes:
    * Add a bit of extra space for the sample rate drop-downs. (PR #336)
    * Add units for SNR gauge to match squelch gauge. (PR #339)
    * Fix compiler errors due to recent samplerate changes. (PR #338)
    * Fix inability to change to certain FreeDV modes for transmit. (PR #340)

## V1.8.6 December 2022

1. Build system:
    * Suppress documentation generation when tagging releases. (PR #314)
    * Simplify build to reduce need for build scripts. (PR #305, #329, #331)
2. Bugfixes:
    * Filter out non-MME devices to match previous behavior. (PR #318)
    * Use 64 bit int for frequency to enable reporting microwave frequencies. (PR #325, #331)
    * Resolves issue with minimum button sizing in the Filter dialog. (PR #326, #331)
    * Update labeling of clipping and BPF options to match actual behavior. (PR #319)
    * Adjusts positioning and spacing of controls in the Options dialog to prevent truncation. (PR #328, #331)
3. Enhancements:
    * Add 2020B to multi-RX feature to enable RX and TX without restarting session. (PR #312)
    * Hide modes not on the SM1000 by default. (PR #313)
    * Increase the default Record From Modulator time to 60 seconds. (PR #321)
4. Code Cleanup:
    * Adjusted function prototypes to use bool instead of int. (PR #316)

## V1.8.5 December 2022

1. Build system:
    * Add checks for .git folder to prevent errors when building from official release tarballs. (PR #294)
    * Simplify PortAudio static build to fix multi-core build issue on macOS. (PR #304, #308)
    * Upgrade bootstrapped wxWidgets to v3.2.1. (PR #302)
    * Upgrade Docker container to Fedora 37. (PR #306)
2. Enhancements:
    * Update FreeDV configuration defaults to improve first-time usability. (PR #293)
3. Bugfixes:
    * Fix issue preventing macOS binaries from running on releases older than 12.0. (PR #301)
    * Fix issue with 2020B not being selected as default on next start (PR #299)
4. Documentation:
    * Update manual to reflect Ubuntu renaming libsndfile-dev to libsnd1file-dev. (PR #297)

## V1.8.4 October 2022

1. Build system:
    * Updates to reflect LPCNet decoupling from Codec2 (PR #274)
2. Bugfixes:
    * Add missed UI disable on startup for 2020B mode. (PR #279)
    * Fixed TX audio dropouts when using different sample rates. (PR #287)
    * Remove sample rates above 48K from audio configuration. (PR #288)
3. Enhancements:
    * Add alternate method of determining 2020 support for non-x86 machines. (PR #280)
    * Remove unnecessary BW and DPSK options from UI. (PR #283)
    * Stats on left hand side of main window now auto-reset after user-configurable time period (default 10s). (PR #262, #286)

## V1.8.3.1 August 2022

1. Build system:
    * Fix issue preventing patch version from being passed to Windows installer. (PR #271)
    
## V1.8.3 August 2022

1. Build system:
    * Build Git version of Hamlib for Windows builds. (PR #261)
    * Remove build date and time from libsox. (PR #267)
    * Refactor CMakeList.txt using newer project format. (PR #268)
1. Enhancements:
    * Update frequency and mode display every 5 sec. (PR #266)
    
## V1.8.2 July 2022

1. Enhancements:
    * Save rig names instead of IDs to prevent Hamlib off by one issues. (PR #256)
2. Bugfixes:
    * Increase plot buffer size to resolve issues with "To Spkr/Headphones" tab (PR #258)
3. Build system:
    * Depend on Codec2 1.0.5. (PR #259)
    
## V1.8.1 July 2022

1. Bugfixes:
    * Disable 2020B unless the installed Codec2 provides it. (PR #257)
2. Build system:
    * Update build scripts to use specific Codec2 and LPCNet versions. (PR #257)
    
## V1.8.0 July 2022

1. Enhancements:
    * PSK Reporter: Encodes callsign regardless of whether the internet is working. (PR #214)
    * PSK Reporter: Sends report upon pushing Stop (vs. simply clearing the report list). (PR #214)
    * PSK Reporter: Performs reporting in background instead of hanging the caller of the PskReporter class. (PR #214)
    * PSK Reporter: Suppress reporting if we're playing back a radio file (to avoid false reports). (PR #214)
    * Filter dialog: Increase length of vertical sliders to simplify fine-tuning. (PR #224)
    * Modem compression (Tools-Options-Modem Clipping checkbox) added to FreeDV 2020 for increased RMS power. (PR #211)
    * Added experimental 2020B mode. (PR #211)
    * Refactored audio handling to use pipeline design pattern. (PR #219)
    * Eliminated requirement to use the same audio sample rate for both mic and speaker devices. (PR #219, #234)
    * 60 meters shows as USB and not LSB for countries where FreeDV usage is legal on that band. (PR #243)
    * Improved audio quality and reduced CPU usage for multi-RX. (PR #246)
2. Build system:
    * Add spell checking of codebase on every Git push. (PR #216)
    * Build Windows build on every Git push. (PR #220)
    * Default branch and repo to the current branch and repo for Docker (or else reasonable defaults). (PR #233)
3. Documentation:
    * Removed obsolete references to required sample rates for voice keyer files. (PR #219)
    * Add troubleshooting instructions for serial port PTT on Windows. (PR #226)
    * Add missing gcc-g++ package to Fedora build instructions. (PR #235)
    * Add missing sox package to Fedora build instructions. (PR #241)
4. Bugfixes:
    * Suppress refresh of the sync indicator if disabled/no change in sync. (PR #230)
    * Clarify location from where to run Docker build script. (PR #231)
    * Change shutdown ordering to prevent hangs on slower systems. (PR #236)
    * Disable PulseAudio suspend failure due to interactions with pipewire. (PR #239)

## V1.7.0 February 2022

1. Bugfixes:
    * Resolves issue with waterfall appearing garbled on some systems. (PR #205)
    * Resolves issue with Restore Defaults restoring previous settings on exit. (PR #207)
    * Resolves issue with some sound valid sound devices causing PortAudio errors during startup checks. (PR #192)
2. Enhancements:
    * Removes requirement to restart FreeDV after using Restore Defaults. (PR #207)
    * Hides frequency display on main window unless PSK Reporter reporting is turned on. (PR #207)
    * Scales per-mode squelch settings when in multi-RX mode to reduce unwanted noise. (PR #186)
    * Single-thread mode is now the default when multi-RX is turned on. (PR #175)
    * Makes multi-RX mode the default. (PR #175)
    * Mic In/Speaker Out volume controls added to Filter window. (PR #208)
    * Cleans up UI for filters and makes the dialog non-modal. (PR #208)
    * Adds optional support for PulseAudio on Linux systems. (PR #194)
3. Documentation:
    * Adds section on creating Windows shortcuts to handle multiple configurations. (PR #204)
    * Resolves issue with PDF image placement. (PR #203)
4. Build System:
    * Uses more portable way of referring to Bash in build scripts. (PR #200)
    * User manual now installed along with executable. (PR #187)
    * macOS app bundle generated by CMake instead of manually. (PR #184)
    * Fail as soon as a step in the build script fails. (PR #183)
    * Have Windows uninstaller clean up Registry. (PR #182)
    * Windows installer now installs sample .wav files. (PR #182)
    
## V1.6.1 September 2021

1. Bugfixes:
    * Uses UTF-8 for device names from PortAudio to resolve display problems on non-English systems. (PR #153)
    * Resolves crash when using click to tune feature on main window. (PR #157)
    * Resolves issue where test plots inside Audio Options dialog hang during test. (PR #154)
    * Disable multi-RX options in Tools->Options when a session is active. (PR #154)
    * Resolves buffer overflow when using mono-only TX sound devices. (PR #169)
2. Enhancements:
    * Updates mode indicator on transition between TX and RX instead of only on start. (PR #158)
    * Updates PSK Reporter feature to use new Codec2 reliable\_text API. (PR #156, #162, #166, #168)
    * Suppress unnecessary rig_init() calls to prevent FreeDV from changing the current VFO. (PR #173)

_Note: The PSK Reporter feature beginning in this release is incompatible with versions older than 1.6.1 due to a change in how callsigns are encoded._

## V1.6.0 August 2021

1. Bugfixes: 
    * Suppressed clipping of TX speech when PTT is released. (PR #123)
    * Added missing mode labels for 800XA and 2400B as a result of implementing multi-RX in 1.5.3. (PR #128)
    * Fixed analog passthrough when using 2400B. (PR #130)
    * Fixed non-responsive scroll controls on macOS. (PR #139)
    * Auto EQ now working for 800XA. (PR #141)
    * Reset scatter plot state when multi-RX switches modes. (PR #146)
    * Use selected sound device sample rates for the equalizer controls. (PR #142)
2. Enhancements:
    * Frequency ticks moved to the top of the waterfall. (PR #115)
    * Optimized rendering code for the waterfall display to improve responsiveness on slower machines. (PR #127, #137)
    * Fixed navigation issues encountered while using screen readers. (PR #121)
    * Allow main window to expand horizontally for shorter displays. (PR #135, #121)
    * Allow autoconversion of voice keyer file to selected TX mode's sample rate. (PR #145)
    * Multi-RX: decode each supported mode on its own thread. (PR #129)
3. New features:
    * Added support for alternative configuration files by specifying -f/--config options. (PR #119, #125)
    * Added support for PTT input, e.g. for foot switches. (PR #136)
4. Build system:
    * Use MacPorts/Homebrew PortAudio for macOS builds. (PR #134, #138)
    * Bootstrapped wxWidgets now uses version 3.1.5. (PR #147)
    * Added support for bootstrapped wxWidgets on Windows builds. (PR #124)
    * Updated Docker container for Windows builds to Fedora 34. (PR #124)
    * Created "make dist" target for easy tarball generation. (PR #152)

## V1.5.3 April 2021

1. Simultaneous decode of 2020, 1600 and 700C/D/E (without needing to push Stop first, change the mode and push Start again).
2. Dynamic switching of the current Tx mode between the aforementioned modes, again without needing to restart the session.
3. A Tx level slider on the right hand side of the main screen to fine-tune transmit output (to more easily avoid clipping ALC and conflicting with other soundcard ham radio applications).

## V1.5.2 January 2021

1. Updates storage for sound card configuration to use device names instead of IDs.
2. Detects changes to computer sound card configuration and notifies user when devices go away.

## V1.5.1 January 2021

1. Experimental support for reporting to [PSK Reporter](https://pskreporter.info) added.
2. Bug fixes with audio configuration to allow mono devices to be used along with stereo ones.
3. Tweaks to user interface and record/playback functionality to improve usability.
4. Bug fixes and tweaks to improve voice keyer support.

## V1.5.0 December 2020

1. FreeDV 700E, better performance than 700D on fast fading channels
1. FreeDV 700D/700E clipper to increase average transmit power by 6dB

## V1.4.3 August 2020

1. Maintenance Release (no major new features)
1. Changes to support wxWidgets 3.1 (but Windows versions built against wxWidgets 3.0)
1. Under the hood - OFDM modem has been refactored, shouldn't affect freedv-gui operation

## V1.4.2 July 2020

1. Maintenance Release (no major new features)
1. Improved squelch/audio pass through on 700D/2020/2400B
1. Under the hood - Codec2 library has been refactored, shouldn't affect freedv-gui operation
1. Removed Project Horus support (now being maintained outside of Codec2/FreeDV)

## V1.4 June-October 2019

1. FreeDV 2020, Project Horus Binary Modes.
1. [Improved OFDM Modem Acquisition](http://www.rowetel.com/?p=6824), this will improve sync time on FreeDV 700D and 2020 on HF fading channels, and can also handle +/- 60 Hz frequency offsets when tuning.
1. Fixed FreeDV 700C frequency offset bug fix, was losing sync at certain frequency offsets.
1. Wide bandwidth phase estimation and DPSK for OFDM modes (700D/2020) for fast fading/QO-100 channels (Tools-Options)
1. Better speech quality on FreeDV 700C/700D with Auto equaliser (Tools-Filter)

## V1.3 May 2018

* FreeDV 700D

# References

* [FreeDV Web site](http://freedv.org)
* [FreeDV Technology Overview](https://github.com/drowe67/codec2/blob/master/README_freedv.md)
* [Digitalvoice mailing list](https://groups.google.com/forum/#!forum/digitalvoice)
 
