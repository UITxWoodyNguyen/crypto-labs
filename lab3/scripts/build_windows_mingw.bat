@echo off
REM Build + test voi MinGW-w64 (can cmake, mingw32-make, OpenSSL 3.x; dat OPENSSL_ROOT_DIR neu can)
cd /d "%~dp0.."
if not exist build_windows_gcc mkdir build_windows_gcc
cd build_windows_gcc
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build . || exit /b 1
ctest --output-on-failure
