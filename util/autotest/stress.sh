#!/bin/sh

cmake_opt="-DBUILD_SERVER=1 -DBUILD_CLIENT=1"

confdir=`pwd`
port=63001
time=30
run_opts=" --address ::1 --port $port --go --config $confdir/freeminer.headless.conf --autoexit $time"
run_server_opts="--worldname autotest --port $port --config $confdir/freeminer.headless.conf --autoexit $time"

logdir=`pwd`/logs

make="nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)" 

#run="nice /usr/bin/time --verbose" #linux
#run="nice /usr/bin/time -lp"       #freebsd
run="nice"

cd ../..

rootdir=.

mkdir -p $logdir

mkdir -p worlds/autotest
echo "gameid = default" > worlds/autotest/world.mt
echo "backend = leveldb" >> worlds/autotest/world.mt

cmake $rootdir $clang $cmake_opt
$make

name=sserver
echo $name =============
$run ./bin/freeminerserver $run_server_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log &
sleep 3

for i in 1 2 3 4 5 6 7 8 9; do
name=sclient_$i
echo $name =============
$run ./bin/freeminer $run_opts --name a$i --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log &
done;
