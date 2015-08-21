#!/bin/sh

# Build a new version of "libs/" folder with dynamic libraries


# Move old folder out of the way and copy unmodified binary to current folder
if [ -d "freeminer.app/Contents/libs" ]; then
  mv freeminer.app/Contents/libs freeminer.app/Contents/libs.old
fi
cp ../freeminer-git/bin/freeminer .

# Run dylibbundler; this copies the libraries and adjusts their paths
dylibbundler/dylibbundler -x freeminer -b -d ./freeminer.app/Contents/libs/ -p @executable_path/../../libs/ -cd

rm freeminer

# dylibbundler does not update libvorbisfile correctly, so fix this here
install_name_tool -change /usr/local/Cellar/libvorbis/1.3.4/lib/libvorbis.0.dylib @executable_path/../../libs/libvorbis.0.dylib freeminer.app/Contents/libs/libvorbisfile.3.dylib

# This command should *not* print any libraries that still point to Homebrew's Cellar
echo "*** The following command should not print anything."
echo "*** If it does, please fix the offending library with install_name_tool!"
cd freeminer.app/Contents/libs
for x in *.dylib ; do otool -L $x|grep Cell; done

