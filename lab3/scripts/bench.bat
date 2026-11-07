@echo off
REM Benchmark theo protocol cua lab. Ket qua -> Output\windows\{csv,logs}
REM Dung: scripts\bench.bat build_windows_msvc
setlocal
cd /d "%~dp0.."
set DIR=%1
if "%DIR%"=="" set DIR=build_windows_msvc
set BIN=%DIR%\rsatool.exe
if not exist "%BIN%" set BIN=%DIR%\Release\rsatool.exe
if not exist "%BIN%" ( echo Khong thay rsatool.exe trong %DIR% & exit /b 1 )
for %%A in ("%DIR%") do set NAME=%%~nxA
for /f "tokens=3 delims=_" %%C in ("%NAME%") do set COMPILER=%%C
if not exist Output\windows\csv mkdir Output\windows\csv
if not exist Output\windows\logs mkdir Output\windows\logs
"%BIN%" bench --what all --bits both --runs 30 --keygen-runs 30 --rsa-block 100 --warmup 1.0 --csv Output\windows\csv\bench_windows_%COMPILER%.csv > Output\windows\logs\bench.log
type Output\windows\logs\bench.log
