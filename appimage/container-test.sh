#!/bin/bash

cd /workspace
Xvfb :99 -screen 0 1024x768x16 &
sleep 5
export DISPLAY=:99.0
export XDG_RUNTIME_DIR=/run/user/$(id -u)
mkdir -p $XDG_RUNTIME_DIR
chmod 700 $XDG_RUNTIME_DIR
eval "$(dbus-launch --sh-syntax --exit-with-x11)"
sudo systemctl restart polkit
sudo systemctl enable rtkit-daemon
sudo systemctl start rtkit-daemon
systemctl --user enable pipewire
systemctl --user start pipewire
systemctl --user status pipewire
systemctl --user enable pipewire-pulse
systemctl --user start pipewire-pulse
systemctl --user status pipewire-pulse
systemctl --user enable wireplumber
systemctl --user start wireplumber
systemctl --user status wireplumber
metacity --sm-disable --replace &
sleep 5
./test/test_rade_loss.sh
