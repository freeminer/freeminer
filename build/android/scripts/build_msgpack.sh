#!/bin/bash

export PATH=/tmp/ndk-arm/bin/:$PATH

./bootstrap
./configure --prefix=$(pwd)/build --host=arm-linux-androideabi --disable-shared CFLAGS="-march=armv7-a" CXXFLAGS="-march=armv7-a -I$CXXSTL/include -I$CXXSTL/libs/armeabi-v7a/include"
make -j8 && make install
