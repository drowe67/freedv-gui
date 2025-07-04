#!/bin/bash -e

APPNAME="FreeDV"
APPDIR="$APPNAME.AppDir"
BUILDDIR="../"

# Change to the directory where this script is located
cd "$(dirname "$(realpath "$0")")"

if [ -d "$APPDIR" ]; then
    echo "Deleting $APPDIR..."
    rm -rf "$APPDIR"
else
    echo "$APPDIR does not exist."
fi

echo "Bundle dependencies..."
if test -f linuxdeploy-x86_64.AppImage; then
  echo "linuxdeploy exists"
else
    wget -c "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh"
    wget https://github.com/linuxdeploy/linuxdeploy/releases/latest/download/linuxdeploy-x86_64.AppImage
    chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-gtk.sh
fi

./linuxdeploy-x86_64.AppImage \
--executable /usr/bin/python3 \
--executable ../build_linux/src/freedv \
--appdir "$APPDIR" \
--icon-file ../contrib/freedv256x256.png \
--custom-apprun=AppRun.sh \
--desktop-file FreeDV.desktop

# create the virtual environment (copied from Brian's build script)
cd FreeDV.AppDir
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
./linuxdeploy-x86_64.AppImage \
--appdir "$APPDIR" \
--plugin gtk \
--output appimage

# Include version number in AppImage filename
FREEDV_VERSION=`cat ../build_linux/freedv-version.txt`
mv FreeDV-x86_64.AppImage FreeDV-$FREEDV_VERSION-x86_64.AppImage

echo "Done"
