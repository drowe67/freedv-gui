#!/usr/bin/env bash
# build_windows_old.sh
#
# Script that cross compiles freedv-gui for Windows on Fedora
# Linux. Git pulls older versions of freedv-gui for test/debug purposes,
# then constructs a zip file for testing.  Zips files are convenient
# when testing several versions, as everything is self contained.
#
# usage: ./build_old.sh githash

export FREEDVGUIDIR=${PWD}
export CODEC2DIR=$FREEDVGUIDIR/codec2
export LPCNETDIR=$FREEDVGUIDIR/LPCNet
#git checkout $1
#mkdir -p build_win && cd build_win && rm -Rf *
#make VERBOSE=1
git checkout dr-debug-vac-3
#git checkout $1
cd $FREEDVGUIDIR/build_win
rm -Rf *
# old cmake line
mingw64-cmake -DCMAKE_BUILD_TYPE=Debug -DCODEC2_BUILD_DIR=$CODEC2DIR/build_win -DLPCNET_BUILD_DIR=$LPCNETDIR/build_win ..
make
make package
cd $FREEDVGUIDIR
export zipdir=freedv-gui-$1
mkdir -p $zipdir
cp -f `find build_win/_CPack_Packages -name *.dll` $zipdir
cp -f `find codec2/build_win/ -name *.dll` $zipdir
cp -f `find LPCNet/build_win/ -name *.dll` $zipdir
cp build_win/src/freedv.exe $zipdir
zip -r $zipdir'.zip' $zipdir
