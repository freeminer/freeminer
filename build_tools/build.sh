#!/usr/bin/env bash
# script for fast installing on raspberry pi, odroid and other arm boards with debian

set -e
set -x

# There's no package available, you have to compile it from source.
# you can place this text to freeminer.sh file and run it

# or
# curl https://raw.githubusercontent.com/freeminer/freeminer/master/build_tools/build.sh | sh

#1. To compile need to install packages:
DIST=${DIST=`lsb_release --short --id`} ||:
DIST=${DIST=`cat /etc/issue /etc/issue.net | head -n1 | cut -d " " -f1`}
if [ -z "$NO_DEPS" ]; then
  if [ "$DIST" = "Debian" ] || [ "$DIST" = "Ubuntu" ]; then
    sudo apt install -y git subversion build-essential cmake ninja-build ccache libbz2-dev libzstd-dev  libjpeg-dev libfreetype6-dev libxxf86vm-dev libxi-dev libsqlite3-dev libhiredis-dev libvorbis-dev libopenal-dev libcurl4-openssl-dev libluajit-5.1-dev libgettextpo0 libmsgpack-dev libboost-system-dev  clang lld llvm libc++-dev libc++abi-dev
    for PACKAGE in libpng12-dev libpng-dev libgles1-mesa-dev libgles2-mesa-dev libgl1-mesa-dev libunwind-dev ; do
        sudo apt install -y $PACKAGE ||:
    done
  elif [ -e /etc/arch-release ]; then
    sudo pacman --needed --noconfirm -S git subversion cmake ninja ccache bzip2 zstd libjpeg-turbo freetype2 libxxf86vm libxi sqlite3 hiredis libvorbis openal curl luajit gettext msgpack-cxx boost  clang lld llvm libc++ libc++abi libpng12 libpng libunwind
    echo Todo
  fi
fi



if [ -n "" ]; then
#2. get and compile irrlicht with oppengl es support:

#svn checkout svn://svn.code.sf.net/p/irrlicht/code/branches/ogl-es irrlicht
#OR using git:
git clone --recursive -b ogl-es  https://github.com/zaki/irrlicht.git irrlicht 
#TODO FIXME REMOVEME: (latest working revision)
git --git-dir=irrlicht/.git --work-tree=irrlicht/ checkout 63c2864

#compile irrlicht:
nice make -j $(nproc || sysctl -n hw.ncpu || echo 2) -C irrlicht/source/Irrlicht
fi

#3. get freeminer
[ ! -s ../src/CMakeLists.txt ] && git clone --depth 1 --recursive https://github.com/freeminer/freeminer.git && mkdir -p freeminer/build && cd freeminer/build
[ -s ../src/CMakeLists.txt ] && mkdir -p ../build && cd ../build

#update if second+ run
git pull --rebase

#compile
cmake .. -GNinja -DENABLE_GLES=1 -DCMAKE_C_COMPILER=`which clang` -DCMAKE_CXX_COMPILER=`which clang++` # -DIRRLICHT_INCLUDE_DIR=../irrlicht/include -DIRRLICHT_LIBRARY=../irrlicht/lib/Linux/libIrrlicht.a
nice cmake --build .

# link dir with /Shaders/
#ln -s ../irrlicht/media ./

#run!
./freeminer
