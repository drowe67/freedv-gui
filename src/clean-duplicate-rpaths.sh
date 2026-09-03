#!/bin/bash

for i in `otool -l $1 | grep -A2 LC_RPATH | grep path | awk '{ print $2; }' | tail +2`; do
    install_name_tool -delete_rpath "$i" $1
done
