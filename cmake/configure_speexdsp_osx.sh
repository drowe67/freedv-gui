#!/usr/bin/env bash

CFLAGS="-g -O2 -mmacosx-version-min=10.9" ./configure --prefix=$1 --disable-examples
