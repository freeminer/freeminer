#!/usr/bin/perl

# install:
# sudo apt-get install valgrind clang

our $help = qq{
#simple task
$0 valgrind_massif

# run one task with headless config
$0 --options_add=headless gdb

# pass options to app
$0 -num_emerge_threads=1 bot_tsan

#run all tasks except interactive
$0 all

#manual play with gdb trace if segfault
$0 play_gdb

#normal play
$0 play

# run server with debug in gdb
$0 server_gdb

# run server without debug in gdb
$0 server_gdb_nd

# timelapse video
$0 timelapse

$0 stress_tsan  --clients_autoexit=30 --clients_runs=5 --clients_sleep=25 --options_add=headless

$0 --cgroup=10g bot_tsannta --address=192.168.0.1 --port=30005

#if you have installed Intel(R) VTune(TM) Amplifier
$0 play_vtune --vtune_gui=1
$0 bot_vtune --autoexit=60 --vtune_gui=1
$0 bot_vtune --autoexit=60
$0 stress_vtune

};

no if $] >= 5.017011, warnings => 'experimental::smartmatch';
use strict;
use feature qw(say);
use Data::Dumper;
use Cwd;
use POSIX ();

sub sy (@);
sub dmp (@);

our $signal;
our $script_path;

BEGIN {
    ($0) =~ m|^(.+)[/\\].+?$|;    #v0w
    $script_path = $1;
    ($script_path = ($script_path =~ m{^/} ? $script_path . '/' : Cwd::cwd() . '/' . $script_path . '/')) =~ tr|\\|/|;
}

our $root_path = $script_path . '../../';
1 while $root_path =~ s{[^/\.]+/\.\./}{}g;
my @ar = grep { !/^-/ } @ARGV;
my $logdir_add = (@ar == 1 and $ar[0] =~ /^\w+$/) ? '.' . $ar[0] : '';
our $config = {};
our $g = {date => POSIX::strftime("%Y-%m-%dT%H-%M-%S", localtime()),};

sub init_config () {
    $config = {
        #address           => '::1',
        port             => 60001,
        clients_num      => 5,
        autoexit         => 600,
        clang_version    => "",                                                  # "-3.6",
        autotest_dir_rel => 'util/autotest/',
        root_prefix      => $root_path . 'auto',
        root_path        => $root_path,
        date             => $g->{date},
        world            => $script_path . 'world',
        config           => $script_path . 'auto.json',
        logdir           => $script_path . 'logs.' . $g->{date} . $logdir_add,
        screenshot_dir   => 'screenshot.' . $g->{date},
        env              => 'OPENSSL_armcap=0',
        runner           => 'nice ',
        name             => 'bot',
        go               => '--go',
        gameid           => 'default',
        #tsan_opengl_fix   => 1,
        options_display => ($ENV{DISPLAY} ? '' : 'headless'),
        options_bot     => 'bot_random',
        cmake_minetest  => '-DMINETEST_PROTO=1',
        cmake_nothreads => '-DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=0 -DHAVE_FUTURE=0',
        cmake_nothreads_a => '-DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=1 -DHAVE_FUTURE=0',
        valgrind_tools    => [qw(memcheck exp-sgcheck exp-dhat   cachegrind callgrind massif exp-bbv)],
        cgroup            => ($^O ~~ 'linux' ? 1 : undef),
        tee               => '2>&1 | tee -a ',
        run_task          => 'run_single',
        #cmake_add     => '', # '-DIRRLICHT_INCLUDE_DIR=~/irrlicht/include -DIRRLICHT_LIBRARY=~/irrlicht/lib/Linux/libIrrlicht.a',
        #make_add     => '',
        #run_add       => '',
        vtune_amplifier => '~/intel/vtune_amplifier_xe/bin64/',
    };

    map { /^--(\w+)(?:=(.*))/ and $config->{$1} = $2; } @ARGV;
}
init_config();

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
    timelapse => {
        timelapse                   => 1,
        enable_fog                  => 0,
        enable_particles            => 0,
        active_block_range          => 8,
        max_block_generate_distance => 8,
        max_block_send_distance     => 8,
        weather_biome               => 1,
        screenshot_path             => $config->{autotest_dir_rel} . $config->{screenshot_dir},
    },
};

map { /^-(\w+)(?:=(.*))/ and $options->{opt}{$1} = $2; } @ARGV;

