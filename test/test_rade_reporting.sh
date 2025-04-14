#!/bin/bash

# Determine sox driver to use for recording/playback
OPERATING_SYSTEM=`uname`
SOX_DRIVER=alsa
FREEDV_BINARY=src/freedv
if [ "$OPERATING_SYSTEM" == "Darwin" ]; then
    SOX_DRIVER=coreaudio
    FREEDV_BINARY=src/FreeDV.app/Contents/MacOS/freedv
fi

createVirtualAudioCable () {
    CABLE_NAME=$1
    pactl load-module module-null-sink sink_name=$CABLE_NAME sink_properties=device.description=$CABLE_NAME latency_msec=1
}

FREEDV_RADIO_TO_COMPUTER_DEVICE="${FREEDV_RADIO_TO_COMPUTER_DEVICE:-FreeDV_Radio_To_Computer}"
FREEDV_COMPUTER_TO_SPEAKER_DEVICE="${FREEDV_COMPUTER_TO_SPEAKER_DEVICE:-FreeDV_Computer_To_Speaker}"
FREEDV_MICROPHONE_TO_COMPUTER_DEVICE="${FREEDV_MICROPHONE_TO_COMPUTER_DEVICE:-FreeDV_Microphone_To_Computer}"
FREEDV_COMPUTER_TO_RADIO_DEVICE="${FREEDV_COMPUTER_TO_RADIO_DEVICE:-FreeDV_Computer_To_Radio}"

# Automated script to help find audio dropouts.
# NOTE: this must be run from "build_linux". Also assumes PulseAudio/pipewire.
if [ "$OPERATING_SYSTEM" == "Linux" ]; then
    DRIVER_INDEX_FREEDV_RADIO_TO_COMPUTER=$(createVirtualAudioCable FreeDV_Radio_To_Computer)
    DRIVER_INDEX_FREEDV_COMPUTER_TO_SPEAKER=$(createVirtualAudioCable FreeDV_Computer_To_Speaker)
    DRIVER_INDEX_FREEDV_MICROPHONE_TO_COMPUTER=$(createVirtualAudioCable FreeDV_Microphone_To_Computer)
    DRIVER_INDEX_FREEDV_COMPUTER_TO_RADIO=$(createVirtualAudioCable FreeDV_Computer_To_Radio)
    DRIVER_INDEX_LOOPBACK=`pactl load-module module-loopback source="FreeDV_Computer_To_Radio.monitor" sink="FreeDV_Radio_To_Computer" latency_msec=1`
fi

# Determine correct record device to retrieve TX data
if [ "$2" == "mpp" ]; then
    FREEDV_CONF_FILE=freedv-ctest-reporting-mpp.conf 
else
    FREEDV_CONF_FILE=freedv-ctest-reporting.conf 
fi

if [ "$OPERATING_SYSTEM" == "Linux" ]; then
    REC_DEVICE="$FREEDV_COMPUTER_TO_RADIO_DEVICE.monitor"
else
    REC_DEVICE="$FREEDV_COMPUTER_TO_RADIO_DEVICE"
fi

# Generate config file
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
if [ "$FREEDV_RADIO_TO_COMPUTER_DEVICE" == "FreeDV_Radio_To_Computer" ] && [ "$OPERATING_SYSTEM" == "Linux" ]; then
    sed "s/@FREEDV_RADIO_TO_COMPUTER_DEVICE@/$FREEDV_RADIO_TO_COMPUTER_DEVICE.monitor/g" $SCRIPTPATH/$FREEDV_CONF_FILE.tmpl > $(pwd)/$FREEDV_CONF_FILE
else
    sed "s/@FREEDV_RADIO_TO_COMPUTER_DEVICE@/$FREEDV_RADIO_TO_COMPUTER_DEVICE/g" $SCRIPTPATH/$FREEDV_CONF_FILE.tmpl > $(pwd)/$FREEDV_CONF_FILE
fi

