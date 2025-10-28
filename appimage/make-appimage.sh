#!/bin/bash -e

TARGET=${1:-all}

if [[ "${TARGET}" == "all" ]]; then
    export APPNAME="FreeDV"
    export APPEXEC=../build_linux/src/freedv
    export APPRUN="AppRun.sh"
elif [[ "${TARGET}" == "freedv-flex" ]]; then
    export APPNAME="FreeDV-FlexRadio"
    export APPEXEC=../build_linux/src/integrations/flex/freedv-flex
    export APPRUN="AppRun-FlexRadio.sh"
elif [[ "${TARGET}" == "freedv-ka9q" ]]; then
    export APPNAME="FreeDV-KA9Q"
    export APPEXEC=../build_linux/src/integrations/ka9q/freedv-ka9q
    export APPRUN="AppRun-KA9Q.sh"
fi

APPDIR="$APPNAME.AppDir"
BUILDDIR="../"
MACH_ARCH=`uname -m`

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
--executable /usr/bin/python3 \
--executable "$APPEXEC" \
--appdir "$APPDIR" \
--icon-file ../contrib/freedv256x256.png \
--custom-apprun=AppRun.sh \
--desktop-file FreeDV.desktop

# create the virtual environment (copied from Brian's build script)
cd $APPDIR
python3.11 -m venv rade-venv # || { echo "ERROR: create venv failed"; exit 1; }
# Activate it
source rade-venv/bin/activate # || { echo "ERROR: activate venv failed"; exit 1; }

# Clear cache in venv
pip3 cache purge
pip3 install --upgrade pip || echo "WARNING: pip upgrade failed"
pip3 install numpy
pip3 install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
pip3 install matplotlib
cd -

echo "Fix venv python links..."
echo "Now in $(pwd)"
cd "$APPDIR/rade-venv/bin"
echo "Now in $(pwd)"
ln -s -f ../../usr/bin/python3 python
ln -s -f ../../usr/bin/python3 python3
ln -s -f ../../usr/bin/python3 python3.11
cd - # back to the previous directory
echo "### Now in $(pwd)"

# Copy /usr/lib/python3.11 to image
cd $APPDIR/usr
cp -a /usr/lib/python3.11 lib/
cd -

# Copy the models and symlink
echo "Copying rade_src..."
# ls freedv-rade/freedv-gui/build_linux/rade_src/model
# model05/        model17/        model18/        model19/        model19_check3/ model_bbfm_01/  
cp -r "$BUILDDIR/build_linux/rade_src" "$APPDIR/."
cd "$APPDIR/usr/bin"
ln -s "../../rade_src/model19_check3" "model19_check3"
cd -

# Create the output
export LINUXDEPLOY_OUTPUT_APP_NAME=$APPNAME
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
