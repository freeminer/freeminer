#NDK_TOOLCHAIN_VERSION := clang3.4      # broken curl
NDK_TOOLCHAIN_VERSION := 4.8

APP_PLATFORM := android-9
APP_MODULES := freeminer
APP_STL := gnustl_static

APP_CPPFLAGS += -std=c++0x -fexceptions
APP_GNUSTL_FORCE_CPP_FEATURES := rtti
