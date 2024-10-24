#!/bin/bash

FREEDV_TEST=$1
FREEDV_MODE=$2
FREEDV_RX_FILE=$3

# Automated script to help find audio dropouts.
# NOTE: assumes "sudo modprobe snd_aloop enable=1,1,1,1 index=1,2,3,4" was run before this script. Also, this must be run from "build_linux".

# Start playback if RX
if [ "$FREEDV_TEST" == "rx" ]; then
    paplay --device "alsa_output.platform-snd_aloop.0.analog-stereo.monitor" $FREEDV_RX_FILE &
    PLAY_PID=$!
    REC_DEVICE="alsa_output.platform-snd_aloop.1.analog-stereo.monitor"
else
    REC_DEVICE="alsa_output.platform-snd_aloop.3.analog-stereo.monitor"
fi
 
# Start recording
parecord --channels=1 --rate 8000 --file-format=wav --device $REC_DEVICE --latency 1 test.wav &
RECORD_PID=$!

# Start FreeDV in test mode
src/freedv -f $(pwd)/../freedv-ctest.conf -ut $FREEDV_TEST -utmode $FREEDV_MODE 

# Stop recording and process data
kill $RECORD_PID
if [ "$FREEDV_TEST" == "rx" ]; then
    kill $PLAY_PID
fi
sox test.wav -t raw -r 8k -c 1 -b 16 -e signed-integer test.raw silence 1 0.1 0.1% reverse silence 1 0.1 0.1% reverse
python3 check-for-zeros.py test.raw
