@echo off
REM Build + test voi MSVC (chay trong "x64 Native Tools Command Prompt"; OPENSSL_ROOT_DIR tro toi OpenSSL 3.x, vd vcpkg)
cd /d "%~dp0.."
if not exist build_windows_msvc mkdir build_windows_msvc
cd build_windows_msvc
cmake .. -A x64 || exit /b 1
cmake --build . --config Release || exit /b 1
ctest -C Release --output-on-failure
