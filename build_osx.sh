#!/usr/bin/env bash
# build_ubuntu.sh
#
# Build script for OSX using MacPorts, git pulls codec2 and
# lpcnet repos so they are available for parallel development.

export FREEDVGUIDIR=${PWD}
export HAMLIBDIR=$FREEDVGUIDIR/hamlib
export UT_ENABLE=${UT_ENABLE:-0}
export UNIV_BUILD=${UNIV_BUILD:-1}
export CODESIGN_IDENTITY=${CODESIGN_IDENTITY:--}
export BUILD_DEPS=${BUILD_DEPS:-1}
export USE_NATIVE_AUDIO=${USE_NATIVE_AUDIO:-1}
export BUILD_TYPE=${BUILD_TYPE:-Debug}
export WITH_ASAN=${WITH_ASAN:-0}
export WITH_RTSAN=${WITH_RTSAN:-0}
export WITH_TSAN=${WITH_TSAN:-0}
export WITH_UBSAN=${WITH_UBSAN:-0}
export ENABLE_LTO=${ENABLE_LTO:-0}

# Prerequisite: build dylibbundler
if [ ! -d macdylibbundler ]; then
    git clone https://github.com/tmiw/macdylibbundler.git
    cd macdylibbundler && git checkout main && git pull
    make -j$(sysctl -n hw.logicalcpu)
    cd ..
fi

# Prerequisite: Python
if [ ! -d Python.framework ]; then
    git clone https://github.com/gregneagle/relocatable-python.git
    cd relocatable-python
    ./make_relocatable_python_framework.py --python-version 3.14.2 --os-version=11 --destination $PWD/../
    cd ..

    # Prerequisite: Python packages
    rm -rf pkg-tmp
    ./generate-univ-pkgs.sh
    ./Python.framework/Versions/Current/bin/pip3 install pkg-tmp/*.whl
fi

# Prerequisite: build hamlib
cd $FREEDVGUIDIR
if [ $BUILD_DEPS == 1 ]; then 
    if [ ! -d hamlib-code ]; then
        git clone https://github.com/Hamlib/Hamlib.git hamlib-code
    fi
    cd hamlib-code && git checkout 4.6.5 && git pull
    ./bootstrap 
    if [ $UNIV_BUILD == 1 ]; then
        CFLAGS="-g -O2 -mmacosx-version-min=10.9 -arch x86_64 -arch arm64 ${CFLAGS}" CXXFLAGS="-g -O2 -mmacosx-version-min=10.9 -arch x86_64 -arch arm64 ${CXXFLAGS}" ./configure --enable-shared --prefix $HAMLIBDIR
    else
        CFLAGS="-g -O2 -mmacosx-version-min=10.9 ${CFLAGS}" CXXFLAGS="-g -O2 -mmacosx-version-min=10.9 ${CXXFLAGS}" ./configure --enable-shared --prefix $HAMLIBDIR
    fi

    make -j$(sysctl -n hw.logicalcpu) 
    make install 
fi

# Finally, build freedv-gui
cd $FREEDVGUIDIR
if [ -d .git ]; then
    git pull
fi
mkdir  -p build_osx && cd build_osx && rm -Rf *

if [ "$CODESIGN_KEYCHAIN_PROFILE" != "" ]; then
    export CODESIGN_KEYCHAIN_PROFILE_ARG=-DMACOS_CODESIGN_KEYCHAIN_PROFILE=$CODESIGN_KEYCHAIN_PROFILE
fi

if [ $BUILD_DEPS == 1 ]; then 
    cmake -DENABLE_LTO=${ENABLE_LTO} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DENABLE_UBSAN=${WITH_UBSAN} -DENABLE_TSAN=${WITH_TSAN} -DENABLE_RTSAN=${WITH_RTSAN} -DENABLE_ASAN=${WITH_ASAN} -DUSE_NATIVE_AUDIO=$USE_NATIVE_AUDIO -DPython3_ROOT_DIR=$PWD/../Python.framework/Versions/3.14 -DBUILD_OSX_UNIVERSAL=${UNIV_BUILD} -DUNITTEST=$UT_ENABLE -DBOOTSTRAP_WXWIDGETS=1 -DUSE_STATIC_SPEEXDSP=1 -DUSE_STATIC_PORTAUDIO=1 -DUSE_STATIC_SAMPLERATE=1 -DUSE_STATIC_SNDFILE=1 -DHAMLIB_INCLUDE_DIR=${HAMLIBDIR}/include -DHAMLIB_LIBRARY=${HAMLIBDIR}/lib/libhamlib.dylib -DMACOS_CODESIGN_IDENTITY=${CODESIGN_IDENTITY} ${CODESIGN_KEYCHAIN_PROFILE_ARG} ..
else
    cmake -DENABLE_LTO=${ENABLE_LTO} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DENABLE_UBSAN=${WITH_UBSAN} -DENABLE_TSAN=${WITH_TSAN} -DENABLE_RTSAN=${WITH_RTSAN} -DENABLE_ASAN=${WITH_ASAN} -DUSE_NATIVE_AUDIO=$USE_NATIVE_AUDIO -DPython3_ROOT_DIR=$PWD/../Python.framework/Versions/3.14 -DBUILD_OSX_UNIVERSAL=${UNIV_BUILD} -DUNITTEST=$UT_ENABLE -DMACOS_CODESIGN_IDENTITY=${CODESIGN_IDENTITY} ${CODESIGN_KEYCHAIN_PROFILE_ARG} ..
fi

make -j$(sysctl -n hw.logicalcpu)
