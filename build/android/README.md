#Download and unpack to home dir:

##Manual way:
https://developer.android.com/tools/sdk/ndk/index.html
http://developer.android.com/sdk/index.html
run Android SDK Manager
~/android-sdk-linux/tools/android
 and install
  Android SDK build-tools 23.0.1
  API 15 - SDK platform


##or semi-aumomatic way: (todo: make .sh)


```bash
cd ~

sudo apt-get install -y default-jdk android-tools-adb ant m4 gcc-multilib lib32z1
wget https://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.bin
chmod +x android-ndk-r10e-linux-x86_64.bin
./android-ndk-r10e-linux-x86_64.bin
wget https://dl.google.com/android/android-sdk_r24.3.4-linux.tgz
tar xf android-sdk_r24.3.4-linux.tgz
echo "yyyyyy" | ~/android-sdk-linux/tools/android update sdk --no-ui
echo y | ~/android-sdk-linux/tools/android update sdk --no-ui --filter platform-tool,android-15,build-tools-23.0.1

```


#Build:

run make in freeminer/build/android , answer to questions, it will create path.cfg with
```
ANDROID_NDK = /home/user/android-ndk-r10e
NDK_MODULE_PATH = /home/user/android-ndk-r10e/toolchains
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
