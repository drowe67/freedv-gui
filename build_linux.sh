#!/usr/bin/env bash
# build_linux.sh
#
# Build script for Ubuntu and Fedora Linux, git pulls codec2 and
# lpcnet repos so they are available for parallel development.

# Echo what you are doing, and fail if any of the steps fail:
set -x -e

UT_ENABLE=${UT_ENABLE:-0}
USE_NATIVE_AUDIO=${USE_NATIVE_AUDIO:-1}
BUILD_TYPE=${BUILD_TYPE:-Debug}
WITH_ASAN=${WITH_ASAN:-0}
WITH_RTSAN=${WITH_RTSAN:-0}

export FREEDVGUIDIR=${PWD}

# Finally, build freedv-gui
cd $FREEDVGUIDIR
if [ -d .git ]; then
     git pull
fi
mkdir  -p build_linux && cd build_linux && rm -Rf *
cmake -DENABLE_RTSAN=${WITH_RTSAN} -DENABLE_ASAN=${WITH_ASAN} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DUSE_NATIVE_AUDIO=$USE_NATIVE_AUDIO -DUNITTEST=$UT_ENABLE ..
make VERBOSE=1 -j$(nproc)
