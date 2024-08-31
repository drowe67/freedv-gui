@echo off

set RADAE_PATH=%1
set RADAE_VENV=%2

rem The below does the following:
rem     * For each block of OTA audio from freedv-gui:
rem         * Convert the 8 kHz audio into IQ data via zero-padding.
rem         * Pass the IQ data into the RADAE decoder 
rem         * Send the resulting 16 kHz audio back to freedv-gui for playback.
rem
rem Note: Current RADAE scripts seem to require being executed from
rem the RADAE folder.
cd %RADAE_PATH%
set PATH=%RADAE_VENV%\scripts;%PATH%
%RADAE_VENV%\scripts\python.exe -u int16tof32.py --zeropad | %RADAE_VENV%\scripts\python.exe -u radae_rx.py model19_check3\checkpoints\checkpoint_epoch_100.pth -v 2 --auxdata | build\src\lpcnet_demo -fargan-synthesis - -
