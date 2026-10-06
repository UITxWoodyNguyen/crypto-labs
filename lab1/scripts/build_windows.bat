@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM ===========================================================================
REM build_windows.bat - Build + test Lab 1 (aestool) bang MSVC, dung Crypto++ co san
REM
REM Cach dung (trong cmd da co "cl", vi du x64 Native Tools Command Prompt):
REM     scripts\build_windows.bat [clean]
REM
REM Bien moi truong tuy chon (khong dat thi dung mac dinh / tu dong tim):
REM     CRYPTOPP_ROOT         thu muc source Crypto++     (mac dinh D:\UIT\NT219\libs\cryptopp)
REM     MSVC_RUNTIME          MultiThreaded (/MT, mac dinh) hoac MultiThreadedDLL (/MD)
REM ===========================================================================

cd /d "%~dp0\..\.."

if "%CRYPTOPP_ROOT%"=="" set "CRYPTOPP_ROOT=D:\UIT\NT219\libs\cryptopp"
if "%MSVC_RUNTIME%"=="" set "MSVC_RUNTIME=MultiThreaded"

where cl >nul 2>nul
if errorlevel 1 goto :no_cl
where cmake >nul 2>nul
if errorlevel 1 goto :no_cmake

if /I "%~1"=="clean" (
  echo Xoa thu muc build...
  if exist build rmdir /s /q build
)

REM ---- Tim include dir (thu muc cha cua "cryptopp") de chuyen cho CMake module ----
set "INC=%CRYPTOPP_INCLUDE_DIR%"
if not "%INC%"=="" goto :inc_ok
for %%I in ("%CRYPTOPP_ROOT%\..") do set "PARENT=%%~fI"
if exist "%PARENT%\cryptopp\aes.h" set "INC=%PARENT%"
if "%INC%"=="" if exist "%CRYPTOPP_ROOT%\include\cryptopp\aes.h" set "INC=%CRYPTOPP_ROOT%\include"
if "%INC%"=="" if exist "%CRYPTOPP_ROOT%\include\aes.h" set "INC=%CRYPTOPP_ROOT%\include"
if "%INC%"=="" goto :no_inc
:inc_ok

REM ---- Tim file .lib (x64, Release) de chuyen cho CMake module ----
set "LIBFILE=%CRYPTOPP_LIBRARY%"
if not "%LIBFILE%"=="" goto :lib_ok
for %%F in ("%CRYPTOPP_ROOT%\x64\Output\Release\cryptlib.lib" "%CRYPTOPP_ROOT%\library\msvc\cryptlib.lib" "%CRYPTOPP_ROOT%\library\msvc\cryptopp.lib") do (
  if exist "%%~F" if "!LIBFILE!"=="" set "LIBFILE=%%~F"
)
if "!LIBFILE!"=="" if exist "%CRYPTOPP_ROOT%\library\msvc" (
  for /r "%CRYPTOPP_ROOT%\library\msvc" %%F in (*.lib) do if "!LIBFILE!"=="" set "LIBFILE=%%F"
)
if "!LIBFILE!"=="" goto :no_lib
:lib_ok

REM ---- Doi \ thanh / cho CMake ----
set "INC_CM=!INC:\=/!"
set "LIB_CM=!LIBFILE:\=/!"

REM ---- Chon generator: Luon dung Visual Studio generator neu co cl (MSVC) ----
set "GEN="
REM Neu co cl thi dung generator mac dinh (Visual Studio), khong dung Ninja
where ninja >nul 2>nul
if not errorlevel 1 (
    REM Co ninja nhung uu tien MSVC neu dang trong x64 Native Tools
    where cl >nul 2>nul
    if errorlevel 1 set "GEN=-G Ninja"
)

echo.
echo Crypto++ include : !INC_CM!
echo Crypto++ library : !LIB_CM!
echo MSVC runtime     : %MSVC_RUNTIME%
echo Generator        : %GEN%
echo.

echo == 1/4 Cau hinh ==
set "OPENSSL_ROOT_DIR=D:/UIT/NT219/libs/openssl/msvc"
cmake -S . -B build %GEN% -DCMAKE_BUILD_TYPE=Release -DCRYPTOPP_INCLUDE_DIR="!INC_CM!" -DCRYPTOPP_LIBRARY="!LIB_CM!" -DCMAKE_MSVC_RUNTIME_LIBRARY=%MSVC_RUNTIME%
if errorlevel 1 goto :fail

echo == 2/4 Build ==
cmake --build build --config Release
if errorlevel 1 goto :fail

echo == 3/4 ctest ==
pushd build
ctest -C Release --output-on-failure
set "RC=!ERRORLEVEL!"
popd
if not "!RC!"=="0" goto :fail

echo == 4/4 KAT ==
set "EXE=build\lab1\Release\aestool.exe"
if not exist "!EXE!" set "EXE=build\lab1\aestool.exe"
"!EXE!" --kat lab1\kat\vectors.json
if errorlevel 1 goto :fail

echo.
echo Xong. Binary: %CD%\!EXE!
exit /b 0

:no_cl
echo [LOI] Khong thay "cl". Hay mo "x64 Native Tools Command Prompt for VS" roi chay lai.
exit /b 1
:no_cmake
echo [LOI] Khong thay "cmake" trong PATH. Cai dat bang: winget install Kitware.CMake
exit /b 1
:no_inc
echo [LOI] Khong tim thay cryptopp\aes.h hoac aes.h. Dat CRYPTOPP_INCLUDE_DIR la thu muc CHA cua thu muc "cryptopp".
echo        Da thu: "%PARENT%\cryptopp\aes.h", "%CRYPTOPP_ROOT%\include\cryptopp\aes.h", "%CRYPTOPP_ROOT%\include\aes.h"
exit /b 1
:no_lib
echo [LOI] Khong tim thay file .lib cua Crypto++ ban x64 Release trong "%CRYPTOPP_ROOT%".
echo        Dat bien CRYPTOPP_LIBRARY=duong\dan\toi\cryptlib.lib roi chay lai.
exit /b 1
:fail
echo.
echo [LOI] Build/test that bai. Xem thong bao phia tren. Thu "scripts\build_windows.bat clean".
echo        Neu loi LNK2038 RuntimeLibrary: dat MSVC_RUNTIME=MultiThreadedDLL roi chay "clean".
exit /b 1
