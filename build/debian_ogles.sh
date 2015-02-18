# script for fast installing on raspberry pi, odroid and other arm boards with debian

# There's no package available, you have to compile it from source.
# you can place this text to freeminer.sh file and run it

# or
# curl https://raw.githubusercontent.com/freeminer/freeminer/master/build/debian_ogles.sh | sh

#1. To compile need to install packages:
sudo apt-get install -y git subversion build-essential cmake libbz2-dev libpng12-dev libjpeg8-dev libfreetype6-dev libxxf86vm-dev libgl1-mesa-dev libsqlite3-dev libvorbis-dev
sudo apt-get install -y libopenal-dev libcurl4-openssl-dev libluajit-5.1-dev libleveldb-dev libsnappy-dev libgettextpo0 libmsgpack-dev libgles1-mesa-dev libgles2-mesa-dev

#2. get and compile irrlicht with oppengl es support:

#svn checkout svn://svn.code.sf.net/p/irrlicht/code/branches/ogl-es irrlicht
#OR using git:
git clone -b ogl-es --recursive https://github.com/zaki/irrlicht.git irrlicht

#compile irrlicht:
time nice make -j $(nproc || sysctl -n hw.ncpu || echo 2) -C irrlicht/source/Irrlicht

#3. get freeminer
git clone --recursive https://github.com/freeminer/freeminer.git

cd freeminer
#update if second+ run
git pull

#compile
cmake . -DENABLE_GLES=1 -DIRRLICHT_INCLUDE_DIR=../irrlicht/include -DIRRLICHT_LIBRARY=../irrlicht/lib/Linux/libIrrlicht.a
time nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)

#run!
bin/freeminer
