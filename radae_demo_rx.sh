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
$RADAE_VENV/bin/python3 $RADAE_PATH/int16tof32.py --zeropad | $RADAE_VENV/bin/python3 $RADAE_PATH/radae_rx.py $RADAE_PATH/model19_check3/checkpoints/checkpoint_epoch_100.pth -v 2 --auxdata | $RADAE_PATH/build/src/lpcnet_demo -fargan-synthesis - -
