#!/bin/bash -x

BUILDROOT=/home/build
mkdir -p $BUILDROOT 
cd $BUILDROOT 

# variables that can be passed in to Docker process ------------------

# note this is just the freedv-gui repo, codec2 and LPCNet repos are hard coded in build_windows.sh
GIT_REPO=${FDV_GIT_REPO:-https://github.com/drowe67/freedv-gui.git}
GIT_BRANCH=${FDV_GIT_BRANCH:-master}

# override with "mingw32-cmake" for a 32 bit build
CMAKE=${FDV_CMAKE:-mingw64-cmake}

echo "FDV_GIT_REPO=$GIT_REPO"
echo "FDV_GIT_BRANCH=$GIT_BRANCH"
echo "FDV_CLEAN=$CLEAN"
echo "FDV_CMAKE=$CMAKE"

# OK lets get started -----------------------------------------------

if [ ! -d freedv-gui ] ; then git clone $GIT_REPO ; fi
cd freedv-gui
git pull -v
git branch
git checkout $GIT_BRANCH
echo "--------------------- starting build_windows.sh ---------------------"
GIT_REPO=$GIT_REPO GIT_REF=$GIT_REF CMAKE=$CMAKE ./build_windows.sh
if [ $CMAKE = "mingw64-cmake" ]; then
    cd build_win64
else
    cd build_win32
fi
make package
