# Changes in older releases

## V1.9.9.2 June 2024

1. Bugfixes:
    * Remove TX attenuation and squelch tooltips. (PR #717)
    * Disable 800XA radio button when in RX Only mode. (PR #716)

## V1.9.9.1 April 2024

1. Bugfixes:
    * Revert PR #689 and reimplement fix for original startup delay issue. (PR #712)
2. Enhancements:
    * Allow "Msg" column to be resized by the user. (PR #721)

## V1.9.9 April 2024

1. Bugfixes:
    * Cache PortAudio sound info to improve startup performance. (PR #689)
    * Fix typo in cardinal directions list. (PR #688)
    * Shrink size of callsign list to prevent it from disappearing off the screen. (PR #692)
    * Clean up memory leak in FreeDV Reporter window. (PR #705)
    * Fix issue causing delayed filter updates when going from tracking band to frequency. (PR #710)
    * Fix hanging issue with footswitch configured. (PR #707)
2. Enhancements:
    * Add additional error reporting in case of PortAudio failures. (PR #695)
    * Allow longer length user messages. (PR #694)
    * Add context menu for copying messages to the clipboard. (PR #694)
3. Documentation:
    * Remove broken links in README. (PR #709)
4. Build system:
    * Add ability to build without LPCNet in preparation for potential future deprecation of 2020/2020B. (PR #711)

## V1.9.8 February 2024

1. Bugfixes:
    * Prevent unnecessary recreation of resamplers in analog mode. (PR #661)
    * Better handle high sample rate audio devices and those with >2 channels. (PR #668)
    * Fix issue preventing errors from being displayed for issues involving the FreeDV->Speaker sound device. (PR #668)
    * Fix issue resulting in incorrect audio device usage after validation failure if no valid default exists. (PR #668)
    * Fix bug where PTT button background color doesn't change when toggling PTT via space bar. (PR #669)
    * Fix bug where FreeDV crashes if only RX sound devices are configured with mic filters turned on. (PR #673)
    * Fix Windows-specific off by one issue in FreeDV Reporter sorting code. (PR #681)
2. Enhancements:
    * Add Frequency column to RX drop-down. (PR #663)
    * Update tooltip for the free form text field to indicate that it's not covered by FEC. (PR #665)
    * Enable use of space bar for PTT when in the FreeDV Reporter window. (PR #666)
    * Move TX Mode column to left of Status in FreeDV Reporter window. (PR #670)
    * Add heading column to FreeDV Reporter window. (PR #672, #675)
    * Prevent FreeDV Reporter window from being above the main window. (PR #679)
    * Add support for displaying cardinal directions instead of headings. (PR #685)
3. Code cleanup:
    * Move FreeDV Reporter dialog code to dialogs section of codebase. (PR #664)

## V1.9.7.2 January 2024

1. Bugfixes:
    * Another attempt at fixing the crash previously thought to have been fixed by 1.9.7.1. (PR #659)

## V1.9.7.1 January 2024

1. Bugfixes:
    * Fix issue causing intermittent crashes when filters are enabled while running. (PR #656)

## V1.9.7 January 2024

1. Bugfixes:
    * Use double precision instead of float for loading frequency list. (PR #627)
    * Improve validation of frequencies in Options dialog. (PR #628)
    * Fix typo resulting in TX device sample rate being used for filter initialization. (PR #630)
    * Fix intermittent crash resulting from object thread starting before object is fully initialized. (PR #630)
    * Prevent creation of filters if not enabled. (PR #631)
    * Fix issue preventing Start button from re-enabling itself on audio device errors. (PR #636)
    * Fix issue preventing proper FreeDV Reporter column sizing on Windows. (PR #638)
    * Fix flicker in FreeDV Reporter window when tracking by frequency. (PR #637)
    * Update Filter dialog to better handle resizing. (PR #641)
    * Fix capitalization of distance units in FreeDV Reporter window. (PR #642)
    * Rename KHz to kHz in documentation and UI. (PR #643)
    * Avoid calculating distances in FreeDV Reporter window for those with invalid grid squares. (PR #646, #649)
    * Fix display bugs in FreeDV Reporter window when switching between dark and light mode. (PR #646)
    * Add guard code to prevent FreeDV Reporter window from being off screen on startup. (PR #650)
    * Fix issue preventing FreeDV startup on macOS <= 10.13. (PR #652)
    * On startup, only jiggle height and not width. (PR #653)
    * Fix issue preventing FreeDV from being linked with older versions of Xcode. (PR #654)
    * Fix issue preventing TX audio from resuming after going from TX->RX in full duplex mode. (PR #655)
2. Enhancements:
    * Allow user to refresh status message even if it hasn't been changed. (PR #632)
    * Increase priority of status message highlight. (PR #632)
    * Adjust FreeDV Reporter data display to better match accepted UX standards. (PR #644)
    * Further reduce required space for each column in FreeDV Reporter window. (PR #646)
    * Provide an option Do save only certain FreeDV Reporter messages sent to the server. (PR #647)
3. Build system:
    * Include PDB debugging file for FreeDV. (PR #633)
    * End support for 32 bit ARM on Windows. (PR #651)
    * Begin performing CI builds for macOS. (PR #654)
4. Documentation:
    * Fix spelling, etc. mistakes in the documentation. (PR #640)
    * Update README to reflect latest state of codebase. (PR #654)
    * Move older changelog from user manual to separate file. (PR #654)
5. Code cleanup:
    * Move GUI related files into their own folder. (PR #654)
    * Move build scripts into cmake folder. (PR #654)
    * Remove no longer used scripts and patch files. (PR #654)

## V1.9.6 December 2023

1. Bugfixes:
    * Use SetSize/GetSize instead of SetClientSize/GetClientSize to work around startup sizing issue. (PR #611)
    * Check for RIGCAPS_NOT_CONST in Hamlib 4.6. (PR #615)
    * Make main screen gauges horizontal to work around sizing/layout issues. (PR #613)
    * Fix compiler issue with certain versions of MinGW. (PR #622)
    * Suppress use of space bar when in RX Only mode. (PR #623)
    * Fix Windows-specific issue preventing entry of very high frequencies. (PR #624)
2. Enhancements:
    * Add option to add a delay after starting TX and before ending TX. (PR #618)
    * Allow serial PTT to be enabled along with OmniRig. (PR #619)
    * Add 800XA to multi-RX list. (PR #617)
    * Add logic to report status message to FreeDV Reporter. (PR #620)
    * Allow display and entry of frequencies in kHz. (PR #621)
    * Add 5368.5 kHz to the default frequency list. (PR #626)

## V1.9.5 November 2023

1. Bugfixes:
    * Fix bug preventing frequency updates from being properly suppressed when frequency control is in focus. (PR #585)
    * Fix bug preventing 60 meter frequencies from using USB with DIGU/DIGL disabled. (PR #589)
    * Additional fix for PR #561 to parse/format frequencies using current locale. (PR #595)
    * Add entitlements to work around macOS Sonoma permissions bug. (PR #598)
    * Fix bug preventing FreeDV Reporter window from closing after resetting configuration to defaults. (PR #593)
    * Fix bug preventing reload of manually entered frequency on start. (PR #608)
2. Enhancements:
    * FreeDV Reporter: Add support for filtering the exact frequency. (PR #596)
    * Add confirmation dialog box before actually resetting configuration to defaults. (PR #593)
    * Add ability to double-click FreeDV Reporter entries to change the radio's frequency. (PR #592)
    * FreeDV Reporter: Add ability to force RX Only reporting in Tools->Options. (PR #599)
    * Add new 160m/80m/40m calling frequencies for IARU R2. (PR #601)
    * Add Help button to allow users to get help more easily. (PR #607)
3. Build system:
    * Upgrade wxWidgets to 3.2.4. (PR #607)
4. Other:
    * Report OS usage to FreeDV Reporter. (PR #606)

## V1.9.4 October 2023

1. Bugfixes:
    * Fix issue causing hanging while testing serial port PTT. (PR #577)
    * Fix issue causing improper RX Only reporting when Hamlib is disabled. (PR #579)
    * Fix compiler error on some Linux installations. (PR #578)
    * Fix issue causing error on startup after testing setup with Easy Setup. (PR #575)
    * Fix issue preventing PSK Reporter from being enabled by default. (PR #575)
2. Enhancements:
    * Add experimental support for OmniRig to FreeDV. (PR #575)
        * *Note: This is only available on Windows.*

## V1.9.3 October 2023

1. Bugfixes:
    * FreeDV Reporter:
        * Fix regression preventing proper display of "RX Only" stations. (PR #542)
        * Fix issue causing duplicate entries when filtering is enabled and users disconnect/reconnect. (PR #557)
    * Default to the audio from the current TX mode if no modes decode (works around Codec2 bug with 1600 mode). (PR #544)
    * Fix bug preventing proper restore of selected tabs. (PR #548)
    * Properly handle frequency entry based on user's current location. (PR #561)
    * Improve labeling of PTT/CAT control options. (PR #550)
    * Clarify behavior of "On Top" menu option. (PR #549)
    * Work around Xcode issue preventing FreeDV from starting on macOS < 12. (PR #553)
    * Fix issue preventing selection of FreeDV Reporter users during band tracking. (PR #555)
    * Work around issue preventing consistent switchover to 'From Mic' tab on voice keyer TX. (PR #563)
    * Fix rounding error when changing reporting frequency. (PR #562)
    * Fix issue causing multiple macOS microphone permissions popups to appear. (PR #566, 567)
    * macOS: Fix crash on start when using Rosetta. (PR #569)
2. Enhancements:
    * Add configuration for background/foreground colors in FreeDV Reporter. (PR #545)
    * Always connect to FreeDV Reporter (in view only mode if necessary), regardless of valid configuration. (PR #542, #547)
    * Add None as a valid PTT method and make it report RX Only. (PR #556)
    * Increase RX coloring timeout in FreeDV Reporter to 20 seconds. (PR #558)
3. Build system:
    * Upgrade wxWidgets on binary builds to 3.2.3. (PR #565)
4. Documentation:
    * Add information about multiple audio devices and macOS. (PR #554)
    * Fix Registry and config file paths in documentation. (PR #571, #572)
5. Cleanup:
    * Refactor rig control handling to improve performance and maintainability. (PR #564)

## V1.9.2 September 2023

1. Bugfixes:
    * Initialize locale so that times appear correctly. (PR #509)
    * Fix issue with Voice Keyer button turning blue even if file doesn't exist. (PR #511)
    * Fix issue with Voice Keyer file changes via Tools->Options not taking effect until restart. (PR #511)
    * Eliminate mutex errors during Visual Studio debugging. (PR #512)
    * Add timeout during deletion of FreeDVReporter object. (PR #515)
    * Fixes bug preventing display of reporting UI if enabled on first start. (PR #520)
    * Adjust vertical tick lengths on waterfall to prevent text overlap. (PR #518)
    * Adjust coloring of text and ticks on spectrum plot to improve visibility when in dark mode. (PR #518)
    * Resolve issue preventing proper device name display in Easy Setup for non-English versions of Windows. (PR #524)
    * Fix intermittent crash on exit due to improperly closing stderr. (PR #526)
2. Enhancements:
    * Add tooltip to Record button to claify its behavior. (PR #511)
    * Add highlighting for RX rows in FreeDV Reporter (to match web version). (PR #519)
    * Add Distance column in FreeDV Reporter window. (PR #519)
    * Add support for sorting columns in FreeDV Reporter window. (PR #519, #537)
    * Allow use of FreeDV Reporter without having a session running. (PR #529, #535)
    * Adds support for saving and restoring tab state. (PR #497)
        * *NOTE: Requires 'Enable Experimental Features' to be turned on, see below.*
    * Adds configuration item allowing optional use of experimental features. (PR #497)
        * This option is called "Enable Experimental Features" in Tools->Options->Debugging.
    * Add FreeDV Reporter option to have the band filter track the current frequency. (PR #534)
3. Build system:
    * Upgrade wxWidgets on binary builds to 3.2.2.1. (PR #531)
    * Fix issue preventing proper generation of unsigned Windows installers. (PR #528)
    * Update code signing documentation and defaults to use certificate provider's token instead of our own. (PR #533)
4. Cleanup:
    * Remove unneeded 2400B and 2020B sample files. (PR #521)

## V1.9.1 August 2023

1. Bugfixes:
    * Revert BETA back to prior 1.9.0 value for waterfall. (PR #503)
    * Optimize FreeDV Reporter window logic to reduce likelihood of waterfall stuttering. (PR #505)
    * Fix intermittent crash during FreeDV Reporter updates. (PR #505)
    * Fix intermittent crash on exit due to Hamlib related UI update code executing after deletion. (PR #506)
    * Fix serial port contention issue while testing PTT multiple times. (PR #506)
2. Enhancements:
    * Add support for monitoring voice keyer and regular TX audio. (PR #500)
    * Add background coloring to indicate that the voice keyer is active. (PR #500)

## V1.9.0 August 2023

1. Bugfixes:
    * Fix bug preventing proper Options window sizing on Windows. (PR #478)
    * Fix various screen reader accessibility issues. (PR #481)
    * Use separate maximums for each slider type on the Filter dialog. (PR #485)
    * Fix minor UI issues with the Easy Setup dialog. (PR #484)
    * Adjust waterfall settings to better visualize 2 Hz fading. (PR #487)
    * Fix issue causing the waterfall to not scroll at the expected rate. (PR #487)
    * Resolve bug preventing certain radios' serial ports from being listed on macOS. (PR #496)
2. Enhancements
    * Allow users to configure PTT port separately from CAT if Hamlib is enabled. (PR #488)
    * Add ability to average spectrum plot across 1-3 samples. (PR #487, 492)
    * Adjust sizing of main page tabs for better readability. (PR #487)
    * Add ability to sign Windows binaries for official releases. (PR #486)
    * Allow use of a different voice keyer file by right-clicking on the Voice Keyer button. (PR #493)
    * Include TX audio in recorded audio files to enable recording a full QSO. (PR #493)
    * Add band filtering in the FreeDV Reporter dialog. (PR #490, #494)
    * Add ability to record new voice keyer files by right-clicking on the Voice Keyer button. (PR #493)
3. Build system:
    * Update Codec2 to v1.2.0. (PR #483)
    * Deprecate PortAudio support on Linux. (PR #489, #491)
4. Cleanup:
    * Remove 2400B mode from the UI. (PR #479)
    * Remove rarely-used "Record File - From Modulator" and "Play File - Mic In" menu options. (PR #493)

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
