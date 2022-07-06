NDK_TOOLCHAIN_VERSION := 4.9

#APP_PLATFORM := android-15
APP_MODULES := freeminer
APP_STL := gnustl_static

#NDK_TOOLCHAIN_VERSION := clang3.6      # broken curl

#APP_STL := c++_shared

APP_CPPFLAGS += -std=c++11 -fexceptions
APP_GNUSTL_FORCE_CPP_FEATURES := rtti