sed "s/@FREEDV_COMPUTER_TO_RADIO_DEVICE@/$FREEDV_COMPUTER_TO_RADIO_DEVICE/g" $(pwd)/$FREEDV_CONF_FILE > $(pwd)/$FREEDV_CONF_FILE.tmp
mv $(pwd)/$FREEDV_CONF_FILE.tmp $(pwd)/$FREEDV_CONF_FILE
sed "s/@FREEDV_COMPUTER_TO_SPEAKER_DEVICE@/$FREEDV_COMPUTER_TO_SPEAKER_DEVICE/g" $(pwd)/$FREEDV_CONF_FILE > $(pwd)/$FREEDV_CONF_FILE.tmp
mv $(pwd)/$FREEDV_CONF_FILE.tmp $(pwd)/$FREEDV_CONF_FILE

if [ "$FREEDV_MICROPHONE_TO_COMPUTER_DEVICE" == "FreeDV_Microphone_To_Computer" ] && [ "$OPERATING_SYSTEM" == "Linux" ]; then
    sed "s/@FREEDV_MICROPHONE_TO_COMPUTER_DEVICE@/$FREEDV_MICROPHONE_TO_COMPUTER_DEVICE.monitor/g" $(pwd)/$FREEDV_CONF_FILE > $(pwd)/$FREEDV_CONF_FILE.tmp
else
    sed "s/@FREEDV_MICROPHONE_TO_COMPUTER_DEVICE@/$FREEDV_MICROPHONE_TO_COMPUTER_DEVICE/g" $(pwd)/$FREEDV_CONF_FILE > $(pwd)/$FREEDV_CONF_FILE.tmp
fi
mv $(pwd)/$FREEDV_CONF_FILE.tmp $(pwd)/$FREEDV_CONF_FILE

# Start recording
if [ "$OPERATING_SYSTEM" == "Linux" ]; then
    parecord --channels=1 --rate 8000 --file-format=wav --device "$REC_DEVICE" --latency 1 test.wav &
else
    sox -t $SOX_DRIVER "$REC_DEVICE" -c 1 -r 8000 -t wav test.wav >/dev/null 2>&1 &
fi
RECORD_PID=$!

# Start "radio"
python3 $SCRIPTPATH/hamlibserver.py $RECORD_PID &
RADIO_PID=$!

# Start FreeDV in test mode to record TX
if [ "$2" == "mpp" ]; then
    TX_ARGS="-txtime 1 -txattempts 6 "
else
    TX_ARGS="-txtime 5 "
fi
$FREEDV_BINARY -f $(pwd)/$FREEDV_CONF_FILE -ut tx -utmode RADE $TX_ARGS

FDV_PID=$!
#sleep 30 
#screencapture ../screenshot.png
#wpctl status
#pw-top -b -n 5
#wait $FDV_PID

# Stop recording, play back in RX mode
kill $RECORD_PID

if [ "$1" != "" ]; then
    FADING_DIR="$(pwd)/fading"
    if [ ! -d "$FADING_DIR" ]; then
        mkdir $FADING_DIR
        (cd $1/../unittest && ./fading_files.sh $FADING_DIR)
    fi
    # Add noise to recording to test performance
    if [ "$2" == "mpp" ]; then
        sox $(pwd)/test.wav -t raw -r 8000 -c 1 -e signed-integer -b 16 - | $1/src/ch - - --No -24 --mpp --fading_dir $FADING_DIR | sox -t raw -r 8000 -c 1 -e signed-integer -b 16 - -t wav $(pwd)/testwithnoise.wav
    elif [ "$2" == "awgn" ]; then
        sox $(pwd)/test.wav -t raw -r 8000 -c 1 -e signed-integer -b 16 - | $1/src/ch - - --No -18 | sox -t raw -r 8000 -c 1 -e signed-integer -b 16 - -t wav $(pwd)/testwithnoise.wav
    fi
    mv $(pwd)/testwithnoise.wav $(pwd)/test.wav
fi

$FREEDV_BINARY -f $(pwd)/$FREEDV_CONF_FILE -ut rx -utmode RADE -rxfile $(pwd)/test.wav

# Clean up PulseAudio virtual devices
if [ "$OPERATING_SYSTEM" == "Linux" ]; then
    pactl unload-module $DRIVER_INDEX_LOOPBACK
    pactl unload-module $DRIVER_INDEX_FREEDV_RADIO_TO_COMPUTER
    pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_SPEAKER
    pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_RADIO
    pactl unload-module $DRIVER_INDEX_FREEDV_MICROPHONE_TO_COMPUTER
fi

# End radio process as it's no longer needed
kill $RADIO_PID
