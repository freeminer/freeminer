# NDK_TOOLCHAIN_VERSION := clang3.3

APP_PLATFORM := android-10
APP_MODULES := Freeminer
APP_STL := gnustl_static

APP_CPPFLAGS += -fexceptions
APP_GNUSTL_FORCE_CPP_FEATURES := rtti
