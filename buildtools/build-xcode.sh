#!/bin/bash

TARGETS="mordor mordorprotobuf mordortest run_tests"
ARCH=${arch:-"x86_64"}
CONFIG=Debug

if [[ $ARCH == "amd64" ]] ; then
    ARCH=x86_64
fi

for t in $TARGETS
do
    echo "build xcodebuild -target $t -configuration $CONFIG -arch $ARCH ..."
    xcodebuild -target $t -configuration $CONFIG -arch $ARCH
done
