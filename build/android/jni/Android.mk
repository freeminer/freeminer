LOCAL_PATH := $(call my-dir)/..

LOCAL_ADDRESS_SANITIZER:=true

include $(CLEAR_VARS)
LOCAL_MODULE := Irrlicht
LOCAL_SRC_FILES := deps/irrlicht/lib/Android/libIrrlicht.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := LevelDB
LOCAL_SRC_FILES := deps/leveldb/libleveldb.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libiconv
LOCAL_SRC_FILES := deps/libiconv/obj/local/armeabi/libiconv.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := msgpack
LOCAL_SRC_FILES := deps/msgpack-c/build/lib/libmsgpack.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := freetype
LOCAL_SRC_FILES := deps/freetype/build/lib/libfreetype.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := Freeminer

LOCAL_CPP_FEATURES += exceptions

LOCAL_CFLAGS := -D_IRR_ANDROID_PLATFORM_ -DANDROID -pipe -fstrict-aliasing -DHAS_SOCKLEN_T

ifndef NDEBUG
LOCAL_CFLAGS += -g -D_DEBUG -O0 -fno-omit-frame-pointer -fno-function-sections
else
LOCAL_CFLAGS += -fexpensive-optimizations -O3
endif

# LOCAL_CFLAGS += -fsanitize=address
# LOCAL_LDFLAGS += -fsanitize=address

ifeq ($(TARGET_ARCH_ABI),x86)
LOCAL_CFLAGS += -fno-stack-protector
endif

LOCAL_C_INCLUDES := jni/src jni/src/sqlite jni/src/script jni/src/lua/src jni/src/json jni/src/cguittfont jni/src/enet/include \
	deps/leveldb/include deps/irrlicht/include deps/msgpack-c/build/include deps/freetype/build/include/freetype2/ deps/libiconv/include/

LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/jni/src/*.cpp) $(wildcard $(LOCAL_PATH)/jni/src/util/*.cpp)
LOCAL_SRC_FILES += jni/src/cguittfont/xCGUITTFont.cpp

# lua api
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/jni/src/script/common/*.cpp) $(wildcard $(LOCAL_PATH)/jni/src/script/cpp_api/*.cpp) \
	$(wildcard $(LOCAL_PATH)/jni/src/script/lua_api/*.cpp) $(wildcard $(LOCAL_PATH)/jni/src/script/*.cpp) \
	jni/src/script/lua_api/marshall.c

# lua
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/jni/src/lua/src/*.c)

# sqlite
LOCAL_SRC_FILES += jni/src/sqlite/sqlite3.c

# jthread
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/jni/src/jthread/pthread/*.cpp)

# json
LOCAL_SRC_FILES += jni/src/json/jsoncpp.cpp

# enet
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/jni/src/enet/*.c)

LOCAL_SRC_FILES := $(filter-out %/sound_openal.cpp, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out %/lua.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out %/luac.c, $(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(filter-out %/win32.c, $(LOCAL_SRC_FILES))

LOCAL_LDLIBS := -lEGL -llog -lGLESv1_CM -lGLESv2 -lz -landroid

LOCAL_STATIC_LIBRARIES := Irrlicht LevelDB msgpack freetype libiconv android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
