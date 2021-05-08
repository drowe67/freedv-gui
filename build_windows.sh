#!/bin/bash
# build_windows.sh
#
# Script that cross compiles freedv-gui for Windows on Fedora
# Linux. Git pulls codec2 and LPCNet repos so they are available for
# parallel development.

# override this at the command line for a 32 bit build
#   $ CMAKE=mingw32-cmake ./build_windows.sh
: ${CMAKE=mingw64-cmake}

if [ $CMAKE = "mingw64-cmake" ]; then
    BUILD_DIR=build_win64
else
    BUILD_DIR=build_win32
fi
export FREEDVGUIDIR=${PWD}
export CODEC2DIR=$FREEDVGUIDIR/codec2
export LPCNETDIR=$FREEDVGUIDIR/LPCNet

# First build and install vanilla codec2 as we need -lcodec2 to build LPCNet
cd $FREEDVGUIDIR
git clone https://github.com/drowe67/codec2.git
cd codec2 && git checkout master && git pull
mkdir -p $BUILD_DIR && cd $BUILD_DIR && rm -Rf *
$CMAKE .. && make

# OK, build and test LPCNet
cd $FREEDVGUIDIR
git clone https://github.com/drowe67/LPCNet.git
cd $LPCNETDIR && git checkout master && git pull
mkdir -p $BUILD_DIR && cd $BUILD_DIR && rm -Rf *
$CMAKE -DCODEC2_BUILD_DIR=$CODEC2DIR/$BUILD_DIR ..
make
# sanity check test
#cd src &&  ../../wav/wia.wav -t raw -r 16000 - | ./lpcnet_enc -s | ./lpcnet_dec -s > /dev/null

# Re-build codec2 with LPCNet and test FreeDV 2020 support
cd $CODEC2DIR/$BUILD_DIR && rm -Rf *
$CMAKE -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR ..
make VERBOSE=1
# sanity check test
#cd src
#export LD_LIBRARY_PATH=$LPCNETDIR/$BUILD_DIR/src
#./freedv_tx 2020 $LPCNETDIR/wav/wia.wav - | ./freedv_rx 2020 - /dev/null

# Finally, build freedv-gui
cd $FREEDVGUIDIR && git pull
mkdir -p $BUILD_DIR && cd $BUILD_DIR && rm -Rf *
$CMAKE -DBOOTSTRAP_WXWIDGETS=1 -DCMAKE_BUILD_TYPE=Debug -DCODEC2_BUILD_DIR=$CODEC2DIR/$BUILD_DIR -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR ..
make VERBOSE=1

# And again once wxWidgets is built
$CMAKE -DBOOTSTRAP_WXWIDGETS=1 -DCMAKE_BUILD_TYPE=Debug -DCODEC2_BUILD_DIR=$CODEC2DIR/$BUILD_DIR -DLPCNET_BUILD_DIR=$LPCNETDIR/$BUILD_DIR ..
make VERBOSE=1
