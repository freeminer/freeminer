NDK_TOOLCHAIN_VERSION := 4.9

APP_PLATFORM := android-14
APP_MODULES := freeminer
APP_STL := gnustl_static

#NDK_TOOLCHAIN_VERSION := clang3.5      # broken curl
#APP_STL := c++_static

APP_CPPFLAGS += -std=c++0x -fexceptions
APP_GNUSTL_FORCE_CPP_FEATURES := rtti
