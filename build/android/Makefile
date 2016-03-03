# build options

OS := $(shell uname)

# compile with GPROF
# GPROF = 1

# build for build platform
APP_PLATFORM = android-15

# paths used for timestaps, dependencys, tree config and libs
PATHCFGFILE = path.cfg

ROOT = $(shell pwd)

GAMES_TO_COPY = default
MODS_TO_COPY =


VERSION_MAJOR := $(shell cat $(ROOT)/../../CMakeLists.txt | \
	grep ^set\(VERSION_MAJOR\ | sed 's/)/ /' | cut -f2 -d' ')
VERSION_MINOR := $(shell cat $(ROOT)/../../CMakeLists.txt | \
	grep ^set\(VERSION_MINOR\ | sed 's/)/ /' | cut -f2 -d' ')
VERSION_PATCH := $(shell cat $(ROOT)/../../CMakeLists.txt | \
	grep ^set\(VERSION_PATCH\ | sed 's/)/ /' | cut -f2 -d' ')
VERSION_TWEAK := $(shell cat $(ROOT)/../../CMakeLists.txt | \
	grep ^set\(VERSION_TWEAK\ | sed 's/)/ /' | cut -f2 -d' ')

################################################################################
# Android Version code
# Increase for each build!
################################################################################
# Play Store actual version (16/03/15): 11
ANDROID_VERSION_CODE = 13


#and for freeminer for skip conflict:
BUILD_NUMBER = 66

ANDROID_VERSION_CODE = $(BUILD_NUMBER)

PROJECT_NAME_C = freeminer

GAMES_VERSION = ""

GIT_CLONE = git clone --recursive --depth 1

################################################################################
# toolchain config for arm old processors
################################################################################
#TARGET_HOST = arm-linux
#TARGET_ABI = armeabi
#TARGET_LIBDIR = armeabi
#TARGET_TOOLCHAIN = arm-linux-androideabi-
#TARGET_CFLAGS_ADDON = -mfloat-abi=softfp -mfpu=vfp
#TARGET_ARCH = arm
#CROSS_PREFIX = arm-linux-androideabi-
#COMPILER_VERSION = 4.8
#HAVE_LEVELDB = 1

################################################################################
# toolchain config for arm new processors
################################################################################
TARGET_HOST = arm-linux
#TARGET_ABI = armeabi-v7a
TARGET_ABI = armeabi-v7a-hard
TARGET_LIBDIR = armeabi-v7a
TARGET_TOOLCHAIN = arm-linux-androideabi-
#TARGET_CFLAGS_ADDON = -mfloat-abi=softfp -mfpu=vfpv3
TARGET_CFLAGS_ADDON = -Ofast -mfpu=vfpv3-d16 -D_NDK_MATH_NO_SOFTFP=1 -mfloat-abi=hard -march=armv7-a
TARGET_CXXFLAGS_ADDON = $(TARGET_CFLAGS_ADDON)
TARGET_LDFLAGS_ADDON = -Wl,--no-warn-mismatch -lm_hard
TARGET_ARCH = armv7
CROSS_PREFIX = arm-linux-androideabi-
COMPILER_VERSION = 4.9
#COMPILER_VERSION = clang3.6
HAVE_LEVELDB = 1
USE_LUAJIT = 1
USE_ENET = 1
APP_STL = gnustl       # gnustl libcxx|libc++ stlport

################################################################################
# toolchain config for little endian mips
################################################################################
ifeq ($(TARGET_MIPS),1)
ifneq ($(NO_AUTO_VERSION),1)
ANDROID_VERSION_CODE := $(shell echo $$(( $(ANDROID_VERSION_CODE) + 2 )))
endif
TARGET_HOST = mipsel-linux
TARGET_ABI = mips
TARGET_LIBDIR = mips
TARGET_TOOLCHAIN = mipsel-linux-android-
TARGET_CFLAGS_ADDON = -Ofast
TARGET_CXXFLAGS_ADDON = $(TARGET_CFLAGS_ADDON)
TARGET_LDFLAGS_ADDON =
TARGET_ARCH = mips32
CROSS_PREFIX = mipsel-linux-android-
HAVE_LEVELDB = 0
endif

################################################################################
# toolchain config for x86
################################################################################
ifeq ($(TARGET_x86),1)
ifneq ($(NO_AUTO_VERSION),1)
ANDROID_VERSION_CODE := $(shell echo $$(( $(ANDROID_VERSION_CODE) + 1 )))
endif
TARGET_HOST = x86-linux
TARGET_ABI = x86
TARGET_LIBDIR = x86
TARGET_TOOLCHAIN = x86-
TARGET_CFLAGS_ADDON = -Ofast
TARGET_CXXFLAGS_ADDON = $(TARGET_CFLAGS_ADDON)
TARGET_LDFLAGS_ADDON =
CROSS_PREFIX = i686-linux-android-
TARGET_ARCH = x86
endif

################################################################################
ASSETS_TIMESTAMP = deps/assets_timestamp

LEVELDB_DIR = $(ROOT)/deps/leveldb/
LEVELDB_LIB = $(LEVELDB_DIR)/out-static/libleveldb.a
LEVELDB_TIMESTAMP = $(LEVELDB_DIR)/timestamp
LEVELDB_TIMESTAMP_INT = $(ROOT)/deps/leveldb_timestamp
LEVELDB_URL_GIT = https://github.com/google/leveldb
LEVELDB_COMMIT = 2d0320a458d0e6a20fff46d5f80b18bfdcce7018

OPENAL_DIR = $(ROOT)/deps/openal-soft/
OPENAL_LIB = $(OPENAL_DIR)libs/$(TARGET_ABI)/libopenal.so
OPENAL_TIMESTAMP = $(OPENAL_DIR)/timestamp
OPENAL_TIMESTAMP_INT = $(ROOT)/deps/openal_timestamp
OPENAL_URL_GIT = https://github.com/apportable/openal-soft

OGG_DIR = $(ROOT)/deps/libvorbis-libogg-android/
OGG_LIB = $(OGG_DIR)libs/$(TARGET_ABI)/libogg.so
VORBIS_LIB = $(OGG_DIR)libs/$(TARGET_ABI)/libogg.so
OGG_TIMESTAMP = $(OGG_DIR)timestamp
OGG_TIMESTAMP_INT = $(ROOT)/deps/ogg_timestamp
OGG_URL_GIT = https://github.com/vincentjames501/libvorbis-libogg-android

IRRLICHT_REVISION = 5122
IRRLICHT_DIR = $(ROOT)/deps/irrlicht/
IRRLICHT_LIB = $(IRRLICHT_DIR)lib/Android/libIrrlicht.a
IRRLICHT_TIMESTAMP = $(IRRLICHT_DIR)timestamp
IRRLICHT_TIMESTAMP_INT = $(ROOT)/deps/irrlicht_timestamp
#IRRLICHT_URL_SVN = http://svn.code.sf.net/p/irrlicht/code/branches/ogl-es/
IRRLICHT_URL_GIT = https://github.com/zaki/irrlicht.git
IRRLICHT_GIT_BRANCH = ogl-es
IRRLICHT_GIT_COMMIT = 58fa0cf
#IRRLICHT_GIT_COMMIT = ogl-es

#IRRLICHT_URL_SVN = http://svn.code.sf.net/p/irrlicht/code/branches/ogl-es@$(IRRLICHT_REVISION)

OPENSSL_VERSION = 1.0.2f
OPENSSL_BASEDIR = openssl-$(OPENSSL_VERSION)
OPENSSL_DIR = $(ROOT)/deps/$(OPENSSL_BASEDIR)/
OPENSSL_LIB = $(OPENSSL_DIR)/libssl.so.1.0.0
OPENSSL_TIMESTAMP = $(OPENSSL_DIR)timestamp
OPENSSL_TIMESTAMP_INT = $(ROOT)/deps/openssl_timestamp
OPENSSL_URL = http://www.openssl.org/source/openssl-$(OPENSSL_VERSION).tar.gz

CURL_VERSION = 7.47.1
CURL_DIR = $(ROOT)/deps/curl-$(CURL_VERSION)
CURL_LIB = $(CURL_DIR)/lib/.libs/libcurl.a
CURL_TIMESTAMP = $(CURL_DIR)/timestamp
CURL_TIMESTAMP_INT = $(ROOT)/deps/curl_timestamp
CURL_URL_HTTP = https://curl.haxx.se/download/curl-${CURL_VERSION}.tar.bz2

GMP_VERSION = 6.1.0
GMP_DIR = $(ROOT)/deps/gmp-$(GMP_VERSION)
GMP_LIB = $(GMP_DIR)/usr/lib/libgmp.so
GMP_TIMESTAMP = $(GMP_DIR)/timestamp
GMP_TIMESTAMP_INT = $(ROOT)/deps/gmp_timestamp
GMP_URL_HTTP = https://gmplib.org/download/gmp/gmp-$(GMP_VERSION).tar.bz2

FREETYPE_DIR = $(ROOT)/deps/freetype2-android/
FREETYPE_LIB = $(FREETYPE_DIR)/Android/obj/local/$(TARGET_ABI)/libfreetype2-static.a
FREETYPE_TIMESTAMP = $(FREETYPE_DIR)timestamp
FREETYPE_TIMESTAMP_INT = $(ROOT)/deps/freetype_timestamp
FREETYPE_URL_GIT = https://github.com/cdave1/freetype2-android

ICONV_VERSION = 1.14
ICONV_DIR = $(ROOT)/deps/libiconv/
ICONV_LIB = $(ICONV_DIR)/lib/.libs/libiconv.so
ICONV_TIMESTAMP = $(ICONV_DIR)timestamp
ICONV_TIMESTAMP_INT = $(ROOT)/deps/iconv_timestamp
ICONV_URL_HTTP = http://ftp.gnu.org/pub/gnu/libiconv/libiconv-$(ICONV_VERSION).tar.gz

SQLITE3_FOLDER = sqlite-amalgamation-3100200
SQLITE3_URL = http://www.sqlite.org/2016/$(SQLITE3_FOLDER).zip

-include $(PATHCFGFILE)

#use interim target variable to switch leveldb on or off
ifeq ($(HAVE_LEVELDB),1)
	LEVELDB_TARGET = $(LEVELDB_LIB)
endif

.PHONY : debug release reconfig delconfig \
	leveldb_download clean_leveldb leveldb\
	irrlicht_download clean_irrlicht irrlicht \
	clean_assets assets sqlite3_download \
	freetype_download clean_freetype freetype \
	apk clean_apk \
	clean_all clean prep_srcdir \
	install_debug install envpaths all \
	manifest clean_manifest\
	$(ASSETS_TIMESTAMP) $(LEVELDB_TIMESTAMP) \
	$(OPENAL_TIMESTAMP) $(OGG_TIMESTAMP) \
	$(IRRLICHT_TIMESTAMP) $(CURL_TIMESTAMP) \
	$(OPENSSL_TIMESTAMP) \
	$(ROOT)/jni/src/android_version.h \
	$(ROOT)/jni/src/android_version_githash.h

default: release

debug : $(PATHCFGFILE)
	export NDEBUG=;                                                            \
	export BUILD_TYPE=debug;                                                   \
	$(MAKE) apk

all : debug release

release : $(PATHCFGFILE)
	@export NDEBUG=1;                                                          \
	export BUILD_TYPE=release;                                                 \
	$(MAKE) apk

reconfig: delconfig
	@$(MAKE) $(PATHCFGFILE)

delconfig :
	$(RM) ${PATHCFGFILE}

$(PATHCFGFILE) :
	@echo "Please specify path of ANDROID NDK";                                \
	echo "e.g. /home/user/android-ndk-r9c/";                                   \
	read ANDROID_NDK ;                                                         \
	if [ ! -d $$ANDROID_NDK ] ; then                                           \
		echo "$$ANDROID_NDK is not a valid folder";                            \
		exit 1;                                                                \
	fi;                                                                        \
	echo "ANDROID_NDK = $$ANDROID_NDK" > ${PATHCFGFILE};                       \
	echo "NDK_MODULE_PATH = $$ANDROID_NDK/toolchains" >> ${PATHCFGFILE};            \
	echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";\
	echo "+ Note: NDK_MODULE_PATH is set to $$ANDROID_NDK/toolchains";              \
	echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";\
	echo "Please specify path of ANDROID SDK";                                 \
	echo "e.g. /home/user/adt-bundle-linux-x86_64-20131030/sdk/";              \
	read SDKFLDR ;                                                             \
	if [ ! -d $$SDKFLDR ] ; then                                               \
		echo "$$SDKFLDR is not a valid folder";                                \
		exit 1;                                                                \
	fi;                                                                        \
	echo "SDKFOLDER = $$SDKFLDR" >> ${PATHCFGFILE};

$(OPENAL_TIMESTAMP) : openal_download
	@LAST_MODIF=$$(find ${OPENAL_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${OPENAL_TIMESTAMP};                                            \
	fi

openal_download :
	@if [ ! -d ${OPENAL_DIR} ] ; then                                          \
		echo "openal sources missing, downloading...";                         \
		mkdir -p ${ROOT}/deps;                                                 \
		cd ${ROOT}/deps ;                                                      \
		${GIT_CLONE} ${OPENAL_URL_GIT} || exit 1;                                 \
	fi

openal : $(OPENAL_LIB)

$(OPENAL_LIB): $(OPENAL_TIMESTAMP)
	+ @REFRESH=0;                                                                \
	if [ ! -e ${OPENAL_TIMESTAMP_INT} ] ; then                                 \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${OPENAL_TIMESTAMP} -nt ${OPENAL_TIMESTAMP_INT} ] ; then              \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};             \
	echo "changed timestamp for openal detected building...";                  \
	cd ${OPENAL_DIR};                                                          \
	ndk-build NDEBUG=${NDEBUG} NDK_MODULE_PATH=${NDK_MODULE_PATH}              \
	APP_ABI=${TARGET_ABI} TARGET_ARCH_ABI=${TARGET_ABI}                        \
	APP_PLATFORM=${APP_PLATFORM} TARGET_CFLAGS+="${TARGET_CFLAGS_ADDON}"       \
	TARGET_LDFLAGS+="${TARGET_LDFLAGS_ADDON}"                                  \
	TARGET_CXXFLAGS+="${TARGET_CXXFLAGS_ADDON}" || exit 1;                     \
	touch ${OPENAL_TIMESTAMP};                                                 \
	touch ${OPENAL_TIMESTAMP_INT};                                             \
	else                                                                       \
		echo "nothing to be done for openal";                                  \
	fi

clean_openal :
	$(RM) -rf ${OPENAL_DIR}

$(OGG_TIMESTAMP) : ogg_download
	@LAST_MODIF=$$(find ${OGG_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${OGG_TIMESTAMP};                                                \
	fi

ogg_download :
	@if [ ! -d ${OGG_DIR} ] ; then                                             \
		echo "ogg sources missing, downloading...";                            \
		mkdir -p ${ROOT}/deps;                                                 \
		cd ${ROOT}/deps ;                                                      \
		${GIT_CLONE} ${OGG_URL_GIT}|| exit 1;                                     \
		cd libvorbis-libogg-android ;                                          \
		patch -p1 < ../../libvorbis-libogg-fpu.patch || exit 1;                \
		sed -i 's-:-?-' jni/Application.mk;                                    \
	fi

ogg : $(OGG_LIB)

$(OGG_LIB): $(OGG_TIMESTAMP)
	+ @REFRESH=0;                                                                \
	if [ ! -e ${OGG_TIMESTAMP_INT} ] ; then                                    \
		echo "${OGG_TIMESTAMP_INT} doesn't exist";                             \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${OGG_TIMESTAMP} -nt ${OGG_TIMESTAMP_INT} ] ; then                    \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};             \
	echo "changed timestamp for ogg detected building...";                     \
	cd ${OGG_DIR};                                                             \
	ndk-build NDEBUG=${NDEBUG} NDK_MODULE_PATH=${NDK_MODULE_PATH}              \
	APP_ABI=${TARGET_ABI} APP_PLATFORM=${APP_PLATFORM}                         \
	TARGET_CFLAGS+="${TARGET_CFLAGS_ADDON}"                                    \
	TARGET_LDFLAGS+="${TARGET_LDFLAGS_ADDON}"                                  \
	TARGET_CXXFLAGS+="${TARGET_CXXFLAGS_ADDON}" || exit 1;                     \
	touch ${OGG_TIMESTAMP};                                                    \
	touch ${OGG_TIMESTAMP_INT};                                                \
	else                                                                       \
		echo "nothing to be done for libogg/libvorbis";                        \
	fi

clean_ogg :
	$(RM) -rf ${OGG_DIR}

$(OPENSSL_TIMESTAMP) : openssl_download
	@LAST_MODIF=$$(find ${OPENSSL_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${OPENSSL_TIMESTAMP};                                            \
	fi

openssl_download :
	@if [ ! -d ${OPENSSL_DIR} ] ; then                                         \
		echo "openssl sources missing, downloading...";                        \
		mkdir -p ${ROOT}/deps;                                                 \
		cd ${ROOT}/deps ;                                                      \
		wget ${OPENSSL_URL} -O ${OPENSSL_BASEDIR}.tar.gz || exit 1;                                         \
		tar -xzf ${OPENSSL_BASEDIR}.tar.gz;                                    \
		cd ${OPENSSL_BASEDIR};                                                 \
		patch -p1 < ../../openssl_arch.patch || true; \
	fi

openssl : $(OPENSSL_LIB)

$(OPENSSL_LIB): $(OPENSSL_TIMESTAMP)
	@REFRESH=0;                                                                \
	if [ ! -e ${OPENSSL_TIMESTAMP_INT} ] ; then                                \
		echo "${OPENSSL_TIMESTAMP_INT} doesn't exist";                         \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${OPENSSL_TIMESTAMP} -nt ${OPENSSL_TIMESTAMP_INT} ] ; then            \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	echo "changed timestamp for openssl detected building...";                 \
	cd ${OPENSSL_DIR};                                                         \
	ln -s ${OPENSSL_DIR} ../openssl;                                           \
	export TOOLCHAIN=/tmp/ndk-${TARGET_HOST}-openssl;                          \
	${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh                    \
	--toolchain=${TARGET_TOOLCHAIN}${COMPILER_VERSION}                         \
	--stl=${APP_STL} \
	--install-dir=$${TOOLCHAIN};                                               \
	export PATH="$${TOOLCHAIN}/bin:$${PATH}";                                  \
	CC=${CROSS_PREFIX}gcc ./Configure android-${TARGET_ARCH} -no-sha0 -DL_ENDIAN;\
	CC=${CROSS_PREFIX}gcc ANDROID_DEV=/tmp/ndk-${TARGET_HOST} make build_libs; \
	touch ${OPENSSL_TIMESTAMP};                                                \
	touch ${OPENSSL_TIMESTAMP_INT};                                            \
	$(RM) -rf $${TOOLCHAIN};                                                   \
	else                                                                       \
		echo "nothing to be done for openssl";                        \
	fi

clean_openssl :
	$(RM) -rf ${OPENSSL_DIR};                                                  \
	$(RM) -rf $(ROOT)/deps/${OPENSSL_BASEDIR}.tar.gz;                          \
	$(RM) -rf $(ROOT)/deps/openssl

$(LEVELDB_TIMESTAMP) : leveldb_download
	@LAST_MODIF=$$(find ${LEVELDB_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${LEVELDB_TIMESTAMP};                                            \
	fi

leveldb_download :
	@if [ ! -d ${LEVELDB_DIR} ] ; then                                         \
		echo "leveldb sources missing, downloading...";                        \
		mkdir -p ${ROOT}/deps;                                                 \
		cd ${ROOT}/deps ;                                                      \
		${GIT_CLONE} ${LEVELDB_URL_GIT} || exit 1; \
		curl https://patch-diff.githubusercontent.com/raw/google/leveldb/pull/341.diff | patch -d ${LEVELDB_DIR} --forward -p1 || true; \
		#cd ${LEVELDB_DIR} || exit 1;                                           \
		#git checkout ${LEVELDB_COMMIT} || exit 1;                              \
	fi


leveldb : $(LEVELDB_LIB)

$(LEVELDB_LIB): $(LEVELDB_TIMESTAMP)
	@REFRESH=0;                                                                \
	if [ ! -e ${LEVELDB_TIMESTAMP_INT} ] ; then                                \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${LEVELDB_TIMESTAMP} -nt ${LEVELDB_TIMESTAMP_INT} ] ; then            \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	export PATH=$${PATH}:${SDKFOLDER}/platform-tools:${ANDROID_NDK};           \
	echo "changed timestamp for leveldb detected building...";                 \
	cd deps/leveldb;                                                           \
	export CROSS_PREFIX=${CROSS_PREFIX};                                       \
	export TOOLCHAIN=/tmp/ndk-${TARGET_HOST}-leveldb;                          \
	${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh                    \
	--toolchain=${TARGET_TOOLCHAIN}${COMPILER_VERSION}                         \
	--stl=${APP_STL} \
	--install-dir=$${TOOLCHAIN};                                               \
	export PATH="$${TOOLCHAIN}/bin:$${PATH}";                                  \
	export CC=${CROSS_PREFIX}gcc;                                              \
	export CXX=${CROSS_PREFIX}g++;                                             \
	export CFLAGS="$${CFLAGS} ${TARGET_CFLAGS_ADDON}";                         \
	export CPPFLAGS="$${CPPFLAGS} ${TARGET_CFLAGS_ADDON}";                     \
	export LDFLAGS="$${LDFLAGS} ${TARGET_LDFLAGS_ADDON}";                      \
	export TARGET_OS=OS_ANDROID_CROSSCOMPILE;                                  \
	$(MAKE) || exit 1; \
	touch ${LEVELDB_TIMESTAMP};                                                \
	touch ${LEVELDB_TIMESTAMP_INT};                                            \
	$(RM) -rf $${TOOLCHAIN};                                                   \
	else                                                                       \
		echo "nothing to be done for leveldb";                                 \
	fi

clean_leveldb :
	$(RM) -rf deps/leveldb

$(FREETYPE_TIMESTAMP) : freetype_download
	@LAST_MODIF=$$(find ${FREETYPE_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${FREETYPE_TIMESTAMP};                                           \
	fi

freetype_download :
	@if [ ! -d ${FREETYPE_DIR} ] ; then                                        \
		echo "freetype sources missing, downloading...";                       \
		mkdir -p ${ROOT}/deps;                                                 \
		cd deps;                                                               \
		${GIT_CLONE} ${FREETYPE_URL_GIT} || exit 1; \
	fi

freetype : $(FREETYPE_LIB)

$(FREETYPE_LIB) : $(FREETYPE_TIMESTAMP)
	+ @REFRESH=0;                                                                \
	if [ ! -e ${FREETYPE_TIMESTAMP_INT} ] ; then                               \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ! -e ${FREETYPE_LIB} ] ; then                                         \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${FREETYPE_TIMESTAMP} -nt ${FREETYPE_TIMESTAMP_INT} ] ; then          \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	mkdir -p ${FREETYPE_DIR};                                                  \
	export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};             \
	echo "changed timestamp for freetype detected building...";                \
	cd ${FREETYPE_DIR}/Android/jni;                                            \
	ndk-build NDEBUG=${NDEBUG} NDK_MODULE_PATH=${NDK_MODULE_PATH}              \
	APP_PLATFORM=${APP_PLATFORM} APP_ABI=${TARGET_ABI}                         \
	TARGET_CFLAGS+="${TARGET_CFLAGS_ADDON}"                                    \
	TARGET_LDFLAGS+="${TARGET_LDFLAGS_ADDON}"                                  \
	TARGET_CXXFLAGS+="${TARGET_CXXFLAGS_ADDON}" || exit 1;                     \
	touch ${FREETYPE_TIMESTAMP};                                               \
	touch ${FREETYPE_TIMESTAMP_INT};                                           \
	else                                                                       \
		echo "nothing to be done for freetype";                                \
	fi

clean_freetype :
	$(RM) -rf ${FREETYPE_DIR}

$(ICONV_TIMESTAMP) : iconv_download
	@LAST_MODIF=$$(find ${ICONV_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${ICONV_TIMESTAMP};                                           \
	fi

iconv_download :
	@if [ ! -d ${ICONV_DIR} ] ; then                                           \
		echo "iconv sources missing, downloading...";                      \
		mkdir -p ${ROOT}/deps;                                             \
		cd ${ROOT}/deps;                                                   \
		wget -O libiconv-${ICONV_VERSION}.tar.gz ${ICONV_URL_HTTP} || exit 1; \
		tar -xzf libiconv-${ICONV_VERSION}.tar.gz || exit 1;               \
		rm libiconv-${ICONV_VERSION}.tar.gz;                               \
		ln -s libiconv-${ICONV_VERSION} libiconv;                          \
		cd ${ICONV_DIR};                                                   \
		patch -p1 < ${ROOT}/libiconv_android.patch;                        \
		patch -p1 < ${ROOT}/libiconv_stdio.patch;                          \
	fi

iconv : $(ICONV_LIB)

$(ICONV_LIB) : $(ICONV_TIMESTAMP)
	@REFRESH=0;                                                                \
	if [ ! -e ${ICONV_TIMESTAMP_INT} ] ; then                                  \
		REFRESH=1;                                                         \
	fi;                                                                        \
	if [ ! -e ${ICONV_LIB} ] ; then                                            \
		REFRESH=1;                                                         \
	fi;                                                                        \
	if [ ${ICONV_TIMESTAMP} -nt ${ICONV_TIMESTAMP_INT} ] ; then                \
		REFRESH=1;                                                         \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	mkdir -p ${ICONV_DIR};                                                     \
	export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};             \
	echo "changed timestamp for iconv detected building...";                   \
	cd ${ICONV_DIR};                                                           \
	\
	export TOOLCHAIN=/tmp/ndk-${TARGET_HOST}-iconv;                            \
	${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh                    \
	--toolchain=${TARGET_TOOLCHAIN}${COMPILER_VERSION}                         \
	--stl=${APP_STL} \
	--install-dir=$${TOOLCHAIN};                                               \
	export PATH="$${TOOLCHAIN}/bin:$${PATH}";                                  \
	export CC=${CROSS_PREFIX}gcc;                                              \
	export CXX=${CROSS_PREFIX}g++;                                             \
	export TARGET_OS=OS_ANDROID_CROSSCOMPILE;                                  \
	./configure --host=${TARGET_HOST} || exit 1;                               \
	sed -i 's/LIBICONV_VERSION_INFO) /LIBICONV_VERSION_INFO) -avoid-version /g' lib/Makefile; \
	grep "iconv_LDFLAGS" src/Makefile;                                         \
	$(MAKE) || exit 1; \
	touch ${ICONV_TIMESTAMP};                                                  \
	touch ${ICONV_TIMESTAMP_INT};                                              \
	rm -rf ${TOOLCHAIN};                                                       \
	else                                                                       \
		echo "nothing to be done for iconv";                               \
	fi

clean_iconv :
	$(RM) -rf ${ICONV_DIR}

#Note: Texturehack patch is required for gpu's not supporting color format
#      correctly. Known bad GPU:
#      -geforce on emulator
#      -Vivante Corporation GC1000 core (e.g. Galaxy Tab 3)

irrlicht_download :
	@if [ ! -d "deps/irrlicht" ] ; then                                        \
		echo "irrlicht sources missing, downloading...";                       \
		mkdir -p ${ROOT}/deps;                                                 \
		cd deps;                                                               \
		#svn co ${IRRLICHT_URL_SVN} irrlicht || exit 1;                         \
		${GIT_CLONE} --depth 200 --branch ${IRRLICHT_GIT_BRANCH} ${IRRLICHT_URL_GIT} irrlicht || exit 1; \
		cd irrlicht;                                                           \
		git checkout ${IRRLICHT_GIT_COMMIT}; \
		mkdir -p lib/Android; \
		patch -p1 < ../../irrlicht-crystax.patch; \
		patch -p1 < ../../irrlicht-touchcount.patch || exit 1;                 \
		patch -p1 < ../../irrlicht-back_button.patch || exit 1;                \
		patch -p1 < ../../irrlicht-texturehack.patch || exit 1;                \
	fi

$(IRRLICHT_TIMESTAMP) : irrlicht_download
	@LAST_MODIF=$$(find ${IRRLICHT_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${IRRLICHT_TIMESTAMP};                                           \
	fi

irrlicht : $(IRRLICHT_LIB)

$(IRRLICHT_LIB): $(IRRLICHT_TIMESTAMP) $(FREETYPE_LIB)
	+ @REFRESH=0;                                                                \
	if [ ! -e ${IRRLICHT_TIMESTAMP_INT} ] ; then                               \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ! -e ${IRRLICHT_LIB} ] ; then                                         \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${IRRLICHT_TIMESTAMP} -nt ${IRRLICHT_TIMESTAMP_INT} ] ; then          \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	mkdir -p ${IRRLICHT_DIR};                                                  \
	export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};             \
	echo "changed timestamp for irrlicht detected building...";                \
	cd deps/irrlicht/source/Irrlicht/Android;                                  \
	ndk-build NDEBUG=${NDEBUG} NDK_MODULE_PATH=${NDK_MODULE_PATH}              \
	APP_ABI=${TARGET_ABI} APP_PLATFORM=${APP_PLATFORM}                         \
	TARGET_CFLAGS+="${TARGET_CFLAGS_ADDON}"                                    \
	TARGET_LDFLAGS+="${TARGET_LDFLAGS_ADDON}"                                  \
	TARGET_CXXFLAGS+="${TARGET_CXXFLAGS_ADDON}" || exit 1;                     \
	touch ${IRRLICHT_TIMESTAMP};                                               \
	touch ${IRRLICHT_TIMESTAMP_INT};                                           \
	else                                                                       \
		echo "nothing to be done for irrlicht";                                \
	fi

clean_irrlicht :
	$(RM) -rf deps/irrlicht

$(CURL_TIMESTAMP) : curl_download
	@LAST_MODIF=$$(find ${CURL_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${CURL_TIMESTAMP};                                               \
	fi

curl_download :
	@if [ ! -d "deps/curl-${CURL_VERSION}" ] ; then                            \
		echo "curl sources missing, downloading...";                           \
		mkdir -p ${ROOT}/deps;                                                 \
		cd deps;                                                               \
		wget ${CURL_URL_HTTP} -O curl-${CURL_VERSION}.tar.bz2 || exit 1;                                       \
		tar -xjf curl-${CURL_VERSION}.tar.bz2 || exit 1;                       \
		rm curl-${CURL_VERSION}.tar.bz2;                                       \
		ln -s curl-${CURL_VERSION} curl;                                       \
	fi

curl : $(CURL_LIB)

$(CURL_LIB): $(CURL_TIMESTAMP) $(OPENSSL_LIB)
	@REFRESH=0;                                                                \
	if [ ! -e ${CURL_TIMESTAMP_INT} ] ; then                                   \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ! -e ${CURL_LIB} ] ; then                                             \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${CURL_TIMESTAMP} -nt ${CURL_TIMESTAMP_INT} ] ; then                  \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	mkdir -p ${CURL_DIR};                                                      \
	export PATH="$${PATH}:${SDKFOLDER}/platform-tools:${ANDROID_NDK}";         \
	echo "changed timestamp for curl detected building...";                    \
	cd deps/curl-${CURL_VERSION};                                              \
	export CROSS_PREFIX=${CROSS_PREFIX};                                       \
	export TOOLCHAIN=/tmp/ndk-${TARGET_HOST}-curl;                             \
	${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh                    \
	--toolchain=${TARGET_TOOLCHAIN}${COMPILER_VERSION}                         \
	--stl=${APP_STL} \
	--install-dir=$${TOOLCHAIN};                                               \
	export PATH="$${TOOLCHAIN}/bin:$${PATH}";                                  \
	export CC=${CROSS_PREFIX}gcc;                                              \
	export CXX=${CROSS_PREFIX}g++;                                             \
	export TARGET_OS=OS_ANDROID_CROSSCOMPILE;                                  \
	export CPPFLAGS="$${CPPFLAGS} -I${OPENSSL_DIR}/include                     \
	-L${OPENSSL_DIR} ${TARGET_CFLAGS_ADDON}";                                  \
	export CFLAGS="$${CFLAGS} ${TARGET_CFLAGS_ADDON}";                         \
	export LDFLAGS="$${LDFLAGS} -L${OPENSSL_DIR} ${TARGET_LDFLAGS_ADDON}";     \
	./configure --host=${TARGET_HOST} --disable-shared --enable-static --with-ssl --enable-ipv6;  \
	$(MAKE) || exit 1; \
	touch ${CURL_TIMESTAMP};                                                   \
	touch ${CURL_TIMESTAMP_INT};                                               \
	$(RM) -rf $${TOOLCHAIN};                                                   \
	else                                                                       \
		echo "nothing to be done for curl";                                    \
	fi

clean_curl :
	$(RM) -rf deps/curl-${CURL_VERSION}                                        \
	$(RM) -f deps/curl

$(GMP_TIMESTAMP) : gmp_download
	@LAST_MODIF=$$(find ${GMP_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${GMP_TIMESTAMP};                                            \
	fi

gmp_download :
	@if [ ! -d "${GMP_DIR}" ] ; then                                           \
		echo "gmp sources missing, downloading...";                        \
		mkdir -p ${ROOT}/deps;                                             \
		cd deps;                                                           \
		wget ${GMP_URL_HTTP} -O gmp-${GMP_VERSION}.tar.bz2 || exit 1;                                    \
		tar -xjf gmp-${GMP_VERSION}.tar.bz2 || exit 1;                     \
		rm gmp-${GMP_VERSION}.tar.bz2;                                     \
		ln -s gmp-${GMP_VERSION} gmp;                                      \
	fi

gmp : $(GMP_LIB)

$(GMP_LIB): $(GMP_TIMESTAMP)
	@REFRESH=0;                                                                \
	if [ ! -e ${GMP_TIMESTAMP_INT} ] ; then                                    \
		REFRESH=1;                                                         \
	fi;                                                                        \
	if [ ! -e ${GMP_LIB} ] ; then                                              \
		REFRESH=1;                                                         \
	fi;                                                                        \
	if [ ${GMP_TIMESTAMP} -nt ${GMP_TIMESTAMP_INT} ] ; then                    \
		REFRESH=1;                                                         \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	mkdir -p ${GMP_DIR};                                                       \
	export PATH="$${PATH}:${SDKFOLDER}/platform-tools:${ANDROID_NDK}";         \
	echo "changed timestamp for gmp detected building...";                     \
	cd deps/gmp-${GMP_VERSION};                                                \
	export CROSS_PREFIX=${CROSS_PREFIX};                                       \
	export TOOLCHAIN=/tmp/ndk-${TARGET_HOST}-gmp;                              \
	${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh                    \
	--toolchain=${TARGET_TOOLCHAIN}${COMPILER_VERSION}                         \
	--stl=${APP_STL} \
	--install-dir=$${TOOLCHAIN};                                               \
	export PATH="$${TOOLCHAIN}/bin:$${PATH}";                                  \
	export CC=${CROSS_PREFIX}gcc;                                              \
	export CXX=${CROSS_PREFIX}g++;                                             \
	export LIBGMP_LDFLAGS="-avoid-version";                                    \
	export LIBGMPXX_LDFLAGS="-avoid-version";                                  \
	./configure --disable-static --host=${TARGET_HOST} --prefix=/usr;          \
	$(MAKE) install DESTDIR=/${GMP_DIR} || exit 1;                             \
	touch ${GMP_TIMESTAMP};                                                    \
	touch ${GMP_TIMESTAMP_INT};                                                \
	$(RM) -rf $${TOOLCHAIN};                                                   \
	else                                                                       \
		echo "nothing to be done for gmp";                                 \
	fi

clean_gmp:
	$(RM) -rf deps/gmp-${GMP_VERSION}                                          \
	$(RM) -f deps/gmp

sqlite3_download: deps/${SQLITE3_FOLDER}/sqlite3.c

deps/${SQLITE3_FOLDER}/sqlite3.c :
	cd deps;                                                                    \
	wget ${SQLITE3_URL} -O ${SQLITE3_FOLDER}.zip;                               \
	unzip ${SQLITE3_FOLDER}.zip;                                                \
	ln -s ${SQLITE3_FOLDER} sqlite

clean_sqlite3:
	cd deps && $(RM) -rf ${SQLITE3_FOLDER} && $(RM) -f ${SQLITE3_FOLDER}.zip && \
	$(RM) -f sqlite

$(ASSETS_TIMESTAMP) : $(IRRLICHT_LIB)
	@mkdir -p ${ROOT}/deps;                                                     \
	for DIRNAME in {builtin,client,doc,fonts,games,mods,po,textures}; do        \
		LAST_MODIF=$$(find ${ROOT}/../../${DIRNAME} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
		if [ $$(basename $$LAST_MODIF) != "timestamp" ]; then               \
			touch ${ROOT}/../../${DIRNAME}/timestamp;                   \
			touch ${ASSETS_TIMESTAMP};                                  \
			echo ${DIRNAME} changed $$LAST_MODIF;                       \
		fi;                                                                 \
	done;                                                                       \
	LAST_MODIF=$$(find ${IRRLICHT_DIR}/media -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${IRRLICHT_DIR}/media/timestamp;                                 \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ ${ROOT}/../../freeminer.conf.example -nt ${ASSETS_TIMESTAMP} ] ; then \
		echo "conf changed";                                                   \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ ${ROOT}/../../README.md -nt ${ASSETS_TIMESTAMP} ] ; then              \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ ! -e $(ASSETS_TIMESTAMP) ] ; then                                     \
		touch $(ASSETS_TIMESTAMP);                                             \
	fi

assets : $(ASSETS_TIMESTAMP) locale
	@REFRESH=0;                                                                \
	if [ ! -e ${ASSETS_TIMESTAMP}.old ] ; then                                 \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${ASSETS_TIMESTAMP} -nt ${ASSETS_TIMESTAMP}.old ] ; then              \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ! -d ${ROOT}/assets ] ; then                                          \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	echo "assets changed, refreshing...";                                      \
	$(MAKE) clean_assets;                                                       \
	mkdir -p ${ROOT}/assets/freeminer; \
	cp ${ROOT}/../../freeminer.conf.example ${ROOT}/assets/freeminer; \
	cp -r ${ROOT}/worlds ${ROOT}/assets/freeminer; \
	cp ${ROOT}/../../README.md ${ROOT}/assets/freeminer; \
	cp -r ${ROOT}/../../builtin ${ROOT}/assets/freeminer; \
	mkdir ${ROOT}/assets/freeminer/client;                                      \
	cp -r ${ROOT}/../../client/shaders ${ROOT}/assets/freeminer/client;         \
	#cp ${ROOT}/../../doc/lgpl-2.1.txt ${ROOT}/assets/freeminer/LICENSE.txt;     \
	mkdir ${ROOT}/assets/freeminer/fonts; \
	cp -r ${ROOT}/../../fonts/*.ttf ${ROOT}/assets/freeminer/fonts/; \
	mkdir ${ROOT}/assets/freeminer/games${GAMES_VERSION}; \
	for game in ${GAMES_TO_COPY};                                              \
	do                                                                         \
	    cp -Lr ${ROOT}/../../games/$$game ${ROOT}/assets/freeminer/games${GAMES_VERSION}/; \
	done;                                                                      \
	mkdir ${ROOT}/assets/freeminer/mods; \
	for mod in ${MODS_TO_COPY}; do                                             \
	    cp -r ${ROOT}/../../mods/$$mod ${ROOT}/assets/freeminer/mods/; \
	done;                                                                      \
	cp -r ${ROOT}/cmake/locale ${ROOT}/assets/freeminer; \
	cp -r ${ROOT}/../../textures ${ROOT}/assets/freeminer; \
	mkdir -p ${ROOT}/assets/freeminer/media; \
	cp -r ${IRRLICHT_DIR}/media/Shaders ${ROOT}/assets/freeminer/media; \
	cd ${ROOT}/assets ||  exit 1;                                              \
	find . -name "timestamp" -exec rm {} \; ;                                  \
	find . \( -name "favoriteservers.json" -o -name "servers_public.json" \) -exec rm {} \; ;                       \
	find . -name "*.blend" -exec rm {} \; ;                                    \
	find . -name "*.xcf" -exec rm {} \; ; \
	find . -name "*~" -exec rm {} \; ;                                         \
	find . -type d -path "*.git" -exec rm -rf {} \; ;                          \
	find . -type d -path "*.svn" -exec rm -rf {} \; ;                          \
	find . -type f -path "*.gitignore" -exec rm -rf {} \; ;                    \
	find . \( -name "lucida_sans_*" -o -name "mono_dejavu_sans_*" -o -name "README.txt" -o -name "game_api.txt" -o -name ".git*" \) -exec rm {} \; ; \
	ls -R | grep ":$$" | sed -e 's/:$$//' -e 's/\.//' -e 's/^\///' > "index.txt"; \
	#find freeminer >"filelist.txt"; \
	find freeminer ! -path '*/games/*' > filelist.txt; \
	find freeminer -path '*/games/*' >> filelist.txt; \
	cp ${ROOT}/${ASSETS_TIMESTAMP} ${ROOT}/${ASSETS_TIMESTAMP}.old;            \
	else                                                                       \
		echo "nothing to be done for assets";                                  \
	fi

clean_assets :
	@$(RM) -r assets


#=== freeminer libs:
.PHONY : default $(MSGPACK_TIMESTAMP) msgpack_download msgpack clean_msgpack \
		$(ICONV_TIMESTAMP) iconv_download iconv clean_iconv \
		$(LUAJIT_TIMESTAMP) luajit luajit_download clean_luajit

MSGPACK_DIR = $(ROOT)/deps/msgpack/
MSGPACK_LIB = $(MSGPACK_DIR)libmsgpackc.a
MSGPACK_TIMESTAMP = $(MSGPACK_DIR)timestamp
MSGPACK_TIMESTAMP_INT = $(ROOT)/deps/msgpack_timestamp
MSGPACK_GIT_URL = https://github.com/msgpack/msgpack-c
MSGPACK_GIT_BRANCH = cpp-1.4.0
MSGPACK_TARGET = $(MSGPACK_DIR)/include/msgpack.hpp

$(MSGPACK_TIMESTAMP) : msgpack_download
	@LAST_MODIF=$$(find ${MSGPACK_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${MSGPACK_TIMESTAMP};                                            \
	fi

msgpack_download :
	@if [ ! -d ${MSGPACK_DIR} ] ; then                                         \
		echo "msgpack sources missing, downloading...";                        \
		mkdir -p ${ROOT}/deps;                                                 \
		cd ${ROOT}/deps ;                                                      \
		${GIT_CLONE} --branch ${MSGPACK_GIT_BRANCH} ${MSGPACK_GIT_URL} ${MSGPACK_DIR} || exit 1; \
	fi
	
msgpack : $(MSGPACK_LIB)

$(MSGPACK_TARGET): $(MSGPACK_TIMESTAMP)
	@REFRESH=0;                                                                \
	if [ ! -e ${MSGPACK_TIMESTAMP_INT} ] ; then                                \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${MSGPACK_TIMESTAMP} -nt ${MSGPACK_TIMESTAMP_INT} ] ; then            \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	export PATH=$${PATH}:${SDKFOLDER}/platform-tools:${ANDROID_NDK};           \
	echo "changed timestamp for msgpack detected building...";                 \
	cd deps/msgpack;                                                           \
	export CROSS_PREFIX=${CROSS_PREFIX};                                       \
	export TOOLCHAIN=/tmp/ndk-${TARGET_HOST}-msgpack;                          \
	${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh                    \
	--toolchain=${TARGET_TOOLCHAIN}${COMPILER_VERSION}                         \
	--stl=${APP_STL} \
	--install-dir=$${TOOLCHAIN};                                               \
	export PATH="$${TOOLCHAIN}/bin:$${PATH}";                                  \
	export CC=${CROSS_PREFIX}gcc;                                              \
	export CXX=${CROSS_PREFIX}g++;                                             \
	export TARGET_OS=OS_ANDROID_CROSSCOMPILE;                                  \
	export CPPFLAGS="$${CPPFLAGS} ${TARGET_CFLAGS_ADDON}";                     \
	export CFLAGS="$${CFLAGS} ${TARGET_CFLAGS_ADDON}";                         \
	export LDFLAGS="$${LDFLAGS} ${TARGET_LDFLAGS_ADDON}";                      \
	cmake -DMSGPACK_CXX11=1 -DMSGPACK_BUILD_EXAMPLES=0 .; \
	$(MAKE) -j${PARALLEL} || exit 1; \
	touch ${MSGPACK_TIMESTAMP};                                                \
	touch ${MSGPACK_TIMESTAMP_INT};                                            \
	else                                                                       \
		echo "nothing to be done for msgpack";                                 \
	fi
	
clean_msgpack :
	$(RM) -rf deps/msgpack

GETTEXT_DIR = $(ROOT)/deps/gettext/
GETTEXT_LIB = $(GETTEXT_DIR)libintl.a
GETTEXT_TIMESTAMP = $(GETTEXT_DIR)timestamp
GETTEXT_TIMESTAMP_INT = $(ROOT)/deps/gettext_timestamp
GETTEXT_URL_GIT=https://github.com/j-jorge/libintl-lite.git

$(GETTEXT_TIMESTAMP) : gettext_download
	@LAST_MODIF=$$(find ${GETTEXT_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${GETTEXT_TIMESTAMP};                                              \
	fi

gettext_download :
	@if [ ! -d ${GETTEXT_DIR} ] ; then                                           \
		echo "gettext sources missing, downloading...";                          \
		mkdir -p ${ROOT}/deps;                                                 \
		cd ${ROOT}/deps ;                                                      \
		${GIT_CLONE} ${GETTEXT_URL_GIT} || exit 1; \
		mv libintl-lite ${GETTEXT_DIR};                            \
		cd ${GETTEXT_DIR};                                                       \
	fi

gettext : $(GETTEXT_LIB)

$(GETTEXT_LIB): $(GETTEXT_TIMESTAMP)
	@REFRESH=0;                                                                \
	if [ ! -e ${GETTEXT_TIMESTAMP_INT} ] ; then                                  \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${GETTEXT_TIMESTAMP} -nt ${GETTEXT_TIMESTAMP_INT} ] ; then                \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	#export PATH=$${PATH}:${SDKFOLDER}/platform-tools:${ANDROID_NDK};           \
	#echo "changed timestamp for gettext detected building...";                   \
	cd $(GETTEXT_DIR);                                                           \
	#export CROSS_PREFIX=${CROSS_PREFIX};                                       \
	#export TOOLCHAIN=/tmp/ndk-${TARGET_HOST}-gettext;                            \
	#${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh                    \
	#--toolchain=${TARGET_TOOLCHAIN}${COMPILER_VERSION}                         \
	#--stl=${APP_STL} \
	#--install-dir=$${TOOLCHAIN};                                               \
	#export PATH="$${TOOLCHAIN}/bin:$${PATH}";                                  \
	#export CXX=${CROSS_PREFIX}g++;                                             \
	#export AR=${CROSS_PREFIX}ar;                                             \
	#export TARGET_OS=OS_ANDROID_CROSSCOMPILE;                                  \
	#export CPPFLAGS="$${CPPFLAGS} ${TARGET_CFLAGS_ADDON}";                     \
	#export CFLAGS="$${CFLAGS} ${TARGET_CFLAGS_ADDON}";                         \
	#export LDFLAGS="$${LDFLAGS} ${TARGET_LDFLAGS_ADDON}";                      \
	touch ${GETTEXT_TIMESTAMP};                                                  \
	touch ${GETTEXT_TIMESTAMP_INT};                                              \
	else                                                                       \
		echo "nothing to be done for gettext";                                   \
	fi

clean_gettext :
	$(RM) -rf $(GETTEXT_DIR)


LUAJIT_DIR = $(ROOT)/deps/luajit/
LUAJIT_LIB = $(LUAJIT_DIR)src/libluajit.a
LUAJIT_TIMESTAMP = $(LUAJIT_DIR)timestamp
LUAJIT_TIMESTAMP_INT = $(ROOT)/deps/luajit_timestamp
LUAJIT_URL_GIT = http://luajit.org/git/luajit-2.0.git


$(LUAJIT_TIMESTAMP) : luajit_download
	@LAST_MODIF=$$(find ${LUAJIT_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${LUAJIT_TIMESTAMP};                                                \
	fi

luajit_download :
	@if [ ! -d ${LUAJIT_DIR} ] ; then                                             \
		echo "luajit sources missing, downloading...";                            \
		mkdir -p ${ROOT}/deps;                                                 \
		cd ${ROOT}/deps ;                                                      \
		git clone --recursive ${LUAJIT_URL_GIT} ${LUAJIT_DIR} || exit 1; \
	fi

luajit : $(LUAJIT_LIB)

$(LUAJIT_LIB): $(LUAJIT_TIMESTAMP)
	@REFRESH=0; \
	if [ ! -e ${LUAJIT_TIMESTAMP_INT} ] ; then \
		echo "${LUAJIT_TIMESTAMP_INT} doesn't exist"; \
		REFRESH=1; \
	fi; \
	if [ ${LUAJIT_TIMESTAMP} -nt ${LUAJIT_TIMESTAMP_INT} ] ; then  \
		REFRESH=1; \
	fi; \
	if [ $$REFRESH -ne 0 ] ; then \
	export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK}; \
	echo "changed timestamp for luajit detected building..."; \
	cd ${LUAJIT_DIR}; \
	export CROSS_PREFIX=${CROSS_PREFIX}; \
	export TOOLCHAIN=/tmp/ndk-${TARGET_HOST}-luajit; \
	${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh \
	--toolchain=${TARGET_TOOLCHAIN}${COMPILER_VERSION} \
	--stl=${APP_STL} \
	--install-dir=$${TOOLCHAIN} --platform=${APP_PLATFORM}; \
	export PATH="$${TOOLCHAIN}/bin:$${PATH}"; \
	export CROSS=${CROSS_PREFIX}; \
	export TARGET_FLAGS="${TARGET_CFLAGS_ADDON}"; \
	export TARGET_LDFLAGS="${TARGET_LDFLAGS_ADDON}"; \
	$(MAKE) -j${PARALLEL} HOST_CC="gcc -m32" BUILDMODE=static; \
	touch ${LUAJIT_TIMESTAMP}; \
	touch ${LUAJIT_TIMESTAMP_INT}; \
	else \
		echo "nothing to be done for libluajit"; \
	fi

clean_luajit :
	$(RM) -rf ${LUAJIT_DIR}

ifeq ($(USE_LUAJIT),1)
LUAJIT_TARGET=$(LUAJIT_LIB)
endif


locale:
	-mkdir -p cmake && cd cmake && cmake ../../.. -DBUILD_SERVER=0 -DNO_BUILD=1 && make translations

clean_locale:
	rm -rf cmake

#==== end of freeminer libs


# $(ICONV_LIB)
apk: $(PATHCFGFILE) assets $(IRRLICHT_LIB) $(CURL_LIB) $(GMP_LIB) $(LEVELDB_TARGET)       \
	$(OPENAL_LIB) $(OGG_LIB) prep_srcdir $(ROOT)/jni/src/android_version.h     \
	$(MSGPACK_TARGET) $(GETTEXT_LIB) $(LUAJIT_TARGET) $(FREETYPE_LIB) \
	$(ROOT)/jni/src/android_version_githash.h sqlite3_download
	@export NDEBUG=$$NDEBUG; $(MAKE) manifest;                                 \
	export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};             \
	export ANDROID_HOME=${SDKFOLDER};                                          \
	mkdir -p ${ROOT}/src;                                                      \
	ndk-build NDK_MODULE_PATH=${NDK_MODULE_PATH}                               \
	GPROF=${GPROF} APP_ABI=${TARGET_ABI} HAVE_LEVELDB=${HAVE_LEVELDB}          \
	USE_LUAJIT=${USE_LUAJIT} \
	USE_ENET=${USE_ENET} \
	GAMES_VERSION=${GAMES_VERSION} \
	PROJECT_NAME_C=${PROJECT_NAME_C} \
	APP_PLATFORM=${APP_PLATFORM}                                               \
	TARGET_LIBDIR=${TARGET_LIBDIR}                                             \
	TARGET_CFLAGS+="${TARGET_CFLAGS_ADDON}"                                    \
	TARGET_LDFLAGS+="${TARGET_LDFLAGS_ADDON}"                                  \
	TARGET_CXXFLAGS+="${TARGET_CXXFLAGS_ADDON}" &&                             \
	ant $$BUILD_TYPE &&                                                        \
	echo "++ Success!" &&                                                      \
	echo "APK: bin/freeminer-$$BUILD_TYPE.apk" &&                               \
	echo "You can install it with \`adb install -r bin/freeminer-$$BUILD_TYPE.apk\`"

prep_srcdir :
	@if [ ! -e ${ROOT}/jni/src ]; then                                         \
		ln -s ${ROOT}/../../src ${ROOT}/jni/src;                           \
	fi

clean_apk : manifest
	@export PATH=$$PATH:${SDKFOLDER}platform-tools:${ANDROID_NDK};             \
	export ANDROID_HOME=${SDKFOLDER};                                          \
	ant clean

install_debug :
	@export PATH=$$PATH:${SDKFOLDER}platform-tools:${ANDROID_NDK};             \
	adb install -r ${ROOT}/bin/freeminer-debug.apk

install :
	@export PATH=$$PATH:${SDKFOLDER}platform-tools:${ANDROID_NDK};             \
	adb install -r ${ROOT}/bin/freeminer-release.apk

envpaths :
	@echo "export PATH=$$PATH:${SDKFOLDER}platform-tools:${ANDROID_NDK}" > and_env;\
	echo "export ANDROID_HOME=${SDKFOLDER}" >> and_env;

clean_all :
	@$(MAKE) clean_apk;                                                        \
	$(MAKE) clean_assets clean_iconv clean_irrlicht clean_leveldb clean_curl clean_openssl \
	clean_msgpack clean_gettext clean_luajit clean_locale \
	clean_openal clean_ogg clean_gmp clean_manifest;                           \
	sleep 1;                                                                   \
	$(RM) -r gen libs obj deps bin Debug and_env

$(ROOT)/jni/src/android_version_githash.h : prep_srcdir
	@export VERSION_FILE=${ROOT}/jni/src/android_version_githash.h;            \
	export VERSION_FILE_NEW=$${VERSION_FILE}.new;                              \
	{                                                                          \
	echo "#ifndef ANDROID_MT_VERSION_GITHASH_H";                               \
	echo "#define ANDROID_MT_VERSION_GITHASH_H";                               \
	export GITHASH=$$(git rev-parse --short=8 HEAD);                           \
	export VERSION_STR="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}"; \
	echo "#define VERSION_GITHASH \"$$VERSION_STR-$$GITHASH-Android\"";        \
	echo "#endif";                                                             \
	} > "$${VERSION_FILE_NEW}";                                                \
	if ! cmp -s $${VERSION_FILE} $${VERSION_FILE_NEW}; then                    \
		echo "android_version_githash.h changed, updating...";             \
		mv "$${VERSION_FILE_NEW}" "$${VERSION_FILE}";                      \
	else                                                                       \
		rm "$${VERSION_FILE_NEW}";                                         \
	fi


$(ROOT)/jni/src/android_version.h : prep_srcdir
	@export VERSION_FILE=${ROOT}/jni/src/android_version.h;                    \
	export VERSION_FILE_NEW=$${VERSION_FILE}.new;                              \
	{                                                                          \
	echo "#ifndef ANDROID_MT_VERSION_H";                                       \
	echo "#define ANDROID_MT_VERSION_H";                                       \
	echo "#define VERSION_MAJOR ${VERSION_MAJOR}";                             \
	echo "#define VERSION_MINOR ${VERSION_MINOR}";                             \
	echo "#define VERSION_PATCH ${VERSION_PATCH}";                             \
	echo "#define VERSION_TWEAK ${VERSION_TWEAK}";                             \
	echo "#define VERSION_STRING STR(VERSION_MAJOR) \".\" STR(VERSION_MINOR) \
	\".\" STR(VERSION_PATCH) \".\" STR(VERSION_TWEAK)"; \
	echo "#endif";                                                             \
	} > $${VERSION_FILE_NEW};                                                  \
	if ! cmp -s $${VERSION_FILE} $${VERSION_FILE_NEW}; then                    \
		echo "android_version.h changed, updating...";                     \
		mv "$${VERSION_FILE_NEW}" "$${VERSION_FILE}";                      \
	else                                                                       \
		rm "$${VERSION_FILE_NEW}";                                         \
	fi

manifest :
	@BASE_VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}"; \
	if [ "${NDEBUG}x" != "x" ] ; then                                          \
		DBG='';                                                                \
		DBG_FLAG="android:debuggable=\"false\"";                               \
	else                                                                       \
		DBG="<uses-permission android:name=\"android.permission.SET_DEBUG_APP\" />"; \
		DBG_FLAG="android:debuggable=\"true\"";                                \
	fi;                                                                        \
	cat ${ROOT}/AndroidManifest.xml.template |                                 \
	sed "s/###ANDROID_VERSION###/${ANDROID_VERSION_CODE}/g" |                  \
	sed "s/###BUILD_NUMBER###/${BUILD_NUMBER}/g" | \
	sed "s/###BASE_VERSION###/$$BASE_VERSION/g" |                              \
	sed -e "s@###DEBUG_BUILD###@$$DBG@g" |                                     \
	sed -e "s@###DEBUG_FLAG###@$$DBG_FLAG@g" >${ROOT}/AndroidManifest.xml

clean_manifest :
	rm -rf ${ROOT}/AndroidManifest.xml

clean : clean_apk clean_assets
