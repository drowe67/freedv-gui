#!/bin/bash

CFLAGS="-g -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=10.9" ./configure --prefix=$1 --disable-examples
