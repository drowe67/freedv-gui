#!/bin/sh

RADAE_PATH=$1
RADAE_VENV=$2

# Current RADAE scripts seem to require being executed from
# the RADAE folder.
cd $RADAE_PATH

# The below does the following:
#     * For each block of OTA audio from freedv-gui:
#         * Convert the audio into IQ data via zero-padding.
#         * Pass the IQ data into the RADAE decoder 
#         * Send the resulting 16 kHz audio back to freedv-gui.
export PATH=$RADAE_VENV/bin:$PATH
$RADAE_PATH/inference.sh $RADAE_PATH/model19_check3/checkpoints/checkpoint_epoch_100.pth /dev/stdin /dev/null --end_of_over --auxdata --EbNodB 100 --bottleneck 3 --pilots --cp 0.004 --rate_Fs --write_rx /dev/stdout | python3 $RADAE_PATH/f32toint16.py --real --scale 16383
