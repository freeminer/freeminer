#!/bin/bash
LOCAL_PATH=$(pwd)

rm -rf "$LOCAL_PATH/assets/share/"
mkdir -p "$LOCAL_PATH/assets/share/"

cp -r "$LOCAL_PATH/../../builtin/" "$LOCAL_PATH/assets/share/"
cp -r "$LOCAL_PATH/../../client/" "$LOCAL_PATH/assets/share/"
cp -r "$LOCAL_PATH/../../fonts/" "$LOCAL_PATH/assets/share/"
cp -r "$LOCAL_PATH/../../games/" "$LOCAL_PATH/assets/share/"
cp -r "$LOCAL_PATH/../../textures/" "$LOCAL_PATH/assets/share/"

echo 2 > "$LOCAL_PATH/assets/share/version.txt"
find "$LOCAL_PATH/assets/share/" -type f | wc -l > "$LOCAL_PATH/assets/share/count.txt"
