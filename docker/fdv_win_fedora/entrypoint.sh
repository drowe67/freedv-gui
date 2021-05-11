#!/bin/bash -x

BUILDROOT=/home/build
mkdir -p $BUILDROOT 
cd $BUILDROOT 

# variables are passed into this Docker process ------------------

# note the codec2 and LPCNet repos are hard coded in build_windows.sh
GIT_REPO=${FDV_GIT_REPO:-https://github.com/drowe67/freedv-gui.git}
GIT_BRANCH=${FDV_GIT_BRANCH:-master}

# override with "mingw32-cmake" for a 32 bit build
CMAKE=${FDV_CMAKE:-mingw64-cmake}

CLEAN=${FDV_CLEAN:-1}
BOOTSTRAP_WX=${FDV_BOOTSTRAP_WX:-0}

echo "FDV_GIT_REPO=$GIT_REPO"
echo "FDV_GIT_BRANCH=$GIT_BRANCH"
echo "FDV_CLEAN=$CLEAN"
echo "FDV_CMAKE=$CMAKE"

if [ $CLEAN -eq 1 ]; then 
    # start with a fresh clone
    rm -Rf freedv-gui; 
    git clone $GIT_REPO
    cd freedv-gui
else
    cd freedv-gui
    git pull
fi
git checkout $GIT_BRANCH

echo "--------------------- starting build_windows.sh ---------------------"
CMAKE=$CMAKE BOOTSTRAP_WX=$BOOTSTRAP_WX ./build_windows.sh

if [ $CMAKE = "mingw64-cmake" ]; then
    cd build_win64
else
    cd build_win32
fi
make package
