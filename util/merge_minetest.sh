#!/bin/sh

cd ..

git remote add minetest https://github.com/minetest/minetest.git
git fetch --all
git pull
git merge --no-edit minetest/master | grep -v "Conflict" && git push
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
git commit -a -m "up"
git merge --no-edit minetest_game/master && git push

cd ..
git commit -a -m "up"
git push

git status
