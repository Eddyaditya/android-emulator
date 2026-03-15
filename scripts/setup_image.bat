@echo off
:: setup_image.bat – Prepare an Android x86 QEMU disk image (Windows)
::
:: Usage:
::   scripts\setup_image.bat [--iso <path_to_iso>]
::
:: The script will:
::   1. Check that qemu-system-x86_64.exe and qemu-img.exe are in PATH.
::   2. Download the Android-x86 ISO if it is not already present.
::   3. Create a writable QEMU disk image (android-x86.img) in the project root.

setlocal EnableDelayedExpansion

:: ── Configuration ────────────────────────────────────────────────────────────
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

set "ISO_FILENAME=android-x86_64-9.0-r2.iso"
set "ISO_URL=https://sourceforge.net/projects/android-x86/files/Release%%209.0/%ISO_FILENAME%/download"
set "ISO_PATH=%PROJECT_ROOT%\%ISO_FILENAME%"
set "IMAGE_PATH=%PROJECT_ROOT%\android-x86.img"
set "IMAGE_SIZE=16G"

:: ── Parse arguments ───────────────────────────────────────────────────────────
:parse_args
if "%~1"=="--iso"         ( set "ISO_PATH=%~2"  & shift & shift & goto parse_args )
if "%~1"=="--image-size"  ( set "IMAGE_SIZE=%~2" & shift & shift & goto parse_args )
if "%~1"=="--help"        ( goto show_help )
if "%~1"=="-h"            ( goto show_help )
if not "%~1"=="" (
    echo [setup_image] ERROR: Unknown argument: %~1
    exit /b 1
)
goto check_deps

:show_help
echo Usage: setup_image.bat [--iso ^<path^>] [--image-size ^<size^>]
echo.
echo  --iso          Path to an existing Android-x86 ISO (skips download)
echo  --image-size   Size for the new disk image (default: 16G)
exit /b 0

:: ── Check dependencies ────────────────────────────────────────────────────────
:check_deps
echo [setup_image] Checking dependencies...

where qemu-system-x86_64 >nul 2>&1
if errorlevel 1 (
    echo [setup_image] ERROR: qemu-system-x86_64 not found.
    echo   Download QEMU for Windows from: https://qemu.weilnetz.de/w64/
    echo   Then add the QEMU install folder to your PATH.
    exit /b 1
)

where qemu-img >nul 2>&1
if errorlevel 1 (
    echo [setup_image] ERROR: qemu-img not found. It should ship with QEMU.
    exit /b 1
)

echo [setup_image]   qemu-system-x86_64 found
echo [setup_image]   qemu-img found

:: ── Download ISO (if needed) ──────────────────────────────────────────────────
if exist "%ISO_PATH%" (
    echo [setup_image] ISO already present: %ISO_PATH%
    goto create_image
)

echo [setup_image] Downloading Android-x86 ISO...
echo [setup_image]   URL:  %ISO_URL%
echo [setup_image]   Dest: %ISO_PATH%

:: Use PowerShell's Invoke-WebRequest (available on Windows 7+)
powershell -NoProfile -Command ^
    "Invoke-WebRequest -Uri '%ISO_URL%' -OutFile '%ISO_PATH%' -UseBasicParsing"
if errorlevel 1 (
    echo [setup_image] ERROR: Download failed. Check your internet connection.
    exit /b 1
)
echo [setup_image] ISO downloaded: %ISO_PATH%

:: ── Create writable disk image ────────────────────────────────────────────────
:create_image
if exist "%IMAGE_PATH%" (
    echo [setup_image] Disk image already exists: %IMAGE_PATH%
    goto summary
)

echo [setup_image] Creating %IMAGE_SIZE% QEMU disk image at %IMAGE_PATH%...
qemu-img create -f qcow2 "%IMAGE_PATH%" %IMAGE_SIZE%
if errorlevel 1 (
    echo [setup_image] ERROR: qemu-img failed.
    exit /b 1
)
echo [setup_image] Disk image created.

:: ── Summary ───────────────────────────────────────────────────────────────────
:summary
echo.
echo ────────────────────────────────────────────────────────────
echo   Setup complete!
echo.
echo   ISO image:  %ISO_PATH%
echo   Disk image: %IMAGE_PATH%
echo.
echo   Next steps:
echo     1. Boot from the ISO to install Android to the disk image:
echo.
echo        qemu-system-x86_64 ^
echo            -m 2048 -smp 2 ^
echo            -cdrom "%ISO_PATH%" ^
echo            -hda  "%IMAGE_PATH%" ^
echo            -boot d
echo.
echo     2. In the Android-x86 installer choose "Installation" and
echo        select the QEMU disk as the target.
echo.
echo     3. After installation run:  build\edu_emulator.exe
echo.
echo   See INSTALLATION.md for hardware-acceleration setup (HAXM).
echo ────────────────────────────────────────────────────────────
endlocal
