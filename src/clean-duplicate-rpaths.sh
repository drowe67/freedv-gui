#!/bin/bash

RPATHS=`otool -l $1 | grep -A2 LC_RPATH | grep path | awk '{ print $2; }' | sort | uniq`
for i in $RPATHS; do
    for j in `otool -l $1 | grep -A2 LC_RPATH | grep path | awk '{ print $2; }' | grep $i | tail +2`; do
        echo "Removing RPATH $j"
        install_name_tool -delete_rpath "$j" $1
    done
done
