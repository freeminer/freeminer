#!/bin/sh

NDK_SDK_ROOT=~

NDK_VER=11
NDK_V=android-ndk-r$NDK_VER
NDK=$NDK_V-linux-x86_64
NDK_FILE=$NDK.zip
NDK_DIR=$NDK_V

SDK_FILE=android-sdk_r24.3.4-linux.tgz
SDK_DIR=android-sdk-linux

TEST_PLATFORM=android-16

WGET="wget --continue"

sudo apt-get install -y default-jdk android-tools-adb ant m4 gcc-multilib lib32z1 libgettextpo0

if [ ! -s path.cfg ] ; then
	echo ANDROID_NDK = $NDK_SDK_ROOT/$NDK_DIR >> path.cfg
	echo NDK_MODULE_PATH = $NDK_SDK_ROOT/$NDK_DIR/toolchains >> path.cfg
	echo SDKFOLDER = $NDK_SDK_ROOT/$SDK_DIR/ >> path.cfg
fi

DIR_SAVE=`pwd`
mkdir -p $NDK_SDK_ROOT
cd $NDK_SDK_ROOT

if [ ! -d $NDK_DIR ] ; then
	if [ ! -s $NDK_FILE ] ; then
		$WGET -O $NDK_FILE http://dl.google.com/android/repository/$NDK_FILE
	fi
	unzip $NDK_FILE
fi

if [ ! -d $SDK_DIR ] ; then
	if [ ! -s $SDK_FILE ] ; then
		$WGET -O $SDK_FILE https://dl.google.com/android/$SDK_FILE
	fi
	tar xf $SDK_FILE
fi

if [ ! -d $SDK_DIR/platforms/$TEST_PLATFORM ] ; then
	#( sleep 2 && while [ 1 ]; do sleep 1; echo y; done ) | $SDK_DIR/tools/android update sdk --no-ui
	( sleep 2 && while [ 1 ]; do sleep 1; echo y; done ) | $SDK_DIR/tools/android update sdk --no-ui --filter platform-tool,$TEST_PLATFORM,android-15,android-21,build-tools-23.0.2
	echo autoinstall build-tools-23.0.2 now maybe broken, install manually: $NDK_SDK_ROOT/$SDK_DIR/tools/android
fi

cd $DIR_SAVE
MAKE="nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)"

$MAKE                arch_dirs release && cp bin/freeminer-release-unsigned.apk bin/freeminer-release-unsigned-armv7.apk
$MAKE TARGET_x86=1   arch_dirs release && cp bin/freeminer-release-unsigned.apk bin/freeminer-release-unsigned-x86.apk
$MAKE TARGET_arm64=1 arch_dirs release && cp bin/freeminer-release-unsigned.apk bin/freeminer-release-unsigned-arm64.apk
#$MAKE TARGET_mips=1  arch_dirs release && cp bin/freeminer-release-unsigned.apk bin/freeminer-release-unsigned-mips.apk
