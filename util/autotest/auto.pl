#!/usr/bin/env perl

# install:
# sudo apt install -y clang valgrind google-perftools libgoogle-perftools-dev kcachegrind

our $help = qq{
$0 [-config_variables] [--freeminer_params] [---this_script_params] [---verbose] [----presets] [cmds]

#simple task
$0 valgrind_massif

# run one task with headless config
$0 ----headless gdb

# pass options to app
$0 -num_emerge_threads=1 tsan bot

#run all tasks except interactive
$0 all

#manual play with gdb trace if segfault
$0 gdb go

#normal play
$0 go

#build with latests installed clang and play
$0 ---cmake_clang=1 ---cmake_libcxx=1 go
#build with clang-3.8 and play
$0 ---cmake_clang=-3.8 go

# run server with debug in gdb
$0 gdb server

# run server without debug in gdb
$0 server_gdb_nd

# with periodic profiler
$0 ----headless ----headless_optimize ----info ---clients_num=10 -profiler_print_interval=5 stress

$0 ---clients_autoexit=30 ---clients_runs=5 ---clients_sleep=25 ----headless tsan stress

$0 ---cgroup=10g --address=192.168.0.1 --port=30005 tsan bot

# Maybe some features should be disabled for run some sanitizers
$0 ---cmake_clang=1 -DENABLE_WEBSOCKET=0 -DENABLE_TCMALLOC=0 tsan bot
$0 ---cmake_clang=1 -DENABLE_WEBSOCKET=0                    asan bot
$0 ---cmake_clang=1 -DENABLE_WEBSOCKET=0 ---cmake_leveldb=0 usan bot
$0 ---cmake_clang=1 -DENABLE_TIFF=0                        gperf bot

# debug touchscreen gui. use irrlicht branch ogl-es with touchscreen patch /build/android/irrlicht-touchcount.patch
$0 ---build_name="_touch_asan" ---cmake_touch=1 -touchscreen=0 asan go

# build and use custom leveldb
$0 ---cmake_add="-DLEVELDB_INCLUDE_DIR=../../leveldb/include -DLEVELDB_LIBRARY=../../leveldb/out-static/libleveldb.a"

$0 ---cmake_clang=1 -DUSE_WEBSOCKET=0 ---cmake_leveldb=0 usan bot

# sctp debug
VERBOSE=1 $0 ---cmake_sctp=1 ---cmake_clang=1 ---cmake_add="-DSCTP_DEBUG=1" gdb server
VERBOSE=1 $0 ---cmake_sctp=1 ---cmake_clang=1 --address=localhost --port=60001 ---cmake_add="-DSCTP_DEBUG=1" gdb bot

# build debug
$0 ---verbose -DCMAKE_VERBOSE_MAKEFILE=1 build

#if you have installed Intel(R) VTune(TM) Amplifier
$0 ---vtune_gui=1 play_vtune
$0 --autoexit=60 ---vtune_gui=1 bot_vtune
$0 --autoexit=60 bot_vtune 
$0 stress_vtune

# google-perftools https://github.com/gperftools/gperftools
$0 ---gperf_heapprofile=1 ---gperf_cpuprofile=1 gperf bot
$0 ---gperf_heapprofile=1 ---gperf_cpuprofile=1 ----headless ----headless_optimize ----info ---clients_num=50 -profiler_print_interval=10 stress_gperf

# stress test of flowing liquid
$0 ----world_water

# stress test of falling sand
$0 ----world_sand

# earth
$0 -mg_name=earth -mg_earth='{"center":{"z":36.822183, "y":0, "x":30.583390}}' bot
#$0 -mg_name=earth -mg_earth='{"scale":{"z":10000, "y":0.01, "x":10000}}' bot
$0 -mg_name=earth -mg_earth='{"scale":{"z":10000, "y":100, "x":10000}}' bot

$0 ---cmake_minetest=1 ---build_name=_minetest ----headless ----headless_optimize --address=cool.server.org --port=30001 ---clients_num=25 clients

# timelapse video (every 10 seconds)
$0 -timelapse=10 timelapse
$0 -timelapse=10 -fixed_map_seed=123 ---world_local=1 timelapse_stay
# continue timelapse:
$0 -timelapse=10 ---screenshot_dir=screenshot.2023-08-03T15-52-09 ---world=`pwd`/logs.2023-08-03T15-52-09.timelapse_stay/world timelapse_stay
# remake drom dir
$0 ---screenshot_dir=screenshot.2023-08-03T15-52-09 ---ffmpeg_add_i='-r 120' ---ffmpeg_add_o='-r 120' timelapse_video
#fly
$0 ----server_optimize ----far fly
$0 ----mg_math_tglag ----server_optimize ----far -static_spawnpoint='(10000,30030,-22700)' fly
$0 ----mg_math_tglag ----server_optimize ----far -static_spawnpoint='(24110,24110,-30000)' fly
$0 ----mg_math_tglag ----server_optimize ----far -static_spawnpoint='(24600,30000,0)'
$0 ----mg_math_tglag ----server_optimize ----far -static_spawnpoint='(24100,30000,24100)'
$0 ----fall1 -continuous_forward=1 bot

ASAN_OPTIONS=detect_container_overflow=0 $0 ---cmake_leveldb=0 -DENABLE_SYSTEM_JSONCPP=0 -DENABLE_WEBSOCKET=0 -keymap_toggle_block_bounds=KEY_F9 ----fall2 set_client asan build_client run_single
};

