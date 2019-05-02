#!/bin/bash
# build_ubuntu.sh
#
# Build script for Ubuntu, git pulls codec2 and lpcnet repos so they are
# available for parallel development.

export FREEDVGUIDIR=${PWD}
export CODEC2DIR=$FREEDVGUIDIR/codec2
export LPCNETDIR=$FREEDVGUIDIR/LPCNet

# First build and install vanilla codec2 as we need -lcodec2 to build LPCNet
cd $FREEDVGUIDIR
git clone https://github.com/drowe67/codec2.git
cd codec2 && git checkout brad-2020
mkdir -p build_linux && cd build_linux && cmake .. && make

# OK, build and test LPCNet
cd $FREEDVGUIDIR
git clone https://github.com/drowe67/LPCNet.git
cd $LPCNETDIR
mkdir  -p build_linux && cd build_linux
cmake -DCODEC2_BUILD_DIR=$CODEC2DIR/build_linux ..
make
# sanity check test
cd src && sox ../../wav/wia.wav -t raw -r 16000 - | ./lpcnet_enc -s | ./lpcnet_dec -s > /dev/null

# Re-build codec2 with LPCNet and test FreeDV 2020 support
cd $CODEC2DIR/build_linux && rm -Rf *
cmake -DLPCNET_BUILD_DIR=$LPCNETDIR/build_linux ..
make VERBOSE=1
# sanity check test
cd src
export LD_LIBRARY_PATH=$LPCNETDIR/build_linux/src
./freedv_tx 2020 $LPCNETDIR/wav/wia.wav - | ./freedv_rx 2020 - /dev/null

# Finally, build freedv-gui
cd $FREEDVGUIDIR
mkdir  -p build_linux && cd build_linux
cmake -DCMAKE_BUILD_TYPE=Debug -DCODEC2_BUILD_DIR=$CODEC2DIR/build_linux -DLPCNET_BUILD_DIR=$LPCNETDIR/build_linux ..
make
    
