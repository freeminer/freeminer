#!/bin/sh

# sudo apt-get install valgrind clang

# to decode stacktraces:
# asan_symbolize < autotest.asannt.err.log | c++filt > autotest.asannt.err.decoded.log

cmake_opt="-DBUILD_SERVER=0"
#cmake_opt="-DBUILD_SERVER=0 -DIRRLICHT_INCLUDE_DIR=~/irrlicht/include -DIRRLICHT_LIBRARY=~/irrlicht/lib/Linux/libIrrlicht.a"

time=600
port=63000

confdir=`pwd`
config=$confdir/freeminer.bot.conf
#config=$confdir/freeminer.headless.conf
world=$confdir/world

run_opts="--world $world --port $port --go --config $config --autoexit $time"

logdir=`pwd`/logs_`date +%Y-%m-%d-%H-%M`

make="nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)"

clang_version="-3.6"
clang="-DCMAKE_CXX_COMPILER=`which clang++$clang_version` -DCMAKE_C_COMPILER=`which clang$clang_version`"
# -DLOCK_PROFILE=1

#run="nice /usr/bin/time --verbose" #linux
#run="nice /usr/bin/time -lp"       #freebsd
run="nice"

cd ../..
mv CMakeCache.txt CMakeCache.txt.backup
mv src/cmake_config.h src/cmake_config.backup

rootdir=..
mkdir -p $logdir

mkdir -p $world
echo "gameid = default" > $world/world.mt
echo "backend = leveldb" >> $world/world.mt

name=tsan
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DENABLE_LUAJIT=0 -DSANITIZE_THREAD=1  -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=tsannt
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DENABLE_LUAJIT=0 -DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=0 -DHAVE_FUTURE=0 -DSANITIZE_THREAD=1  -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
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

name=asannt
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DENABLE_THREADS=0 -DENABLE_LUAJIT=0 -DSANITIZE_ADDRESS=1 -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

if false; then
#useless
name=msan
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DSANITIZE_MEMORY=1  -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \
$run ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..
fi

name=debug
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir -DENABLE_LUAJIT=0 -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1 && \

# too many errors: helgrind
# too slow: drd
# ?: exp-bbv
# usable: memcheck exp-sgcheck exp-dhat   cachegrind callgrind massif exp-bbv
for tool in memcheck ; do
name=valgrind_$tool
echo $name =============
$run valgrind --tool=$tool ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
done;

cd ..

if false; then

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

fi

name=minetest_proto
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir -DMINETEST_PROTO=1 -DENABLE_LUAJIT=0 -DDEBUG=1 -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1
name=minetest_proto_valgrind
$run valgrind ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..
