#!/usr/bin/python3

import sys
import struct

index = 0
detected = False
detected_index = -1
first_detected_index = -1
with open(sys.argv[1], "rb") as f:
    while True:
        bytes_to_read = struct.calcsize("h")
        buffer = f.read(bytes_to_read)
        if len(buffer) != bytes_to_read:
            break

        if buffer[0] == 0:
            if first_detected_index == -1:
                first_detected_index = index
                detected = True
            detected_index = index
        elif index == (detected_index + 1):
            if (index - first_detected_index) > 1:
                sys.stderr.write(f"Zero audio detected at index {index} ({index / 8000} seconds)")
                sys.stderr.write(f" - lasted {(index - first_detected_index) / 8000} seconds\n")
            first_detected_index = -1
        index = index + 1

if detected is False:
    print("No zeros found.")
