#!/bin/bash
cd /Users/patrickropohl/programming/voltaic_iplug2
rm -rf cmake-build-ios
cmake -S . -B cmake-build-ios -G Xcode \
  -DCMAKE_TOOLCHAIN_FILE=/Users/patrickropohl/programming/sources/iPlug2/Dependencies/cmake/ios.toolchain.cmake \
  -DPLATFORM=SIMULATORARM64 \
  -DDEPLOYMENT_TARGET=16.0
xcodebuild -project cmake-build-ios/Voltaic.xcodeproj \
  -scheme Voltaic-ios-app -sdk iphonesimulator \
  -configuration Debug build 2>&1 | tail -5
