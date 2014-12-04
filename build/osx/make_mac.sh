#!/bin/sh

STARTDIR=`pwd`
ROOTDIR='../../..'
BRANCH='stable'

brew install cmake freetype gettext hiredis irrlicht jpeg leveldb libogg libvorbis luajit msgpack
git submodule update --init --recursive

# Clone MT source code if not already there
#if [ ! -d "freeminer-git" ]; then
#  git clone --recursive -b $BRANCH https://github.com/freeminer/freeminer freeminer-git
#fi

# Get default if it is not already there
#if [ ! -d "default" ]; then
#  git clone --recursive https://github.com/freeminer/default
#fi

# Update default from GitHub
#(cd default && git pull)

# Get Carbone if it is not already there
if [ ! -d "carbone" ]; then
  git clone --recursive https://git.gitorious.org/calinou/carbone.git
fi

# Update Carbone
(cd carbone && git pull)

# Get Voxelgarden if it is not already there
if [ ! -d "Voxelgarden" ]; then
  git clone https://github.com/CasimirKaPazi/Voxelgarden.git
fi

# Update Voxelgarden
(cd Voxelgarden && git pull)

mkdir -p freeminer-git
# Update source code and set version string
cd freeminer-git
#git checkout --force $BRANCH --
#git pull
#git submodule update --init --recursive
gitver=`git log -1 --format='%cd.%h' --date=short | tr -d -`

#patch -p1 <../u64.patch

rm -f $ROOTDIR/CMakeCache.txt
cmake $ROOTDIR -DCMAKE_BUILD_TYPE=Release -DRUN_IN_PLACE=0 -DENABLE_FREETYPE=on -DENABLE_LEVELDB=on -DENABLE_GETTEXT=on -DENABLE_REDIS=on -DBUILD_SERVER=NO -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_CXX_FLAGS="-mmacosx-version-min=10.10 -march=core2 -msse4.1" -DCMAKE_C_FLAGS="-mmacosx-version-min=10.10 -march=core2 -msse4.1" -DCUSTOM_GETTEXT_PATH=/usr/local/opt/gettext -DCMAKE_EXE_LINKER_FLAGS="-L/usr/local/lib"

make clean
make VERBOSE=1

if [ ! -f "bin/freeminer" ]; then
    echo "compile fail"
    exit
fi

cp -p bin/freeminer ../releases/freeminer.app/Contents/Resources/bin
cd ../releases

make -C dylibbundler
# Change library paths in binary to point to bundle directory
./dylibbundler/dylibbundler -x freeminer.app/Contents/Resources/bin/freeminer -d ./freeminer.app/Contents/libs/ -p @executable_path/../../libs/
echo "======== otool ======="

# Print library paths which should now point to the executable path
otool -L freeminer.app/Contents/Resources/bin/freeminer | grep executable

# Remove shared directories...
mkdir -p freeminer.app/Contents/Resources/bin/share/games
(cd freeminer.app/Contents/Resources/bin/share && rm -fr builtin client fonts locale textures)

# ...and copy new ones from source code directory
for i in builtin client fonts locale textures
do
cp -pr $ROOTDIR/$i freeminer.app/Contents/Resources/bin/share
done

# Copy subgames into games directory
rm -fr freeminer.app/Contents/Resources/bin/share/games/*
(cd freeminer.app/Contents/Resources/bin/share/games && mkdir default carbone Voxelgarden)
cp -pr $ROOTDIR/games/default/* freeminer.app/Contents/Resources/bin/share/games/default/
cp -pr $STARTDIR/carbone/* freeminer.app/Contents/Resources/bin/share/games/carbone/
cp -pr $STARTDIR/Voxelgarden/* freeminer.app/Contents/Resources/bin/share/games/Voxelgarden/

# Create updated Info.plist with new version string
sysver=`sw_vers -productVersion`
sed -e "s/GIT_VERSION/$gitver/g" -e "s/MACOSX_DEPLOYMENT_TARGET/$sysver/g" Info.plist >  freeminer.app/Contents/Info.plist

# Compress app bundle as a ZIP file
fname=freeminer-osx-bin-$gitver.zip
rm -f $fname
zip -9 -r $fname freeminer.app

