#!/bin/sh

RADAE_PATH=$1
RADAE_VENV=$2

# The below does the following:
#     * For each block of microphone audio from freedv-gui:
#         * Extract features from 16 kHz input audio.
#         * Pass features into the RADAE encoder.
#         * Send the resulting 8 kHz audio back to freedv-gui.
#
# Note: Current RADAE scripts seem to require being executed from
# the RADAE folder.
cd $RADAE_PATH
export PATH=$RADAE_VENV/bin:$PATH
stdbuf -i0 -o0 build/src/lpcnet_demo -features - - | \
stdbuf -i0 -o0 python3 radae_tx.py model19_check3/checkpoints/checkpoint_epoch_100.pth --auxdata | \
stdbuf -i0 -o0 python3 f32toint16.py --real --scale 16383
