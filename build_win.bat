@echo off
setlocal

set BUILD_DIR=build-win
set CONFIG=Release

cmake -S . -B %BUILD_DIR% -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%CONFIG%

cmake --build %BUILD_DIR%