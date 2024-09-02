@echo off

set RADAE_PATH=%1
set RADAE_VENV=%2

rem The below does the following:
rem     * For each block of microphone audio from freedv-gui:
rem         * Extract features from 16 kHz input audio.
rem         * Pass features into the RADAE encoder.
rem         * Send the resulting 8 kHz audio back to freedv-gui.
rem
rem Note: Current RADAE scripts seem to require being executed from
rem the RADAE folder.
cd %RADAE_PATH%
set PATH=%RADAE_VENV%;%PATH%
build\src\lpcnet_demo -features - - | %RADAE_VENV%\python.exe -u radae_tx.py model19_check3\checkpoints\checkpoint_epoch_100.pth --auxdata | %RADAE_VENV%\python.exe -u f32toint16.py --real --scale 16383
