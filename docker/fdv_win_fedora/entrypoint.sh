#!/bin/bash -x

BUILDROOT=/home/build
mkdir -p $BUILDROOT 
cd $BUILDROOT 

# variables that can be passed in to Docker process ------------------

# note this is just teh freedv-gui repo, codec2 and LPCNet repos are hard coded in build_windows.sh
GIT_REPO=${FDV_GIT_REPO:-http://github.com/drowe67/freedv-gui.git}
GIT_REF=${FDV_GIT_REF:-master}

# if FDV_CLEAN -eq "1" rm's all previous cmake config and binaries (i.e. a complete rebuild from scratch)
# if FDV_CLEAN -eq "0" just runs make which gives a faster build if only minor changes
CLEAN=$(FDV_CLEAN:1}

# override with "mingw32-cmake" for a 32 bit build
CMAKE=$(FDV_CMAKE:mingw64-cmake}

echo "FDV_GIT_REPO=$GIT_REPO"
echo "FDV_GIT_REF=$GIT_REF"
echo "FDV_CLEAN=$CLEAN"
echo "FDV_CMAKE=$CMAKE"

# OK lets get started -----------------------------------------------

if [ ! -d freedv-gui ] ; then git clone --depth=1 $GIT_REPO ; fi
cd freedv-gui && git checkout $GIT_REF && git pull
echo "--------------------- starting build_windows.sh ---------------------"
GIT_REPO=$GIT_REPO GIT_REF=$GIT_REF CLEAN=$CLEAN CMAKE=$CMAKE ./build_windows.sh
cd build_win64 && make package
