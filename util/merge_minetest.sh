#!/bin/sh

cd ..

git remote add minetest https://github.com/minetest/minetest.git
git fetch --all
git pull
( git merge --no-edit minetest/master | grep -i "conflict" ) && exit
#git submodule foreach --recursive "git pull||true"

cd games

cd pixture
git checkout master
git pull
cd ..

cd default
git checkout master
git pull
git remote add minetest_game https://github.com/minetest/minetest_game.git
git fetch --all
git submodule foreach --recursive "git checkout master; git pull||true"
git commit -a -m "update submodules"
( git merge --no-edit minetest_game/master | grep -i "conflict" ) && exit
git push

cd ..
git commit -a -m "update submodules"
git push

git status
