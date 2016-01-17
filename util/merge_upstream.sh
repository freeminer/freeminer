#!/bin/sh

cd ..

git remote add upstream https://github.com/freeminer/freeminer.git 
git fetch --all
git pull
git merge --no-edit upstream/master && git push
git submodule update --init --recursive
git status
