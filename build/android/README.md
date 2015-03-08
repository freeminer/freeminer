Download and unpack to home dir:

Manual way:
https://developer.android.com/tools/sdk/ndk/index.html
http://developer.android.com/sdk/index.html
run Android SDK Manager
~/android-sdk-linux/tools/android
 and install
  API 10 - SDK platform
  API 10 - Google APIs


or semi-aumomatic way: (todo: make .sh)


cd ~
wget https://dl.google.com/android/ndk/android-ndk-r10d-linux-x86_64.bin
chmod +x android-ndk-r10d-linux-x86_64.bin
./android-ndk-r10d-linux-x86_64.bin
wget https://dl.google.com/android/android-sdk_r24.0.2-linux.tgz
tar xf android-sdk_r24.0.2-linux.tgz
# press y here:
~/android-sdk-linux/tools/android update sdk --no-ui --filter platform-tool,android-10,build-tools-21.1.2


run make in freeminer/build/android , answer to questions, it will create path.cfg with
ANDROID_NDK = /home/user/android-ndk-r10d
NDK_MODULE_PATH = /home/user/android-ndk-r10d/toolchains
SDKFOLDER = /home/user/android-sdk-linux/

after build

generate key once:
keytool -genkey -v -keystore freeminer-release-key.keystore -alias freeminer -keyalg RSA -keysize 2048 -validity 20000

sign after each build:
jarsigner -verbose -sigalg SHA1withRSA -digestalg SHA1 -keystore freeminer-release-key.keystore bin/freeminer-release-unsigned.apk freeminer

upload to device:
adb install -r bin/freeminer-release-unsigned.apk
