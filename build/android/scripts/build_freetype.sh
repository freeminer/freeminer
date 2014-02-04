#!/bin/bash

export PATH=/tmp/ndk-arm/bin/:$PATH

./configure --host=arm-linux-androideabi --prefix=$(pwd)/build --without-png
make && make install
