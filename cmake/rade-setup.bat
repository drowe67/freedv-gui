@echo off

rem Installs required packages for RADE.
rem NOTE: must run as administrator!

cd %~dp0
.\python.exe get-pip.py
.\python.exe -m pip install -r rade-requirements.txt
