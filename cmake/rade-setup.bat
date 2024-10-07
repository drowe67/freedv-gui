@echo off

rem Installs required packages for RADE.
rem NOTE: must run as administrator!

echo Completing installation of Python for FreeDV. DO NOT CLOSE THIS WINDOW.
echo In case of error or interruption, you can execute %~dp0\rade-setup.bat as administrator to complete setup.
echo -------------------------------------------------------------------------------------------

cd %~dp0

echo Installing pip...
.\python.exe get-pip.py

echo Installing required packages for RADE mode...
.\python.exe -m pip install --no-warn-script-location -r rade-requirements.txt
