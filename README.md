# Freeminer

[![Build Status](https://img.shields.io/travis/freeminer/freeminer.svg?style=flat-square)](https://travis-ci.org/freeminer/freeminer) [![Latest Stable Version](https://img.shields.io/github/release/freeminer/freeminer.svg?style=flat-square&label=stable)](https://github.com/freeminer/freeminer/releases/latest) [![Total Downloads](https://img.shields.io/github/downloads/freeminer/freeminer/latest/total.svg?style=flat-square)](https://github.com/freeminer/freeminer/releases) [![License](https://img.shields.io/github/license/freeminer/freeminer.svg?style=flat-square)](https://raw.githubusercontent.com/freeminer/freeminer/master/COPYING)

Freeminer is an open source sandbox game inspired by [Minecraft](https://minecraft.net/).

Freeminer is based on Minetest which is developed by a [number of contributors](https://github.com/minetest/minetest/graphs/contributors) from all over the globe.

It aims to make the game fun while trading off some bits of perfectionism.

## Installing
- **Android**:
	* Google Play - https://play.google.com/store/apps/details?id=org.freeminer.freeminer
	* F-droid - https://f-droid.org/repository/browse/?fdid=org.freeminer.freeminer
- **Windows**: https://github.com/freeminer/freeminer/releases
- **Ubuntu**: Install http://www.ubuntuupdates.org/ppa/getdeb_games and run:

	```sh
	sudo apt-get install freeminer
	```
- **Arch Linux**:

	```sh
	yaourt -S freeminer
	```
	<sup>*Stable version*</sup>
	
	```sh
	yaourt -S freeminer-git
	```
	<sup>*Development version*</sup>
	
- **FreeBSD**:

	```sh
	cd /usr/ports/games/freeminer-default && sudo make install clean
	```

## Further documentation
- Website: http://freeminer.org/
- Forums: http://forum.freeminer.org/

## Default controls
- `W` `A` `S` `D`: move
- `Space`: jump/climb
- `Shift`: sneak/go down
- `Q`: drop item
- `I`: inventory
- Mouse: turn/look
- Mouse left: dig/punch
- Mouse right: place/use
- Mouse wheel: select item
- `Esc`: pause menu
- `T`: chat
- `Z`: zoom
- `Tab`: player list
- `~`: toggle console

## Compiling
Install dependencies. Here's an example for

Debian/Ubuntu:
```sh
sudo apt-get install build-essential libirrlicht-dev cmake libbz2-dev \
libpng12-dev libjpeg-dev libfreetype6-dev libxxf86vm-dev libgl1-mesa-dev \
libsqlite3-dev libvorbis-dev libopenal-dev libcurl4-openssl-dev libluajit-5.1-dev \
libleveldb-dev libsnappy-dev libgettextpo0 libmsgpack-dev libgmp-dev libspatialindex-dev
# optional:
sudo apt-get install libhiredis-dev cmake-curses-gui
```
___
Fedora:
```sh
# the first five is the closest to Debian/Ubuntu build-essential
sudo yum install make automake gcc gcc-c++ kernel-devel cmake \
irrlicht-devel bzip2-devel libpng-devel libjpeg-turbo-devel freetype-devel \
libXxf86vm-devel mesa-libGL-devel sqlite-devel libvorbis-devel \
openal-soft-devel libcurl-devel luajit-devel leveldb-devel snappy-devel \
gettext-devel msgpack msgpack-devel spatialindex-devel
```
___
Arch Linux:
```sh
sudo pacman -S curl irrlicht leveldb libvorbis luajit openal sqlite cmake msgpack-c freetype2
```
___
Gentoo/Funtoo:
```sh
emerge -av media-libs/libvorbis media-libs/openal dev-games/irrlicht \
dev-libs/msgpack dev-libs/leveldb sci-libs/libspatialindex
```
___
OS X:
```sh
brew install cmake freetype gettext hiredis irrlicht jpeg leveldb libogg \
libvorbis luajit msgpack
```

<sup>Recommended irrlicht version: `1.8.2`</sup>

Download source code:
```sh
git clone --recursive https://github.com/freeminer/freeminer.git
cd freeminer
```

<sup>Recommended minimum compiler version: `gcc 4.8` or `clang 3.3`</sup>

Build it (GNU/Linux):
```sh
mkdir _build && cd _build
cmake ..
nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)
```

or for debian based:
```sh
curl https://raw.githubusercontent.com/freeminer/freeminer/master/build/debian_ogles.sh | sh
```

Build it (OS X):
```sh
mkdir _build && cd _build
cmake .. -DGETTEXT_LIBRARY=/usr/local/opt/gettext/lib/libgettextlib.dylib -DGETTEXT_INCLUDE_DIR=/usr/local/opt/gettext/include
make -j8 package
```
(if the make command doesn't work on OS X install bsdmake)

Build it (windows):

[vs2013](build/windows_vs2015)

[vs2015](build/windows)


Play it!
```
cd ..
bin/freeminer
```
