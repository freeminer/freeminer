NDK_TOOLCHAIN_VERSION := clang3.4

APP_PLATFORM := android-9
APP_MODULES := minetest
APP_STL := gnustl_static

APP_CPPFLAGS += -std=c++0x -fexceptions
APP_GNUSTL_FORCE_CPP_FEATURES := rtti
