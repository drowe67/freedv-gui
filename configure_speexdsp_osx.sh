#!/bin/bash

CFLAGS="-g -O2 -mmacosx-version-min=10.7" ./configure --prefix=$1 --disable-examples
