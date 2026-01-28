#!/usr/bin/env bash

CFLAGS="-g -O3 -arch arm64 -arch x86_64 -mmacosx-version-min=11.0" ./configure --prefix=$1 --disable-examples
