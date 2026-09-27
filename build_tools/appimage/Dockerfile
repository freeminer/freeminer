#ARG FROM=ubuntu:jammy
#ARG FROM=debian:stable
ARG FROM=debian:oldstable
FROM $FROM

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /workspace

RUN apt update \
    && apt install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        ccache \
        gettext \
        clang \
        lld \
        libc++-dev \
        libc++abi-dev \
        git \
        gnupg \
        gtk-update-icon-cache \
        libboost-all-dev \
        libbz2-dev \
        libcurl4-openssl-dev \
        libdeflate-dev \
        libfreetype6-dev \
        libgmp-dev \
        libjpeg-dev \
        libluajit-5.1-dev \
        libmsgpack-dev \
        libogg-dev \
        libopenal-dev \
        libosmium2-dev \
        libpng-dev \
        libsdl2-dev \
        libsnappy-dev \
        libsqlite3-dev \
        libssl-dev \
        libtiff-dev \
        libvorbis-dev \
        libxi-dev \
        libxxf86vm-dev \
        libzstd-dev \
        ninja-build \
        pkg-config \
        python3 \
        squashfs-tools \
        wget \
        docker.io \
    && rm -rf /var/lib/apt/lists/*

# Resolve the newest stable upstream release when this layer is built.
# Pin with --build-arg CMAKE_VERSION=x.y.z when a reproducible version is needed.
ARG CMAKE_VERSION=latest
RUN set -eu; \
    version="${CMAKE_VERSION}"; \
    if [ "$version" = latest ]; then \
        wget -qO /tmp/cmake-release.json https://api.github.com/repos/Kitware/CMake/releases/latest; \
        version="$(python3 -c 'import json; print(json.load(open("/tmp/cmake-release.json"))["tag_name"][1:])')"; \
    fi; \
    case "$(dpkg --print-architecture)" in \
        amd64) arch=x86_64 ;; \
        arm64) arch=aarch64 ;; \
        *) echo "Unsupported CMake binary architecture" >&2; exit 1 ;; \
    esac; \
    archive="cmake-${version}-linux-${arch}.tar.gz"; \
    base="https://github.com/Kitware/CMake/releases/download/v${version}"; \
    wget -qO "/tmp/$archive" "$base/$archive"; \
    wget -qO /tmp/cmake-SHA-256.txt "$base/cmake-${version}-SHA-256.txt"; \
    cd /tmp; \
    awk -v file="$archive" '$2 == file {print}' cmake-SHA-256.txt > cmake-checksum.txt; \
    test -s cmake-checksum.txt; \
    sha256sum -c cmake-checksum.txt; \
    mkdir -p /opt/cmake; \
    tar -xzf "$archive" --strip-components=1 -C /opt/cmake; \
    rm -f "$archive" cmake-SHA-256.txt cmake-checksum.txt cmake-release.json
ENV PATH="/opt/cmake/bin:${PATH}"
RUN cmake --version

RUN printf '#!/bin/sh\nexit 0\n' > /usr/local/bin/apt-key \
    && chmod 0755 /usr/local/bin/apt-key    

CMD ["/workspace/build_tools/appimage/appimage.sh"]
