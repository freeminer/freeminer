#!/bin/sh

# Create an updated ZIP file from freeminer.app folder

cd freeminer-git
gitver=`git log -1 --format='%cd.%h' --date=short | tr -d -`
cd ../releases

# Compress app bundle as a ZIP file
fname=freeminer-osx-bin-$gitver.zip
rm -f $fname
zip -9 -r $fname freeminer.app

