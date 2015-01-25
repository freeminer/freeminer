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

echo ">> Making toolchain"
$ROOT/scripts/make_standalone_toolchain.sh || exit 1

mkdir -p $ROOT/deps

cd $ROOT/deps
if [ ! -d "leveldb" ]; then
	echo ">> Fetching LevelDB"
	git clone https://code.google.com/p/leveldb/ || exit 1
fi

cd $ROOT/deps
if [ ! -d "irrlicht" ]; then
	echo ">> Fetching Irrlicht"
	git clone https://github.com/freeminer/irrlicht-android.git irrlicht || exit 1
fi

cd $ROOT/deps
if [ ! -d "msgpack-c" ]; then
	echo ">> Fetching msgpack-c"
	git clone -b cpp-0.5.9 https://github.com/msgpack/msgpack-c.git || exit 1

	echo ">> Building msgpack"
	cd $ROOT/deps/msgpack-c
	$ROOT/scripts/build_msgpack.sh || exit 1
fi

cd $ROOT/deps
if [ ! -d "freetype" ]; then
	echo ">> Downloading FreeType"
	wget http://sourceforge.net/projects/freetype/files/freetype2/2.5.2/freetype-2.5.2.tar.bz2
	tar xvf freetype-2.5.2.tar.bz2
	mv freetype-2.5.2 freetype
	echo ">> Building FreeType"
	cd freetype
	$ROOT/scripts/build_freetype.sh || exit 1
fi

cd $ROOT/deps
if [ ! -d "libiconv" ]; then
	echo ">> Downloading libiconv"
	wget http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.13.1.tar.gz
	tar xvf libiconv-1.13.1.tar.gz
	mv libiconv-1.13.1 libiconv
	echo ">> Building libiconv"
	cd libiconv
	$ROOT/scripts/build_libiconv.sh || exit 1
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
