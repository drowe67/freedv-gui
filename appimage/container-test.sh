#!/bin/bash

cd /workspace
Xvfb :99 -screen 0 1024x768x16 &
sleep 5
export DISPLAY=:99.0
export XDG_RUNTIME_DIR=/run/user/$(id -u)
mkdir -p $XDG_RUNTIME_DIR
chmod 700 $XDG_RUNTIME_DIR
eval "$(dbus-launch --sh-syntax --exit-with-x11)"
pipewire &
pipewire-pulse &
wireplumber &
metacity --sm-disable --replace &
sleep 5
./test/test_zeros.sh txrx RADEV1
