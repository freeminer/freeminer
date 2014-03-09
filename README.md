# Freeminer

[![Build Status](https://travis-ci.org/freeminer/freeminer.png)](https://travis-ci.org/freeminer/freeminer)

Freeminer is an open source sandbox game inspired by [Minecraft](https://minecraft.net/).

Freeminer is based on Minetest which is developed by a [number of contributors](https://github.com/minetest/minetest/graphs/contributors) from all over the globe.

It aims to make the game fun while trading off some bits of perfectionism.

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
Install dependencies. Here's an example for Debian/Ubuntu:
```
sudo apt-get install build-essential libirrlicht-dev cmake libbz2-dev libpng12-dev libjpeg8-dev libfreetype6-dev libxxf86vm-dev libgl1-mesa-dev libsqlite3-dev libvorbis-dev libopenal-dev libcurl4-openssl-dev libluajit-5.1-dev  libleveldb-dev libsnappy-dev libgettextpo0 libmsgpack-dev
```
Download source code:
```
git clone --recursive https://github.com/freeminer/freeminer.git
cd freeminer
```
Build it:
```
mkdir _build && cd _build
cmake .. -DRUN_IN_PLACE=1
make -j4
```
Play it!
```
cd ..
./bin/freeminer
```
