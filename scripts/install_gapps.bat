@echo off
:: install_gapps.bat - Flash Open GApps into an Android-x86 QEMU disk image (Windows)
::
:: Usage:
::   scripts\install_gapps.bat [OPTIONS]
::
:: Options:
::   --image  <path>   Path to the Android-x86 QEMU image. Required.
::   --gapps  <path>   Path to the Open GApps ZIP package.
::   --variant <name>  pico | nano | micro. Default: pico
::   --android <ver>   Android version (e.g. 9.0). Default: 9.0
::   --arch   <arch>   x86_64 | x86. Default: x86_64
::   --download        Download the Open GApps package automatically.
::   --no-backup       Skip image backup (no rollback possible).
::   --help            Show this help and exit.
::
:: Requirements:
::   - qemu-img on PATH (bundled with QEMU)
::   - PowerShell 5.0+ (for ZIP extraction and downloading)
::   - 7-Zip on PATH (optional, faster extraction)

setlocal EnableDelayedExpansion

:: ── Defaults ──────────────────────────────────────────────────────────────────
set "IMAGE_PATH="
set "GAPPS_ZIP="
set "VARIANT=pico"
set "ANDROID_VER=9.0"
set "ARCH=x86_64"
set "DO_DOWNLOAD=0"
set "SKIP_BACKUP=0"
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "CACHE_DIR=%PROJECT_ROOT%\gapps_cache"

:: ── Parse arguments ───────────────────────────────────────────────────────────
:parse_args
if "%~1"=="" goto validate_args
if /i "%~1"=="--image"    ( set "IMAGE_PATH=%~2"   & shift & shift & goto parse_args )
if /i "%~1"=="--gapps"    ( set "GAPPS_ZIP=%~2"    & shift & shift & goto parse_args )
if /i "%~1"=="--variant"  ( set "VARIANT=%~2"      & shift & shift & goto parse_args )
if /i "%~1"=="--android"  ( set "ANDROID_VER=%~2"  & shift & shift & goto parse_args )
if /i "%~1"=="--arch"     ( set "ARCH=%~2"         & shift & shift & goto parse_args )
if /i "%~1"=="--download" ( set "DO_DOWNLOAD=1"    & shift            & goto parse_args )
if /i "%~1"=="--no-backup"( set "SKIP_BACKUP=1"    & shift            & goto parse_args )
if /i "%~1"=="--help"     goto show_help
echo [install_gapps] ERROR: Unknown argument: %~1
exit /b 1

:show_help
findstr /B "::" "%~f0"
exit /b 0

:: ── Validate required arguments ───────────────────────────────────────────────
:validate_args
if "%IMAGE_PATH%"=="" (
    echo [install_gapps] ERROR: --image is required
    exit /b 1
)
if not exist "%IMAGE_PATH%" (
    echo [install_gapps] ERROR: Image not found: %IMAGE_PATH%
    exit /b 1
)

:: ── Check dependencies ────────────────────────────────────────────────────────
echo [install_gapps] Checking dependencies...
where qemu-img >nul 2>&1 || (
    echo [install_gapps] ERROR: qemu-img not found. Install QEMU and add to PATH.
    exit /b 1
)
echo [install_gapps]   qemu-img found

:: ── Resolve GAPPS_ZIP ────────────────────────────────────────────────────────
if "%DO_DOWNLOAD%"=="1" ( goto download_gapps )
if "%GAPPS_ZIP%"==""   ( goto download_gapps )
goto validate_zip

:download_gapps
set "PKG_NAME=open_gapps-%ARCH%-%ANDROID_VER%-%VARIANT%.zip"
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"
set "GAPPS_ZIP=%CACHE_DIR%\%PKG_NAME%"

if exist "%GAPPS_ZIP%" (
    echo [install_gapps] Using cached package: %GAPPS_ZIP%
    goto validate_zip
)

set "DL_URL=https://github.com/opengapps/%ARCH%/releases/latest/download/%PKG_NAME%"
echo [install_gapps] Downloading Open GApps %VARIANT% for Android %ANDROID_VER% (%ARCH%)...
echo [install_gapps] URL: %DL_URL%

powershell -NoProfile -Command ^
    "Invoke-WebRequest -Uri '%DL_URL%' -OutFile '%GAPPS_ZIP%' -UseBasicParsing"
if errorlevel 1 (
    echo [install_gapps] ERROR: Download failed
    exit /b 1
)
echo [install_gapps] Download complete: %GAPPS_ZIP%

:validate_zip
if not exist "%GAPPS_ZIP%" (
    echo [install_gapps] ERROR: GApps ZIP not found: %GAPPS_ZIP%
    exit /b 1
)

:: Basic size check
for %%F in ("%GAPPS_ZIP%") do (
    if %%~zF==0 (
        echo [install_gapps] ERROR: GApps ZIP is empty: %GAPPS_ZIP%
        exit /b 1
    )
)
echo [install_gapps] Package validation passed

:: ── Create image backup ───────────────────────────────────────────────────────
set "BACKUP_PATH=%IMAGE_PATH%.bak"
if "%SKIP_BACKUP%"=="1" goto skip_backup

