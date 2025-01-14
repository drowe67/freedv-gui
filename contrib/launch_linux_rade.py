#!/usr/bin/env python3

# Currently (January 2025) the RADE mode of FreeDV on Linux requires a virtual environment.
# This Python script is used on Linux to start the FreeDV GUI program. It removes the need
# to activate a virtual environment, and may be used in a FreeDV.desktop shortcut. Its name is
# "launch_linux_rade.py" and it must be located in freedv-gui/contrib. To use it, run it with any
# Python version.

import sys, subprocess, os, os.path

DEBUG = 0

env = os.environ

# This Python script is <FreeDV-GUI directory>/contrib/launch_linux_rade.py.
# Use the file path of this script to find the FreeDV-GUI directory.
freedv_dir = os.path.abspath(__file__)
freedv_dir, tail = os.path.split(freedv_dir)
freedv_dir, tail = os.path.split(freedv_dir)
if DEBUG: print ("freedv_dir", freedv_dir)
build_linux = os.path.join(freedv_dir, "build_linux")
if DEBUG: print ("build_linux", build_linux)
rade_src = os.path.join(build_linux, "rade_src")
if DEBUG: print ("rade_src", rade_src)
rade_venv_bin = os.path.join(freedv_dir, "rade-venv", "bin")
if DEBUG: print ("rade_venv_bin", rade_venv_bin)

# Site packages are in rade-venv/lib/pythonX.Y/site-packages.
site_pkg = ''
lib = os.path.join(freedv_dir, "rade-venv", "lib")
for pyname in os.listdir(lib):
  pkg = os.path.join(lib, pyname, "site-packages")
  if os.path.isdir(pkg):
    site_pkg = pkg
    break
if not site_pkg:		# Should not happen.
  site_pkg = os.path.join(lib, "site-packages")
if DEBUG: print ("site_pkg", site_pkg)

# Modify PYTHONPATH:
path = rade_src + ":" + site_pkg
if "PYTHONPATH" in env:
  env["PYTHONPATH"] = path + ":" + env["PYTHONPATH"]
else:
  env["PYTHONPATH"] = path

#Modify PATH
if "PATH" in env:
  env["PATH"] = rade_venv_bin + ":" + env["PATH"]
else:
  env["PATH"] = rade_venv_bin

# Remove PYTHONHOME:
if "PYTHONHOME" in env:
  del env["PYTHONHOME"]

if DEBUG: print ("PATH", env["PATH"])
if DEBUG: print ("PYTHONPATH", env["PYTHONPATH"])

os.chdir(build_linux)
subprocess.run(os.path.join(build_linux, "src", "freedv"))

