#!/bin/bash

LIBS=`find $1 -name FreeDV -o -name '*.dylib'`

for i in $LIBS; do
    RPATHS=`otool -l $i | grep -A2 LC_RPATH | grep path | awk '{ print $2; }' | sort | uniq`
    for j in $RPATHS; do
        for k in `otool -l $i | grep -A2 LC_RPATH | grep path | awk '{ print $2; }' | grep $j | tail +2`; do
            echo "Removing RPATH $k"
            install_name_tool -delete_rpath "$k" $i
        done
    done
done
