#!/usr/bin/env bash

set -xeu

pushd ../android
ANDROID_HOME=${ANDROID_HOME-~/Android/Sdk/} ./gradlew assembleRelease

PATH_IN=$(pwd)/app/build/outputs/apk/release/
PATH_OUT=${PATH_IN}
VERSION=$(git describe)
PASS=${PASS-123456}
PATH_KEY=${PATH_KEY-}
NAME=app-arm64-v8a-release

[ -f ${PATH_KEY}release-key.jks ] || keytool -genkeypair -v -keystore ${PATH_KEY}release-key.jks -keyalg RSA -keysize 2048 -validity 10000 -alias release-key \
    -dname "CN=Common Name, OU=Unit, O=Org, L=City, ST=State, C=Country" \
    -noprompt \
    -storepass ${PASS}

zipalign -f -v -p 4 ${PATH_IN}${NAME}-unsigned.apk ${PATH_IN}${NAME}-unsigned-aligned.apk

apksigner sign --ks ${PATH_KEY}release-key.jks \
    --ks-key-alias release-key \
    --ks-pass pass:${PASS} \
    --key-pass pass:${PASS} \
    --out ${PATH_OUT}${NAME}-${VERSION}.apk ${PATH_IN}${NAME}-unsigned-aligned.apk
ln -vfs ${NAME}-${VERSION}.apk ${PATH_OUT}${NAME}-latest.apk

echo Built: ${PATH_OUT}${NAME}-${VERSION}.apk

popd

[ -x ./deploy.sh ] && source deploy.sh
