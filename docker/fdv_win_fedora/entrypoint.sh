#!/bin/bash -x

BUILDROOT=/home/build
mkdir -p $BUILDROOT 
cd $BUILDROOT 
GIT_REPO=${FDV_GIT_REPO:-http://github.com/drowe67/freedv-gui.git}
GIT_REF=${FDV_GIT_REF:-master}
echo "FDV_GIT_REPO=$GIT_REPO"
echo "FDV_GIT_REF=$GIT_REF"

if [ ! -d freedv-gui ] ; then git clone --depth=1 https://github.com/drowe67/freedv-gui ; fi
cd freedv-gui && git checkout master && git pull
echo "--------------------- starting build_windows.sh ---------------------"
./build_windows.sh
cd build_win64 && make package
