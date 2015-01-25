#!/bin/bash

export PATH=/tmp/ndk-arm/bin:$PATH

# run configure to generate iconv.h
./configure

patch -p1 < ../../etc/iconv.patch

cp -r ../../etc/libiconv_jni/ jni
cd jni
$ANDROID_NDK/ndk-build V=1
