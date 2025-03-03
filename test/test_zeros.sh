#!/bin/bash

FREEDV_TEST=$1
FREEDV_MODE=$2
FREEDV_RX_FILE=$3

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
    pactl load-module module-null-sink sink_name=$CABLE_NAME sink_properties=device.description=$CABLE_NAME
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
    DRIVER_INDEX_LOOPBACK=`pactl load-module module-loopback source="FreeDV_Computer_To_Radio.monitor" sink="FreeDV_Radio_To_Computer"`
fi

# For debugging--list sink info
#pactl list sinks
# If full duplex test, use correct config file and assume "rx" mode.  
FREEDV_CONF_FILE=freedv-ctest.conf 
if [ "$FREEDV_TEST" == "txrx" ]; then 
    FREEDV_TEST=rx 
    FREEDV_CONF_FILE=freedv-ctest-fullduplex.conf
    REC_DEVICE="$FREEDV_COMPUTER_TO_SPEAKER_DEVICE"

    # Generate sine wave for input
    if [ "$OPERATING_SYSTEM" == "Linux" ]; then
        sox -r 48000 -n -b 16 -c 1 -t wav - synth 120 sin 1000 vol -10dB | paplay -d "$FREEDV_MICROPHONE_TO_COMPUTER_DEVICE" &
    else
        sox -r 48000 -n -b 16 -c 1 -t $SOX_DRIVER "$FREEDV_MICROPHONE_TO_COMPUTER_DEVICE" - synth 120 sin 1000 vol -10dB &
    fi
    PLAY_PID=$!
elif [ "$FREEDV_TEST" == "rx" ]; then
    # Start playback if RX
    if [ "$OPERATING_SYSTEM" == "Linux" ]; then
        paplay -d "$FREEDV_RADIO_TO_COMPUTER_DEVICE" $FREEDV_RX_FILE &
    else
        sox $FREEDV_RX_FILE -t $SOX_DRIVER "$FREEDV_RADIO_TO_COMPUTER_DEVICE" &
    fi
    PLAY_PID=$!
    REC_DEVICE="$FREEDV_COMPUTER_TO_SPEAKER_DEVICE.monitor"
else
    REC_DEVICE="$FREEDV_COMPUTER_TO_RADIO_DEVICE.monitor"
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
if [ "$FREEDV_TEST" == "tx" ]; then
    if [ "$OPERATING_SYSTEM" == "Linux" ]; then
        parecord --channels=1 --rate 8000 --file-format=wav --device "$REC_DEVICE" --latency 1 test.wav &
    else
        sox -t $SOX_DRIVER "$REC_DEVICE" -c 1 -r 8000 -t wav test.wav &
    fi
    RECORD_PID=$!
fi

# Start FreeDV in test mode
$FREEDV_BINARY -f $(pwd)/$FREEDV_CONF_FILE -ut $FREEDV_TEST -utmode $FREEDV_MODE 2>&1 | tee tmp.log

FDV_PID=$!
#sleep 30 
#screencapture ../screenshot.png
#wpctl status
#pw-top -b -n 5
#wait $FDV_PID

# Stop recording/playback and process data
if [ "$FREEDV_TEST" == "rx" ]; then
    kill $PLAY_PID || echo "Already done playing"
    NUM_RESYNCS=`grep "Sync changed" tmp.log | wc -l | xargs`
    echo "Got $NUM_RESYNCS sync changes"
else
    kill $RECORD_PID
    sox test.wav -t raw -r 8k -c 1 -b 16 -e signed-integer test.raw silence 1 0.1 0.1% reverse silence 1 0.1 0.1% reverse
    python3 $SCRIPTPATH/check-for-zeros.py test.raw
fi

# Clean up PulseAudio virtual devices
if [ "$OPERATING_SYSTEM" == "Linux" ]; then
    pactl unload-module $DRIVER_INDEX_LOOPBACK
    pactl unload-module $DRIVER_INDEX_FREEDV_RADIO_TO_COMPUTER
    pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_SPEAKER
    pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_RADIO
    pactl unload-module $DRIVER_INDEX_FREEDV_MICROPHONE_TO_COMPUTER
fi
