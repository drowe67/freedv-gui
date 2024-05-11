#!/usr/bin/python3

import sys

# Read entire "decoded" output into memory
with open("/home/parallels/freedv-gui/rx_radae_demo.s16", "rb") as f:
    entire_file = f.read()

ctr = 0
num_to_read = 1024
while True:
    b = sys.stdin.buffer.read(num_to_read)
    if len(b) < num_to_read: break

    #sys.stderr.write("read block\n")

    if (ctr + num_to_read) > len(entire_file):
        sys.stdout.buffer.write(entire_file[ctr:])
        ctr = 0
    else:
        sys.stdout.buffer.write(entire_file[ctr:ctr + num_to_read])
        ctr = ctr + num_to_read
