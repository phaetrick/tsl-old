#!/bin/bash
# Builds the AU state harness. The harness links nothing from the plugin — it
# dlopens the bundle at run time — so this is a two-file compile.
set -e
cd "$(dirname "$0")"

OUT="${1:-auhost}"

clang++ -std=c++20 -O1 -g -arch arm64 \
  -Wall -Wno-unused-function -Wno-deprecated-declarations \
  main.cpp AUHost.cpp \
  -framework AudioToolbox -framework CoreFoundation -framework AudioUnit \
  -o "$OUT"

echo "built $(pwd)/$OUT"