no if $] >= 5.017011, warnings => 'experimental::smartmatch';
no if $] >= 5.038, warnings => 'deprecated::smartmatch';
use strict;
use feature      qw(say);
use Data::Dumper ();
$Data::Dumper::Sortkeys = $Data::Dumper::Useqq = $Data::Dumper::Indent = $Data::Dumper::Terse = 1;
#use JSON;
use Cwd         ();
use POSIX       ();
use Time::HiRes qw(sleep);

sub sy (@);
sub sytee (@);
sub sf (@);
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
my @ar         = grep { !/^-/ } @ARGV;
my $logdir_add = (@ar == 1 and $ar[0] =~ /^\w+$/) ? '.' . $ar[0] : '';
our $config = {};
our $g      = {date => POSIX::strftime("%Y-%m-%dT%H-%M-%S", localtime()),};
my $task_run = [grep { !/^-/ } @ARGV];

sub init_config () {
    say "Config reset $g->{keep_config}" if $config->{verbose};
    $config = {
        # address           => '::1',
        autoexit         => 600,
        autotest_dir_rel => 'util/autotest/',
        build_name       => '',
        build_prefix     => 'build',
        cache_clear      => 0,                              # remove cache dir before start client
        cgroup           => ($^O eq 'linux' ? 1 : undef),
        clang_version    => `bash -c "compgen -c clang | grep 'clang[-]*[[:digit:]]' | sort --version-sort --reverse | head -n1"` =~
          s/(?:^clang)|(?:\s+$)//rg,                        #"" "-3.6" "15"
        clients_num   => 5,
        clients_start => 0,
        # cmake_add     => '', # '-DIRRLICHT_INCLUDE_DIR=~/irrlicht/include -DIRRLICHT_LIBRARY=~/irrlicht/lib/Linux/libIrrlicht.a',
        cmake_leveldb     => undef,
        cmake_minetest    => undef,
        cmake_nothreads   => '-DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=0 -DHAVE_FUTURE=0',
        cmake_nothreads_a => '-DENABLE_THREADS=0 -DHAVE_THREAD_LOCAL=1 -DHAVE_FUTURE=0',
        cmake_opts        => [qw(CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER)],
        cmake_san_debug   => 1,
        config            => $script_path . 'auto.json',
        date              => $g->{date},
        env               => 'OPENSSL_armcap=0',
        executable_name   => 'freeminer',
        gameid            => 'default',
        gdb               => `bash -c 'compgen -c gdb' | grep 'gdb[-]*[[:digit:]]*\$' | sort --version-sort --reverse | head -n1` =~
          s/\s+$//rg,    # 'gdb' 'gdb112'
        gdb_stay   => 0,                                                   # dont exit from gdb
        go         => '--go',
        gperf_mode => '--text',
        logdir     => $script_path . 'logs.' . $g->{date} . $logdir_add,
        # make_add     => '',
        makej           => '$(nproc || sysctl -n hw.ncpu || echo 2)',
        name            => 'bot',
        options_display => ($ENV{DISPLAY} ? '' : 'headless'),
        port            => 60001,
        root_path       => $root_path,
        root_prefix     => $root_path,
        # run_add       => '',
        run_task         => 'run_single',
        runner           => 'nice ',
        screenshot_dir   => 'screenshot.' . $g->{date},
        tee              => '2>&1 | tee -a ',
        #tsan_leveldb_fix => 1,
        #tsan_opengl_fix  => 1,
        valgrind_tools   => [qw(memcheck exp-sgcheck exp-dhat   cachegrind callgrind massif exp-bbv)],
        # verbose         => 1,
        vtune_amplifier => '~/intel/vtune_amplifier_xe/bin64/',
        vtune_collect   => 'hotspots',                            # for full list: ~/intel/vtune_amplifier_xe/bin64/amplxe-cl -help collect
        world_clear     => 0,                                     # remove old world before start client
        pid_path        => '/tmp/',
    };

    map { /^---(\w+)(?:=(.*))?/  and $config->{$1} = defined $2 ? $2 : 1; } @ARGV;
    map { /^----(\w+)(?:=(.*))?/ and push @{$config->{options_arr}}, $1; } @ARGV;
    map { /^-D(\w+)(?:=(.*))?/   and $config->{cmake_opt}{$1} = defined $2 ? $2 : 1; } @ARGV;
}
init_config();

