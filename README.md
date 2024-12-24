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

- **openSUSE**: https://software.opensuse.org/package/freeminer

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
sudo apt-get install build-essential libirrlicht-dev cmake libbz2-dev libgettextpo0 \
libsqlite3-dev libleveldb-dev libsnappy-dev libcurl4-openssl-dev libluajit-5.1-dev libgmp-dev libspatialindex-dev libboost-system-dev \
libpng12-dev libjpeg-dev libfreetype6-dev libxxf86vm-dev libgl1-mesa-dev libvorbis-dev libopenal-dev
# First  - needed always
# Second - recommended for servers and singleplayer
# Third  - needed for client only

# optional:
sudo apt-get install libhiredis-dev cmake-curses-gui
```
___
Fedora:
```sh
# the first five is the closest to Debian/Ubuntu build-essential
sudo yum install make automake gcc gcc-c++ kernel-devel cmake \
irrlicht-devel bzip2-libs libpng-devel libjpeg-turbo-devel freetype-devel \
libXxf86vm-devel mesa-libGL-devel sqlite-devel libvorbis-devel \
openal-soft-devel libcurl-devel luajit-devel leveldb-devel snappy-devel \
gettext-devel msgpack msgpack-devel spatialindex-devel bzip2-devel
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
mkdir -p build && cd build
cmake ..
nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)
```

or for debian based:
```sh
curl https://raw.githubusercontent.com/freeminer/freeminer/master/build_tools/build.sh | sh
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





Luanti (formerly Minetest)
==========================

![Build Status](https://github.com/minetest/minetest/workflows/build/badge.svg)
[![Translation status](https://hosted.weblate.org/widgets/minetest/-/svg-badge.svg)](https://hosted.weblate.org/engage/minetest/?utm_source=widget)
[![License](https://img.shields.io/badge/license-LGPLv2.1%2B-blue.svg)](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html)

Luanti is a free open-source voxel game engine with easy modding and game creation.

Copyright (C) 2010-2024 Perttu Ahola <celeron55@gmail.com>
and contributors (see source file comments and the version control log)

Table of Contents
------------------

1. [Further Documentation](#further-documentation)
2. [Default Controls](#default-controls)
3. [Paths](#paths)
4. [Configuration File](#configuration-file)
5. [Command-line Options](#command-line-options)
6. [Compiling](#compiling)
7. [Docker](#docker)
8. [Version Scheme](#version-scheme)


Further documentation
----------------------
- Website: https://www.minetest.net/
- Wiki: https://wiki.minetest.net/
- Forum: https://forum.luanti.org/
- GitHub: https://github.com/minetest/minetest/
- [Developer documentation](doc/developing/)
- [doc/](doc/) directory of source distribution

Default controls
----------------
All controls are re-bindable using settings.
Some can be changed in the key config dialog in the settings tab.

| Button                        | Action                                                         |
|-------------------------------|----------------------------------------------------------------|
| Move mouse                    | Look around                                                    |
| W, A, S, D                    | Move                                                           |
| Space                         | Jump/move up                                                   |
| Shift                         | Sneak/move down                                                |
| Q                             | Drop itemstack                                                 |
| Shift + Q                     | Drop single item                                               |
| Left mouse button             | Dig/punch/use                                                  |
| Right mouse button            | Place/use                                                      |
| Shift + right mouse button    | Build (without using)                                          |
| I                             | Inventory menu                                                 |
| Mouse wheel                   | Select item                                                    |
| 0-9                           | Select item                                                    |
| Z                             | Zoom (needs zoom privilege)                                    |
| T                             | Chat                                                           |
| /                             | Command                                                        |
| Esc                           | Pause menu/abort/exit (pauses only singleplayer game)          |
| +                             | Increase view range                                            |
| -                             | Decrease view range                                            |
| K                             | Enable/disable fly mode (needs fly privilege)                  |
| J                             | Enable/disable fast mode (needs fast privilege)                |
| H                             | Enable/disable noclip mode (needs noclip privilege)            |
| E                             | Aux1 (Move fast in fast mode. Games may add special features)  |
| C                             | Cycle through camera modes                                     |
| V                             | Cycle through minimap modes                                    |
| Shift + V                     | Change minimap orientation                                     |
| F1                            | Hide/show HUD                                                  |
| F2                            | Hide/show chat                                                 |
| F3                            | Disable/enable fog                                             |
| F4                            | Disable/enable camera update (Mapblocks are not updated anymore when disabled, disabled in release builds)  |
| F5                            | Cycle through debug information screens                        |
| F6                            | Cycle through profiler info screens                            |
| F10                           | Show/hide console                                              |
| F12                           | Take screenshot                                                |

Paths
-----
Locations:

* `bin`   - Compiled binaries
* `share` - Distributed read-only data
* `user`  - User-created modifiable data

Where each location is on each platform:

* Windows .zip / RUN_IN_PLACE source:
    * `bin`   = `bin`
    * `share` = `.`
    * `user`  = `.`
* Windows installed:
    * `bin`   = `C:\Program Files\Minetest\bin (Depends on the install location)`
    * `share` = `C:\Program Files\Minetest (Depends on the install location)`
    * `user`  = `%APPDATA%\Minetest` or `%MINETEST_USER_PATH%`
* Linux installed:
    * `bin`   = `/usr/bin`
    * `share` = `/usr/share/minetest`
    * `user`  = `~/.minetest` or `$MINETEST_USER_PATH`
* macOS:
    * `bin`   = `Contents/MacOS`
    * `share` = `Contents/Resources`
    * `user`  = `Contents/User` or `~/Library/Application Support/minetest` or `$MINETEST_USER_PATH`

Worlds can be found as separate folders in: `user/worlds/`

Configuration file
------------------
- Default location:
    `user/minetest.conf`
- This file is created by closing Luanti for the first time.
- A specific file can be specified on the command line:
    `--config <path-to-file>`
- A run-in-place build will look for the configuration file in
    `location_of_exe/../minetest.conf` and also `location_of_exe/../../minetest.conf`

Command-line options
--------------------
- Use `--help`

Compiling
---------

- [Compiling - common information](doc/compiling/README.md)
- [Compiling on GNU/Linux](doc/compiling/linux.md)
- [Compiling on Windows](doc/compiling/windows.md)
- [Compiling on MacOS](doc/compiling/macos.md)

Docker
------

- [Developing minetestserver with Docker](doc/developing/docker.md)
- [Running a server with Docker](doc/docker_server.md)

Version scheme
--------------
We use `major.minor.patch` since 5.0.0-dev. Prior to that we used `0.major.minor`.

- Major is incremented when the release contains breaking changes, all other
numbers are set to 0.
- Minor is incremented when the release contains new non-breaking features,
patch is set to 0.
- Patch is incremented when the release only contains bugfixes and very
minor/trivial features considered necessary.

Since 5.0.0-dev and 0.4.17-dev, the dev notation refers to the next release,
i.e.: 5.0.0-dev is the development version leading to 5.0.0.
Prior to that we used `previous_version-dev`.
