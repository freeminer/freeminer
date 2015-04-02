#!/bin/sh

# sudo apt-get install valgrind clang

# to decode stacktraces:
# asan_symbolize < autotest.asannt.err.log | c++filt > autotest.asannt.err.decoded.log

cmake_opt="-DBUILD_SERVER=0"
#cmake_opt="-DBUILD_SERVER=0 -DIRRLICHT_INCLUDE_DIR=~/irrlicht/include -DIRRLICHT_LIBRARY=~/irrlicht/lib/Linux/libIrrlicht.a"

confdir=`pwd`

run_opts="--worldname autotest --port 63000 --go --config $confdir/freeminer.bot.conf --autoexit 1000"
#run_opts="--worldname autotest --port 63000 --go --config $confdir/freeminer.headless.conf --autoexit 1000"

logdir=`pwd`/logs

make="nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)"

clang_version="-3.5"
clang="-DCMAKE_CXX_COMPILER=`which clang++$clang_version` -DCMAKE_C_COMPILER=`which clang$clang_version`"

#run="nice /usr/bin/time --verbose" #linux
#run="nice /usr/bin/time -lp"       #freebsd
run="nice"

cd ../..

rootdir=..
mkdir -p $logdir

mkdir -p worlds/autotest
echo "gameid = default" > worlds/autotest/world.mt
echo "backend = leveldb" >> worlds/autotest/world.mt

name=tsan
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DENABLE_LUAJIT=0 -DSANITIZE_THREAD=1  -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=asannt
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DENABLE_THREADS=0 -DENABLE_LUAJIT=0 -DSANITIZE_ADDRESS=1 -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=asan
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DENABLE_LUAJIT=0 -DSANITIZE_ADDRESS=1 -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=msan
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DSANITIZE_MEMORY=1  -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=debug
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir -DENABLE_LUAJIT=0 -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt

name=valgrind
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run valgrind ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=nothreads
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir -DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=0 -DHAVE_FUTURE=0 -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=normal
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=minetest_proto
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir -DMINETEST_PROTO=1 -DENABLE_LUAJIT=0 -DDEBUG=1 -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1
name=minetest_proto_valgrind
$run valgrind ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..
