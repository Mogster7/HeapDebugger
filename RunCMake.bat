@echo off

set BUILD_CONFIG=%1
set BUILD_DIR=%2

IF NOT DEFINED BUILD_CONFIG (SET BUILD_CONFIG="Debug") 
IF NOT DEFINED BUILD_DIR (SET BUILD_DIR="build-msvc") 

cmake -G "Visual Studio 16 2019" -A x64 -S . -B %BUILD_DIR%
cd %BUILD_DIR%
cmake --build . --target HeapDebugger --config %BUILD_CONFIG%
