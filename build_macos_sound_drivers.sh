#!/bin/bash

# BLACKHOLE_LATENCY_FRAMES sets the latency the drivers report (and pad their ring buffer
# by). It was meant to be 128, but kLatency_Frame_Size used to be passed outside the quoted
# preprocessor definitions, and BlackHole.c defines it unconditionally anyway, so it stayed
# 0. It's now set by patching BlackHole.c; 0 keeps the old behavior as this experiment's
# baseline.
echo "Building BlackHole drivers with kLatency_Frame_Size=${BLACKHOLE_LATENCY_FRAMES:-0}"

git clone https://github.com/tmiw/BlackHole.git
cd BlackHole

for i in {1..2}; do
    git reset --hard
    rm -rf build
    sed -i '' -E "s/^(#define[[:space:]]+kLatency_Frame_Size[[:space:]]+)0$/\1${BLACKHOLE_LATENCY_FRAMES:-0}/" BlackHole/BlackHole.c
    grep -E "^#define[[:space:]]+kLatency_Frame_Size" BlackHole/BlackHole.c

    export bundleID=audio.existential.BlackHole$i
    export driverName=BlackHole$i

    xcodebuild \
        -project BlackHole.xcodeproj \
        -configuration Release \
        -target BlackHole \
        CONFIGURATION_BUILD_DIR=build \
        PRODUCT_BUNDLE_IDENTIFIER=$bundleID \
        GCC_PREPROCESSOR_DEFINITIONS="$GCC_PREPROCESSOR_DEFINITIONS \
            kNumber_Of_Channels='2' \
            kPlugIn_BundleID='\"$bundleID\"' \
            kDriver_Name='\"$driverName\"' \
            kDevice2_IsHidden=false \
            kDevice2_HasInput=true \
            kDevice2_HasOutput=true" \
        MACOSX_DEPLOYMENT_TARGET=11.0

    sudo mv build/BlackHole.driver /Library/Audio/Plug-Ins/HAL/$driverName.driver
done

sudo killall -9 coreaudiod