our $commands = {
    init => sub { init_config(); 0 },
    prepare => sub {
        $config->{clang_version} = $config->{clang} if $config->{clang} and $config->{clang} ne '1';
        $g->{build_name} .= $config->{clang_version};
        chdir $config->{root_path};
        rename qw(CMakeCache.txt CMakeCache.txt.backup);
        rename qw(src/cmake_config.h src/cmake_config.backup);
        sy qq{mkdir -p $config->{root_prefix}$g->{build_name} $config->{logdir}};
        chdir "$config->{root_prefix}$g->{build_name}";
        rename $config->{config} => $config->{config} . '.old';
        return 0;
    },
    cmake => sub {
        my %D;
        $D{CMAKE_RUNTIME_OUTPUT_DIRECTORY} = "`pwd`";    # -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=`pwd`
        local $config->{cmake_clang} = 1, local $config->{cmake_debug} = 1, $D{SANITIZE_THREAD}  = 1, if $config->{cmake_tsan};
        local $config->{cmake_clang} = 1, local $config->{cmake_debug} = 1, $D{SANITIZE_ADDRESS} = 1, if $config->{cmake_asan};
        local $config->{cmake_clang} = 1, local $config->{cmake_debug} = 1, $D{SANITIZE_MEMORY}  = 1, if $config->{cmake_msan};

        $D{ENABLE_LUAJIT} = 0, $D{DEBUG} = 1 if $config->{cmake_debug};

        $D{CMAKE_C_COMPILER}     = qq{`which clang$config->{clang_version}`},
          $D{CMAKE_CXX_COMPILER} = qq{`which clang++$config->{clang_version}`}
          if $config->{cmake_clang} || $config->{clang};
        $D{BUILD_CLIENT} = (0 + !$config->{no_build_client});
        $D{BUILD_SERVER} = (0 + !$config->{no_build_server});
        #warn 'D=', Data::Dumper::Dumper \%D;
        my $D = join ' ', map { '-D' . $_ . '=' . $D{$_} } sort keys %D;
        sy qq{cmake .. $D @_ $config->{cmake_int} $config->{cmake_add} $config->{tee} $config->{logdir}/autotest.$g->{task_name}.cmake.log};
    },
    make => sub {
        sy
qq{nice make -j \$(nproc || sysctl -n hw.ncpu || echo 2) $config->{make_add} $config->{tee} $config->{logdir}/autotest.$g->{task_name}.make.log};
    },
    run_single => sub {
        my $args = join ' ', map { '--' . $_ . ' ' . $config->{$_} } grep { $config->{$_} } qw(gameid world address port config autoexit);
        sy
qq{$config->{env} $config->{runner} @_ ./freeminer $args $config->{go} --logfile $config->{logdir}/autotest.$g->{task_name}.game.log }
          . options_make()
          . qq{$config->{run_add} $config->{tee} $config->{logdir}/autotest.$g->{task_name}.out.log };
        0;
    },
    run_single_tsan => sub {
        local $config->{options_display}      = 'software' if $config->{tsan_opengl_fix} and !$config->{options_display};
        local $config->{runner}               = $config->{runner} . " env TSAN_OPTIONS=second_deadlock_stack=1 ";
        local $options->{opt}{enable_minimap} = 0;                                                                          # too unsafe
        commands_run($config->{run_task});
    },

    valgrind => sub {
        local $config->{runner} = $config->{runner} . " valgrind @_";
        commands_run($config->{run_task});
    },
    run_server => sub {
        sy
qq{$config->{env} $config->{runner} @_ ./freeminerserver $config->{tee} $config->{logdir}/autotest.$g->{task_name}.server.out.log};
    },
    run_server_auto => sub {
        my $args = join ' ', map { '--' . $_ . ' ' . $config->{$_} } grep { $config->{$_} } qw(gameid world port config autoexit);
        sy qq{$config->{env} $config->{runner} @_ ./freeminerserver $args --logfile $config->{logdir}/autotest.$g->{task_name}.game.log }
          . options_make()
          . qq{ $config->{run_add} $config->{tee} $config->{logdir}/autotest.$g->{task_name}.server.out.log &};
    },
    run_clients => sub {
        for (0 .. ($config->{clients_runs} || 0)) {
            my $autoexit = $config->{clients_autoexit} || $config->{autoexit};
            local $config->{address} = '::1' if not $config->{address};
            my $args = join ' ',
              map { '--' . $_ . ' ' . $config->{$_} } grep { $config->{$_} } qw( address gameid world address port config);
            sy
qq{$config->{env} $config->{runner} @_ ./freeminer $args --name $config->{name}$_ --go --autoexit $autoexit --logfile $config->{logdir}/autotest.$g->{task_name}.game.log }
              . options_make()
              . qq{ $config->{run_add} $config->{tee} $config->{logdir}/autotest.$g->{task_name}.$config->{name}$_.err.log & }
              for 0 .. $config->{clients_num};
            sleep $config->{clients_sleep} || 1;
        }
    },
    symbolize => sub {
        sy
qq{asan_symbolize$config->{clang_version} < $config->{logdir}/autotest.$g->{task_name}.out.log | c++filt > $config->{logdir}/autotest.$g->{task_name}.out.symb.log};
    },
    cgroup => sub {
        return 0 unless $config->{cgroup};
        local $config->{cgroup} = '4G' if $config->{cgroup} eq 1;
        sy
qq(sudo sh -c "mkdir /sys/fs/cgroup/memory/0; echo $$ > /sys/fs/cgroup/memory/0/tasks; echo $config->{cgroup} > /sys/fs/cgroup/memory/0/memory.limit_in_bytes");
    },
    timelapse_video => sub {
        sy
qq{ cat ../$config->{autotest_dir_rel}$config->{screenshot_dir}/*.png | ffmpeg -f image2pipe -i - -vcodec libx264 ../$config->{autotest_dir_rel}timelapse-$config->{date}.mp4 };
    },
    sleep => sub {
        sleep $_[0] || 1;
        0;
    },
    fail => sub {
        warn 'fail:', join ' ', @_;
    },

};

our $tasks = {
    build_normal => [{build_name => '_normal'}, 'prepare', 'cmake', 'make',],
    build_debug => [sub { $g->{build_name} .= '_debug'; 0 }, {-cmake_debug => 1,}, 'prepare', 'cmake', 'make',],
    build_nothreads => [sub { $g->{build_name} .= '_nt'; 0 }, 'prepare', ['cmake', $config->{cmake_nothreads}], 'make',],
    build_server       => [{-no_build_client => 1,}, 'build_normal',],
    build_server_debug => [{-no_build_client => 1,}, 'build_debug',],
    #run_single => ['run_single'],
    clang => ['prepare', {-cmake_clang => 1,}, 'cmake', 'make',],
    build_tsan => [sub { $g->{build_name} .= '_tsan'; 0 }, {-cmake_tsan => 1,}, 'prepare', 'cmake', 'make',],
    bot_tsan   => [{-no_build_server => 1,}, 'build_tsan', 'cgroup', 'run_single_tsan',],
    bot_tsannt => sub {
        $g->{build_name} .= '_nt';
        local $config->{no_build_server} = 1;
        local $config->{cmake_int}       = $config->{cmake_int} . $config->{cmake_nothreads};
        commands_run('bot_tsan');
    },
    bot_tsannta => sub {
        $g->{build_name} .= '_nta';
        local $config->{no_build_server} = 1;
        local $config->{cmake_int}       = $config->{cmake_int} . $config->{cmake_nothreads_a};
        commands_run('bot_tsan');
    },
    build_asan => [
        sub {
            $g->{build_name} .= '_asan';
            0;
        }, {
            -cmake_asan => 1,
            #-env=>'ASAN_OPTIONS=symbolize=1 ASAN_SYMBOLIZER_PATH=llvm-symbolizer$config->{clang_version}',
        },
        'prepare',
        'cmake',
        'make',
    ],
    bot_asan => [
        {-no_build_server => 1,},
        'build_asan',
        $config->{run_task},
        'symbolize',
    ],
    bot_asannta => sub {
        $g->{build_name} .= '_nta';
        local $config->{cmake_int} = $config->{cmake_int} . $config->{cmake_nothreads_a};
        commands_run('bot_asan');
    },
    bot_msan => [
        {build_name => '_msan', -cmake_msan => 1,},
        'prepare',
        'cmake', 'make', $config->{run_task}, 'symbolize',
    ],
    debug     => [{-no_build_server => 1,}, 'build_debug',      $config->{run_task},],
    nothreads => [{-no_build_server => 1,}, \'build_nothreads', $config->{run_task},],    #'
    (
        map {
            'valgrind_' . $_ => [
                {build_name => ''},
                #{build_name => 'debug'}, 'prepare', ['cmake', qw(-DBUILD_SERVER=0 -DENABLE_LUAJIT=0 -DDEBUG=1)], 'make',
                \'build_debug',                                                           #'
                ['valgrind', '--tool=' . $_],
              ],
        } @{$config->{valgrind_tools}}
    ),

    build_minetest => [{build_name => '_minetest',}, 'prepare', {-no_build_server => 1,}, ['cmake', $config->{cmake_minetest}], 'make',],
    bot_minetest      => ['build_minetest', $config->{run_task},],
    bot_minetest_tsan => sub {
        $g->{build_name} .= '_minetest';
        local $config->{no_build_server} = 1;
        local $config->{cmake_int}       = $config->{cmake_int} . $config->{cmake_minetest};
        commands_run('bot_tsan');
    },
    bot_minetest_tsannt => sub {
        $g->{build_name} .= '_minetest';
        local $config->{no_build_server} = 1;
        local $config->{cmake_int}       = $config->{cmake_int} . $config->{cmake_minetest};
        commands_run('bot_tsannt');
    },
    bot_minetest_asan => sub {
        $g->{build_name} .= '_minetest';
        local $config->{no_build_server} = 1;
        local $config->{cmake_int}       = $config->{cmake_int} . $config->{cmake_minetest};
        commands_run('bot_asan');
    },
    #stress => [{ZZbuild_name => 'normal'}, 'prepare', 'cmake', 'make', 'run_server_auto', 'run_clients',],
    stress => sub {
        commands_run($_[0] || 'build_normal');
        for ('run_server_auto', 'run_clients') { my $r = commands_run($_); return $r if $r; }
        return 0;
    },
    #clients     => [{ZZbuild_name => 'normal'}, 'prepare', {-no_build_client => 0, -no_build_server => 1}, 'cmake', 'make', 'run_clients'],
    clients_build => [{build_name => '_normal'}, 'prepare', {-no_build_client => 0, -no_build_server => 1}, 'cmake', 'make'],
    clients_run   => [{build_name => '_normal'}, 'run_clients'],
    clients => ['clients_build', 'clients_run'],

    stress_tsan => [
        {-no_build_client => 1, -no_build_server => 0}, 'build_tsan', 'cgroup',
        'run_server_auto', ['sleep', 10], {build_name => '_normal', -cmake_tsan => 0,}, 'clients',
    ],
    stress_asan => [
        {-no_build_client => 1, -no_build_server => 0}, 'build_asan', 'cgroup',
        'run_server_auto', ['sleep', 10], {build_name => '_normal', -cmake_asan => 0,}, 'clients',
    ],

    stress_massif => [
        'clients_build',
        sub {
            local $config->{run_task} = 'run_server_auto';
            commands_run('valgrind_massif');
        },
        ['sleep', 10],
        'clients_run',
    ],

    stress => ['build_normal', 'run_server_auto', ['sleep', 5], 'clients_run'],

    debug_mapgen => [
        #{build_name => 'debug'},
        sub {
            local $config->{world} = "$config->{logdir}/world_$g->{task_name}";
            commands_run('debug');
          }
    ],
    gdb => sub {
        local $config->{runner} = $config->{runner} . q{gdb -ex 'run' -ex 't a a bt' -ex 'cont' -ex 'quit' --args };
        @_ = ('debug') if !@_;
        for (@_) { my $r = commands_run($_); return $r if $r; }
    },
    server_gdb    => [{-no_build_client => 1,}, 'build_debug',  ['gdb', 'run_server']],
    server_gdb_nd => [{-no_build_client => 1,}, 'build_normal', ['gdb', 'run_server']],

    bot_gdb => [{-no_build_server => 1,}, 'build_debug', ['gdb', 'run_single']],

    vtune => sub {
        local $config->{runner} = $config->{runner} . qq{$config->{vtune_amplifier}amplxe-cl -collect hotspots -r $config->{logdir}/rh0};
        @_ = ('debug') if !@_;
        for (@_) { my $r = commands_run($_); return $r if $r; }
    },
    vtune_report => sub {
        if ($config->{vtune_gui}) {
            sy qq{$config->{vtune_amplifier}amplxe-gui $config->{logdir}/rh0};    # -limit=1000
        } else {
            for my $report (qw(hotspots top-down)) {                              # summary callstacks
                sy
qq{$config->{vtune_amplifier}amplxe-cl -report $report -report-width=250 -report-output=$config->{logdir}/vtune.$report.log -r $config->{logdir}/rh0}
                  ;                                                               # -limit=1000
            }
        }
    },
    bot_vtune => [{-no_build_server => 1,}, 'build_debug', ['vtune', 'run_single'], 'vtune_report'],
    stress_vtune => [
        'build_debug',
        sub {
            commands_run('vtune', 'run_server_auto');
        },
        ['sleep', 10],
        'clients_run',
    ],

    play_task => sub {
        return 1 if $config->{all_run};
        local $config->{no_build_server} = 1;
        local $config->{go}              = undef;
        local $config->{options_bot}     = undef;
        local $config->{autoexit}        = undef;
        for (@_) { my $r = commands_run($_); return $r if $r; }
    },

    (map { 'play_' . $_ => [{-no_build_server => 1,}, [\'play_task', 'bot_'.$_]] } qw(tsan asan msan asannta minetest)),
    (
        map { 'play_' . $_ => [{-no_build_server => 1,}, [\'play_task', $_]] } qw(gdb nothreads vtune),
        map { 'valgrind_' . $_ } @{$config->{valgrind_tools}},
    ),
    play => [{-no_build_server => 1,}, [\'play_task', 'build_normal', $config->{run_task}]],    #'
    timelapse => [{-options_add => 'timelapse',}, \'play', 'timelapse_video'],                  #'
    up => sub {
        my $cwd = Cwd::cwd();
        chdir $config->{root_path};
        sy qq{(git stash && git pull --rebase >&2) | grep -v "No local changes to save" && git stash pop}
          and sy qq{git submodule update --init --recursive};
        chdir $cwd;
        return 0;
    },
};

sub dmp (@) { say +(join ' ', (caller)[0 .. 5]), ' ', Data::Dumper::Dumper \@_ }

sub sy (@) {
    say 'running ', join ' ', @_;
    system @_;
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
        $r->{$_} = $options->{$name}{$_} for sort keys %{$options->{$name}};
    }
    return join ' ', map {"-$_=$r->{$_}"} sort keys %$r;
}

sub command_run(@);

sub command_run(@) {
    my $cmd = shift;
    #say "command_run $cmd ", @_;
    if ('CODE' eq ref $cmd) {
        return $cmd->(@_);
    } elsif ('HASH' eq ref $cmd) {
        for my $k (keys %$cmd) {
            if ($k =~ /^-+(.+)/) {
                $config->{$1} = $cmd->{$k};
            } else {
                $g->{$k} = $cmd->{$k};
            }
        }
    } elsif ('ARRAY' eq ref $cmd) {
        #for (@{$cmd}) {
        my $r = command_run(array $cmd, @_);
        warn("command $_ returned $r"), return $r if $r;
        #}
    } elsif ($cmd) {
        #return sy $cmd, @_;
        return commands_run($cmd, @_);
    } else {
        dmp 'no cmd', $cmd;
    }
}

sub commands_run(@);

sub commands_run(@) {
    my $name = shift;
    #say "commands_run $name ", @_;
    my $c = $commands->{$name} || $tasks->{$name};
    if ('SCALAR' eq ref $name) {
        commands_run($$name, @_);
    } elsif ('ARRAY' eq ref $c) {
        for (@{$c}) {
            my $r = command_run $_, @_;
            warn("command $_ returned $r"), return $r if $r;
        }
    } elsif ($c) {
        return command_run $c, @_;
    } elsif (ref $name) {
        return command_run $name, @_;
    } else {
        say 'msg ', $name;
        return 0;
    }
}

sub task_start(@) {
    my $name = shift;
    $name = $1, unshift @_, $2 if $name =~ /^(.*?)=(.*)$/;
    say "task start $name ", @_;
    #$g = {task_name => $name, build_name => $name,};
    $g->{task_name}  = $name;
    $g->{build_name} = '';
    #task_run($name, @_);
    commands_run($name, @_);
}

my $task_run = [grep { !/^-/ } @ARGV];
$task_run = [qw(bot_tsan bot_asan bot_tsannt bot_tsannta valgrind_memcheck bot_minetest_tsan bot_minetest_tsannt bot_minetest_asan)]
  unless @$task_run;
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
    init_config();
    warn "task failed [$task]" if task_start($task);
    last if $signal ~~ [2, 3];
}
