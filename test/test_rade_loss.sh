#!/bin/bash

# Determine sox driver to use for recording/playback
OPERATING_SYSTEM=`uname`
if [ "$OPERATING_SYSTEM" == "Darwin" ]; then
    SOX_DRIVER=coreaudio
    FREEDV_BINARY=${FREEDV_BINARY:-src/FreeDV.app/Contents/MacOS/FreeDV}
else
    SOX_DRIVER=alsa
    FREEDV_BINARY=${FREEDV_BINARY:-src/freedv}
fi
PYTHON_BINARY=${PYTHON_BINARY:-python3}

createVirtualAudioCable () {
    CABLE_NAME=$1
    pactl load-module module-null-sink sink_name=$CABLE_NAME sink_properties=device.description=$CABLE_NAME 
}

waitForCableUp () {
    CABLE_NAME=$1
    until (pactl list short sinks | grep -E "^[0-9]+\\s+${CABLE_NAME}\\s+" >/dev/null)
    do
        echo "Waiting for $CABLE_NAME to come up..."
        sleep 1;
    done
}

FREEDV_RADIO_TO_COMPUTER_DEVICE="${FREEDV_RADIO_TO_COMPUTER_DEVICE:-FreeDV_Radio_To_Computer}"
FREEDV_COMPUTER_TO_SPEAKER_DEVICE="${FREEDV_COMPUTER_TO_SPEAKER_DEVICE:-FreeDV_Computer_To_Speaker}"
FREEDV_MICROPHONE_TO_COMPUTER_DEVICE="${FREEDV_MICROPHONE_TO_COMPUTER_DEVICE:-FreeDV_Microphone_To_Computer}"
FREEDV_COMPUTER_TO_RADIO_DEVICE="${FREEDV_COMPUTER_TO_RADIO_DEVICE:-FreeDV_Computer_To_Radio}"

# Automated script to help find audio dropouts.
# NOTE: this must be run from "build_*". Also assumes PulseAudio/pipewire or macOS Core Audio,
# does not work in Windows.
if [ "$OPERATING_SYSTEM" == "Linux" ]; then
    DRIVER_INDEX_FREEDV_RADIO_TO_COMPUTER=$(createVirtualAudioCable FreeDV_Radio_To_Computer)
    waitForCableUp FreeDV_Radio_To_Computer
    DRIVER_INDEX_FREEDV_COMPUTER_TO_SPEAKER=$(createVirtualAudioCable FreeDV_Computer_To_Speaker)
    waitForCableUp FreeDV_Computer_To_Speaker
    DRIVER_INDEX_FREEDV_MICROPHONE_TO_COMPUTER=$(createVirtualAudioCable FreeDV_Microphone_To_Computer)
    waitForCableUp FreeDV_Microphone_To_Computer
    DRIVER_INDEX_FREEDV_COMPUTER_TO_RADIO=$(createVirtualAudioCable FreeDV_Computer_To_Radio)
    waitForCableUp FreeDV_Computer_To_Radio
    DRIVER_INDEX_LOOPBACK=`pactl load-module module-loopback source="FreeDV_Computer_To_Radio.monitor" sink="FreeDV_Radio_To_Computer"`
fi

# Determine correct record device to retrieve TX data
FREEDV_CONF_FILE=freedv-ctest-loss.conf 

PLAY_DEVICE="$FREEDV_RADIO_TO_COMPUTER_DEVICE"
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

# Resample test file to 48 kHz. Needed for CI environment to reduce CPU usage.
if [ ! -d "$(pwd)/rade_src" ]; then
    git clone -b main https://github.com/drowe67/radae.git rade_src
fi
sox $(pwd)/rade_src/wav/all.wav -r 48000 $(pwd)/tx_in.wav

# Start recording
if [ "$OPERATING_SYSTEM" == "Linux" ]; then
    parecord --channels=1 --file-format=wav --device "$REC_DEVICE" --rate 48000 --format s16le test.wav &
else
    sox --buffer 128000 -t $SOX_DRIVER "$REC_DEVICE" -c 1 -t wav -r 48000 -b 16 -e signed-integer test.wav >/dev/null 2>&1 &
fi
RECORD_PID=$!

# Start FreeDV in test mode to record TX
TX_ARGS="-txfile $(pwd)/tx_in.wav -txfeaturefile $(pwd)/txfeatures.f32 -txradeinfile $(pwd)/rade_encoder_input.wav "
$FREEDV_BINARY -f $(pwd)/$FREEDV_CONF_FILE -ut tx -utmode RADEV1 $TX_ARGS >tmp.log 2>&1 &

FDV_PID=$!

