#!/usr/bin/perl
no if $] >= 5.017011, warnings => 'experimental::smartmatch';
use strict;
use 5.16.0;
use Data::Dumper;
use Cwd;
use POSIX ();

sub sy (@);
sub dmp (@);

our $script_path;

BEGIN {
    ($ENV{'SCRIPT_FILENAME'} || $0) =~ m|^(.+)[/\\].+?$|;    #v0w
    ($script_path = (($1 and $1 ne '.') ? $1 : Cwd::cwd()) . '/') =~ tr|\\|/|;
}

our $root_path = $script_path . '../../';
our $config    = {
    logdir        => $script_path . POSIX::strftime("%Y-%m-%dT%H-%M-%S", localtime()),
    clang_version => "-3.6",
    root_prefix   => $root_path . 'auto_',
    root_path     => $root_path,
    runner        => 'nice',
    clients_num   => 5,
    name          => 'bot',
    autoexit      => 600,
    port          => 60001,
    world         => $script_path . 'world',
    config        => $script_path . 'freeminer.bot.conf',                #'auto.conf',
    go            => '--go',
    address       => '::1',
    gameid        => 'default',
    #cmake_add     => '', # '-DIRRLICHT_INCLUDE_DIR=~/irrlicht/include -DIRRLICHT_LIBRARY=~/irrlicht/lib/Linux/libIrrlicht.a',
    #make_add     => '',
    #run_add       => '',
};

our $c = $config;
our $g = {};

