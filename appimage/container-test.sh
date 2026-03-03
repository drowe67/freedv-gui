#!/bin/bash

cd /workspace
export XDG_RUNTIME_DIR=/run/user/$(id -u)
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_DATA_HOME="$HOME/.local/share"
export XDG_CACHE_HOME="$HOME/.cache"
export GIO_USE_VFS=local
mkdir -p $XDG_RUNTIME_DIR
chmod 700 $XDG_RUNTIME_DIR
Xvfb :99 -screen 0 1024x768x16 &
sleep 5
export DISPLAY=:99.0
eval "$(dbus-launch --sh-syntax --exit-with-x11)"
sudo systemctl restart polkit
sudo systemctl enable rtkit-daemon
sudo systemctl start rtkit-daemon
pipewire &
pipewire-pulse &
wireplumber &
metacity --sm-disable --replace &
sleep 5
./test/test_rade_loss.sh