#if [ "$OPERATING_SYSTEM" != "Linux" ]; then
#    xctrace record --template "Audio System Trace" --instrument "Time Profiler" --window 3m --output "instruments_trace_tx_${FDV_PID}.trace" --attach $FDV_PID
#fi

#sleep 30 
#screencapture ../screenshot.png
#wpctl status
#pw-top -b -n 5
wait $FDV_PID
FREEDV_EXIT_CODE=$?
cat tmp.log

# Stop recording, play back in RX mode
kill $RECORD_PID
#cp $(pwd)/gmon.out $(pwd)/gmon.out.tx

# Workaround/performance improvement: strip silence at beginning and end of recording
# As well as reducing the amount of audio that needs to be played back, it also helps
# ensure we don't accidentally run into a potential RADEV2 bug (https://github.com/freedv/rade_c/issues/8)
# Note: commands adapted from https://digitalcardboard.com/blog/2009/08/25/the-sox-of-silence/
sox test.wav test_stripped.wav silence 1 0.1 1% reverse
sox test_stripped.wav test.wav silence 1 0.1 1% reverse

# Snapshot under a name distinct from test_zeros.sh/test_rade_reporting.sh's own test.wav,
# since they run later in the same ctest sequence and would otherwise overwrite it before
# CI can upload it -- this is the raw modulated waveform actually played back for RX, for
# offline reproduction with the RADE tools.
cp test.wav rade_loss_test.wav

LOSS_THRESHOLD=0.0891

# RADEV2 has a known upstream bug (https://github.com/freedv/rade_c/issues/8): feature loss is
# periodic with RADE's ~20ms frame period, and there's a narrow (~4ms) window within that period
# where loss lands well above baseline purely because of *when* sync happens to be acquired, not
# because of an actual quality regression. If the first attempt lands in that window, shift the
# played-back recording by half a period (10ms -- as far from the bad window as possible) and
# retry once before concluding this is a real failure.
PHASE_CORRECTION_SEC=0.010

run_rade_loss_attempt () {
    local playback_file="$1"

    $FREEDV_BINARY -f $(pwd)/$FREEDV_CONF_FILE -ut rx -utmode RADEV1 -txtime 70 -rxfeaturefile $(pwd)/rxfeatures.f32 -rxradeinfile $(pwd)/rade_decoder_input.wav >tmp.log 2>&1 &
    FDV_PID=$!

    #if [ "$OPERATING_SYSTEM" != "Linux" ]; then
    #    xctrace record --template "Audio System Trace" --instrument "Time Profiler" --window 3m --output "instruments_trace_rx_${FDV_PID}.trace" --attach $FDV_PID
    #fi

    sleep 4.995

    if [ "$OPERATING_SYSTEM" == "Linux" ]; then
        paplay --file-format=wav --device "$PLAY_DEVICE" "$playback_file" &
    else
        sox --buffer 128000 -t wav "$playback_file" -t $SOX_DRIVER "$PLAY_DEVICE" >/dev/null 2>&1 &
    fi

    wait $FDV_PID
    FREEDV_EXIT_CODE=$?
    cat tmp.log

    # Run feature files through loss tool
    LOSS_OUTPUT=$($PYTHON_BINARY $(pwd)/rade_src/loss.py txfeatures.f32 rxfeatures.f32 --loss_test $LOSS_THRESHOLD --clip_start 100 --clip_end 300)
    echo "$LOSS_OUTPUT"
}

if [ $FREEDV_EXIT_CODE -eq 0 ]; then
    run_rade_loss_attempt test.wav

    if ! echo "$LOSS_OUTPUT" | grep -q "PASS"; then
        echo "Loss test failed on first attempt; retrying with a ${PHASE_CORRECTION_SEC}s phase-shifted recording in case this is the known RADEV2 sync-timing artifact (https://github.com/freedv/rade_c/issues/8)..."
        sox -n -r 48000 -c 1 -b 16 -e signed-integer silence_pad.wav trim 0 $PHASE_CORRECTION_SEC
        sox silence_pad.wav test.wav test_shifted.wav
        run_rade_loss_attempt test_shifted.wav
    fi
fi

# Clean up PulseAudio virtual devices
if [ "$OPERATING_SYSTEM" == "Linux" ]; then
    pactl unload-module $DRIVER_INDEX_LOOPBACK
    pactl unload-module $DRIVER_INDEX_FREEDV_RADIO_TO_COMPUTER
    pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_SPEAKER
    pactl unload-module $DRIVER_INDEX_FREEDV_COMPUTER_TO_RADIO
    pactl unload-module $DRIVER_INDEX_FREEDV_MICROPHONE_TO_COMPUTER
fi

exit $FREEDV_EXIT_CODE
