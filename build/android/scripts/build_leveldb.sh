#!/bin/bash

# Taken from https://github.com/yixia/LevelDB-Android/

export CROSS_PREFIX=arm-linux-androideabi-
export TOOLCHAIN=/tmp/ndk-arm
$ANDROID_NDK/build/tools/make-standalone-toolchain.sh --toolchain=${CROSS_PREFIX}4.6 --install-dir=${TOOLCHAIN} --system=linux-x86_64

export PATH=$TOOLCHAIN/bin:$PATH
export CC=arm-linux-androideabi-gcc
export CXX=arm-linux-androideabi-g++
export TARGET_OS=OS_ANDROID_CROSSCOMPILE

# make clean
make -j4
