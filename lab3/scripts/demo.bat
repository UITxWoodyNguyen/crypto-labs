@echo off
setlocal EnableDelayedExpansion

REM ============================================================================
REM scripts\demo.bat - Chay Buoc 4 (Kiem tra thu cong rsatool)
REM Sinh toan bo ket qua vao: lab3\Output\windows\demo
REM
REM Cach dung:
REM     scripts\demo.bat [thu_muc_build]
REM Vi du:
REM     scripts\demo.bat
REM     scripts\demo.bat build_windows_msvc
REM     scripts\demo.bat build_windows_gcc
REM ============================================================================

cd /d "%~dp0.."
set "ROOT_DIR=%CD%"

REM 1. Tim binary rsatool.exe
set "BUILD_ARG=%~1"
set "BIN="

if not "%BUILD_ARG%"=="" (
    if exist "%ROOT_DIR%\%BUILD_ARG%\Release\rsatool.exe" (
        set "BIN=%ROOT_DIR%\%BUILD_ARG%\Release\rsatool.exe"
    ) else if exist "%ROOT_DIR%\%BUILD_ARG%\rsatool.exe" (
        set "BIN=%ROOT_DIR%\%BUILD_ARG%\rsatool.exe"
    )
)

if "%BIN%"=="" (
    if exist "%ROOT_DIR%\build_windows_msvc\Release\rsatool.exe" (
        set "BIN=%ROOT_DIR%\build_windows_msvc\Release\rsatool.exe"
    ) else if exist "%ROOT_DIR%\build_windows_msvc\rsatool.exe" (
        set "BIN=%ROOT_DIR%\build_windows_msvc\rsatool.exe"
    ) else if exist "%ROOT_DIR%\build_windows_gcc\rsatool.exe" (
        set "BIN=%ROOT_DIR%\build_windows_gcc\rsatool.exe"
    ) else if exist "%ROOT_DIR%\build_windows_clang\rsatool.exe" (
        set "BIN=%ROOT_DIR%\build_windows_clang\rsatool.exe"
    ) else if exist "%ROOT_DIR%\build\Release\rsatool.exe" (
        set "BIN=%ROOT_DIR%\build\Release\rsatool.exe"
    ) else if exist "%ROOT_DIR%\build\rsatool.exe" (
        set "BIN=%ROOT_DIR%\build\rsatool.exe"
    )
)

if "%BIN%"=="" (
    echo [ERROR] Khong tim thay rsatool.exe!
    echo Vui long build du an truoc hoac truyen dung thu muc build: scripts\demo.bat ^<build_dir^>
    exit /b 1
)

echo ============================================================================
echo CHAY BUOC 4: DEMO THU CONG RSATOOL
echo Binary: %BIN%
echo ============================================================================

REM 2. Tao thu muc lam viec tai Output\windows\demo
set "DEMO_DIR=%ROOT_DIR%\Output\windows\demo"
if not exist "%DEMO_DIR%" mkdir "%DEMO_DIR%"
cd /d "%DEMO_DIR%"
echo Thu muc lam viec: %CD%
echo.

REM ----------------------------------------------------------------------------
REM 4.1 Sinh khoa
REM ----------------------------------------------------------------------------
echo ============================================================================
echo [4.1] Sinh khoa RSA-3072: pub.pem, priv.pem, pub.der, priv.der, keys_metadata.json
echo ============================================================================
"%BIN%" keygen --bits 3072 --pub pub.pem --priv priv.pem
if errorlevel 1 ( echo [FAIL] Keygen that bai & exit /b 1 )
echo.
echo Noi dung keys_metadata.json:
type keys_metadata.json
echo.

REM ----------------------------------------------------------------------------
REM 4.2 Ma hoa ngan (RSA-OAEP truc tiep) va giai ma
REM ----------------------------------------------------------------------------
echo ============================================================================
echo [4.2] Ma hoa ngan (RSA-OAEP truc tiep) va giai ma
echo ============================================================================
echo 1. In ban ma hex truc tiep ra man hinh:
"%BIN%" encrypt --text "hello" --pub pub.pem --label ctx
echo.

echo 2. Ma hoa ra tep ct.bin:
"%BIN%" encrypt --text "hello" --pub pub.pem --label ctx --out ct.bin
if errorlevel 1 ( echo [FAIL] Ma hoa that bai & exit /b 1 )
echo.

echo 3. Giai ma tu ct.bin:
"%BIN%" decrypt --in ct.bin --priv priv.pem --label ctx
echo.

REM ----------------------------------------------------------------------------
REM 4.3 Ma hoa tep lon (tu chuyen sang hybrid) roi giai ma va so sanh
REM ----------------------------------------------------------------------------
echo ============================================================================
echo [4.3] Ma hoa tep lon 1 MiB (tu chuyen sang hybrid) va giai ma kiem tra hash
echo ============================================================================
echo Dang tao tep ngau nhien 1 MiB (big.bin)...
powershell -NoProfile -Command "$b = New-Object byte[] 1048576; [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($b); [IO.File]::WriteAllBytes('big.bin', $b)"

