#Download and unpack to home dir:

##Manual way:
https://developer.android.com/tools/sdk/ndk/index.html
http://developer.android.com/sdk/index.html
run Android SDK Manager
~/android-sdk-linux/tools/android
 and install
  Android SDK build-tools 23.0.1
  API 15 - SDK platform
  API 21 - SDK platform


##or automatic way:


```bash
build.sh
```


#Build:

run make in freeminer/build/android , answer to questions, it will create path.cfg with
```
ANDROID_NDK = /home/user/android-ndk-r11
NDK_MODULE_PATH = /home/user/android-ndk-r11/toolchains
SDKFOLDER = /home/user/android-sdk-linux/
```

##After build

generate key once:
```
keytool -genkey -v -keystore freeminer-release-key.keystore -alias freeminer -keyalg RSA -keysize 2048 -validity 20000
```

sign after each build:
```
jarsigner -verbose -sigalg SHA1withRSA -digestalg SHA1 -keystore freeminer-release-key.keystore bin/freeminer-release-unsigned.apk freeminer
```

upload to device:
```
adb install -r bin/freeminer-release-unsigned.apk
```
