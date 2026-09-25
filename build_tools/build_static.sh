#!/usr/bin/env bash

# Configure and build Freeminer with static libraries where the toolchain
# provides them. Dependencies are assumed to be installed already.

set -euo pipefail
export CCACHE_DISABLE=1

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_dir=$(CDPATH= cd -- "${script_dir}/.." && pwd)
build_dir=${BUILD_DIR:-"${source_dir}/build_static"}
if [ -n "${CMAKE_GENERATOR:-}" ]; then
	generator=${CMAKE_GENERATOR}
elif command -v ninja >/dev/null 2>&1 && [ -x "$(command -v ninja)" ]; then
	generator=Ninja
else
	generator="Unix Makefiles"
fi

cmake_args=(
	-B "${build_dir}"
	-G "${generator}"
	-S "${source_dir}"
	-DBoost_NO_SYSTEM_PATHS=ON
	-DBUILD_BENCHMARKS=OFF
	-DBUILD_DOCUMENTATION=OFF
	-DBUILD_SHARED_LIBS=OFF
	-DBUILD_UNITTESTS=OFF
	-DCMAKE_BUILD_TYPE=Release
	-DENABLE_LTO=ON
	-DENABLE_SYSTEM_JSONCPP=OFF
	-DSTATIC_BUILD=ON
	-DUSE_SDL2_STATIC=ON
	-DUSE_STATIC_LIBRARIES=ON
    -DCURL_USE_STATIC_LIBS=ON
    -DFETCH_DEPS=ON
    -DZLIB_USE_STATIC_LIBS=ON

)

# Use -static only when every required dependency has a static archive. The
# default still prefers static libraries, while avoiding failures with common
# packages that ship only a shared object (for example OpenAL).
if [ "${FULL_STATIC:-0}" = 1 ]; then
	static_sdl_ready=1
	for archive in \
		/usr/lib/x86_64-linux-gnu/libasound.a \
		/usr/lib/x86_64-linux-gnu/libpulse.a \
		/usr/lib/x86_64-linux-gnu/libsamplerate.a \
		/usr/lib/x86_64-linux-gnu/libgbm.a \
		/usr/lib/x86_64-linux-gnu/libwayland-client.a \
		/usr/lib/x86_64-linux-gnu/libwayland-cursor.a \
		/usr/lib/x86_64-linux-gnu/libxkbcommon.a; do
		[ -f "${archive}" ] || static_sdl_ready=0
	done
	if [ "${static_sdl_ready}" = 0 ]; then
		echo "Static SDL2 dependencies are incomplete; using dynamic SDL2/OpenGL." >&2
		cmake_args+=(
			-DUSE_SDL2_STATIC=OFF
			-DCMAKE_EXE_LINKER_FLAGS=
			-DCMAKE_CXX_STANDARD_LIBRARIES=
			-DCURL_LIBRARY=/usr/lib/x86_64-linux-gnu/libcurl.so
			-DCURL_LIBRARY=/usr/lib/x86_64-linux-gnu/libcurl.so
			-DCURL_LIBRARY_RELEASE=/usr/lib/x86_64-linux-gnu/libcurl.so
			-DTIFF_LIBRARY=/usr/lib/x86_64-linux-gnu/libtiff.so
			-DTIFF_LIBRARY_RELEASE=/usr/lib/x86_64-linux-gnu/libtiff.so
			-DFREETYPE_LIBRARY=/usr/lib/x86_64-linux-gnu/libfreetype.so
		)
		FULL_STATIC=0
	fi
fi

if [ "${FULL_STATIC:-0}" = 1 ]; then
	cmake_args+=(
		-DCMAKE_CXX_STANDARD_LIBRARIES=-lbsd
		-DCMAKE_EXE_LINKER_FLAGS=-static\ -static-libgcc\ -static-libstdc++\ -lbsd
		#-DSDL2_LIBRARY=/usr/lib/x86_64-linux-gnu/libSDL2.a
	)
fi

cmake "${cmake_args[@]}" "$@"
cmake --build "${build_dir}"

binary="${build_dir}/bin/freeminer"
if [ ! -x "${binary}" ]; then
	binary="${build_dir}/freeminer"
fi

if [ -x "${binary}" ]; then
	echo "Dynamic libraries linked by ${binary}:"
	ldd "${binary}" || true
else
	echo "Built freeminer binary was not found under ${build_dir}" >&2
fi
