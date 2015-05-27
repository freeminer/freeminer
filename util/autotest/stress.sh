#!/bin/sh

time=600
clients=10
address=::1
port=63001
cmake_opt="-DBUILD_SERVER=1 -DBUILD_CLIENT=1"

confdir=`pwd`
#config=$confdir/freeminer.bot.conf
config=$confdir/freeminer.headless.conf
world=$confdir/world

run_opts=" --address $address --port $port --go --config $config --autoexit $time"
run_server_opts="--world $world --port $port --config $config --autoexit $time"

logdir=`pwd`/logs_`date +%Y-%m-%d-%H-%M`

make="nice make -j $(nproc || sysctl -n hw.ncpu || echo 2)"

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


name=stress
echo $name =============
mkdir -p _$name && cd _$name
cmake $rootdir -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $clang $cmake_opt
$make >> $logdir/autotest.$name.make.log 2>&1

name=sserver
echo $name =============
$run ./freeminerserver $run_server_opts --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log &
sleep 3

for i in $(seq 1 $clients); do
	name=sclient_$i
	echo $name =============
	$run ./freeminer $run_opts --name a$i --logfile $logdir/autotest.$name.game.log >> $logdir/autotest.$name.out.log 2>>$logdir/autotest.$name.err.log &
	sleep 1
done;

cd ..
