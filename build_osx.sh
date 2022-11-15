#!/usr/bin/env bash
# build_ubuntu.sh
#
# Build script for OSX using MacPorts, git pulls codec2 and
# lpcnet repos so they are available for parallel development.

export FREEDVGUIDIR=${PWD}
export CODEC2DIR=$FREEDVGUIDIR/codec2
export LPCNETDIR=$FREEDVGUIDIR/LPCNet
export HAMLIBDIR=$FREEDVGUIDIR/hamlib
export CODEC2_BRANCH=ms-macos-older-releases
export LPCNET_BRANCH=master
export UT_ENABLE=${UT_ENABLE:-0}

# Prerequisite: build dylibbundler
if [ ! -d macdylibbundler ]; then
    git clone https://github.com/tmiw/macdylibbundler.git
fi
cd macdylibbundler && git checkout main && git pull
make -j4

# Prerequisite: build hamlib
cd $FREEDVGUIDIR
if [ ! -d hamlib-code ]; then
    git clone https://github.com/Hamlib/Hamlib.git hamlib-code
fi
cd hamlib-code && git checkout master && git pull
./bootstrap
CFLAGS="-g -O2 -mmacosx-version-min=10.9 -arch x86_64 -arch arm64" CXXFLAGS="-g -O2 -mmacosx-version-min=10.9 -arch x86_64 -arch arm64" ./configure --disable-shared --prefix $HAMLIBDIR
make -j4
make install

# OK, build and test LPCNet
cd $FREEDVGUIDIR
if [ ! -d LPCNet ]; then
    git clone https://github.com/drowe67/LPCNet.git
fi
cd $LPCNETDIR && git switch master && git pull && git checkout $LPCNET_BRANCH
mkdir  -p build_osx && cd build_osx && rm -Rf *
cmake -DBUILD_OSX_UNIVERSAL=1 ..
make -j4

# sanity check test
cd src && sox ../../wav/wia.wav -t raw -r 16000 - | ./lpcnet_enc -s | ./lpcnet_dec -s > /dev/null

# Re-build codec2 with LPCNet and test FreeDV 2020 support
cd $FREEDVGUIDIR
if [ ! -d codec2 ]; then
    git clone https://github.com/drowe67/codec2.git
fi
cd codec2 && git switch master && git pull && git checkout $CODEC2_BRANCH
mkdir -p build_osx && cd build_osx && rm -Rf * && cmake -DLPCNET_BUILD_DIR=$LPCNETDIR/build_osx -DBUILD_OSX_UNIVERSAL=1 .. && make VERBOSE=1 -j4

# sanity check test
cd src
export LD_LIBRARY_PATH=$LPCNETDIR/build_osx/src
./freedv_tx 2020 $LPCNETDIR/wav/wia.wav - | ./freedv_rx 2020 - /dev/null

# Finally, build freedv-gui
cd $FREEDVGUIDIR
if [ -d .git ]; then
    git pull
fi
mkdir  -p build_osx && cd build_osx && rm -Rf *
cmake -DBUILD_OSX_UNIVERSAL=1 -DCMAKE_BUILD_TYPE=Debug  -DBOOTSTRAP_WXWIDGETS=1 -DUSE_STATIC_SPEEXDSP=1 -DUSE_STATIC_PORTAUDIO=1 -DHAMLIB_INCLUDE_DIR=${HAMLIBDIR}/include -DHAMLIB_LIBRARY=${HAMLIBDIR}/lib/libhamlib.a -DCODEC2_BUILD_DIR=$CODEC2DIR/build_osx -DLPCNET_BUILD_DIR=$LPCNETDIR/build_osx ..
make VERBOSE=1

# Rebuild now that wxWidgets is bootstrapped
cmake -DBUILD_OSX_UNIVERSAL=1 -DCMAKE_BUILD_TYPE=Debug  -DBOOTSTRAP_WXWIDGETS=1 -DUSE_STATIC_SPEEXDSP=1 -DUSE_STATIC_PORTAUDIO=1 -DUSE_STATIC_SAMPLERATE=1 -DUSE_STATIC_SNDFILE=1 -DUNITTEST=$UT_ENABLE -DHAMLIB_INCLUDE_DIR=${HAMLIBDIR}/include -DHAMLIB_LIBRARY=${HAMLIBDIR}/lib/libhamlib.a -DCODEC2_BUILD_DIR=$CODEC2DIR/build_osx -DLPCNET_BUILD_DIR=$LPCNETDIR/build_osx ..
make VERBOSE=1 

