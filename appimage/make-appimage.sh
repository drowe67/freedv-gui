#!/bin/bash -e

TARGET=${1:-all}

if [[ "${TARGET}" == "all" ]]; then
    export APPNAME="FreeDV"
    export APPEXEC=../build_linux/src/freedv
    export APPRUN="AppRun.sh"
    export RADE_SRC="rade_src"
elif [[ "${TARGET}" == "freedv-flex" ]]; then
    export APPNAME="FreeDV-FlexRadio"
    export APPEXEC=../build_linux/src/integrations/flex/freedv-flex
    export APPRUN="AppRun-FlexRadio.sh"
    export RADE_SRC="rade_integ_src"
elif [[ "${TARGET}" == "freedv-ka9q" ]]; then
    export APPNAME="FreeDV-KA9Q"
    export APPEXEC=../build_linux/src/integrations/ka9q/freedv-ka9q
    export APPRUN="AppRun-KA9Q.sh"
    export RADE_SRC="rade_integ_src"
fi

DESKTOP_FILE="$APPNAME.desktop"
APPDIR="$APPNAME.AppDir"
BUILDDIR="../"
MACH_ARCH=`uname -m`

export NO_STRIP=1

# Change to the directory where this script is located
cd "$(dirname "$(realpath "$0")")"

if [ -d "$APPDIR" ]; then
    echo "Deleting $APPDIR..."
    rm -rf "$APPDIR"
else
    echo "$APPDIR does not exist."
fi

echo "Bundle dependencies..."
if test -f linuxdeploy-${MACH_ARCH}.AppImage; then
  echo "linuxdeploy exists"
else
    wget -c "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh"
    wget https://github.com/linuxdeploy/linuxdeploy/releases/latest/download/linuxdeploy-${MACH_ARCH}.AppImage
    chmod +x linuxdeploy-${MACH_ARCH}.AppImage linuxdeploy-plugin-gtk.sh
fi

./linuxdeploy-${MACH_ARCH}.AppImage \
--executable "$APPEXEC" \
--appdir "$APPDIR" \
--icon-file ../contrib/freedv256x256.png \
--custom-apprun=$APPRUN \
--desktop-file $DESKTOP_FILE

# Create the output
if [[ "${TARGET}" == "all" ]]; then
    ./linuxdeploy-${MACH_ARCH}.AppImage \
        --appdir "$APPDIR" \
        --plugin gtk \
        --output appimage
else
    # GTK plugin not needed for integrations
    ./linuxdeploy-${MACH_ARCH}.AppImage \
        --appdir "$APPDIR" \
        --output appimage
fi

# Include version number in AppImage filename
FREEDV_VERSION=`cat ../build_linux/freedv-version.txt`
mv ${APPNAME}-${MACH_ARCH}.AppImage ${APPNAME}-$FREEDV_VERSION-${MACH_ARCH}.AppImage

echo "Done"
