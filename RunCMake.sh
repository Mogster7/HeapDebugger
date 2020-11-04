#!/usr/bin/env bash

BUILD_CONFIG=${1:-Debug}
BUILD_DIR=${2:-build-unix}

mkdir -p $BUILD_DIR
echo "cmake -G \"Unix Makefiles\" -DCMAKE_BUILD_TYPE=$BUILD_CONFIG -S . -B$BUILD_DIR"
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$BUILD_CONFIG -S . -B$BUILD_DIR 

cd $BUILD_DIR
cmake --build . --config $BUILD_CONFIG
