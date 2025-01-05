#!/usr/bin/env bash
# build_ubuntu.sh
#
# Build script for OSX using MacPorts, git pulls codec2 and
# lpcnet repos so they are available for parallel development.

export FREEDVGUIDIR=${PWD}
export CODEC2DIR=$FREEDVGUIDIR/codec2
export LPCNETDIR=$FREEDVGUIDIR/LPCNet
export HAMLIBDIR=$FREEDVGUIDIR/hamlib
export CODEC2_BRANCH=1.2.0
export LPCNET_BRANCH=v0.5
export UT_ENABLE=${UT_ENABLE:-0}
export LPCNET_DISABLE=${LPCNET_DISABLE:-1}
export UNIV_BUILD=${UNIV_BUILD:-1}
export CODESIGN_IDENTITY=${CODESIGN_IDENTITY:--}

# Prerequisite: build dylibbundler
if [ ! -d macdylibbundler ]; then
    git clone https://github.com/tmiw/macdylibbundler.git
fi
cd macdylibbundler && git checkout main && git pull
make -j4
cd ..

# Prerequisite: Python
if [ ! -d Python.framework ]; then
    git clone https://github.com/gregneagle/relocatable-python.git
    cd relocatable-python
    ./make_relocatable_python_framework.py --python-version 3.12.7 --os-version=11 --destination $PWD/../
    cd ..
fi

# Prerequisite: Python packages
rm -rf pkg-tmp
./generate-univ-pkgs.sh
./Python.framework/Versions/Current/bin/pip3 install pkg-tmp/*.whl

# Prerequisite: build hamlib
cd $FREEDVGUIDIR
if [ ! -d hamlib-code ]; then
    git clone https://github.com/Hamlib/Hamlib.git hamlib-code
fi
cd hamlib-code && git checkout 4.6 && git pull
./bootstrap
if [ $UNIV_BUILD == 1 ]; then
    CFLAGS="-g -O2 -mmacosx-version-min=10.9 -arch x86_64 -arch arm64" CXXFLAGS="-g -O2 -mmacosx-version-min=10.9 -arch x86_64 -arch arm64" ./configure --enable-shared --prefix $HAMLIBDIR
else
    CFLAGS="-g -O2 -mmacosx-version-min=10.9" CXXFLAGS="-g -O2 -mmacosx-version-min=10.9" ./configure --enable-shared --prefix $HAMLIBDIR
fi

make -j4
make install

# OK, build and test LPCNet
cd $FREEDVGUIDIR
if [ $LPCNET_DISABLE == 0 ]; then
    if [ ! -d LPCNet ]; then
        git clone https://github.com/drowe67/LPCNet.git
    fi
    cd $LPCNETDIR && git checkout master && git pull && git checkout $LPCNET_BRANCH
    mkdir  -p build_osx && cd build_osx && rm -Rf *
    cmake -DBUILD_OSX_UNIVERSAL=${UNIV_BUILD} ..
    if [ $? == 0 ]; then
        make -j4
        if [ $? == 0 ]; then
            # sanity check test
            cd src && sox ../../wav/wia.wav -t raw -r 16000 - | ./lpcnet_enc -s | ./lpcnet_dec -s > /dev/null
        else
            echo "Warning: LPCNet build failed, disabling"
            LPCNET_DISABLE=1
        fi
    else
        echo "Warning: LPCNet build failed, disabling"
        LPCNET_DISABLE=1
    fi
fi

if [ $LPCNET_DISABLE == 0 ]; then
    LPCNET_CMAKE_CMD="-DLPCNET_BUILD_DIR=$LPCNETDIR/build_osx"
fi

# Build codec2 with LPCNet and test FreeDV 2020 support
cd $FREEDVGUIDIR
if [ ! -d codec2 ]; then
    git clone https://github.com/drowe67/codec2-new.git codec2
fi
cd codec2 && git switch main && git pull && git checkout $CODEC2_BRANCH
mkdir -p build_osx && cd build_osx && rm -Rf * && cmake ${LPCNET_CMAKE_CMD} -DBUILD_OSX_UNIVERSAL=${UNIV_BUILD} .. && make VERBOSE=1 -j4

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

if [ "$CODESIGN_KEYCHAIN_PROFILE" != "" ]; then
    export CODESIGN_KEYCHAIN_PROFILE_ARG=-DMACOS_CODESIGN_KEYCHAIN_PROFILE=$CODESIGN_KEYCHAIN_PROFILE
fi
 
cmake -DPython3_ROOT_DIR=$PWD/../Python.framework/Versions/3.12 -DUNITTEST=1 -DBUILD_OSX_UNIVERSAL=${UNIV_BUILD} -DUNITTEST=$UT_ENABLE -DCMAKE_BUILD_TYPE=Debug  -DBOOTSTRAP_WXWIDGETS=1 -DUSE_STATIC_SPEEXDSP=1 -DUSE_STATIC_PORTAUDIO=1 -DUSE_STATIC_SAMPLERATE=1 -DUSE_STATIC_SNDFILE=1 -DHAMLIB_INCLUDE_DIR=${HAMLIBDIR}/include -DHAMLIB_LIBRARY=${HAMLIBDIR}/lib/libhamlib.dylib -DCODEC2_BUILD_DIR=$CODEC2DIR/build_osx ${LPCNET_CMAKE_CMD} -DMACOS_CODESIGN_IDENTITY=${CODESIGN_IDENTITY} ${CODESIGN_KEYCHAIN_PROFILE_ARG} ..
make VERBOSE=1 -j8
