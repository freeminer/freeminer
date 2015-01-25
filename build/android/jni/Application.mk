NDK_TOOLCHAIN_VERSION := 4.9
APP_OPTIM := release
APP_ABI := armeabi-v7a

APP_PLATFORM := android-10
APP_MODULES := Freeminer
APP_STL := gnustl_static

APP_CPPFLAGS += -std=c++0x -fexceptions
APP_GNUSTL_FORCE_CPP_FEATURES := rtti
