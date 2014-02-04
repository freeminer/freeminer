#!/bin/bash

# Taken from https://github.com/yixia/LevelDB-Android/

export PATH=/tmp/ndk-arm/bin:$PATH
export CC=arm-linux-androideabi-gcc
export CXX=arm-linux-androideabi-g++
export TARGET_OS=OS_ANDROID_CROSSCOMPILE

# make clean
make -j4
