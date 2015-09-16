#!/usr/bin/perl

# install:
# sudo apt-get install valgrind clang

our $help = qq{
#simple task
$0 valgrind_massif

# run one task with headless config
$0 --options_add=headless gdb

# pass options to app
$0 -num_emerge_threads=1 tsan

#run all tasks except interactive
$0 all

#manual play with gdb trace if segfault
$0 play_gdb

#normal play
$0 play
};

no if $] >= 5.017011, warnings => 'experimental::smartmatch';
use strict;
use 5.14.0;
use Data::Dumper;
use Cwd;
use POSIX ();

sub sy (@);
sub dmp (@);

our $signal;
our $script_path;

BEGIN {
    ($ENV{'SCRIPT_FILENAME'} || $0) =~ m|^(.+)[/\\].+?$|;    #v0w
    ($script_path = (($1 and $1 ne '.') ? $1 : Cwd::cwd()) . '/') =~ tr|\\|/|;
}

our $root_path = $script_path . '../../';
my $logdir_add = (@ARGV == 1 and $ARGV[0] =~ /^\w+$/) ? '.' . $ARGV[0] : '';
our $config = {
    address           => '::1',
    port              => 60001,
    clients_num       => 5,
    autoexit          => 600,
    clang_version     => "", # "-3.6",
    root_prefix       => $root_path . 'auto_',
    root_path         => $root_path,
    world             => $script_path . 'world',
    config            => $script_path . 'auto.json',
    logdir            => $script_path . 'logs.' . POSIX::strftime("%Y-%m-%dT%H-%M-%S", localtime()) . $logdir_add,
    runner            => 'nice ',
    name              => 'bot',
    go                => '--go',
    gameid            => 'default',
    tsan_opengl_fix   => 1,
    options_display   => ($ENV{DISPLAY} ? '' : 'headless'),
    options_bot       => 'bot_random',
    cmake_minetest    => '-DMINETEST_PROTO=1',
    cmake_nothreads   => '-DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=0 -DHAVE_FUTURE=0',
    cmake_nothreads_a => '-DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=1 -DHAVE_FUTURE=0',
    valgrind_tools    => [qw(memcheck exp-sgcheck exp-dhat   cachegrind callgrind massif exp-bbv)],
    cgroup            => ($^O ~~ 'linux' ? 1 : undef),
    #cmake_add     => '', # '-DIRRLICHT_INCLUDE_DIR=~/irrlicht/include -DIRRLICHT_LIBRARY=~/irrlicht/lib/Linux/libIrrlicht.a',
    #make_add     => '',
    #run_add       => '',
};

map { /^--(\w+)(?:=(.*))/ and $config->{$1} = $2; } @ARGV;
$config->{cgroup} = '4G' if $config->{cgroup} eq 1;

our $g = {};

our $options = {
    default => {
        name                     => 'autotest',
        enable_sound             => 0,
        autojump                 => 1,
        respawn_auto             => 1,
        disable_anticheat        => 1,
        reconnects               => 10000,
        debug_log_level          => 4,
        enable_mapgen_debug_info => 1,
        profiler_print_interval  => 100000,
        default_game             => $config->{gameid},
    },
    bot_random => {
        random_input       => 1,
        continuous_forward => 1,
    },
    bot_forward => {
        continuous_forward => 1,
    },
    headless => {
        video_driver     => 'null',
        enable_sound     => 0,
        enable_clouds    => 0,
        enable_fog       => 0,
        enable_particles => 0,
        enable_shaders   => 0,
    },
    software => {
        video_driver => 'software',
    },
};

map { /^-(\w+)(?:=(.*))/ and $options->{opt}{$1} = $2; } @ARGV;

