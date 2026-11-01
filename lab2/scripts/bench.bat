@echo off
setlocal EnableExtensions

cd /d "%~dp0\.."
set "OUT_DIR=%CD%\Output\windows\bench"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

if defined AESTOOL (
    set "TOOL=%AESTOOL%"
) else if exist "build_windows_msvc\Release\aestool.exe" (
    set "TOOL=build_windows_msvc\Release\aestool.exe"
) else if exist "build_windows_gcc\aestool.exe" (
    set "TOOL=build_windows_gcc\aestool.exe"
) else if exist "build_windows_clang\aestool.exe" (
    set "TOOL=build_windows_clang\aestool.exe"
) else (
    echo [ERROR] aestool executable not found.
    echo         Set AESTOOL or build with scripts\build_windows.bat first.
    exit /b 1
)

call :run_bench 1MiB 500
if errorlevel 1 exit /b 1
call :run_bench 100MiB 100
if errorlevel 1 exit /b 1
call :run_bench 1GiB 30
if errorlevel 1 exit /b 1
exit /b 0

:run_bench
echo [INFO] %~1: %~2 runs ^> %OUT_DIR%\bench_windows_%~1.csv
"%TOOL%" bench --sizes %~1 --runs %~2 --csv "%OUT_DIR%\bench_windows_%~1.csv"
exit /b %errorlevel%
