# Changes in older releases

## V1.8.12 July 2023

1. Bugfixes:
    * Clear audio plots when recording or playback starts. (PR #439)
    * Clear button now clears the callsign list. (PR #436)
    * Fix bug causing the PTT button to stay red after the voice keyer finishes TX. (PR #440)
    * Fix FreeDV Reporter crash when sending RX record. (PR #443)
    * Hamlib: set mode before frequency to avoid accidental offsetting. (PR #442, #452)
    * Fix audio dialog plot display and lockup issues. (PR #450)
    * Disable PTT and Voice Keyer buttons if only RX devices are configured. (PR #449)
    * Fix Linux display bugs when switching between dark and light mode. (PR #454)
2. Enhancements:
    * Add the ability to request that another FreeDV Reporter user QSY. 
      (PR #434, #453, #456, #458, #459, #467, #468)
    * Display 'Digital' on button when Analog mode is active. (PR #447)
    * Set minimum size for Mode box to 250px. (PR #446)
    * Notify FreeDV Reporter if only capable of RX. (PR #449)
    * Hamlib: allow frequency and mode changes during TX. (PR #455)
    * Save and restore size and position of FreeDV Reporter window on startup. (PR #462)
    * Auto-size columns in Audio Options to improve readability. (PR #461)
    * Add support for modifying the drop down frequency list. (PR #460)
    * Preserve size and position of Audio Configuration dialog. (PR #466)
    * Add ability to suppress automatic frequency reporting on radio changes. (PR #469)
3. Build system:
    * Bump Codec2 version to v1.1.1. (PR #437)
    * Generate PDF/HTML docs only on PR merge. (PR #471)
4. Documentation
    * Add RF bandwidth information to user manual. (PR #444)
5. Cleanup:
    * Refactor configuration handling in the codebase. (PR #457, #474)
    * Clean up compiler warnings on Windows builds. (PR #475)
6. Miscellaneous:
    * Set default FreeDV Reporter hostname to qso.freedv.org. (PR #448)

*Note for Windows users: you may receive a one-time error message on startup 
after upgrading from v1.8.12-20230705 indicating that certain Registry keys 
have incorrect types. This is expected as part of the bugfix merged in PR #474.*

## V1.8.11 June 2023

1. Bugfixes:
    * Add missed captures for pavucontrol related handlers. (PR #420)
    * Fix issue causing intermittent failures to report the current frequency to FreeDV Reporter. (PR #421)
    * Set initial audio device count on startup. (PR #422)
    * Make sure focus isn't on the Report Frequency text box immediately after starting. (PR #430)
2. Enhancements:
    * Add option to enable/disable Hamlib frequency/mode control. (PR #424, #427)
    * Add option to enable/disable Space key for PTT. (PR #425)
    * Turn PTT button red when transmitting. (PR #423)

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
    * Downgrade Hamlib for Windows and macOS due to PTT and CAT control bugs on various radios. (PR #357)

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
