#!/usr/bin/env bash
# build_linux.sh
#
# Build script for Ubuntu and Fedora Linux, git pulls codec2 and
# lpcnet repos so they are available for parallel development.

# Echo what you are doing, and fail if any of the steps fail:
set -x -e

UT_ENABLE=${UT_ENABLE:-0}
USE_NATIVE_AUDIO=${USE_NATIVE_AUDIO:-1}
BUILD_TYPE=${BUILD_TYPE:-Debug}
WITH_ASAN=${WITH_ASAN:-0}

export FREEDVGUIDIR=${PWD}
export CODEC2DIR=$FREEDVGUIDIR/codec2

# change this when working on combined codec2/freedv-gui changes
CODEC2_BRANCH=1.2.0

# First build and install vanilla codec2 as we need -lcodec2 to build LPCNet
cd $FREEDVGUIDIR
if [ ! -d codec2 ]; then
    git clone https://github.com/drowe67/codec2.git
fi
cd codec2 && git switch main && git pull && git checkout $CODEC2_BRANCH
mkdir -p build_linux && cd build_linux && rm -Rf * && cmake .. && make VERBOSE=1 -j$(nproc)

# Finally, build freedv-gui
cd $FREEDVGUIDIR
if [ -d .git ]; then
     git pull
fi
mkdir  -p build_linux && cd build_linux && rm -Rf *
cmake -DENABLE_ASAN=${WITH_ASAN} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DUSE_NATIVE_AUDIO=$USE_NATIVE_AUDIO -DUNITTEST=$UT_ENABLE -DCODEC2_BUILD_DIR=$CODEC2DIR/build_linux ..
make VERBOSE=1 -j$(nproc)
