# script for fast installing on raspberry pi, odroid and other arm boards with debian

set -x

# There's no package available, you have to compile it from source.
# you can place this text to freeminer.sh file and run it

# or
# curl https://raw.githubusercontent.com/freeminer/freeminer/master/build_tools/debian_ogles.sh | sh

#1. To compile need to install packages:
sudo apt install -y git subversion build-essential cmake ninja-build ccache libbz2-dev libzstd-dev "libpng12-dev|libpng-dev" libjpeg-dev libfreetype6-dev libxxf86vm-dev libsqlite3-dev libvorbis-dev  libopenal-dev libcurl4-openssl-dev libluajit-5.1-dev libleveldb-dev libsnappy-dev libgettextpo0 libmsgpack-dev libgl1-mesa-dev "libgles1-mesa-dev|libgles2-mesa-dev" libgles2-mesa-dev libboost-system-dev libunwind-dev libc++-dev libc++abi-dev

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
git pull

#compile
cmake .. -GNinja -DENABLE_GLES=1 # -DIRRLICHT_INCLUDE_DIR=../irrlicht/include -DIRRLICHT_LIBRARY=../irrlicht/lib/Linux/libIrrlicht.a
nice cmake --build .

# link dir with /Shaders/
#ln -s ../irrlicht/media ./

#run!
./freeminer
