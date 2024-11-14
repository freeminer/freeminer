ARG DOCKER_IMAGE=alpine:3.19
FROM $DOCKER_IMAGE AS dev

ENV LUAJIT_VERSION v2.1

RUN apk add --no-cache git build-base cmake curl-dev zlib-dev zstd-dev \
		sqlite-dev postgresql-dev hiredis-dev leveldb-dev boost-system ccache \
		gmp-dev jsoncpp-dev ninja ca-certificates

WORKDIR /usr/src/
RUN git clone --recursive https://github.com/jupp0r/prometheus-cpp && \
		cd prometheus-cpp && \
		cmake -B build \
			-DCMAKE_INSTALL_PREFIX=/usr/local \
			-DCMAKE_BUILD_TYPE=Release \
			-DENABLE_TESTING=0 \
			-GNinja && \
		cmake --build build && \
		cmake --install build && \
	cd /usr/src/ && \
	git clone --recursive https://github.com/libspatialindex/libspatialindex && \
		cd libspatialindex && \
		cmake -B build \
			-DCMAKE_INSTALL_PREFIX=/usr/local && \
		cmake --build build && \
		cmake --install build && \
	cd /usr/src/ && \
	git clone --recursive https://luajit.org/git/luajit.git -b ${LUAJIT_VERSION} && \
		cd luajit && \
		make amalg && make install && \
	cd /usr/src/

COPY mods /usr/src/minetest/mods

FROM dev as builder

COPY .git /usr/src/luanti/.git
COPY CMakeLists.txt /usr/src/luanti/CMakeLists.txt
COPY README.md /usr/src/luanti/README.md
COPY freeminer.conf.example /usr/src/luanti/freeminer.conf.example
COPY builtin /usr/src/luanti/builtin
COPY cmake /usr/src/luanti/cmake
COPY doc /usr/src/luanti/doc
COPY fonts /usr/src/luanti/fonts
COPY lib /usr/src/luanti/lib
COPY misc /usr/src/luanti/misc
COPY po /usr/src/luanti/po
COPY src /usr/src/luanti/src
COPY irr /usr/src/luanti/irr
COPY textures /usr/src/luanti/textures

COPY games/default /usr/src/minetest/games/default

WORKDIR /usr/src/luanti
RUN cmake -B build \
		-DCMAKE_INSTALL_PREFIX=/usr/local \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SERVER=TRUE \
		-DENABLE_PROMETHEUS=TRUE \
		-DBUILD_UNITTESTS=FALSE -DBUILD_BENCHMARKS=FALSE \
		-DBUILD_CLIENT=FALSE \
		-DRUN_IN_PLACE=0 \
		-GNinja && \
	cmake --build build && \
	cmake --install build

FROM $DOCKER_IMAGE AS runtime

RUN apk add --no-cache curl gmp libstdc++ libgcc libpq jsoncpp zstd-libs \
				sqlite-libs postgresql hiredis leveldb && \
	adduser -D minetest --uid 30000 -h /var/lib/minetest && \
	chown -R minetest:minetest /var/lib/minetest

WORKDIR /var/lib/minetest

COPY --from=builder /usr/local/share/freeminer /usr/local/share/freeminer
COPY --from=builder /usr/local/bin/freeminerserver /usr/local/bin/freeminerserver
COPY --from=builder /usr/local/share/doc/freeminer/freeminer.conf.example /etc/freeminer/freeminer.conf
COPY --from=builder /usr/local/lib/libspatialindex* /usr/local/lib/
COPY --from=builder /usr/local/lib/libluajit* /usr/local/lib/
USER minetest:minetest

EXPOSE 30200/udp
EXPOSE 30000/udp 30000/tcp
VOLUME /var/lib/minetest/ /etc/minetest/

ENTRYPOINT ["/usr/local/bin/freeminerserver"]
CMD ["--config", "/etc/freeminer/freeminer.conf"]
