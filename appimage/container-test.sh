#!/bin/bash

cd /workspace
export XDG_RUNTIME_DIR=/run/user/$(id -u)
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_DATA_HOME="$HOME/.local/share"
export XDG_CACHE_HOME="$HOME/.cache"
export GIO_USE_VFS=local
Xvfb :99 -screen 0 1024x768x16 &
sleep 5
export DISPLAY=:99.0
#eval "$(dbus-launch --sh-syntax --exit-with-x11)"
ls -la $XDG_RUNTIME_DIR
systemd-run --user --scope --shell
ls -la $XDG_RUNTIME_DIR
env
systemctl --user enable --now pipewire-pulse.socket
systemctl --user enable --now pipewire.service
systemctl --user enable --now wireplumber.service
metacity --sm-disable --replace &
sleep 5
./test/test_rade_loss.sh
