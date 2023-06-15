#!/usr/bin/env bash
# build_windows.sh
#
# Script that cross compiles freedv-gui for Windows on Fedora
# Linux. Git pulls codec2 and LPCNet repos so they are available for
# parallel development.

# override this at the command line for a 32 bit build
#   $ CMAKE=mingw32-cmake ./build_windows.sh
: ${CMAKE=mingw64-cmake}

UT_ENABLE=${UT_ENABLE:-0}

if [ $CMAKE = "mingw64-cmake" ]; then
    BUILD_DIR=build_win64
    MINGW_TRIPLE=x86_64-w64-mingw32
else
    BUILD_DIR=build_win32
    MINGW_TRIPLE=i686-w64-mingw32
fi
export FREEDVGUIDIR=${PWD}
export CODEC2DIR=$FREEDVGUIDIR/codec2
export LPCNETDIR=$FREEDVGUIDIR/LPCNet

CODEC2_BRANCH=v1.1.1
LPCNET_BRANCH=v0.5

# OK, build and test LPCNet
cd $FREEDVGUIDIR
git clone https://github.com/drowe67/LPCNet.git
cd $LPCNETDIR && git switch master && git pull && git checkout $LPCNET_BRANCH
mkdir -p $BUILD_DIR && cd $BUILD_DIR && rm -Rf *
$CMAKE ..
make

# Build codec2 with LPCNet and test FreeDV 2020 support
# First build and install vanilla codec2 as we need -lcodec2 to build LPCNet
cd $FREEDVGUIDIR
if [ ! -d codec2 ]; then
    git clone https://github.com/drowe67/codec2.git
fi
cd codec2 && git switch master && git pull && git checkout $CODEC2_BRANCH
mkdir -p $BUILD_DIR && cd $BUILD_DIR && rm -Rf * && $CMAKE -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR .. && make VERBOSE=1

cd $FREEDVGUIDIR
if [ -d .git ]; then
    git pull
fi
mkdir -p $BUILD_DIR && cd $BUILD_DIR 
if [ $CLEAN -eq 1 ]; then rm -Rf *; fi

if [ $BOOTSTRAP_WX -eq 1 ]; then
    # build freedv-gui
    $CMAKE -DBOOTSTRAP_WXWIDGETS=1 -DCMAKE_BUILD_TYPE=Debug -DCODEC2_BUILD_DIR=$CODEC2DIR/$BUILD_DIR -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR ..
    make VERBOSE=1 -j4
else
    # build freedv-gui
    $CMAKE -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=$UT_ENABLE -DCODEC2_BUILD_DIR=$CODEC2DIR/$BUILD_DIR -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR ..
    make VERBOSE=1 -j4
fi
