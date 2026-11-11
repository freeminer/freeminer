#!/usr/bin/env bash
# script for fast installing on raspberry pi, odroid and other arm boards with debian

set -exu

# There's no package available, you have to compile it from source.
# you can place this text to freeminer.sh file and run it

# or
# curl https://raw.githubusercontent.com/freeminer/freeminer/master/build_tools/build.sh | sh

# Usage: build.sh [all|deps|checkout|update|configure|build|run][,...]
# Individual stages expect the preceding stages to have been completed.
STAGE=${STAGE:-${1:-all}}
case "$STAGE" in
    ,* | *, | *,,*) echo "Invalid stage list: $STAGE" >&2; exit 1 ;;
esac
IFS=, read -r -a STAGES <<< "$STAGE"
for stage in "${STAGES[@]}"; do
    case "$stage" in
        all | deps | checkout | update | configure | build | run) ;;
        *) echo "Invalid stage: $stage" >&2; exit 1 ;;
    esac
done

has_stage() {
    case ",$STAGE," in
        *,all,* | *,"$1",*) return 0 ;;
        *) return 1 ;;
    esac
}

if has_stage deps; then
    DIST=${DIST:-$(lsb_release --short --id)} || :
    DIST=${DIST:-$(cat /etc/issue /etc/issue.net | head -n1 | cut -d " " -f1)}
    DIST=${DIST:-$(sh -c '. /etc/os-release && echo $NAME')}

    if [ -z "${NO_DEPS-}" ]; then
        SUDO=${SUDO-$(which sudo || :)}
        if command -v apt > /dev/null 2>&1; then
            ${SUDO} apt update
            ${SUDO} env DEBIAN_FRONTEND=noninteractive apt install --yes --no-install-recommends \
                build-essential \
                ca-certificates \
                ccache \
                clang \
                cmake \
                git \
                libboost-all-dev \
                libbrotli-dev \
                libbz2-dev \
                libc++-dev \
                libc++abi-dev \
                libcurl4-openssl-dev \
                libdeflate-dev \
                libfreetype6-dev \
                libgettextpo0 \
                libhiredis-dev \
                libidn2-dev \
                libjpeg-dev \
                libmsgpack-dev \
                libnghttp2-dev \
                libopenal-dev \
                libosmium2-dev \
                libpq-dev \
                libprotozero-dev \
                libpsl-dev \
                libsdl2-dev \
                libspatialindex-dev \
                libsqlite3-dev \
                libssl-dev \
                libtiff-dev \
                libvorbis-dev \
                libxi-dev \
                libxxf86vm-dev \
                libzip-dev \
                libzstd-dev \
                lld \
                ninja-build \
                zlib1g-dev \

            for PACKAGE in \
                libgl1-mesa-dev \
                libgles1-mesa-dev \
                libgles2-mesa-dev \
                libluajit-5.1-dev \
                libpng12-dev \
                libpng-dev \
                clang-20 \
                clang-21 \
                clang-22 \
                clang-23 \
                lld-20 \
                lld-21 \
                lld-22 \
                lld-23; do
                ${SUDO} apt install -y $PACKAGE || :
            done
        elif command -v dnf > /dev/null 2>&1; then
            ${SUDO} dnf install -y --skip-unavailable \
                automake \
                boost-devel \
                brotli-devel \
                bzip2-devel \
                ca-certificates \
                ccache \
                clang \
                cmake \
                freetype-devel \
                gettext \
                git \
                gmp-devel \
                hiredis-devel \
                jsoncpp-devel \
                leveldb-devel \
                libc++-devel \
                libcurl-devel \
                libcxx-devel \
                libcxxabi-devel \
                libdeflate-devel \
                libidn2-devel \
                libjpeg-turbo-devel \
                libogg-devel \
                libosmium-devel \
                libpng-devel \
                libpq-devel \
                libpsl-devel \
                libtiff-devel \
                libvorbis-devel \
                libXi-devel \
                libXxf86vm-devel \
                libzip-devel \
                libzstd-devel \
                lld \
                luajit-devel \
                make \
                mesa-libGL-devel \
                mesa-libGLES-devel \
                msgpack-devel \
                ncurses-devel \
                nghttp2-devel \
                ninja-build \
                openal-soft-devel \
                openssl-devel \
                protozero-devel \
                sdl2-compat-devel \
                SDL2-devel \
                spatialindex-devel \
                sqlite-devel \
                sudo \
                which \
                zlib-devel

        elif command -v pacman > /dev/null 2>&1; then
            ${SUDO} pacman --needed --noconfirm -Sy \
                boost \
                boost-libs \
                bzip2 \
                ccache \
                clang \
                cmake \
                curl \
                freetype2 \
                gettext \
                git \
                glfw-x11 \
                hiredis \
                libc++ \
                libc++abi \
                libdeflate \
                libjpeg-turbo \
                libpng \
                libtiff \
                libvorbis \
                libxi \
                libxxf86vm \
                lld \
                llvm \
                luajit \
                msgpack-cxx \
                ninja \
                openal \
                protozero \
                sdl2 \
                sqlite3 \
                subversion \
                which \
                zstd \

            # libunwind
            CMAKE_OPT=${CMAKE_OPT:-"-DCMAKE_BUILD_WITH_INSTALL_RPATH=1"}
        fi
    fi

fi

# Locate the build directory for the remaining stages.
if has_stage checkout || has_stage update || has_stage configure || has_stage build || has_stage run; then
    if [ -d freeminer/build ]; then
        cd freeminer/build
    elif [ -d freeminer ]; then
        mkdir -p freeminer/build
        cd freeminer/build
    elif [ ! -s ../src/CMakeLists.txt ] && has_stage checkout; then
        git clone --depth 1 --recursive https://github.com/freeminer/freeminer.git && mkdir -p freeminer/build && cd freeminer/build
    elif [ -s ../src/CMakeLists.txt ]; then
        mkdir -p ../build && cd ../build
    else
        echo "Freeminer checkout not found; run the checkout stage first" >&2
        exit 1
    fi
fi

if has_stage update; then
    git pull --rebase || :
    git submodule update --init --recursive || :
fi

if has_stage configure; then
    cmake .. --fresh -GNinja -DBUILD_UNITTESTS=0 ${CMAKE_OPT-}
fi

if has_stage build; then
    nice cmake --build .
fi

if has_stage run; then
    ./freeminer
fi
