LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
TARGET_ARCH_ABI := armeabi-v7a
LOCAL_MODULE    := iconv
LOCAL_CFLAGS    := \
    -Wno-multichar \
    -D_ANDROID \
    -DLIBDIR="\"c\"" \
    -DBUILDING_LIBICONV \
    -DIN_LIBRARY
LOCAL_C_INCLUDES := \
    .. \
    ../include \
    ../lib \
    ../libcharset/include
LOCAL_SRC_FILES := \
    ../lib/iconv.c \
    ../lib/relocatable.c \
    ../libcharset/lib/localcharset.c
include $(BUILD_STATIC_LIBRARY)
