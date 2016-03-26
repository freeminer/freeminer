#!/bin/bash -e

if [[ $PLATFORM == "Unix" ]]; then
	mkdir -p travisbuild
	cd travisbuild || exit 1
	CMAKE_FLAGS=''
	# Clang builds with FreeType fail on Travis
	if [[ $CC == "clang" ]]; then
		CMAKE_FLAGS+=' -DENABLE_FREETYPE=FALSE'
	fi
	if [[ $TRAVIS_OS_NAME == "osx" ]]; then
		CMAKE_FLAGS+=' -DCUSTOM_GETTEXT_PATH=/usr/local/opt/gettext'
	fi
	cmake -DCMAKE_BUILD_TYPE=Debug \
		-DRUN_IN_PLACE=TRUE \
		-DENABLE_GETTEXT=TRUE \
		$CMAKE_FLAGS ..
	make -j2
	echo "Running unit tests."
	../bin/minetest --run-unittests && exit 0
elif [[ $PLATFORM == Win* ]]; then
	[[ $CC == "clang" ]] && exit 1 # Not supposed to happen
	# We need to have our build directory outside of the minetest directory because
	#  CMake will otherwise get very very confused with symlinks and complain that
	#  something is not a subdirectory of something even if it actually is.
	# e.g.:
	# /home/travis/minetest/minetest/travisbuild/minetest
	# \/  \/  \/
	# /home/travis/minetest/minetest/travisbuild/minetest/travisbuild/minetest
	# \/  \/  \/
	# /home/travis/minetest/minetest/travisbuild/minetest/travisbuild/minetest/travisbuild/minetest
	# You get the idea.
	OLDDIR=$(pwd)
	cd ..
	export EXISTING_MINETEST_DIR=$OLDDIR
	export NO_MINETEST_GAME=1
	if [[ $PLATFORM == "Win32" ]]; then
		"$OLDDIR/util/buildbot/buildwin32.sh" travisbuild && exit 0
	elif [[ $PLATFORM == "Win64" ]]; then
		"$OLDDIR/util/buildbot/buildwin64.sh" travisbuild && exit 0
	fi
else
	echo "Unknown platform \"${PLATFORM}\"."
	exit 1
fi

