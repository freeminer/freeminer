# ARG FROM=ubuntu:focal
ARG FROM=ubuntu:noble
FROM $FROM

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /workspace

RUN apt update \
    && apt install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        ccache \
        cmake \
        gettext \
        git \
        gnupg \
        gtk-update-icon-cache \
        libboost-all-dev \
        libbz2-dev \
        libcurl4-openssl-dev \
        libdeflate-dev \
        libfreetype-dev \
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
        squashfs-tools \
        wget \
    && rm -rf /var/lib/apt/lists/*

RUN printf '#!/bin/sh\nexit 0\n' > /usr/local/bin/apt-key \
    && chmod 0755 /usr/local/bin/apt-key    

CMD ["/workspace/build_tools/appimage/appimage.sh"]
