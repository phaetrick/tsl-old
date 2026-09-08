#!/bin/bash
cd /Users/patrickropohl/programming/grainstorm_iplug2
rm -rf cmake-build-ios
cmake -S . -B cmake-build-ios -G Xcode \
  -DCMAKE_TOOLCHAIN_FILE=/Users/patrickropohl/programming/sources/iPlug2/Dependencies/cmake/ios.toolchain.cmake \
  -DPLATFORM=OS64 \
  -DDEPLOYMENT_TARGET=16.0
xcodebuild -project cmake-build-ios/Grainstorm.xcodeproj \
  -scheme Grainstorm-ios-app -sdk iphoneos \
  -configuration Debug \
  CODE_SIGN_STYLE=Automatic \
  DEVELOPMENT_TEAM=ZUGJCP8877 \
  -allowProvisioningUpdates \
  build 2>&1 | tail -5
