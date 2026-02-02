#!/usr/bin/env bash

CFLAGS="-g -O3 -mmacosx-version-min=11.0" ./configure --prefix=$1 --disable-examples
