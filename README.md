# Freeminer

[![Build Status](https://travis-ci.org/freeminer/freeminer.png)](https://travis-ci.org/freeminer/freeminer)

Freeminer is an open source sandbox game inspired by [Minecraft](https://minecraft.net/).

Freeminer is based on Minetest which is developed by a [number of contributors](https://github.com/minetest/minetest/graphs/contributors) from all over the globe.

It aims to make the game fun while trading off some bits of perfectionism.

## Installing
- Android: Google play: https://play.google.com/store/apps/details?id=org.freeminer.freeminer
- Android: F-droid: https://f-droid.org/repository/browse/?fdid=org.freeminer.freeminer
- Windows: Download from https://github.com/freeminer/freeminer/releases
- Ubuntu: install http://www.ubuntuupdates.org/ppa/getdeb_games  and run: sudo apt-get install freeminer
- Archlinux: yaourt -S freeminer
- Freebsd: cd /usr/ports/games/freeminer-default && sudo make install clean

## Further documentation
- Website: http://freeminer.org/
- Forums: http://forum.freeminer.org/

## Default controls
- `WASD`: move
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

## Compiling
Install dependencies. Here's an example for

Debian/Ubuntu:
```bash
sudo apt-get install build-essential libirrlicht-dev cmake libbz2-dev \
libpng12-dev libjpeg8-dev libfreetype6-dev libxxf86vm-dev libgl1-mesa-dev \
libsqlite3-dev libvorbis-dev libopenal-dev libcurl4-openssl-dev libluajit-5.1-dev \
libleveldb-dev libsnappy-dev libgettextpo0 libmsgpack-dev
# optional:
sudo apt-get install libhiredis-dev cmake-curses-gui
```
___
Fedora:
```bash
# the first five is the closest to Debian/Ubuntu build-essential
sudo yum install make automake gcc gcc-c++ kernel-devel cmake irrlicht-devel \
bzip2-devel libpng-devel libjpeg-turbo-devel freetype-devel libXxf86vm-devel \
mesa-libGL-devel sqlite-devel libvorbis-devel openal-soft-devel libcurl-devel \
luajit-devel leveldb-devel snappy-devel gettext-devel msgpack msgpack-devel
```
___
Arch Linux:
```bash
sudo pacman -S curl irrlicht leveldb libvorbis luajit openal sqlite cmake
# From AUR (using pacaur)
pacaur -Sa msgpack-c-0.5
```
___
Gentoo/Funtoo:
```bash
emerge -av media-libs/libvorbis media-libs/openal dev-games/irrlicht dev-libs/msgpack dev-libs/leveldb
```
<sup>Recommended irrlicht version: `1.8.1`</sup>

Download source code:
```bash
git clone --recursive https://github.com/freeminer/freeminer.git
cd freeminer
```

<sup>Recommended minimum compiler version: `gcc 4.8` or `clang 3.3`</sup>

Build it:
```bash
mkdir _build && cd _build
cmake ..
nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)
```

Play it!
```
cd ..
./bin/freeminer
```
