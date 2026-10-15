#!/bin/bash

git clone https://github.com/tmiw/BlackHole.git
cd BlackHole

# Four, each a separate loopback device: the GUI tests (test/gui) need one each
# for the radio's input and output, the microphone and the speaker, and two
# devices of the same driver ("BlackHole1 2ch" and "BlackHole1 2ch 2") share
# what's played into them.
for i in {1..4}; do
    git reset --hard
    rm -rf build

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
            kLatency_Frame_Size='128' \
        MACOSX_DEPLOYMENT_TARGET=12.0

    sudo mv build/BlackHole.driver /Library/Audio/Plug-Ins/HAL/$driverName.driver
done

sudo killall -9 coreaudiod