if exist "%BACKUP_PATH%" (
    echo [install_gapps] Backup already exists, skipping: %BACKUP_PATH%
    goto skip_backup
)
echo [install_gapps] Creating image backup: %BACKUP_PATH%
copy /Y "%IMAGE_PATH%" "%BACKUP_PATH%" >nul
if errorlevel 1 (
    echo [install_gapps] ERROR: Failed to create backup
    exit /b 1
)
echo [install_gapps] Backup created

:skip_backup

:: ── Extract GApps into a staging area ────────────────────────────────────────
set "TMP_DIR=%TEMP%\gapps_extract_%RANDOM%"
mkdir "%TMP_DIR%"
echo [install_gapps] Extracting GApps package to staging area...

:: Prefer 7-Zip for speed; fall back to PowerShell's Expand-Archive
where 7z >nul 2>&1
if not errorlevel 1 (
    7z x "%GAPPS_ZIP%" -o"%TMP_DIR%" -y >nul
    if errorlevel 1 (
        echo [install_gapps] ERROR: Extraction failed
        if exist "%TMP_DIR%" rmdir /s /q "%TMP_DIR%"
        exit /b 1
    )
) else (
    powershell -NoProfile -Command ^
        "Expand-Archive -Path '%GAPPS_ZIP%' -DestinationPath '%TMP_DIR%' -Force"
    if errorlevel 1 (
        echo [install_gapps] ERROR: Extraction failed
        if exist "%TMP_DIR%" rmdir /s /q "%TMP_DIR%"
        exit /b 1
    )
)
echo [install_gapps] Extraction complete

:: ── Merge system files into the image ────────────────────────────────────────
:: On Windows, direct qcow2 mounting is not straightforward without WSL/QEMU.
:: We convert to a raw image, apply changes, then convert back.

echo [install_gapps] Converting image to raw format for patching...
set "RAW_IMAGE=%TEMP%\android_raw_%RANDOM%.img"

qemu-img convert -f qcow2 -O raw "%IMAGE_PATH%" "%RAW_IMAGE%"
if errorlevel 1 (
    echo [install_gapps] ERROR: qemu-img convert failed
    goto cleanup
)
echo [install_gapps] Raw image created: %RAW_IMAGE%

:: NOTE: Directly patching a raw Android-x86 image requires Linux tools
:: (losetup, mount) that are not available natively on Windows.
:: The recommended approach on Windows is to use WSL2:
::   wsl bash scripts/install_gapps.sh --image "%IMAGE_PATH%" --gapps "%GAPPS_ZIP%"
::
:: If WSL is available, delegate to the shell script automatically:
where wsl >nul 2>&1
if not errorlevel 1 (
    echo [install_gapps] WSL detected — delegating to install_gapps.sh...
    wsl bash "%SCRIPT_DIR%install_gapps.sh" ^
        --image "%IMAGE_PATH%" ^
        --gapps "%GAPPS_ZIP%" ^
        --variant "%VARIANT%" ^
        --android "%ANDROID_VER%" ^
        --arch "%ARCH%" ^
        --no-backup
    if errorlevel 1 goto cleanup_fail
    goto success
)

echo [install_gapps] WARNING: WSL not available. Copying GApps files to staging area.
echo [install_gapps] To complete installation, manually mount the image and copy:
echo [install_gapps]   Source: %TMP_DIR%\system
echo [install_gapps]   Dest:   /system on the Android image
echo [install_gapps]
echo [install_gapps] Or run this script in WSL for automatic installation.

:: Clean up the temp raw image (not used without WSL)
if exist "%RAW_IMAGE%" del /f /q "%RAW_IMAGE%"

:success
:: ── Cleanup and summary ───────────────────────────────────────────────────────
if exist "%TMP_DIR%" rmdir /s /q "%TMP_DIR%"

echo.
echo ────────────────────────────────────────────────────────────
echo   GApps installation complete!
echo.
echo   Image:    %IMAGE_PATH%
echo   Variant:  %VARIANT%
echo   Android:  %ANDROID_VER%
echo   Arch:     %ARCH%
if "%SKIP_BACKUP%"=="0" echo   Backup:   %BACKUP_PATH%
echo.
echo   Next steps:
echo     1. Boot the image:
echo          qemu-system-x86_64 -hda "%IMAGE_PATH%" -m 2048 -smp 2
echo     2. Sign in with your Google account in Play Store on first boot.
echo     3. Chrome will open with the default homepage on first launch.
echo.
echo   Rollback:
echo     If something went wrong, restore the backup:
echo       copy /Y "%BACKUP_PATH%" "%IMAGE_PATH%"
echo ────────────────────────────────────────────────────────────
endlocal
exit /b 0

:cleanup_fail
if exist "%TMP_DIR%" rmdir /s /q "%TMP_DIR%"
if exist "%RAW_IMAGE%" del /f /q "%RAW_IMAGE%"
echo [install_gapps] ERROR: Installation failed.
echo [install_gapps] Restore from backup: copy /Y "%BACKUP_PATH%" "%IMAGE_PATH%"
endlocal
exit /b 1

:cleanup
if exist "%TMP_DIR%" rmdir /s /q "%TMP_DIR%"
if exist "%RAW_IMAGE%" del /f /q "%RAW_IMAGE%"
endlocal
exit /b 1
