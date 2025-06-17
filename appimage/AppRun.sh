#!/bin/bash -e
echo "In AppImage AppRun"
export LD_LIBRARY_PATH="${APPIMAGE_LIBRARY_PATH}:${APPDIR}/usr/lib:${LD_LIBRARY_PATH}"
export PATH="$APPDIR/usr/bin:$APPDIR/rade-venv/bin"
echo "PATH=$PATH"
export PYTHONHOME="$APPDIR/usr"
export PYTHONPATH="$APPDIR/rade_src:$APPDIR/rade-venv/lib/python3.12/site-packages"
echo "PYTHONPATH=$PYTHONPATH"
echo "PYTHONHOME=$PYTHONHOME"
cd "$APPDIR"
echo "#### after import"
"$APPDIR/usr/bin/freedv" $@
