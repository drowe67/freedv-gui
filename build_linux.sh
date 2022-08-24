#!/usr/bin/env bash
# build_linux.sh
#
# Build script for Ubuntu and Fedora Linux, git pulls codec2 and
# lpcnet repos so they are available for parallel development.

# Echo what you are doing, and fail if any of the steps fail:
set -x -e

UT_ENABLE=${UT_ENABLE:-0}

# Allow building of either PulseAudio or PortAudio variants
FREEDV_VARIANT=${1:-portaudio}
if [[ "$FREEDV_VARIANT" != "portaudio" && "$FREEDV_VARIANT" != "pulseaudio" ]]; then
    echo "Usage: build_linux.sh [portaudio|pulseaudio]"
    exit -1
fi

export FREEDVGUIDIR=${PWD}
export CODEC2DIR=$FREEDVGUIDIR/codec2
export LPCNETDIR=$FREEDVGUIDIR/LPCNet

# change this when working on combined codec2/freedv-gui changes
CODEC2_BRANCH=master
LPCNET_BRANCH=master

# OK, build and test LPCNet
cd $FREEDVGUIDIR
if [ ! -d LPCNet ]; then
    git clone https://github.com/drowe67/LPCNet.git
fi
cd $LPCNETDIR && git switch master && git pull && git checkout $LPCNET_BRANCH
mkdir  -p build_linux && cd build_linux && rm -Rf *
cmake ..
make
# sanity check test
cd src && sox ../../wav/wia.wav -t raw -r 16000 - | ./lpcnet_enc -s | ./lpcnet_dec -s > /dev/null

# First build and install vanilla codec2 as we need -lcodec2 to build LPCNet
cd $FREEDVGUIDIR
if [ ! -d codec2 ]; then
    git clone https://github.com/drowe67/codec2.git
fi
cd codec2 && git switch master && git pull && git checkout $CODEC2_BRANCH
mkdir -p build_linux && cd build_linux && rm -Rf * && cmake -DLPCNET_BUILD_DIR=$LPCNETDIR/build_linux .. && make VERBOSE=1
# sanity check test
cd src
export LD_LIBRARY_PATH=$LPCNETDIR/build_linux/src
./freedv_tx 2020 $LPCNETDIR/wav/wia.wav - | ./freedv_rx 2020 - /dev/null

# Finally, build freedv-gui
cd $FREEDVGUIDIR && git pull
mkdir  -p build_linux && cd build_linux && rm -Rf *
if [[ "$FREEDV_VARIANT" == "pulseaudio" ]]; then
    PULSEAUDIO_PARAM="-DUSE_PULSEAUDIO=1"
fi
cmake $PULSEAUDIO_PARAM -DUNITTEST=$UT_ENABLE -DCMAKE_BUILD_TYPE=Debug -DCODEC2_BUILD_DIR=$CODEC2DIR/build_linux -DLPCNET_BUILD_DIR=$LPCNETDIR/build_linux ..
make VERBOSE=1
