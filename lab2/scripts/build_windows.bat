@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ===========================================================================
REM build_windows.bat - Build Lab 2 (aestool) on Windows with MSVC, GCC, or Clang
REM
REM Usage (from lab2 folder):
REM     scripts\build_windows.bat [compiler] [clean]
REM
REM Compilers:
REM     msvc    - Microsoft Visual C++ (default, requires x64 Native Tools)
REM     gcc/mingw - MinGW-w64 GCC (from MSYS2 mingw64/ucrt64)
REM     clang   - Clang (from MSYS2 or LLVM)
REM
REM Examples:
REM     scripts\build_windows.bat msvc
REM     scripts\build_windows.bat gcc
REM     scripts\build_windows.bat mingw
REM     scripts\build_windows.bat clang
REM     scripts\build_windows.bat msvc clean
REM
REM Env vars (optional):
REM     CRYPTOPP_ROOT     Crypto++ root (default: D:\UIT\NT219\libs\cryptopp)
REM     OPENSSL_ROOT_DIR  OpenSSL root per compiler (auto-detected if not set)
REM     MSVC_RUNTIME      MultiThreaded (/MT, default) or MultiThreadedDLL (/MD)
REM ===========================================================================

cd /d "%~dp0\.."

REM Parse arguments
set "COMPILER="
set "ACTION="

:PARSE_ARGS
if "%~1"=="" goto :DONE_PARSE
if /I "%~1"=="msvc"  set "COMPILER=msvc"  & shift & goto :PARSE_ARGS
if /I "%~1"=="gcc"   set "COMPILER=gcc"   & shift & goto :PARSE_ARGS
if /I "%~1"=="mingw" set "COMPILER=gcc"   & shift & goto :PARSE_ARGS
if /I "%~1"=="clang" set "COMPILER=clang" & shift & goto :PARSE_ARGS
if /I "%~1"=="clean" set "ACTION=clean"   & shift & goto :PARSE_ARGS
echo [WARN] Unknown argument: %~1
shift
goto :PARSE_ARGS

:DONE_PARSE

REM Defaults
if "%COMPILER%"=="" set "COMPILER=msvc"

REM Handle clean action
if /I "%ACTION%"=="clean" (
    echo Cleaning build directories...
    for /d %%D in (build_windows_*) do (
        echo Removing %%D
        rmdir /s /q "%%D" 2>nul
    )
    exit /b 0
)

REM Validate compiler choice
set "VALID_COMPILER=0"
if /I "%COMPILER%"=="msvc"   set "VALID_COMPILER=1"
if /I "%COMPILER%"=="gcc"    set "VALID_COMPILER=1"
if /I "%COMPILER%"=="clang"  set "VALID_COMPILER=1"
if "%VALID_COMPILER%"=="0" (
    echo [ERROR] Invalid compiler: %COMPILER%
    echo Usage: scripts\build_windows.bat ^<msvc^|gcc^|mingw^|clang^> [clean]
    exit /b 1
)

REM Check for cmake
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH. Install: winget install Kitware.CMake
    exit /b 1
)

REM Set compiler-specific variables
if /I "%COMPILER%"=="msvc" (
    set "BUILD_DIR=build_windows_msvc"
    set "GENERATOR=-G \"Visual Studio 17 2022\" -A x64"
    set "CMAKE_CXX_COMPILER="
    set "OPENSSL_ROOT_DIR=D:/UIT/NT219/libs/openssl/msvc"
    if "%MSVC_RUNTIME%"=="" set "MSVC_RUNTIME=MultiThreaded"
    set "RUNTIME_FLAG=-DCMAKE_MSVC_RUNTIME_LIBRARY=%MSVC_RUNTIME%"
    REM Check for cl.exe
    where cl >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] MSVC compiler (cl.exe) not found.
        echo Run from "x64 Native Tools Command Prompt for VS 2022"
        exit /b 1
    )
)

if /I "%COMPILER%"=="gcc" (
    set "BUILD_DIR=build_windows_gcc"
    set "GENERATOR=-G \"MinGW Makefiles\""
    set "CMAKE_CXX_COMPILER=-DCMAKE_CXX_COMPILER=g++.exe"
    set "OPENSSL_ROOT_DIR=D:/UIT/NT219/libs/openssl/gcc"
    set "RUNTIME_FLAG="
    where g++.exe >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] g++.exe not found in PATH.
        echo Use MSYS2 mingw64 shell: C:\msys64\mingw64\bin\g++.exe
        exit /b 1
    )
)

if /I "%COMPILER%"=="clang" (
    set "BUILD_DIR=build_windows_clang"
    set "GENERATOR=-G \"MinGW Makefiles\""
    set "CMAKE_CXX_COMPILER=-DCMAKE_CXX_COMPILER=clang++.exe"
    set "OPENSSL_ROOT_DIR=D:/UIT/NT219/libs/openssl/clang"
    set "RUNTIME_FLAG="
    where clang++.exe >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] clang++.exe not found in PATH.
        echo Use MSYS2 mingw64 shell: C:\msys64\mingw64\bin\clang++.exe
        exit /b 1
    )
)

echo.
echo ================================================================================
echo Building Lab 2 - AES-128-CTR (aestool)
echo Compiler: %COMPILER%
echo Build dir: %BUILD_DIR%
echo OpenSSL root: %OPENSSL_ROOT_DIR%
echo ================================================================================
echo.

REM Configure
echo == 1/4 Configure ==
cmake -S . -B "%BUILD_DIR%" %GENERATOR% %CMAKE_CXX_COMPILER% ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DOPENSSL_ROOT_DIR="%OPENSSL_ROOT_DIR%" ^
    %RUNTIME_FLAG%
if errorlevel 1 (
    echo [ERROR] Configure failed.
    exit /b 1
)

REM Build
echo == 2/4 Build ==
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

REM Test
echo == 3/4 Test (ctest) ==
pushd "%BUILD_DIR%"
ctest -C Release --output-on-failure
set "CTEST_RC=!ERRORLEVEL!"
popd
if not "!CTEST_RC!"=="0" (
    echo [ERROR] Tests failed.
    exit /b 1
)

REM KAT
echo == 4/4 KAT (Known Answer Tests) ==
set "EXE=%BUILD_DIR%\aestool.exe"
if not exist "!EXE!" set "EXE=%BUILD_DIR%\Release\aestool.exe"
if not exist "!EXE!" (
    echo [ERROR] Executable not found: !EXE!
    exit /b 1
)
if not exist "Output\windows\kat" mkdir "Output\windows\kat"
"!EXE!" --kat kat\vectors.json --out Output\windows\kat\result.txt
if errorlevel 1 (
    echo [ERROR] KAT failed.
    exit /b 1
)

echo.
echo ================================================================================
echo SUCCESS! Binary: %CD%\!EXE!
echo ================================================================================
exit /b 0

