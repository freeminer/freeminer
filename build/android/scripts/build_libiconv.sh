#!/bin/bash

# run configure to generate iconv.h
./configure

patch -p1 < ../../etc/iconv.patch

cp -r ../../etc/libiconv_jni/ jni
cd jni
ndk-build V=1
