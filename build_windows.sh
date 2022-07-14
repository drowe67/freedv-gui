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
else
    BUILD_DIR=build_win32
fi
export FREEDVGUIDIR=${PWD}
export CODEC2DIR=$FREEDVGUIDIR/codec2
export LPCNETDIR=$FREEDVGUIDIR/LPCNet

CODEC2_BRANCH=v1.0.4_rc2
LPCNET_BRANCH=v0.3

# First build and install vanilla codec2 as we need -lcodec2 to build LPCNet
cd $FREEDVGUIDIR
git clone https://github.com/drowe67/codec2.git
cd codec2 && git switch master && git pull && git checkout $CODEC2_BRANCH
mkdir -p $BUILD_DIR && cd $BUILD_DIR && rm -Rf *
$CMAKE .. && make

# OK, build and test LPCNet
cd $FREEDVGUIDIR
git clone https://github.com/drowe67/LPCNet.git
cd $LPCNETDIR && git switch master && git pull && git checkout $LPCNET_BRANCH
mkdir -p $BUILD_DIR && cd $BUILD_DIR && rm -Rf *
$CMAKE -DCODEC2_BUILD_DIR=$CODEC2DIR/$BUILD_DIR ..
make

# Re-build codec2 with LPCNet and test FreeDV 2020 support
cd $CODEC2DIR/$BUILD_DIR && rm -Rf *
$CMAKE -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR ..
make VERBOSE=1

cd $FREEDVGUIDIR && git pull
mkdir -p $BUILD_DIR && cd $BUILD_DIR 
if [ $CLEAN -eq 1 ]; then rm -Rf *; fi

if [ $BOOTSTRAP_WX -eq 1 ]; then
    # build wxWidgets
    $CMAKE -DBOOTSTRAP_WXWIDGETS=1 -DCMAKE_BUILD_TYPE=Debug -DCODEC2_BUILD_DIR=$CODEC2DIR/$BUILD_DIR -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR ..
    make VERBOSE=1

    # build freedv-gui
    $CMAKE -DBOOTSTRAP_WXWIDGETS=1 -DUNITTEST=$UT_ENABLE -DCMAKE_BUILD_TYPE=Debug -DCODEC2_BUILD_DIR=$CODEC2DIR/$BUILD_DIR -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR ..
    make VERBOSE=1
else
    # build freedv-gui
    $CMAKE -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=$UT_ENABLE -DCODEC2_BUILD_DIR=$CODEC2DIR/$BUILD_DIR -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR ..
    make VERBOSE=1
fi
