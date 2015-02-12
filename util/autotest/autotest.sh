#!/bin/sh

# sudo apt-get install valgrind clang

cmake_opt="-DBUILD_SERVER=0"
confdir=`pwd`
run_opts="--worldname autotest --port 63000 --go --config $confdir/freeminer.bot.conf--autoexit 1000"
#run_opts="--worldname autotest --port 63000 --go --config $confdir/freeminer.headless.conf--autoexit 100"
logdir=`pwd`/logs
make="nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)"
clang="-DCMAKE_CXX_COMPILER=`which clang++-3.5` -DCMAKE_C_COMPILER=`which clang-3.5`"
cd ../..

rootdir=..
mkdir -p $logdir

mkdir -p worlds/autotest
echo "gameid = default" > worlds/autotest/world.mt
echo "backend = leveldb" >> worlds/autotest/world.mt

name=asan
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DDISABLE_LUAJIT=1 -DSANITIZE_ADDRESS=1 -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make
nice ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=tsan
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DDISABLE_LUAJIT=1 -DSANITIZE_THREAD=1  -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make
nice ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=msan
mkdir -p _$name && cd _$name
cmake $rootdir $clang -DSANITIZE_MEMORY=1  -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make
nice ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=valgrind
mkdir -p _$name && cd _$name
cmake $rootdir -DDISABLE_LUAJIT=1 -DDEBUG=1  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make
nice valgrind ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=nothreads
mkdir -p _$name && cd _$name
cmake $rootdir -DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=0 -DHAVE_FUTURE=0 -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make
nice ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..

name=normal
mkdir -p _$name && cd _$name
cmake $rootdir -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $cmake_opt
$make
nice ./freeminer $run_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log
cd ..