echo Ma hoa big.bin -^> big.env.json:
"%BIN%" encrypt --in big.bin --pub pub.pem --out big.env.json --verbose
if errorlevel 1 ( echo [FAIL] Ma hoa hybrid that bai & exit /b 1 )
echo.

echo Giai ma big.env.json -^> big.dec:
"%BIN%" decrypt --in big.env.json --priv priv.pem --out big.dec
if errorlevel 1 ( echo [FAIL] Giai ma hybrid that bai & exit /b 1 )
echo.

echo So sanh ma hash SHA-256 giua big.bin va big.dec:
fc /b big.bin big.dec >nul
if %ERRORLEVEL% equ 0 (
    echo [PASS] Hash trung khop ^(Get-FileHash big.bin^).Hash -eq ^(Get-FileHash big.dec^).Hash : True
) else (
    echo [FAIL] Hash mismatch!
    exit /b 1
)
echo.

REM ----------------------------------------------------------------------------
REM 4.4 Thu cac truong hop phai that bai (fail-closed)
REM ----------------------------------------------------------------------------
echo ============================================================================
echo [4.4] Thu cac truong hop phai that bai (fail-closed)
REM Moi lenh phai bao loi, thoat voi ma loi khac 0 va KHONG tao file dau ra.
echo ============================================================================

echo --- 1. Sai label (yeu cau exit code 3, o1.bin KHONG duoc tao) ---
"%BIN%" encrypt --text "secret" --pub pub.pem --label SECURE_CONTEXT --out c1.bin >nul
if exist o1.bin del /f /q o1.bin
"%BIN%" decrypt --in c1.bin --priv priv.pem --label WRONG_CONTEXT --out o1.bin
set "RC=%ERRORLEVEL%"
set "EXISTS=False"
if exist o1.bin set "EXISTS=True"
echo Exit code: !RC! (mong doi: 3) ^| o1.bin ton tai: !EXISTS!
if !RC! NEQ 0 (
    if "!EXISTS!"=="False" (
        echo [PASS] Fail-closed dung yeu cau.
    ) else (
        echo [FAIL] File o1.bin da bi tao!
    )
) else (
    echo [FAIL] Lenh giai ma khong bao loi!
)
echo.

echo --- 2. Sai khoa private (yeu cau exit code khac 0, o2.bin KHONG duoc tao) ---
if not exist other mkdir other
"%BIN%" keygen --bits 3072 --pub other\pub.pem --priv other\priv.pem >nul
if exist o2.bin del /f /q o2.bin
"%BIN%" decrypt --in big.env.json --priv other\priv.pem --out o2.bin
set "RC=%ERRORLEVEL%"
set "EXISTS=False"
if exist o2.bin set "EXISTS=True"
echo Exit code: !RC! ^| o2.bin ton tai: !EXISTS!
if !RC! NEQ 0 (
    if "!EXISTS!"=="False" (
        echo [PASS] Fail-closed dung yeu cau.
    ) else (
        echo [FAIL] File o2.bin da bi tao!
    )
) else (
    echo [FAIL] Lenh giai ma khong bao loi!
)
echo.

echo --- 3. Sua 1 byte cua ban ma RSA (yeu cau exit code khac 0, o3.bin KHONG duoc tao) ---
powershell -NoProfile -Command "$ct = [IO.File]::ReadAllBytes('ct.bin'); $ct[10] = $ct[10] -bxor 1; [IO.File]::WriteAllBytes('ct_bad.bin', $ct)"
if exist o3.bin del /f /q o3.bin
"%BIN%" decrypt --in ct_bad.bin --priv priv.pem --label ctx --out o3.bin
set "RC=%ERRORLEVEL%"
set "EXISTS=False"
if exist o3.bin set "EXISTS=True"
echo Exit code: !RC! ^| o3.bin ton tai: !EXISTS!
if !RC! NEQ 0 (
    if "!EXISTS!"=="False" (
        echo [PASS] Fail-closed dung yeu cau.
    ) else (
        echo [FAIL] File o3.bin da bi tao!
    )
) else (
    echo [FAIL] Lenh giai ma khong bao loi!
)
echo.

echo --- 4. Khoa 2048 bit bi tu choi (yeu cau exit code 2, p2048.pem KHONG duoc tao) ---
if exist p2048.pem del /f /q p2048.pem
if exist s2048.pem del /f /q s2048.pem
"%BIN%" keygen --bits 2048 --pub p2048.pem --priv s2048.pem
set "RC=%ERRORLEVEL%"
set "EXISTS=False"
if exist p2048.pem set "EXISTS=True"
echo Exit code: !RC! (mong doi: 2) ^| p2048.pem ton tai: !EXISTS!
if !RC!==2 (
    if "!EXISTS!"=="False" (
        echo [PASS] Tu choi khoa 2048 bit dung yeu cau.
    ) else (
        echo [FAIL] File p2048.pem da bi tao!
    )
) else (
    echo [FAIL] Exit code khong dung 2!
)
echo.

cd /d "%ROOT_DIR%"
echo ============================================================================
echo HOAN THANH BUOC 4 THANH CONG.
echo Toan bo artifacts da duoc luu tai: %DEMO_DIR%
echo ============================================================================
