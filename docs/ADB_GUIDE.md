# ADB Setup & Usage Guide

This guide covers connecting to the EDU Android Emulator via the **Android Debug Bridge (ADB)**, running shell commands, transferring files, installing APKs, and reading system logs.

---

## Prerequisites

### Install ADB

ADB is part of the Android SDK **platform-tools** package.

**Linux (Ubuntu / Debian)**
```bash
sudo apt-get update
sudo apt-get install -y adb
```

**Linux (Fedora / RHEL)**
```bash
sudo dnf install -y android-tools
```

**macOS (Homebrew)**
```bash
brew install android-platform-tools
```

**Windows**
Download the [SDK Platform-Tools ZIP](https://developer.android.com/tools/releases/platform-tools) from Google, extract it, and add the folder to your `PATH`.

Verify the installation:
```bash
adb version
# Android Debug Bridge version 1.0.41 (or newer)
```

---

## Quick Setup

### Linux / macOS

```bash
# Make the script executable (first time only)
chmod +x scripts/enable_adb.sh

# Basic setup: start server and connect
./scripts/enable_adb.sh

# With root and logcat
./scripts/enable_adb.sh --root --logcat

# Install an APK
./scripts/enable_adb.sh --install MyApp.apk

# Open an interactive shell
./scripts/enable_adb.sh --shell
```

### Windows

```bat
scripts\enable_adb.bat

rem With root and logcat
scripts\enable_adb.bat --root --logcat

rem Install an APK
scripts\enable_adb.bat --install MyApp.apk

rem Open an interactive shell
scripts\enable_adb.bat --shell
```

---

## Manual Setup

If you prefer to run ADB commands directly:

```bash
# 1. Start the ADB server
adb start-server

# 2. Connect to the emulator (TCP)
adb connect localhost:5555

# 3. Verify the connection
adb devices -l
# Expected output:
# List of devices attached
# localhost:5555  device  ...
```

---

## ADB Commands Reference

### Shell Access

```bash
# Interactive shell
adb -s localhost:5555 shell

# Run a single command
adb -s localhost:5555 shell ls /sdcard

# Root shell (userdebug images only)
adb -s localhost:5555 root
adb -s localhost:5555 shell
```

### File Transfer

```bash
# Upload a file to the emulator
adb -s localhost:5555 push ./local_file.txt /sdcard/local_file.txt

# Upload a directory
adb -s localhost:5555 push ./my_dir /sdcard/my_dir

# Download a file from the emulator
adb -s localhost:5555 pull /sdcard/remote_file.txt ./remote_file.txt

# Download a directory
adb -s localhost:5555 pull /sdcard/my_dir ./my_dir
```

### App Installation

```bash
# Install an APK
adb -s localhost:5555 install MyApp.apk

# Reinstall (keeps app data)
adb -s localhost:5555 install -r MyApp.apk

# Install allowing version downgrade
adb -s localhost:5555 install -r -d MyApp.apk

# Uninstall
adb -s localhost:5555 uninstall com.example.myapp
```

### Logcat

```bash
# Stream all logs to the terminal
adb -s localhost:5555 logcat

# Filter by tag
adb -s localhost:5555 logcat -s MyTag

# Filter by priority (V D I W E F)
adb -s localhost:5555 logcat "*:W"

# Save logs to a file
adb -s localhost:5555 logcat > logcat.log

# Clear the log buffer
adb -s localhost:5555 logcat -c
```

### Device Properties

```bash
# List all properties
adb -s localhost:5555 shell getprop

# Query a specific property
adb -s localhost:5555 shell getprop ro.build.version.release
adb -s localhost:5555 shell getprop ro.product.cpu.abi

# Set a runtime property (requires root)
adb -s localhost:5555 shell setprop debug.my.flag 1
```

### Port Forwarding

```bash
# Forward a host port to an emulator port
adb -s localhost:5555 forward tcp:8080 tcp:8080

# Reverse forward (emulator connects to host)
adb -s localhost:5555 reverse tcp:9090 tcp:9090

# List active forwards
adb -s localhost:5555 forward --list

# Remove all forwards
adb -s localhost:5555 forward --remove-all
```

### Emulator Management

```bash
# Reboot the emulator
adb -s localhost:5555 reboot

# Reboot into recovery
adb -s localhost:5555 reboot recovery

# Reboot into bootloader
adb -s localhost:5555 reboot bootloader

# Take a screenshot
adb -s localhost:5555 shell screencap /sdcard/screen.png
adb -s localhost:5555 pull /sdcard/screen.png ./screen.png

# Record screen (Android 4.4+)
adb -s localhost:5555 shell screenrecord /sdcard/record.mp4
```

---

## Root Access

> **⚠ Warning:** Root access gives complete control over the Android system. Only enable it in a trusted, isolated development environment. See [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) for security best practices.

### Enable Root (userdebug images)

```bash
# Restart adbd with root privileges
adb -s localhost:5555 root

# Verify
adb -s localhost:5555 shell id
# uid=0(root) gid=0(root)
```

### Disable Root

```bash
adb -s localhost:5555 unroot
```

### Using `su` Manually

```bash
adb -s localhost:5555 shell
# Inside the shell:
su
id  # Should show uid=0(root)
```

---

## Troubleshooting

### `adb: command not found`
Install the Android SDK platform-tools (see [Prerequisites](#prerequisites)).

### `error: no devices/emulators found`
1. Make sure the emulator is running.
2. Run `adb connect localhost:5555`.
3. Check `adb devices -l` to confirm the device appears.

### `error: device unauthorized`
1. In the emulator, open **Settings → Developer Options → USB Debugging**.
2. Tap **Revoke USB Debugging Authorizations**, then reconnect.
3. Accept the authorization prompt when it appears.

### `adb root` returns `adbd is already running as root`
Root is already active — no action needed.

### `adb root` returns `adbd cannot run as root in production builds`
The Android-x86 image uses a production build. Root is only available on `userdebug` or `eng` builds. You can still use `su` inside a shell if the image ships with the `su` binary.

### Connection drops after `adb root`
This is normal — adbd restarts. Wait 1–2 seconds then reconnect:
```bash
adb connect localhost:5555
```

### Logcat shows no output
```bash
# Clear the buffer and restart
adb -s localhost:5555 logcat -c
adb -s localhost:5555 logcat
```

### Slow file transfers
Use `adb sync` for large directories instead of individual `push` commands, or compress the files first.

---

## Firewall Notes

ADB communicates over TCP:

| Port | Direction | Purpose |
|------|-----------|---------|
| 5037 | host-local | ADB server (client ↔ server) |
| 5555 | host → guest | adbd inside the emulator |

If you are running the emulator on a remote machine, ensure port 5555 is reachable and **restrict access to trusted hosts only** (see [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md)).

---

*For security best practices, trusted-host configuration, and connection logging, see [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md).*
