#!/bin/bash -e
echo "In AppImage AppRun"
export LD_LIBRARY_PATH="${APPIMAGE_LIBRARY_PATH}:${APPDIR}/usr/lib:${LD_LIBRARY_PATH}"
export PATH="$APPDIR/usr/bin:$APPDIR/rade-venv/bin"
echo "PATH=$PATH"
export PYTHONHOME="$APPDIR/usr"
export PYTHONPATH="$APPDIR/rade_src:$APPDIR/rade-venv/lib/python3.12/site-packages"
echo "PYTHONPATH=$PYTHONPATH"
echo "PYTHONHOME=$PYTHONHOME"
#"$APPDIR/rade-venv/bin/python3" -c "print('Hello, world from python!')"
cd "$APPDIR"
. ./rade-venv/bin/activate
export PYTHONPATH="$APPDIR/rade_src:$APPDIR/rade-venv/lib/python3.12/site-packages:$PYTHONPATH"
echo "PYTHONPATH=$PYTHONPATH"
echo "##### About to import ctypes"
"$APPDIR/usr/bin/python3" -v -c "import ctypes"
echo "#### after import ctypes"
echo "##### About to import radae_rxe"
"$APPDIR/usr/bin/python3" -c "import radae_rxe"
echo "#### after import"
"$APPDIR/usr/bin/freedv" $@
deactivate
