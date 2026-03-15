@echo off
:: enable_adb.bat - Set up ADB and connect to the EDU Android Emulator (Windows)
::
:: Usage:
::   scripts\enable_adb.bat [OPTIONS]
::
:: Options:
::   --port   <port>   ADB device TCP port (default: 5555)
::   --host   <host>   Emulator host (default: localhost)
::   --root            Request root shell via "adb root" after connecting
::   --logcat          Start logcat (writes to %TEMP%\adb_logs\logcat.log)
::   --install <apk>   Install an APK after connecting
::   --shell           Drop into an interactive ADB shell after setup
::   --help            Show this help and exit
::
:: Requirements:
::   - adb.exe on PATH (install Android SDK platform-tools)

setlocal EnableDelayedExpansion

:: ── Defaults ──────────────────────────────────────────────────────────────────
set "ADB_HOST=localhost"
set "ADB_PORT=5555"
set "REQUEST_ROOT=0"
set "START_LOGCAT=0"
set "INSTALL_APK="
set "OPEN_SHELL=0"
set "LOG_DIR=%TEMP%\adb_logs"

:: ── Parse arguments ───────────────────────────────────────────────────────────
:parse_args
if "%~1"=="" goto validate
if /i "%~1"=="--port"    ( set "ADB_PORT=%~2"    & shift & shift & goto parse_args )
if /i "%~1"=="--host"    ( set "ADB_HOST=%~2"    & shift & shift & goto parse_args )
if /i "%~1"=="--root"    ( set "REQUEST_ROOT=1"  & shift            & goto parse_args )
if /i "%~1"=="--logcat"  ( set "START_LOGCAT=1"  & shift            & goto parse_args )
if /i "%~1"=="--install" ( set "INSTALL_APK=%~2" & shift & shift & goto parse_args )
if /i "%~1"=="--shell"   ( set "OPEN_SHELL=1"    & shift            & goto parse_args )
if /i "%~1"=="--help"    goto show_help
echo [enable_adb] ERROR: Unknown argument: %~1
exit /b 1

:show_help
findstr /B "::" "%~f0"
exit /b 0

:validate
:: ── Security warning for root ─────────────────────────────────────────────────
if "%REQUEST_ROOT%"=="1" (
    echo.
    echo ╔══════════════════════════════════════════════════════════════╗
    echo ║              WARNING: ROOT ACCESS ENABLED                   ║
    echo ╠══════════════════════════════════════════════════════════════╣
    echo ║  Root access gives full control over the Android system.    ║
    echo ║  Only enable in a trusted development environment.          ║
    echo ║  Do NOT expose ADB over TCP on untrusted networks.          ║
    echo ║  See docs\DEVELOPER_GUIDE.md for security best practices.   ║
    echo ╚══════════════════════════════════════════════════════════════╝
    echo.
)

:: ── Dependency check ──────────────────────────────────────────────────────────
echo [enable_adb] Checking dependencies...
where adb >nul 2>&1
if errorlevel 1 (
    echo [enable_adb] ERROR: adb not found. Install Android SDK platform-tools and add to PATH.
    exit /b 1
)
echo [enable_adb]   adb found

:: ── Start ADB server ──────────────────────────────────────────────────────────
echo [enable_adb] Starting ADB server...
adb start-server
if errorlevel 1 (
    echo [enable_adb] ERROR: Failed to start ADB server
    exit /b 1
)
echo [enable_adb] ADB server started

:: ── Connect to the emulator ───────────────────────────────────────────────────
set "TARGET=%ADB_HOST%:%ADB_PORT%"
echo [enable_adb] Connecting to emulator at %TARGET%...
adb connect "%TARGET%"
if errorlevel 1 (
    echo [enable_adb] ERROR: Connection failed. Is the emulator running?
    exit /b 1
)

:: ── Wait for device ───────────────────────────────────────────────────────────
echo [enable_adb] Waiting for device...
adb -s "%TARGET%" wait-for-device
echo [enable_adb] Device ready

:: ── List devices ──────────────────────────────────────────────────────────────
echo [enable_adb] Connected devices:
adb devices -l

:: ── Request root ──────────────────────────────────────────────────────────────
if "%REQUEST_ROOT%"=="1" (
    echo [enable_adb] Requesting root access...
    adb -s "%TARGET%" root
    if errorlevel 1 (
        echo [enable_adb] WARNING: adb root failed - image may not support userdebug root
    ) else (
        timeout /t 1 /nobreak >nul
        adb connect "%TARGET%" >nul 2>&1
        adb -s "%TARGET%" wait-for-device
        echo [enable_adb] Root shell active
    )
)

:: ── Logcat ────────────────────────────────────────────────────────────────────
if "%START_LOGCAT%"=="1" (
    if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
    set "LOGCAT_FILE=%LOG_DIR%\logcat.log"
    echo [enable_adb] Starting logcat -^> !LOGCAT_FILE!
    start /B adb -s "%TARGET%" logcat > "!LOGCAT_FILE!" 2>&1
    echo [enable_adb] Logcat started in background
)

:: ── APK installation ──────────────────────────────────────────────────────────
if not "%INSTALL_APK%"=="" (
    if not exist "%INSTALL_APK%" (
        echo [enable_adb] ERROR: APK not found: %INSTALL_APK%
        exit /b 1
    )
    echo [enable_adb] Installing APK: %INSTALL_APK%
    adb -s "%TARGET%" install -r "%INSTALL_APK%"
    if errorlevel 1 (
        echo [enable_adb] ERROR: APK installation failed
        exit /b 1
    )
    echo [enable_adb] APK installed successfully
)

:: ── Interactive shell ─────────────────────────────────────────────────────────
if "%OPEN_SHELL%"=="1" (
    echo [enable_adb] Opening interactive shell (type 'exit' to return)...
    adb -s "%TARGET%" shell
)

:: ── Summary ───────────────────────────────────────────────────────────────────
echo.
echo ────────────────────────────────────────────────────────────
echo   ADB setup complete!
echo.
echo   Target:  %TARGET%
if "%REQUEST_ROOT%"=="1" ( echo   Root:    enabled ) else ( echo   Root:    disabled )
if "%START_LOGCAT%"=="1" ( echo   Logcat:  %LOG_DIR%\logcat.log ) else ( echo   Logcat:  not started )
echo.
echo   Useful commands:
echo     adb -s %TARGET% shell              ^| Interactive shell
echo     adb -s %TARGET% push ^<src^> ^<dst^>   ^| Upload file
echo     adb -s %TARGET% pull ^<src^> ^<dst^>   ^| Download file
echo     adb -s %TARGET% install ^<apk^>      ^| Install APK
echo     adb -s %TARGET% logcat             ^| View system logs
echo     adb -s %TARGET% shell getprop      ^| List device properties
echo.
echo   Full guide: docs\ADB_GUIDE.md
echo ────────────────────────────────────────────────────────────
endlocal
exit /b 0