our $commands = {
    prepare => sub {
        chdir $c->{root_path};
        rename qw(CMakeCache.txt CMakeCache.txt.backup);
        rename qw(src/cmake_config.h src/cmake_config.backup);
        sy qq{mkdir -p $c->{root_prefix}$g->{build_name} $c->{logdir}};
        chdir "$c->{root_prefix}$g->{build_name}";
        return 0;
    },
    cmake_clang =>
qq{cmake .. -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` -DCMAKE_CXX_COMPILER=`which clang++$c->{clang_version}` -DCMAKE_C_COMPILER=`which clang$c->{clang_version}` $c->{cmake_add}},
    cmake => qq{cmake .. -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $c->{cmake_add}},
    make  => sub {
        sy qq{nice make -j \$(nproc || sysctl -n hw.ncpu || echo 2)  $c->{make_add} >> $c->{logdir}/autotest.$g->{task_name}.make.log 2>&1};
    },
    run_single => sub {
        sy
qq{$c->{runner} @_ ./freeminer --gameid $c->{gameid} --world $c->{world} --port $c->{port} $c->{go} --config $c->{config} --autoexit $c->{autoexit} --logfile $c->{logdir}/autotest.$g->{task_name}.game.log $c->{run_add} >> $c->{logdir}/autotest.$g->{task_name}.out.log 2>>$c->{logdir}/autotest.$g->{task_name}.err.log };
        return 0;
    },
    valgrind => sub {
        commands_run('run_single', "valgrind @_");
    },
    server => sub {
        sy
qq{$c->{runner} @_ ./freeminerserver --gameid $c->{gameid} --world $c->{world} --port $c->{port} --config $c->{config} --autoexit $c->{autoexit} --logfile $c->{logdir}/autotest.$g->{task_name}.game.log $c->{run_add} >> $c->{logdir}/autotest.$g->{task_name}.out.log 2>>$c->{logdir}/autotest.$g->{task_name}.server.err.log &};
    },
    clients => sub {
        sy
qq{$c->{runner} @_ ./freeminer --name $c->{name}$_ --go --address $c->{address} --port $c->{port} --config $c->{config} --autoexit $c->{autoexit} --logfile $c->{logdir}/autotest.$g->{task_name}.game.log $c->{run_add}  >> $c->{logdir}/autotest.$g->{task_name}.out.log 2>>$c->{logdir}/autotest.$g->{task_name}.$c->{name}$_.err.log & }
          for 0 .. $c->{clients_num};
    },
    symbolize => sub {
        sy
qq{asan_symbolize$c->{clang_version} < $c->{logdir}/autotest.$g->{task_name}.err.log | c++filt > $c->{logdir}/autotest.$g->{task_name}.err.symb.log};
    },
    #fail => '',
};

our $tasks = {
    normal => ['prepare', 'cmake',       'make',],
    clang  => ['prepare', 'cmake_clang', 'make',],
    tsan => ['prepare', ['cmake_clang', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DSANITIZE_THREAD=1 -DDEBUG=1)], 'make', 'run_single',],
    asan => [
        'prepare', ['cmake_clang', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DSANITIZE_ADDRESS=1 -DDEBUG=1)], 'make', 'run_single',
        'symbolize',
    ],
    tsannt => [
        'prepare', [
            'cmake_clang',
            qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DSANITIZE_THREAD=1 -DDEBUG=1 -DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=0 -DHAVE_FUTURE=0)
        ],
        'make',
        'run_single',
    ],
    msan => [
        'prepare', ['cmake_clang', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DSANITIZE_MEMORY=1 -DDEBUG=1)], 'make', 'run_single', 'symbolize',
    ],
    debug => ['prepare', ['cmake_clang', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DDEBUG=1)], 'make', 'run_single',],
    nothreads => [
        'prepare',
        ['cmake_clang', qw(-DBUILD_SERVER=0 -DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=0 -DHAVE_FUTURE=0 )], 'make', 'run_single',
    ], (
        map {
            'valgrind_'
              . $_ => [
                {build_name => 'debug'}, 'prepare', ['cmake', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DDEBUG=1)], 'make',
                ['valgrind', '--tool=' . $_],
              ],
        } qw(memcheck exp-sgcheck exp-dhat   cachegrind callgrind massif exp-bbv)
    ),

    minetest => ['prepare', ['cmake', qw(-DBUILD_SERVER=0 -DMINETEST_PROTO=1)], 'make', 'run_single',],
    stress => [{build_name => 'normal'}, 'prepare', 'cmake', 'make', 'server', 'clients',],
    ui => [sub { return 1 if $config->{all_run}; local $config->{go} = undef; task_run('valgrind_memcheck'); }],
    debug_mapgen => [{build_name => 'debug'}, sub { local $config->{world} = "$c->{logdir}/world_$g->{task_name}"; task_run('debug'); }],
};

sub dmp (@) { say +(join ' ', (caller)[0 .. 5]), ' ', Data::Dumper::Dumper \@_ }

sub sy (@) {
    my $cmd = join ' ', @_;
    say 'running ', $cmd;
    system $cmd;
    #dmp 'system', @_;
    if ($? == -1) {
        print "failed to execute: $!\n";
        return $?;
    } elsif ($? & 127) {
        printf "child died with signal %d, %s coredump\n", ($? & 127), ($? & 128) ? 'with' : 'without';
        return $?;
    } else {
        return $? >> 8;
    }
}

sub array (@) {
    local @_ = map { ref $_ eq 'ARRAY' ? @$_ : $_ } (@_ == 1 and !defined $_[0]) ? () : @_;
    wantarray ? @_ : \@_;
}

sub command_run(@) {
    my $cmd = shift;
    if ('CODE' eq ref $cmd) {
        return $cmd->(@_);
    } elsif ('HASH' eq ref $cmd) {
        for my $k (keys %$cmd) {
            if ($k =~ /^-(.+)/) {
                $config->{$1} = $cmd->{$k};
            } else {
                $g->{$k} = $cmd->{$k};
            }
        }
    } elsif ($cmd) {
        return sy $cmd, @_;
    } else {
        dmp 'no cmd', $cmd;
    }
}

sub commands_run(@) {
    my $name = shift;
    if ('ARRAY' eq ref $commands->{$name}) {
        for (@{$commands->{$name}}) {
            command_run $_, @_;
        }
    } elsif ($commands->{$name}) {
        return command_run $commands->{$name}, @_;
    } elsif (ref $name) {
        return command_run $name;
    } else {
        say 'msg ', $name;
        return 0;
    }
}

sub task_run(@) {
    my $name = shift;
    #say "task run $name";
    if ('ARRAY' eq ref $tasks->{$name}) {
        for my $command (@{$tasks->{$name}}) {
            my $r = commands_run(array $command, @_);
            if ($r) {
                warn("command returned $r");
                commands_run('fail');
                return 1;
            }
        }
    } elsif ('CODE' eq ref $tasks->{$name}) {
        return $tasks->{$name}->(@_);
    }
}

sub task_start(@) {
    my $name = shift;
    say "task start $name";
    $g = {task_name => $name, build_name => $name,};
    task_run($name, @_);
}

map { /-*(\w+)(?:=(.*))/ and $config->{$1} = $2; } grep {/^-/} @ARGV;

my $task_run = @ARGV ? [grep { !/^-/ } @ARGV] : [qw(tsan asan tsannt minetest valgrind_memcheck valgrind_massif)];
if ($task_run->[0] ~~ 'all') {
    $task_run = [sort keys %$tasks];
    $config->{all_run} = 1;
}

unless (@ARGV) {
    say "possible tasks:";
    say for sort keys %$tasks;
    say "but running default list: ", join ' ', @$task_run;
    say '';
}

for my $task (@$task_run) {
    warn "task failed [$task]" if task_start($task);
}
