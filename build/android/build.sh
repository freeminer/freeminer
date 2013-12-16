#!/bin/bash

if [ -z "$ANDROID_NDK" ]; then
	echo "Please set ANDROID_NDK environment variable to point to your NDK root"
	echo "e.g. \`export ANDROID_NDK=/home/xyz/android-ndk-r8-crystax-1/\`"
	echo "and then restart the script."
	exit 1
fi

if [ -z "$NDEBUG" ]; then
	NDEBUG=1
fi

pushd `dirname $0` > /dev/null
ROOT=`pwd`
popd > /dev/null

mkdir -p $ROOT/deps
cd $ROOT/deps

if [ ! -d "leveldb" ]; then
	echo ">> Fetching LevelDB"
	git clone https://code.google.com/p/leveldb/ || exit 1
fi

if [ ! -d "irrlicht" ]; then
	echo ">> Checking out Irrlicht ogl-es branch"
	svn co http://svn.code.sf.net/p/irrlicht/code/branches/ogl-es/ irrlicht || exit 1
	echo ">> Applying irrlicht.patch"
	cd irrlicht
	patch -p0 < $ROOT/irrlicht.patch || exit 1
fi

echo ">> Building LevelDB"
cd $ROOT/deps/leveldb
$ROOT/scripts/build_leveldb.sh || exit 1

echo ">> Building Irrlicht"
cd $ROOT/deps/irrlicht/source/Irrlicht/Android/
$ANDROID_NDK/ndk-build NDEBUG=$NDEBUG -j8 || exit 1

echo ">> Building Freeminer"
cd $ROOT
$ANDROID_NDK/ndk-build NDEBUG=$NDEBUG -j8 || exit 1
ant debug || exit 1

echo "++ Success!"
echo "APK: bin/Freeminer-debug.apk"
echo "You can install it with \`adb install -r bin/Freeminer-debug.apk\`"
echo "or build a release version with \`ant release\`"
