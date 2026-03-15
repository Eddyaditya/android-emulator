# Open GApps Integration Guide

This guide covers everything you need to know about installing Google Play Services, Google Play Store, and Chrome on the EDU Android Emulator.

---

## Table of Contents

1. [Legal & Licensing](#legal--licensing)
2. [What is Open GApps?](#what-is-open-gapps)
3. [GApps Variants](#gapps-variants)
4. [Prerequisites](#prerequisites)
5. [Network Requirements](#network-requirements)
6. [Automated Installation](#automated-installation)
   - [Linux / macOS](#linux--macos)
   - [Windows](#windows)
7. [Manual Installation](#manual-installation)
8. [First-Boot Setup](#first-boot-setup)
9. [Google Play Store](#google-play-store)
10. [Chrome Browser](#chrome-browser)
11. [Troubleshooting](#troubleshooting)
12. [Rollback](#rollback)
13. [Alternative App Stores](#alternative-app-stores)

---

## Legal & Licensing

> **Important — please read before installing GApps.**

**Google Mobile Services (GMS)** — which includes Google Play Store, Google Play Services, and Chrome — is proprietary software owned by Google LLC. It is **not open-source** and is distributed under Google's Terms of Service.

**Open GApps** packages these proprietary apps and make them available for installation on AOSP-based systems. Before installing:

- You **must** agree to [Google's Terms of Service](https://policies.google.com/terms).
- You **must** agree to the [Google Play Terms of Service](https://play.google.com/intl/en_us/about/play-terms/).
- Open GApps itself is covered by the [Open GApps License](https://github.com/opengapps/opengapps/blob/master/LICENSE).
- **Commercial redistribution** of GApps-patched images is generally **not permitted** without explicit Google licensing.
- This emulator is intended for **educational and personal development use only**.

By running the GApps installation scripts, you acknowledge and accept these terms.

---

## What is Open GApps?

[Open GApps](https://opengapps.org/) is a community-maintained project that packages Google's proprietary apps and services for installation on Android-based systems that ship without them (such as Android-x86, custom ROMs, etc.).

The installation process:
1. Downloads the GApps ZIP for your target Android version and architecture.
2. Extracts the ZIP and copies the system files into the Android disk image.
3. On first boot, Google Play Services registers the device and Play Store becomes functional.

---

## GApps Variants

Open GApps comes in several sizes. Larger variants include more pre-installed Google apps.

| Variant | Size (approx.) | Includes |
|---------|---------------|---------|
| **pico** | ~100 MB | Google Play Services, Google Play Store |
| **nano** | ~200 MB | pico + Google TTS, Gmail (lite), Maps (lite) |
| **micro** | ~350 MB | nano + additional core Google apps |

**Recommendation:** Use `pico` for the smallest footprint. Install additional apps from the Play Store as needed.

---

## Prerequisites

### System Tools

| Tool | Purpose | Install |
|------|---------|---------|
| `qemu-img` | Image format conversion | Bundled with QEMU |
| `unzip` | Extracting GApps ZIP | `sudo apt-get install unzip` |
| `wget` or `curl` | Downloading packages | Usually pre-installed |
| `nbd` kernel module | Mounting qcow2 images | `sudo modprobe nbd max_part=8` |

### Linux: Load the NBD Module

```bash
# Load nbd (required to mount qcow2 images)
sudo modprobe nbd max_part=8

# Make it load on boot (optional)
echo "nbd" | sudo tee -a /etc/modules-load.d/nbd.conf
```

### ADB (for post-install verification)

ADB is needed for the Play Store first-boot configuration helper:

```bash
# Ubuntu/Debian
sudo apt-get install -y adb

# macOS (Homebrew)
brew install android-platform-tools

# Windows
# Download Android Platform Tools from:
# https://developer.android.com/tools/releases/platform-tools
```

---

## Network Requirements

Google services require internet access to function:

| Service | Ports | Required for |
|---------|-------|-------------|
| Play Services registration | 443 (HTTPS) | Activating Play Store |
| Play Store | 443 (HTTPS) | Browsing and downloading apps |
| Chrome sync | 443 (HTTPS) | Syncing bookmarks and settings |
| Google account auth | 443 (HTTPS) | Signing in |

The QEMU emulator uses **user-mode networking** (`-net user`) which provides NAT-based internet access via the host machine. No additional network configuration is required in most cases.

---

## Automated Installation

### Linux / macOS

```bash
# Make the script executable
chmod +x scripts/install_gapps.sh

# Install pico GApps (smallest, recommended)
./scripts/install_gapps.sh \
    --image android-x86.img \
    --download \
    --variant pico \
    --android 9.0 \
    --arch x86_64

# Install nano GApps (includes Gmail and Maps)
./scripts/install_gapps.sh \
    --image android-x86.img \
    --download \
    --variant nano

# Use a pre-downloaded GApps ZIP
./scripts/install_gapps.sh \
    --image android-x86.img \
    --gapps /path/to/open_gapps-x86_64-9.0-pico.zip
```

#### Script Options

| Option | Default | Description |
|--------|---------|-------------|
| `--image <path>` | — | **Required.** Android-x86 QEMU image |
| `--gapps <path>` | — | Path to pre-downloaded GApps ZIP |
| `--download` | off | Auto-download the GApps package |
| `--variant <name>` | `pico` | GApps variant: `pico`, `nano`, `micro` |
| `--android <ver>` | `9.0` | Android version string |
| `--arch <arch>` | `x86_64` | Architecture: `x86_64` or `x86` |
| `--mount <dir>` | `/tmp/gapps_mount` | Temporary mount point |
| `--no-backup` | off | Skip backup (faster, no rollback) |

### Windows

```bat
rem Install pico GApps with auto-download
scripts\install_gapps.bat --image android-x86.img --download --variant pico

rem Use a pre-downloaded GApps ZIP
scripts\install_gapps.bat --image android-x86.img --gapps C:\path\to\open_gapps.zip
```

> **Note:** On Windows, the script automatically delegates to WSL2 (`wsl`) if available, which is the recommended approach. Without WSL2, image mounting is not supported natively; the script will print instructions for manual completion.

---

## Manual Installation

If you prefer to install GApps manually or if the automated script does not work on your system:

### Step 1: Download GApps

Visit [opengapps.org](https://opengapps.org/) and download the package for:
- **Platform:** x86_64 (or x86 for 32-bit)
- **Android:** 9.0 (or your Android-x86 version)
- **Variant:** pico (recommended)

### Step 2: Mount the Android image

```bash
# Load the nbd module
sudo modprobe nbd max_part=8

# Connect the image
sudo qemu-nbd --connect=/dev/nbd0 android-x86.img

# Find the system partition
sudo fdisk -l /dev/nbd0

# Mount the system partition (adjust partition number as needed)
sudo mount -t ext4 /dev/nbd0p1 /mnt/android
```

### Step 3: Extract and copy GApps

```bash
# Extract GApps
unzip open_gapps-x86_64-9.0-pico.zip -d /tmp/gapps_extract

# Copy system files
sudo cp -r /tmp/gapps_extract/system/. /mnt/android/
```

### Step 4: Unmount

```bash
sudo sync
sudo umount /mnt/android
sudo qemu-nbd --disconnect /dev/nbd0
```

---

## First-Boot Setup

After GApps is installed in the image:

1. **Boot the emulator:**
   ```bash
   ./build/edu_emulator
   # or directly with QEMU:
   qemu-system-x86_64 -hda android-x86.img -m 2048 -smp 2 -net nic -net user
   ```

2. **Complete the Android setup wizard** — Google services will initialize automatically.

3. **Sign in with a Google account** in the Play Store app to activate Play Services.
   > If you prefer not to sign in, you can use Play Store in limited mode or skip sign-in.

4. **Chrome** (if included in your variant) will be pre-installed and available from the app drawer.

---

## Google Play Store

### Opening Play Store

Tap the Play Store icon in the app drawer. On first launch, you will be prompted to sign in with a Google account.

### Installing Apps

1. Open Play Store
2. Search for the app you want
3. Tap **Install**
4. The app will download and install automatically

### Troubleshooting Play Store

| Problem | Solution |
|---------|---------|
| Play Store opens but shows "Sign in" loop | Sign out, clear Play Store data, sign in again |
| "Device not certified" error | The emulator's Play Protect certification may need updating — see [this guide](https://www.google.com/android/uncertified/) |
| Play Store not found | Verify GApps variant includes it (`pico` and above do) |
| App install fails | Check disk space in the emulator |

---

## Chrome Browser

Chrome is included in the **nano** and **micro** GApps variants. For **pico**, install Chrome from the Play Store.

### Default Homepage

The default homepage is configured to `https://www.google.com`. To change it:

1. Open Chrome
2. Tap the three-dot menu → **Settings** → **Homepage**
3. Enter your preferred URL

### Bookmarks

To add bookmarks for common sites, tap the star icon in the address bar or go to **Bookmarks** in the Chrome menu.

### Enabling Sync (Optional)

1. Open Chrome
2. Tap the profile icon → **Turn on sync**
3. Sign in with your Google account

---

## Troubleshooting

### Script fails: "nbd module not available"

```bash
sudo modprobe nbd max_part=8
# Retry the installation
```

### Script fails: "No free /dev/nbd* device"

```bash
# List used nbd devices
lsmod | grep nbd
# Disconnect any in use
sudo qemu-nbd --disconnect /dev/nbd0
```

### Download fails (network error)

1. Check your internet connection.
2. Try downloading the GApps ZIP manually from [opengapps.org](https://opengapps.org/).
3. Pass the downloaded file with `--gapps /path/to/file.zip`.

### Play Services crashes on boot

1. Ensure the GApps variant matches your Android-x86 version (e.g., don't use a 10.0 package with a 9.0 image).
2. Try the `pico` variant — it has fewer dependencies.
3. Give the emulator more RAM: increase `-m` in the QEMU arguments to `4096`.

### Chrome crashes on startup

1. Clear Chrome data: **Settings** → **Apps** → **Chrome** → **Clear Data**.
2. Reinstall Chrome from Play Store if it persists.

### Google account sign-in loop

This can happen on emulators not certified by Google. Register the device:

1. Find the Android ID: `adb shell settings get secure android_id`
2. Visit [google.com/android/uncertified](https://www.google.com/android/uncertified/)
3. Enter the Android ID to register the device.

---

## Rollback

If the GApps installation fails or causes issues, restore from the automatic backup:

### Linux / macOS

```bash
# The backup is created automatically at <image>.bak
cp android-x86.img.bak android-x86.img
```

### Windows

```bat
copy /Y android-x86.img.bak android-x86.img
```

### Skip backup (faster installs)

Pass `--no-backup` to the script if you do not need rollback capability. **Warning:** this is irreversible without your own backup.

---

## Alternative App Stores

If you prefer not to install Google services (for privacy or licensing reasons), these open-source alternatives are available:

| App Store | URL | Notes |
|-----------|-----|-------|
| **F-Droid** | [f-droid.org](https://f-droid.org) | Open-source apps only; install via APK sideload |
| **Aurora Store** | [auroraoss.com](https://auroraoss.com) | Anonymous Play Store client; no Google account needed |
| **Aptoide** | [aptoide.com](https://aptoide.com) | Third-party app marketplace |

To sideload an APK without Play Store:

```bash
# Enable Unknown Sources in Settings → Security
# Then install via ADB:
adb install /path/to/app.apk
```