our $commands = {
    prepare => sub {
        chdir $config->{root_path};
        rename qw(CMakeCache.txt CMakeCache.txt.backup);
        rename qw(src/cmake_config.h src/cmake_config.backup);
        sy qq{mkdir -p $config->{root_prefix}$g->{build_name} $config->{logdir}};
        chdir "$config->{root_prefix}$g->{build_name}";
        rename $config->{config} => $config->{config} . '.old';
        return 0;
    },
    use_clang => sub {
        $config->{clang_version} = $config->{clang} if $config->{clang} and $config->{clang} ne '1';
        $config->{cmake_compiler} = "-DCMAKE_CXX_COMPILER=`which clang++$config->{clang_version}` -DCMAKE_C_COMPILER=`which clang$config->{clang_version}`";
        return 0;
    },
    cmake => sub {
        commands_run('use_clang',) if $config->{clang};
        sy qq{cmake .. -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd` $config->{cmake_compiler} @_ $config->{cmake_int} $config->{cmake_add} };
    },
    make => sub {
        sy
qq{nice make -j \$(nproc || sysctl -n hw.ncpu || echo 2)  $config->{make_add} >> $config->{logdir}/autotest.$g->{task_name}.make.log 2>&1};
    },
    run_single => sub {
        sy
qq{$config->{runner} @_ ./freeminer --gameid $config->{gameid} --world $config->{world} --port $config->{port} $config->{go} --config $config->{config} --autoexit $config->{autoexit} --logfile $config->{logdir}/autotest.$g->{task_name}.game.log }
          . options_make()
          . qq{$config->{run_add} >> $config->{logdir}/autotest.$g->{task_name}.out.log 2>>$config->{logdir}/autotest.$g->{task_name}.err.log };
        return 0;
    },
    valgrind => sub {
        local $config->{runner} = $config->{runner} . " valgrind @_";
        commands_run('run_single',);
    },
    server => sub {
        sy
qq{$config->{runner} @_ ./freeminerserver --gameid $config->{gameid} --world $config->{world} --port $config->{port} --config $config->{config} --autoexit $config->{autoexit} --logfile $config->{logdir}/autotest.$g->{task_name}.game.log }
          . options_make()
          . qq{ $config->{run_add} >> $config->{logdir}/autotest.$g->{task_name}.out.log 2>>$config->{logdir}/autotest.$g->{task_name}.server.err.log &};
    },
    clients => sub {
        sy
qq{$config->{runner} @_ ./freeminer --name $config->{name}$_ --go --address $config->{address} --port $config->{port} --config $config->{config} --autoexit $config->{autoexit} --logfile $config->{logdir}/autotest.$g->{task_name}.game.log }
          . options_make()
          . qq{ $config->{run_add}  >> $config->{logdir}/autotest.$g->{task_name}.out.log 2>>$config->{logdir}/autotest.$g->{task_name}.$config->{name}$_.err.log & }
          for 0 .. $config->{clients_num};
    },
    symbolize => sub {
        sy
qq{asan_symbolize$config->{clang_version} < $config->{logdir}/autotest.$g->{task_name}.err.log | c++filt > $config->{logdir}/autotest.$g->{task_name}.err.symb.log};
    },
    cgroup => sub {
        return 0 unless $config->{cgroup};
        sy
qq(sudo sh -c "mkdir /sys/fs/cgroup/memory/0; echo $$ > /sys/fs/cgroup/memory/0/tasks; echo $config->{cgroup} > /sys/fs/cgroup/memory/0/memory.limit_in_bytes");
    },
    fail => sub {
        warn 'fail:', join ' ', @_;
    },
};

our $tasks = {
    build_normal => [{build_name => 'normal'}, 'prepare', 'cmake', 'make',],
    build_debug => [{build_name => 'debug'}, 'prepare', ['cmake', qw(-DENABLE_LUAJIT=0 -DDEBUG=1)], 'make',],
    build_nothreads => [{build_name => 'nothreads'}, 'prepare', ['cmake', $config->{cmake_nothreads}], 'make',],
    run_single => ['run_single'],
    clang => ['prepare', 'use_clang', 'cmake', 'make',],
    tsan  => [
        'prepare', 'use_clang',
        ['cmake', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DSANITIZE_THREAD=1 -DDEBUG=1)],
        'make', 'cgroup',
        sub {
            local $config->{options_display} = 'software' if $config->{tsan_opengl_fix} and !$config->{options_display};
            local $config->{runner} = $config->{runner} . " env TSAN_OPTIONS=second_deadlock_stack=1 ";
            commands_run('run_single');
        },
    ],
    tsannt => sub {
        local $config->{cmake_int} = $config->{cmake_int} . $config->{cmake_nothreads};
        task_run('tsan');
    },
    tsannta => sub {
        local $config->{cmake_int} = $config->{cmake_int} . $config->{cmake_nothreads_a};
        task_run('tsan');
    },
    asan => [
        'prepare', 'use_clang', ['cmake', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DSANITIZE_ADDRESS=1 -DDEBUG=1)], 'make', 'run_single',
        'symbolize',
    ],
    asannta => sub {
        local $config->{cmake_int} = $config->{cmake_int} . $config->{cmake_nothreads_a};
        task_run('asan');
    },
    msan => [
        'prepare', 'use_clang', ['cmake', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DSANITIZE_MEMORY=1 -DDEBUG=1)], 'make', 'run_single', 'symbolize',
    ],
    debug => ['prepare', ['cmake', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DDEBUG=1)], 'make', 'run_single',],
    nothreads => [\'build_nothreads', 'run_single',],    #'
    (
        map {
            'valgrind_' . $_ => [
                #{build_name => 'debug'}, 'prepare', ['cmake', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DDEBUG=1)], 'make',
                \'build_debug',                          #'
                ['valgrind', '--tool=' . $_],
              ],
        } @{$config->{valgrind_tools}}
    ),

    minetest => ['prepare', ['cmake', $config->{cmake_minetest}], 'make', 'run_single',],
    minetest_tsan   => sub { local $config->{cmake_int} = $config->{cmake_int} . $config->{cmake_minetest}; task_run('tsan'); },
    minetest_tsannt => sub { local $config->{cmake_int} = $config->{cmake_int} . $config->{cmake_minetest}; task_run('tsannt'); },
    minetest_asan   => sub { local $config->{cmake_int} = $config->{cmake_int} . $config->{cmake_minetest}; task_run('asan'); },
    stress => [{build_name => 'normal'}, 'prepare', 'cmake', 'make', 'server', 'clients',],
    debug_mapgen =>
      [{build_name => 'debug'}, sub { local $config->{world} = "$config->{logdir}/world_$g->{task_name}"; task_run('debug'); }],
    gdb => [
        {build_name => 'debug'},
        sub {
            local $config->{runner} = $config->{runner} . q{gdb -ex 'run' -ex 't a a bt' -ex 'cont' -ex 'quit' --args };
            task_run('debug');
          }
    ],

    play_task => sub { return 1 if $config->{all_run}; local $config->{go} = undef; local $config->{options_bot} = undef; local $config->{autoexit} = undef; task_run($_) for @_; },

    (
        map { 'play_' . $_ => [[\'play_task', $_]] } qw(gdb tsan asan msan asannta nothreads minetest),
        map { 'valgrind_' . $_ } @{$config->{valgrind_tools}}
    ),    #'
    play => [[\'play_task', 'build_normal', 'run_single']],    #'
};

sub dmp (@) { say +(join ' ', (caller)[0 .. 5]), ' ', Data::Dumper::Dumper \@_ }

sub sy (@) {
    my $cmd = join ' ', @_;
    say 'running ', $cmd;
    system $cmd;
    #dmp 'system', @_;
    if ($? == -1) {
        say "failed to execute: $!";
        return $?;
    } elsif ($? & 127) {
        $signal = $? & 127;
        say "child died with signal ", ($signal), ", " . (($? & 128) ? 'with' : 'without') . " coredump";
        return $?;
    } else {
        return $? >> 8;
    }
}

sub array (@) {
    local @_ = map { ref $_ eq 'ARRAY' ? @$_ : $_ } (@_ == 1 and !defined $_[0]) ? () : @_;
    wantarray ? @_ : \@_;
}

sub options_make(@) {
    my $r;
    @_ = ('default', $config->{options_display}, $config->{options_bot}, $config->{options_add}, 'opt') unless @_;
    for my $name (array @_) {
        $r->{$_} = $options->{$name}{$_} for keys %{$options->{$name}};
    }
    return join ' ', map {"-$_=$r->{$_}"} sort keys %$r;
}

sub command_run(@) {
    my $cmd = shift;
    #say "command_run $cmd";
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
    #say "commands_run $name";
    if ('SCALAR' eq ref $name) {
        task_run($$name, @_);
    } elsif ('ARRAY' eq ref $commands->{$name}) {
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
                commands_run('fail', $name, array $command, @_);
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

my $task_run = [grep { !/^-/ } @ARGV];
$task_run = [qw(tsan asan tsannt tsannta valgrind_memcheck minetest_tsan minetest_tsannt minetest_asan)] unless @$task_run;
if ('all' ~~ $task_run) {
    $task_run = [sort keys %$tasks];
    $config->{all_run} = 1;
}

unless (@ARGV) {
    say $help;
    say "possible tasks:";
    say for sort keys %$tasks;
    say "\n but running default list: ", join ' ', @$task_run;
    say '';
    sleep 1;
}

for my $task (@$task_run) {
    warn "task failed [$task]" if task_start($task);
    last if $signal ~~ [2, 3];
}
