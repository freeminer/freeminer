Download and unpack to home dir:
https://developer.android.com/tools/sdk/ndk/index.html
http://developer.android.com/sdk/index.html

run Android SDK Manager
~/adt-bundle-linux-x86_64-20140702/sdk/tools/android
 and install
  API 10 - SDK platform
  API 10 - Google APIs

run make in freeminer/build/android , answer to questions, it will create path.cfg with
ANDROID_NDK = /home/user/android-ndk-r10d
NDK_MODULE_PATH = /home/user/android-ndk-r10d/toolchains
SDKFOLDER = /home/user/adt-bundle-linux-x86_64-20140702/sdk/

after build

generate key once:
keytool -genkey -v -keystore freeminer-release-key.keystore -alias freeminer -keyalg RSA -keysize 2048 -validity 20000

sign after each build:
jarsigner -verbose -sigalg SHA1withRSA -digestalg SHA1 -keystore freeminer-release-key.keystore bin/freeminer-release-unsigned.apk freeminer

upload to device:
adb install -r bin/freeminer-release-unsigned.apk
