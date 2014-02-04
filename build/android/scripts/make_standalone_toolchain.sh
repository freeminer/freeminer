#!/bin/bash

export CROSS_PREFIX=arm-linux-androideabi-
export TOOLCHAIN=/tmp/ndk-arm
$ANDROID_NDK/build/tools/make-standalone-toolchain.sh --toolchain=${CROSS_PREFIX}4.6 --install-dir=${TOOLCHAIN} --system=linux-x86_64
