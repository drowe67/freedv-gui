#!/bin/bash

FREEDV_TEST=$1
FREEDV_MODE=$2
FREEDV_RX_FILE=$3

createVirtualAudioCable () {
    CABLE_NAME=$1
    pactl load-module module-null-sink sink_name=$CABLE_NAME sink_properties=device.description=$CABLE_NAME
}

# Automated script to help find audio dropouts.
# NOTE: this must be run from "build_linux". Also assumes PulseAudio/pipewire.
DRIVER_INDEX_FREEDV_RADIO_TO_COMPUTER=$(createVirtualAudioCable FreeDV_Radio_To_Computer)
DRIVER_INDEX_FREEDV_COMPUTER_TO_SPEAKER=$(createVirtualAudioCable FreeDV_Computer_To_Speaker)
DRIVER_INDEX_FREEDV_MICROPHONE_TO_COMPUTER=$(createVirtualAudioCable FreeDV_Microphone_To_Computer)
DRIVER_INDEX_FREEDV_COMPUTER_TO_RADIO=$(createVirtualAudioCable FreeDV_Computer_To_Radio)

# Start playback if RX
if [ "$FREEDV_TEST" == "rx" ]; then
    paplay -d "FreeDV_Radio_To_Computer" $FREEDV_RX_FILE &
    PLAY_PID=$!
    REC_DEVICE="FreeDV_Computer_To_Speaker.monitor"
else
    REC_DEVICE="FreeDV_Computer_To_Radio.monitor"
fi
 
# Start recording
if [ "$FREEDV_TEST" == "tx" ]; then
    parecord --channels=1 --rate 8000 --file-format=wav --device $REC_DEVICE --latency 1 test.wav &
    RECORD_PID=$!
fi

# Start FreeDV in test mode
src/freedv -f $(pwd)/../freedv-ctest.conf -ut $FREEDV_TEST -utmode $FREEDV_MODE | tee tmp.log

# Stop recording/playback and process data
if [ "$FREEDV_TEST" == "rx" ]; then
    kill $PLAY_PID || echo "Already done playing"
    NUM_RESYNCS=`grep "Sync changed" tmp.log | wc -l`
    if [ $NUM_RESYNCS -gt 1 ]; then
        echo "Got $NUM_RESYNCS syncs"
    fi
else
    kill $RECORD_PID
    sox test.wav -t raw -r 8k -c 1 -b 16 -e signed-integer test.raw silence 1 0.1 0.1% reverse silence 1 0.1 0.1% reverse
    python3 check-for-zeros.py test.raw
fi

# Clean up PulseAudio virtual devices
pactl unload-module $DRIVER_INDEX_FREEDV_RADIO_TO_COMPUTER
pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_SPEAKER
pactl unload-module $DRIVER_INDEX_FREEDV_MICROPHONE_TO_COMPUTER
pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_RADIO
