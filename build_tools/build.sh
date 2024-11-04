#!/usr/bin/env bash
# script for fast installing on raspberry pi, odroid and other arm boards with debian

set -e
set -x

# There's no package available, you have to compile it from source.
# you can place this text to freeminer.sh file and run it

# or
# curl https://raw.githubusercontent.com/freeminer/freeminer/master/build_tools/build.sh | sh

#1. To compile need to install packages:
DIST=${DIST:=$(lsb_release --short --id)} ||:
DIST=${DIST:=$(cat /etc/issue /etc/issue.net | head -n1 | cut -d " " -f1)}
DIST=${DIST:=$(sh -c '. /etc/os-release && echo $NAME')}

if [ -z "$NO_DEPS" ]; then
  SUDO=${SUDO=$(which sudo ||:)}
  if [ "$DIST" = "Debian" ] || [ "$DIST" = "Ubuntu" ]; then
    ${SUDO} apt update
    ${SUDO} env DEBIAN_FRONTEND=noninteractive apt install -y git subversion build-essential cmake ninja-build ccache libbz2-dev libzstd-dev  libjpeg-dev libfreetype6-dev libxxf86vm-dev libxi-dev libsqlite3-dev libhiredis-dev libvorbis-dev libopenal-dev libcurl4-openssl-dev libssl-dev libluajit-5.1-dev libgettextpo0 libmsgpack-dev libboost-system-dev  clang lld llvm libc++-dev libc++abi-dev
    for PACKAGE in libpng12-dev libpng-dev libgles1-mesa-dev libgles2-mesa-dev libgl1-mesa-dev ; do
        ${SUDO} apt install -y $PACKAGE ||:
    done
  elif [ -e /etc/arch-release ]; then
    ${SUDO} pacman --needed --noconfirm -Sy which git subversion cmake ninja ccache bzip2 zstd libjpeg-turbo freetype2 glfw-x11 libxxf86vm libxi sqlite3 hiredis libvorbis openal curl luajit gettext msgpack-cxx boost  clang lld llvm libc++ libc++abi libpng12 libpng libunwind
    CMAKE_OPT=${CMAKE_OPT=-DCMAKE_BUILD_WITH_INSTALL_RPATH=1}
  fi
fi


#3. get freeminer
if [ -d freeminer/build ]; then
    cd freeminer/build
elif [ -d freeminer ]; then
    mkdir -p freeminer/build
    cd freeminer/build
elif [ ! -s ../src/CMakeLists.txt ]; then
    git clone --depth 1 --recursive https://github.com/freeminer/freeminer.git && mkdir -p freeminer/build && cd freeminer/build
elif [ -s ../src/CMakeLists.txt ]; then
    mkdir -p ../build && cd ../build
fi


#update if second+ run
git pull --rebase ||:
git submodule update --init --recursive ||:

#compile
cmake .. -GNinja -DENABLE_GLES=1 -DCMAKE_C_COMPILER=`which clang` -DCMAKE_CXX_COMPILER=`which clang++` ${CMAKE_OPT}
nice cmake --build .


#run!
./freeminer
