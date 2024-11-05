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
DRIVER_INDEX_LOOPBACK=`pactl load-module module-loopback source="FreeDV_Computer_To_Radio.monitor" sink="FreeDV_Radio_To_Computer"`

# For debugging--list sink info
#pactl list sinks

# If full duplex test, use correct config file and assume "rx" mode.
FREEDV_CONF_FILE=freedv-ctest.conf
if [ "$FREEDV_TEST" == "txrx" ]; then
    FREEDV_TEST=rx
    FREEDV_CONF_FILE=freedv-ctest-fullduplex.conf
    REC_DEVICE="FreeDV_Computer_To_Speaker.monitor"

    # Generate sine wave for input
    sox -r 48000 -n -b 16 -c 1 -t wav - synth 120 sin 1000 vol -10dB | paplay -d "FreeDV_Microphone_To_Computer" &
    PLAY_PID=$!
elif [ "$FREEDV_TEST" == "rx" ]; then
    # Start playback if RX
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
src/freedv -f $(pwd)/../$FREEDV_CONF_FILE -ut $FREEDV_TEST -utmode $FREEDV_MODE 2>&1 | tee tmp.log &

FDV_PID=$!
#sleep 30 
#wpctl status
#pw-top -b -n 5
wait $FDV_PID

# Stop recording/playback and process data
if [ "$FREEDV_TEST" == "rx" ]; then
    kill $PLAY_PID || echo "Already done playing"
    NUM_RESYNCS=`grep "Sync changed" tmp.log | wc -l`
    echo "Got $NUM_RESYNCS sync changes"
else
    kill $RECORD_PID
    sox test.wav -t raw -r 8k -c 1 -b 16 -e signed-integer test.raw silence 1 0.1 0.1% reverse silence 1 0.1 0.1% reverse

    SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
    python3 $SCRIPTPATH/check-for-zeros.py test.raw
fi

# Clean up PulseAudio virtual devices
pactl unload-module $DRIVER_INDEX_LOOPBACK
pactl unload-module $DRIVER_INDEX_FREEDV_RADIO_TO_COMPUTER
pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_SPEAKER
pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_RADIO
pactl unload-module $DRIVER_INDEX_FREEDV_MICROPHONE_TO_COMPUTER
