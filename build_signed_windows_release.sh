#!/bin/bash

# Builds signed Windows installers for all supported platforms
# (Intel and ARM 32/64 bit).

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if [ $# -ne 3 ]; then
    cat <<EOF
Usage: ${0##*/} [certificate URL file] [key URL file] [intermediate certs]

Note that the certificate and key URL files required above should contain only the PKCS11
URLs to the requested keys and certificates. Please see CODE_SIGNING.md for more information.
EOF
    exit -1
fi

CERT_URL_FILE=$1
KEY_URL_FILE=$2
INTERMEDIATE_CERT_FILE=$3
CUR_DIR=$PWD

WIN_BUILD_DIR=$SCRIPT_DIR/build_windows
mkdir $WIN_BUILD_DIR
cd $WIN_BUILD_DIR

# Install Wine environment
export WINEPREFIX=`pwd`/wine-env
WINEARCH=win64 DISPLAY= winecfg /v win10
wget https://www.python.org/ftp/python/3.12.7/python-3.12.7-amd64.exe
Xvfb :99 -screen 0 1024x768x16 &
DISPLAY=:99.0 wine ./python-3.12.7-amd64.exe /quiet /log c:\\python.log InstallAllUsers=1 Include_doc=0 Include_tcltk=0
DISPLAY=:99.0 wine c:\\Program\ Files\\Python312\\Scripts\\pip.exe install numpy
killall Xvfb

for arch in x86_64; do
    BUILD_ARCH_DIR="$WIN_BUILD_DIR/build_win_$arch"

    # Clear existing build
    rm -rf $BUILD_ARCH_DIR
    mkdir $BUILD_ARCH_DIR
    cd $BUILD_ARCH_DIR

    # Kick off new build with the given architecture
    cmake -DLPCNET_DISABLE=1 -DPython3_ROOT_DIR=`pwd`/../wine-env/drive_c/Program\ Files/Python312 -DSIGN_WINDOWS_BINARIES=1 -DPKCS11_CERTIFICATE_FILE=$CERT_URL_FILE -DPKCS11_KEY_FILE=$KEY_URL_FILE -DINTERMEDIATE_CERT_FILE=$INTERMEDIATE_CERT_FILE -DCMAKE_TOOLCHAIN_FILE=$SCRIPT_DIR/cross-compile/freedv-mingw-llvm-$arch.cmake $SCRIPT_DIR
    make -j package
    cp FreeDV-*.exe $WIN_BUILD_DIR
    cd $WIN_BUILD_DIR
done

cd $CUR_DIR
echo "All Windows builds complete."
