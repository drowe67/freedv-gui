#!/bin/sh

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
APP_NAME=$1

if [ ! -d dmg-venv ]; then
    python3 -m venv dmg-venv
fi

. ./dmg-venv/bin/activate
pip3 install dmgbuild

dmgbuild -s ${SCRIPT_DIR}/dmg_settings.py -D app=$APP_NAME "FreeDV" FreeDV.dmg