our $options = {
    default => {
        name                    => 'autotest',
        enable_sound            => 0,
        autojump                => 1,
        respawn_auto            => 1,
        disable_anticheat       => 1,
        reconnects              => 10000,
        profiler_print_interval => 10,
        default_game            => $config->{gameid},
        max_users               => 4000,
        show_basic_debug        => 1,
        show_profiler_graph     => 1,
        profiler_max_page       => 1,
        profiler_page           => 1,
        debug_log_level         => 'info',
        movement_speed_fast     => 10000,
        max_block_send_distance => 100,
        default_privs           => 'interact, shout, teleport, settime, privs, fly, noclip, fast, debug',
        default_privs_creative  => 'interact, shout, teleport, settime, privs, fly, noclip, fast, debug',
    },
    no_exit => {
        '--autoexit' => 0,
    },
    info => {
        '--info' => 1,
    },
    verbose => {
        #debug_log_level          => 'verbose',
        '--verbose' => 1,
        #enable_mapgen_debug_info => 1,
    },
    bot => {
        fps_max           => 10,
        fps_max_unfocused => 10,
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
    headless_optimize => {
        fps_max           => 5,
        fps_max_unfocused => 5,
        headless_optimize => 1,
        viewing_range     => 32,
    },
    software => {
        video_driver => 'software',
    },
    timelapse => {
        timelapse                   => 10,
        enable_fog                  => 0,
        enable_particles            => 0,
        active_block_range          => 8,
        max_block_generate_distance => 8,
        max_block_send_distance     => 8,
        weather_biome               => 1,
        farmesh                     => 0,
        enable_dynamic_shadows      => 0,
        enable_clouds               => 0,
        enable_waving_water         => 0,
        enable_waving_leaves        => 0,
        enable_waving_plants        => 0,
        screenshot_path             => $config->{autotest_dir_rel} . $config->{screenshot_dir},
    },
    world_water => {
        '--world' => $script_path . 'world_water',
        mg_name   => 'math',
        mg_params => {"layers"    => [{"name" => "default:water_source"}]},
        mg_math   => {"generator" => "mengersponge"},
    },
    world_sand => {
        '--world' => $script_path . 'world_sand',
        mg_name   => 'math',
        mg_params => {"layers"    => [{"name" => "default:sand"}]},
        mg_math   => {"generator" => "mengersponge"},
    },
    world_torch => {
        '--world' => $script_path . 'world_torch',
        mg_params => {"layers" => [{"name" => "default:torch"}, {"name" => "default:glass"}]},
    },
    world_rooms => {
        '--world' => $script_path . 'world_rooms',
        mg_name   => 'math',
        mg_math   => {"generator" => "rooms"},
    },
    mg_math_tglag => {
        '--world'         => $script_path . 'world_math_tglad',
        mg_name           => 'math',
        mg_math           => {"N" => 30, "generator" => "tglad", "mandelbox_scale" => 1.5, "scale" => 0.000333333333,},
        static_spawnpoint => '(30010,30010,-30010)',
        mg_flags          => '',                                                                                          # "trees",
    },
    fall1 => {
        '--world'         => $script_path . 'world_fall1',
        mg_name           => 'math',
        mg_math           => {"generator" => "menger_sponge"},
        static_spawnpoint => '(-70,20020,-190)',
        mg_flags          => '',                                                                                          # "trees",
    },
    fall3 => {
        '--world'         => $script_path . 'world_fall3',
        mg_name           => 'math',
        mg_math           => {"generator" => "aexion", "N" => 11,"cadd" => -1.0002,"size" => 30000 },
        water_level       => -30000,
        static_spawnpoint => '(10,10,10)',
    },
    far => {
        max_block_generate_distance => 50,
        max_block_send_distance     => 50,
        viewing_range               => 50 * 16,
    },
    server_optimize => {
        chunksize                  => 3,
        active_block_range         => 1,
        weather                    => 0,
        abm_interval               => 20,
        nodetimer_interval         => 20,
        active_block_mgmt_interval => 20,
        server_occlusion           => 0,
    },
    client_optimize => {
        viewing_range => 15,
    },
    creative => {
        default_privs_creative => 'interact,shout,fly,fast,noclip',
        #default_privs => 'interact,shout,fly,fast,noclip',
        creative_mode => 1,
        free_move     => 1,
        noclip        => 1,
        enable_damage => 0,
    },
    forward => {
        continuous_forward => 1,
    },
    fly => {
        crosshair_alpha => 0,
        #time_speed         => 0,
        enable_minimap    => 0,
        random_input      => 0,
        static_spawnpoint => '(0,50,0)',
        creative_mode     => 1,
        enable_damage     => 0,
        free_move         => 1,
    },
    fps1 => {
        fps_max           => 2,
        fps_max_unfocused => 2,
        viewing_range     => 1000,
        wanted_fps        => 1,
    },
    stay => {
        continuous_forward => 0,
    },
    fast => {
        fast_move => 1, movement_speed_fast => 30,
    },
    unload => {server_unload_unused_data_timeout => 20, client_unload_unused_data_timeout => 15,},
};
$options->{fall2} = { %{$options->{fall1}}, static_spawnpoint => '(10,21000,10)',};

map { /^-([^-][^=]+)(?:=(.*))?/  and $options->{opt}{$1}  = $2; } @ARGV;
map { /^--([^-][^=]+)(?:=(.*))?/ and $options->{pass}{$1} = $2; } @ARGV;

my $child;

our $commands;
$commands = {
    build_name => sub {
        join '_', $g->{build_name},
          (map { $config->{build_names}{$_} } sort keys %{$config->{build_names}}),
          (map { $g->{build_names}{$_} } sort keys %{$g->{build_names}});
    },
    env => sub {
        'env ' . join ' ', $config->{env}, map { $config->{envs}{$_} } sort keys %{$config->{envs}};
    },
    build_dir  => sub { "$config->{root_prefix}$config->{build_prefix}" . $commands->{build_name}() },
    executable => sub { $commands->{build_dir}() . "/" . $config->{executable_name} },
    world_name => sub {
        return $config->{world} if defined $config->{world};
        my $name = 'world';
        $name .= '_' . $options->{opt}{mg_name}                   if $options->{opt}{mg_name};
        $name .= '_' . $config->{config_pass}{mg_math}{generator} if $config->{config_pass}{mg_math} && $config->{config_pass}{mg_math}{generator};
        $config->{world} = $script_path . $name;
        $config->{world} = $config->{logdir} . '/' . $name if $config->{world_local};
        $config->{world};
    },

    init          => sub { init_config(); 0 },
    '---'         => 'init',
    cmake_prepare => sub {
        $config->{cmake_clang} //= 1 if $config->{clang_version};
        $config->{clang_version} = $config->{cmake_clang} if $config->{cmake_clang} and $config->{cmake_clang} ne '1';
        $config->{cmake_libcxx} //= 1                     if $config->{cmake_clang};
        $g->{build_names}{x_clang} = $config->{clang_version} if $config->{cmake_clang};
        my $build_dir = $commands->{build_dir}();
        chdir $config->{root_path};
        rename qw(CMakeCache.txt CMakeCache.txt.backup);
        rename qw(src/cmake_config.h src/cmake_config.backup);
        sy qq{mkdir -p $build_dir $config->{logdir}};
        file_append(
            "$config->{logdir}/run.sh",
            join "\n",
            qq{# } . join(' ', $0, map { /[\s"]/ ? "'" . $_ . "'" : $_ } @ARGV),
            qq{cd "$build_dir"},
            ""
        );
        chdir $build_dir;
        rename $config->{config} => $config->{config} . '.old';
        return 0;
    },
    cmake => sub {
        return if $config->{no_cmake};
        my $r = commands_run('cmake_prepare');
        return $r if $r;
        my %D;
        #$D{CMAKE_RUNTIME_OUTPUT_DIRECTORY} = "`pwd`";
    	local $config->{cmake_debug} = $config->{cmake_san_debug}, $D{SANITIZE_THREAD}  = 1, if $config->{cmake_tsan};
	    local $config->{cmake_debug} = $config->{cmake_san_debug}, $D{SANITIZE_ADDRESS} = 1, if $config->{cmake_asan};
	    local $config->{cmake_debug} = $config->{cmake_san_debug}, $D{SANITIZE_MEMORY}  = 1, if $config->{cmake_msan};
	    local $config->{cmake_debug} = $config->{cmake_san_debug}, local $config->{keep_luajit} = 1, $D{SANITIZE_UNDEFINED} = 1,
          if $config->{cmake_usan};

        $D{ENABLE_LUAJIT}     = 0                                if $config->{cmake_debug} and !$config->{keep_luajit};
        $D{ENABLE_LUAJIT}     = $config->{cmake_luajit}          if defined $config->{cmake_luajit};
        $D{CMAKE_BUILD_TYPE}  = 'Debug'                          if $config->{cmake_debug};
        $D{MINETEST_PROTO}    = $config->{cmake_minetest}        if defined $config->{cmake_minetest};
        $D{ENABLE_LEVELDB}    = $config->{cmake_leveldb}         if defined $config->{cmake_leveldb};
        $D{ENABLE_SCTP}       = $config->{cmake_sctp}            if defined $config->{cmake_sctp};
        $D{USE_LIBCXX}        = $config->{cmake_libcxx}          if defined $config->{cmake_libcxx};
        $D{ENABLE_TOUCH}      = $config->{cmake_touch}           if defined $config->{cmake_touch};
        $D{ENABLE_GPERF}      = $config->{cmake_gperf}           if defined $config->{cmake_gperf};
        $D{ENABLE_TCMALLOC}   = $config->{cmake_tcmalloc}        if defined $config->{cmake_tcmalloc};
        $D{USE_LTO}           = $config->{cmake_lto}             if defined $config->{cmake_lto};
        $D{EXCEPTION_DEBUG}   = $config->{cmake_exception_debug} if defined $config->{cmake_exception_debug};
        $D{USE_DEBUG_HELPERS} = 1;

        $D{CMAKE_C_COMPILER} = qq{`which clang$config->{clang_version} clang | head -n1`},
          $D{CMAKE_CXX_COMPILER} = qq{`which clang++$config->{clang_version} clang++ | head -n1`}
          if $config->{cmake_clang};
        $D{BUILD_CLIENT} = (0 + !$config->{no_build_client});
        $D{BUILD_SERVER} = (0 + !$config->{no_build_server});
        $D{uc($_)}       = $config->{lc($_)} for grep { length $config->{lc($_)} } @{$config->{cmake_opts}};
        $D{$_}           = $config->{cmake_opt}{$_} for sort keys %{$config->{cmake_opt}};
        #warn 'D=', Data::Dumper::Dumper \%D;
        my $D     = join ' ', map { '-D' . $_ . '=' . ($D{$_} =~ /\s/ ? qq{"$D{$_}"} : $D{$_}) } sort keys %D;
        my $ninja = `ninja --version` ? '-GNinja' : '';
        unlink("CMakeCache.txt");
        return sytee qq{cmake .. $ninja $D @_ $config->{cmake_int} $config->{cmake_add}},
          qq{$config->{logdir}/autotest.$g->{task_name}.cmake.log};
    },
    make => sub {
        local $config->{make_add} = $config->{make_add};
        $config->{make_add} .= " V=1 VERBOSE=1 " if $config->{make_verbose};
        #sy qq{nice make -j $config->{makej} $config->{make_add} $config->{tee} $config->{logdir}/autotest.$g->{task_name}.make.log};
        return sytee qq{$config->{make_add} nice cmake --build . -- -j $config->{makej} $config->{makev}},
          qq{$config->{logdir}/autotest.$g->{task_name}.make.log};
    },
    run_single => sub {
        sy qq{rm -rf ${root_path}cache/media/* } if $config->{cache_clear} and $root_path;
        $commands->{world_name}();
        sy qq{rm -rf $config->{world} } if $config->{world_clear} and $config->{world};
        $config->{pid_file} = $config->{pid_path} . ($options->{pass}{name} || 'freeminer') . '.pid';
        return
          sytee $config->{runner},
          $commands->{env}(),
          qq{@_},
          $commands->{executable}(),
          qq{$config->{go} --logfile $config->{logdir}/autotest.$g->{task_name}.game.log},
          options_make([qw(gameid world address port config autoexit verbose trace)]),
          qq{$config->{run_add} }, qq{$config->{logdir}/autotest.$g->{task_name}.out.log};
        0;
    },
    run_test => sub {
        sy $config->{runner},
          $commands->{env}(),
          qq{@_},
          $commands->{executable}(),
          qq{--run-unittests --logfile $config->{logdir}/autotest.$g->{task_name}.test.log},
          options_make([qw(verbose trace)]);
    },
    set_bot         => {'----bot' => 1, '----bot_random' => 1,},
    run_bot         => ['set_bot', 'set_client', 'run_single'],
    valgrind => sub {
        local $config->{runner} = $config->{runner} . " valgrind @_";
        commands_run($config->{run_task});
    },
    run_server_simple => sub {
        my $fork = $config->{server_bg} ? '&' : '';
        sytee $config->{runner}, $commands->{env}(), qq{@_}, $commands->{executable}(), $fork,
          qq{$config->{logdir}/autotest.$g->{task_name}.server.out.log};
    },
    run_server => sub {
        my $cmd = join ' ',
          $config->{runner},
          $commands->{env}(),
          qq{@_},
          $commands->{executable}(),
          qq{--logfile $config->{logdir}/autotest.$g->{task_name}.game.log},
          options_make($options->{pass}{config} ? () : [qw(gameid world port config autoexit verbose)]),
          qq{$config->{run_add}};
        $config->{pid_file} = $config->{pid_path} . ($options->{pass}{worldname} || 'freeminerserver') . '.pid';
        if ($config->{server_bg}) {
            return sf $cmd . qq{ $config->{tee} $config->{logdir}/autotest.$g->{task_name}.server.out.log};
        } else {
            return sytee $cmd, qq{$config->{logdir}/autotest.$g->{task_name}.server.out.log};
        }
    },
    run_clients => sub {
        sy qq{rm -rf ${root_path}cache/media/* } if $config->{cache_clear} and $root_path;
        for (0 .. ($config->{clients_runs} || 0)) {
            my $autoexit = $config->{clients_autoexit} || $config->{autoexit};
            local $config->{address} = '::1' if not $config->{address};
            for ($config->{clients_start} .. $config->{clients_num}) {
                Time::HiRes::sleep($config->{clients_spawn_sleep} // 0.2);
                sf
                  $config->{runner},
                  $commands->{env}(),
                  qq{@_},
                  $commands->{executable}(),
                  qq{--name $config->{name}$_ --go --autoexit $autoexit --logfile $config->{logdir}/autotest.$g->{task_name}.game.log},
                  options_make([qw( address gameid world address port config verbose)]),
                  qq{$config->{run_add} $config->{tee} $config->{logdir}/autotest.$g->{task_name}.$config->{name}$_.err.log};
            }
            Time::HiRes::sleep($config->{clients_sleep} || 1) if $config->{clients_runs};
        }
    },
    symbolize => sub {
        sy
qq{asan_symbolize$config->{clang_version} < $config->{logdir}/autotest.$g->{task_name}.out.log | c++filt > $config->{logdir}/autotest.$g->{task_name}.out.symb.log};
    },
    cgroup => sub {
        return 0 unless $config->{cgroup};
        local $config->{cgroup} = '4G' if $config->{cgroup} eq 1;
        if (-e '/sys/fs/cgroup/memory/tasks') {    # cgroups v1
            sy
qq(sudo sh -c "mkdir -p /sys/fs/cgroup/memory/0; echo $$ > /sys/fs/cgroup/memory/0/tasks; echo $config->{cgroup} > /sys/fs/cgroup/memory/0/memory.limit_in_bytes");
        }
        # TODO: cgroup v2:
    },
    timelapse_video => sub {
        sy
qq{ffmpeg -f image2 $config->{ffmpeg_add_i} -pattern_type glob -i '../$config->{autotest_dir_rel}$config->{screenshot_dir}/timelapse_*.png' -vcodec libx264 $config->{ffmpeg_add_o} ../$config->{autotest_dir_rel}$config->{screenshot_dir}/timelapse-$config->{date}.mp4 };

    },
    sleep => sub {
        say 'sleep ', $_[0];
        Time::HiRes::sleep($_[0] || 1);
        0;
    },
    fail => sub {
        warn 'fail:', join ' ', @_;
    },
    set_client => [{'---no_build_client' => 0, '---no_build_server' => 1, '---executable_name' => 'freeminer',}],
    set_server =>
      [{'---no_build_client' => 1, '---no_build_server' => 0, '----no_exit'=>1, '---executable_name' => 'freeminerserver',}],
};

our $tasks = {
    deps  => sub { sy "sudo apt install -y clang valgrind google-perftools libgoogle-perftools-dev kcachegrind" },    # TODO: other os
    build => ['cmake', 'make'],    # "test1", "test2","test3",],
                                   #build        => [\'build_normal'],    #'
    debug => [
        sub {
            $g->{keep_config} = 1;
            $g->{build_names}{debug} = 'debug';
            0;
        },
        {'---cmake_debug' => 1,}
    ],
    build_client => ['set_client',   'build',],
    build_server => ['set_server',   'build',],
    bot          => [{'----default' => 1, '----' . $config->{options_display} => 1}, 'build_client', 'run_bot'],
    clang        => {
        '---cmake_clang'  => 1,
        '---cmake_libcxx' => 1,
    },

    asan => [
        sub {
            $g->{keep_config}      = 1;
            $g->{build_names}{san} = 'asan';
            $config->{envs}{asan}  = " ASAN_SYMBOLIZER_PATH=`which llvm-symbolizer$config->{clang_version}`";
            0;
        },
    	'debug',

        {
            '---cmake_asan' => 1,
        },
    ],
    tsan => [
        sub {
            $g->{keep_config}          = 1;
            $g->{build_names}{san}     = 'tsan';
            #$config->{options_display} = 'software' if $config->{tsan_opengl_fix} and !$config->{options_display};
            #$config->{cmake_leveldb} //= 0 if $config->{tsan_leveldb_fix};
            $config->{envs}{tsan} = " TSAN_OPTIONS='detect_deadlocks=1 second_deadlock_stack=1 history_size=7'";
            #? local $options->{opt}{enable_minimap} = 0;    # too unsafe
            # FATAL: ThreadSanitizer: unexpected memory mapping :
            sy 'sudo --non-interactive sysctl vm.mmap_rnd_bits=28';
            0;
        },
    	'debug',
        {
            '---cmake_tsan' => 1,
        },
        'cgroup',
    ],
    msan => [
        sub {
            $g->{keep_config} = 1;
            $g->{build_names}{san} = 'msan';
            0;
        },
	    'debug',
        {
            '---cmake_msan' => 1,
        },
    ],
    usan => [
        sub {
            $g->{keep_config} = 1;
            $g->{build_names}{san} = 'usan';
            0;
        },
	    'debug',
        {
            '---cmake_usan' => 1,
        },
    ],
    (
        map {
            'valgrind_' . $_ => [
                'debug', 'build',
                ['valgrind', '--tool=' . $_],
            ],
        } @{$config->{valgrind_tools}}
    ),

    stress => ['build', {'---server_bg' => 1,}, 'run_server', ['sleep', 10], 'clients_run', ['sleep', $config->{autoexit}]],

    clients_run => [
        'set_bot',    #{build_name => ''},
        'run_clients'
    ],
    clients => ['build_client', 'clients_run'],

    stress_massif => [
        'build_client',
        sub {
            local $config->{run_task} = 'run_server';
            commands_run('valgrind_massif');
        },
        ['sleep', 10],
        'clients_run',
    ],

    debug_mapgen => [
        #{build_name => 'debug'},
        sub {
            local $config->{world} = "$config->{logdir}/world_$g->{task_name}";
            commands_run('debug');
        }
    ],
    gdb => sub {
        $g->{keep_config} = 1;
        $config->{runner} =
            ' env ASAN_OPTIONS=abort_on_error=1 '
          . $config->{runner}
          . $config->{gdb}
          . q{ -ex 'set debuginfod enabled on' }
          . q{ -ex 'run' -ex 't a a bt' }
          . ($config->{gdb_stay} ? '' : q{ -ex 'cont' -ex 'quit' })
          . q{ --args };
        #@_ = ('debug') if !@_;
        for (@_) { my $r = commands_run($_); return $r if $r; }
        0;
    },

    server => ['set_server', 'build_server', 'run_server'],

    vtune => sub {
        sy 'echo 0|sudo tee /proc/sys/kernel/yama/ptrace_scope';
        local $config->{runner} =
          $config->{runner} . qq{$config->{vtune_amplifier}amplxe-cl -collect $config->{vtune_collect} -r $config->{logdir}/rh0};
        local $config->{run_escape} = '\\\\';
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
    stress_vtune => [
        #'build_debug',sub { commands_run('vtune', 'run_server');}, ['sleep', 10], 'clients_run',
        {    #-no_build_client => 1, '---no_build_server'  => 0,
            '---server_bg' => 1,
        },
        'debug', 'build',
        [\'vtune', 'run_server'],
        ['sleep',  10],
        #{build_name => '_normal'}, # '
        'clients',
    ],

    gperf_prepare => [
        'debug',
        {'---cmake_gperf' => 1,},
        sub {
            $g->{keep_config} = 1;
            $g->{build_names}{san} = 'gperf';
            ($config->{PPROF_PATH}) = `which google-pprof pprof`;
            $config->{PPROF_PATH} =~ s{\s+$}{}s;
            $ENV{PPROF_PATH} = $config->{PPROF_PATH};
            $config->{envs}{gperf} .= " MALLOCSTATS=9 PERFTOOLS_VERBOSE=9 "      if $config->{gperf_heapprofile};
            $config->{envs}{gperf} .= " HEAPPROFILE=$config->{logdir}/heap.out " if $config->{gperf_heapprofile};
            $config->{envs}{gperf} .= " CPUPROFILE=$config->{logdir}/cpu.out "   if $config->{gperf_cpuprofile};
            0;
        }
    ],

    gperf => [
        'gperf_prepare',
        sub {
            #$g->{keep_config}      = 1;
            #$g->{build_names}{san} = 'gperf';
            my ($libprofiler) = `ls -1 /usr/lib/*/libprofiler.so | head -n1`;
            $libprofiler =~ s/\s+$//;
            if ($libprofiler and -f $libprofiler) {
                $config->{envs}{gperf} .= " LD_PRELOAD=$libprofiler";
            }
            push @$task_run, 'gperf_report';
            0;
        }, 
        # {'---cmake_gperf' => 1,},
    ],

    stress_gperf => [
        {'---server_bg' => 1,},
        'gperf',
        'server',
        ['sleep', 10], {'---cmake_gperf' => 0,}, 'clients', 'gperf_report',
    ],

    gperf_report => [
        'gperf_prepare',
        sub {
            # sytee 'env | grep PPROF_PATH', "$config->{logdir}/tmp";
            sytee qq{$config->{PPROF_PATH} $config->{gperf_mode} } . $commands->{executable}() . qq{ $config->{logdir}/heap.out*},
              "$config->{logdir}/gperf.heap.out"
              if $config->{gperf_heapprofile};
            if ($config->{gperf_cpuprofile}) {
                sytee qq{$config->{PPROF_PATH} $config->{gperf_mode} } . $commands->{executable}() . qq{ $config->{logdir}/cpu.out },
                  "$config->{logdir}/gperf.cpu.out";
                sy qq{$config->{PPROF_PATH} --callgrind }
                  . $commands->{executable}()
                  . qq{ $config->{logdir}/cpu.out > $config->{logdir}/pprof.callgrind };
                say qq{kcachegrind $config->{logdir}/pprof.callgrind};
            }
            return 0;
        }
    ],

    go => [
        'set_client',
        {
            '---go'       => undef,
            '---autoexit' => undef,
        },
        'build',
        $config->{run_task}
    ],
    fly            => [{'---options_int' => 'fly,forward',}, 'build_client', 'run_single'],
    timelapse_stay => [
        'timelapse', {
            '----fly'           => 1, '----stay'             => 1, '----far'        => 10, '----fps1' => 1, '----no_exit' => 1,
            '-show_basic_debug' => 0, '-show_profiler_graph' => 0, '-profiler_page' => 0,
        },
        'build_client',
        'run_single',
    ],

    timelapse => [
        {'---options_int' => 'timelapse',},
        sub {
            $g->{keep_config} = 1;
            push @$task_run, 'timelapse_video';
            0;
        }
    ],

    bench => [{
            '-fixed_map_seed'          => 1,   '--autoexit' => $options->{pass}{autoexit} || 300, -max_block_generate_distance => 100,
            '-max_block_send_distance' => 100, '----fly'    => 1, '----forward' => 1, '---world_clear' => 1,
            '---world'                 => $script_path . 'world_bench',
            '-static_spawnpoint'       => '(0,60,0)',
            '-fps_max'                 => 120,
            '-fps_max_unfocused'       => 120,
        },
        'fly'
    ],

    bench1 => [{
            '-fixed_map_seed'          => 1,      '--autoexit' => $options->{pass}{autoexit} || 300, -max_block_generate_distance => 100,
            '-max_block_send_distance' => 100,    '---world_clear'     => 1, '--world' => $script_path . 'world_bench1',
            -mg_name                   => 'math', '-static_spawnpoint' => "(0,20202,0)",
            '-fps_max'                 => 120,
            '-fps_max_unfocused'       => 120,
        },
        'set_client',
        'build',
        'run_single'
    ],
    up => sub {
        my $cwd = Cwd::cwd();
        chdir $config->{root_path};
        sy qq{(git stash && git pull --rebase >&2) | grep -v "No local changes to save" && git stash pop};
        sy qq{git submodule update --init --recursive};
        chdir $cwd;
        return 0;
    },

    kill_client => [
        'set_client',
        sub {
            sy qq{killall } . $config->{executable_name};
            return 0;
        }
    ],
    kill_server => [
        'set_server',
        sub {
            sy qq{killall } . $config->{executable_name};
            return 0;
        }
    ],
    kill    => ['kill_client', 'kill_server'],
    install => [
        sub { $config->{cmake_opt}{CMAKE_INSTALL_PREFIX} = $config->{logdir} . '/install'; 0 }, 'build',
        sub { sy qq{nice cmake --install .}; },
    ],
    test => ['build_client', {'---show_profiler_graph' => 0, -show_profiler_graph => 0}, 'run_test'],
};

sub dmp (@) { say +(join ' ', (caller)[0 .. 5]), ' ', Data::Dumper::Dumper \@_ }

sub sig(;$$) {
    if ($? == -1) {
        say 'failed to execute:', $!;
        return $?;
    } elsif ($? & 127) {
        $signal = $? & 127;
        say 'child died with signal ', ($signal), ', ' . (($? & 128) ? 'with' : 'without') . ' coredump';
        return $?;
    } else {
        my $c = $? >> 8;
        say "code=$c system=$_[0] pid=$_[1]" if $c;
        return $c;
    }
}

{
    my %fh;
    my $savetime = 0;

    sub file_append(;$@) {
        local $_ = shift;
        if (!@_) {
            if (exists $fh{$_}) {
                delete $fh{$_};
            } else {
                %fh = ();
            }
            return;
        }
        unless ($fh{$_}) {
            delete($fh{$_}), return unless open $fh{$_}, '>>', $_;
            return unless $fh{$_};
        }
        flock($fh{$_}, Fcntl::LOCK_EX);
        print {$fh{$_}} @_;
        flock($fh{$_}, Fcntl::LOCK_UN);
        if (time() > $savetime + 5) {
            %fh       = ();
            $savetime = time();
        }
        return @_;
    }
}

sub sy (@) {
    say 'running ', join ' ', @_;
    file_append("$config->{logdir}/run.sh", join(' ', @_), "\n");
    return sig system 'bash', '-c', join ' ', @_;
}

sub sytee (@) {
    my $tee = pop;
    say 'running ', join ' ', @_;
    file_append("$config->{logdir}/run.sh", join(' ', @_), "\n");
    my $pid = open my $fh, "-|", "@_ 2>&1" or return "can't open @_: $!";
    if ($config->{pid_file}) {
        unlink $config->{pid_file};
        file_append($config->{pid_file}, $pid);
    }
    while (defined($_ = <$fh>)) {
        print $_;
        file_append($tee, $_);
    }
    close($fh);
    if ($config->{pid_file}) {
        unlink $config->{pid_file};
    }
    return sig(undef, $pid);
}

sub sf (@) {
    say 'forking ', join ' ', @_;
    my $pid = fork();
    push(@$child, $pid), return if $pid;
    sy @_;
    exit;
}

sub array (@) {
    local @_ = map { ref $_ eq 'ARRAY' ? @$_ : $_ } (@_ == 1 and !defined $_[0]) ? () : @_;
    wantarray ? @_ : \@_;
}

sub json (@) {
    local *Data::Dumper::qquote = sub {
        $_[0] =~ s/\\/\\\\/g, s/"/\\"/g for $_[0];
        return $_[0] + 0 eq $_[0] ? $_[0] : '"' . $_[0] . '"';
    };
    return \(Data::Dumper->new(\@_)->Pair(':')->Terse(1)->Indent(0)->Useqq(1)->Useperl(1)->Deparse($config->{verbose} > 1 ? 1 : 0)->Dump());
}

sub options_make(;$$) {
    my ($mm, $m) = @_;
    my ($rm, $rmm);

    $rmm = {map { $_ => $config->{$_} } grep { $config->{$_} } array(@$mm)};
    $m ||= [
        map { split /[,;]+/ } map { array($_) } 
        #'default',  $config->{options_display},    #$config->{options_bot},
        $config->{options_int}, $config->{options_add}, $config->{options_arr},
        (sort { $config->{options_use}{$a} <=> $config->{options_use}{$b} } keys %{$config->{options_use}}), 'opt',
    ];
    for my $name (array(@$m)) {
        $rm->{$_} = $options->{$name}{$_} for sort keys %{$options->{$name}};
        for my $k (sort keys %$rm) {
            if ($k =~ /^--/) {
                $rmm->{$'} = $rm->{$k};
                delete $rm->{$k};
                next;
            }
            next if !ref $rm->{$k};
            #($rm->{$k} = JSON::encode_json($rm->{$k})) =~ s/"/$config->{run_escape}\\"/g;    #"
            ($rm->{$k} = ${json($rm->{$k})});    # =~ s/"/$config->{run_escape}\\"/g;    #"
        }
    }
    $rmm->{$_} = $options->{pass}{$_}       for sort keys %{$options->{pass}};
    $rm->{$_}  = ref $config->{config_pass}{$_} ? "'" . ${json($config->{config_pass}{$_})} . "'" : $config->{config_pass}{$_} for sort keys %{$config->{config_pass}};
    return join ' ', (map {"--$_ $rmm->{$_}"} sort keys %$rmm), (map {"-$_='$rm->{$_}'"} sort keys %$rm);
}

sub command_run(@);

sub command_run(@) {
    my @p   = @_;
    my $cmd = shift @p;
    say join ' ', "command_run", ${json($cmd)}, '(', @p, ')', " g=", ${json($g)} if $config->{verbose};
    if ('CODE' eq ref $cmd) {
        return $cmd->(@p);
    } elsif ('HASH' eq ref $cmd) {
        for my $k (sort keys %$cmd) {
            if ($k =~ /^----(.+)/) {
                $config->{options_use}{$1} = $cmd->{$k};
            } elsif ($k =~ /^---(.+)/) {
                $config->{$1} = $cmd->{$k};
            } elsif ($k =~ /^--(.+)/) {
                $options->{pass}{$1} = $cmd->{$k};
            } elsif ($k =~ /^-D(.+)/) {
                $config->{cmake_opt}{$1} = $cmd->{$k};
            } elsif ($k =~ /^-(.+)/) {
                $config->{config_pass}{$1} = $cmd->{$k};
            } else {
                $g->{$k} = $cmd->{$k};
            }
        }
    } elsif ('ARRAY' eq ref $cmd) {
        #for (@{$cmd}) {
        my $r = command_run(array $cmd, @p);
        warn("command $_ returned $r"), return $r if $r;
        #}
    } elsif ($cmd) {
        return commands_run($cmd, @p);
    } else {
        warn 'unknown command ', $cmd;
        return 0;
    }
}

sub commands_run(@);

sub commands_run(@) {
    my @p    = @_;
    my $name = shift @p;

    if ($config->{'no_' . $name}) {
        warn 'command disabled ', $name;
        return undef;
    }

    say join ' ', "commands_run", $name, @p if $config->{verbose};

    my $c = $commands->{$name} || $tasks->{$name};
    if ('SCALAR' eq ref $name) {
        return commands_run($$name, @p);
    } elsif ('ARRAY' eq ref $c) {
        for my $co (@{$c}) {
            my $r = command_run $co, @p;
            warn("command $_ returned $r"), return $r if $r;
        }
    } elsif ($c) {
        return command_run $c, @p;
    } elsif (ref $name) {
        return command_run $name, @p;
        #} elsif ($options->{$name}) {
        #    $config->{options_add} .= ',' . $name;
        #    #command_run({-options_add => $name});
        #    return 0;
    } else {
        warn 'unknown commands ', $name;
        return 0;
    }
}

sub task_start(@) {
    my @p    = @_;
    my $name = shift @p;
    $name = $1, unshift @p, $2 if $name =~ /^(.*?)=(.*)$/;
    say "task start $name ", @p;
    #$g = {task_name => $name, build_name => $name,};
    $g->{task_name} = $name;
    local $g->{build_name} = $config->{build_name} if $config->{build_name};
    say ${json $g} if $config->{verbose};
    #task_run($name, @_);
    commands_run($name, @p);
}

$task_run = [
    @$task_run,
    qw(test  tsan bot  asan bot  usan bot  gperf  bot  valgrind_memcheck)
  ]
  if !@$task_run or 'default' ~~ $task_run;
if ('all' ~~ $task_run) {
    $task_run = [sort keys %$tasks];
    $config->{all_run} = 1;
}

unless (@ARGV) {
    say $help;
    say "possible tasks:";
    print "$_ " for sort keys %$tasks;
    say "\n\n but running default list: ", join ' ', @$task_run;
    say '';
    say "possible presets:";
    print "----$_ " for sort keys %$options;
    say '';
    sleep 1;
}

while (@$task_run) {
    my $task = shift @$task_run;
    init_config()                     if $g->{keep_config}-- <= 0;
    warn "task failed [$task] = [$_]" if $_ = task_start($task);
    last                              if $signal ~~ [2, 3];
}
