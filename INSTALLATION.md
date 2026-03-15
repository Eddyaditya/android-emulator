# EDU Android Emulator – Installation Guide

This guide explains how to install and run the **EDU Android Emulator** on Linux, Windows, and macOS.

---

## Prerequisites

### Build Tools

| Tool | Minimum version | Purpose |
|------|-----------------|---------|
| CMake | 3.10 | Build system |
| GCC / Clang (Linux/macOS) or MSVC / MinGW (Windows) | C++14 support | Compiler |
| QEMU (`qemu-system-x86_64`) | 6.0+ | Android x86 emulation |
| SDL2 | 2.0.5+ | Display window and input handling |

#### Install QEMU

**Ubuntu / Debian**
```bash
sudo apt-get update
sudo apt-get install -y qemu-system-x86
```

**Fedora / RHEL / CentOS**
```bash
sudo dnf install -y qemu-system-x86
```

**macOS (Homebrew)**
```bash
brew install qemu
```

**Windows**
Download the QEMU installer from <https://qemu.weilnetz.de/w64/> and add the
install directory (e.g. `C:\Program Files\qemu`) to your `PATH`.

---

## SDL2 Installation

SDL2 is required for the display window and input handling.

### Linux

**Ubuntu / Debian**
```bash
sudo apt-get update
sudo apt-get install -y libsdl2-dev
```

**Fedora / RHEL / CentOS**
```bash
sudo dnf install -y SDL2-devel
```

**Arch Linux**
```bash
sudo pacman -S sdl2
```

### macOS (Homebrew)
```bash
brew install sdl2
```

### Windows (vcpkg)
```powershell
vcpkg install sdl2:x64-windows
```

Or download the SDL2 development library from <https://www.libsdl.org/download-2.0.php>
and add it to your Visual Studio / MinGW include and library paths.

---

## Downloading an Android x86 System Image

The emulator requires an **Android-x86** disk image (`.iso` or `.img`).

1. Visit the official Android-x86 download page:  
   <https://www.android-x86.org/download>

2. Download the latest **64-bit ISO** (e.g. `android-x86_64-9.0-r2.iso`).

3. *(Optional)* Convert the ISO to a writable QEMU disk image so that Android
   can save state between sessions:

   ```bash
   # Create a 16 GB writable disk image
   qemu-img create -f qcow2 android-x86.img 16G

   # Boot from the ISO to install Android to the image (first run only)
   qemu-system-x86_64 \
       -m 2048 -smp 2 \
       -cdrom android-x86_64-9.0-r2.iso \
       -hda android-x86.img \
       -boot d
   ```

   After installation completes, place `android-x86.img` in the project root
   directory.

4. Alternatively, use the provided setup scripts (see [Setup Scripts](#setup-scripts)).

---

## Hardware Acceleration

### Linux – KVM

KVM gives near-native performance and is automatically detected at runtime.

```bash
# Verify KVM is available
ls /dev/kvm

# Add your user to the kvm group (log out and back in afterwards)
sudo usermod -aG kvm $USER
```

### Windows / macOS – HAXM

HAXM (Hardware Accelerated Execution Manager) is automatically detected at
runtime.

**Windows**  
Download the HAXM installer from the [Intel Developer Zone](https://github.com/intel/haxm/releases) and follow the setup wizard.

**macOS**  
```bash
brew install --cask intel-haxm
```

If neither KVM nor HAXM is available the emulator falls back to software
(TCG) emulation, which is slower but still functional.

---

## Building the Emulator

```bash
# 1. Clone the repository
git clone https://github.com/Eddyaditya/android-emulator.git
cd android-emulator

# 2. Configure (out-of-source build)
cmake -S . -B build

# 3. Build
cmake --build build

# 4. The binary is at build/edu_emulator (Linux/macOS) or build/edu_emulator.exe (Windows)
```

---

## Running the Emulator

```bash
# Place your Android x86 image in the project root, then run:
./build/edu_emulator
```

The emulator will:
1. Detect KVM/HAXM automatically
2. Validate the system image
3. Spawn `qemu-system-x86_64` with the correct arguments
4. Open an SDL2 window showing the Android display
5. Forward keyboard and mouse input to QEMU

### Keyboard shortcuts

| Key | Action |
|-----|--------|
| **F11** | Toggle fullscreen / windowed mode |
| **Ctrl+C** (console) | Stop the emulator |

---

## Setup Scripts

Automated scripts are provided in the `scripts/` directory to prepare the
Android x86 image in one step.

**Linux / macOS**
```bash
chmod +x scripts/setup_image.sh
./scripts/setup_image.sh
```

**Windows (Command Prompt)**
```bat
scripts\setup_image.bat
```

Both scripts will:
- Check for QEMU installation
- Download the Android-x86 ISO (requires `wget`/`curl`)
- Create a writable QEMU disk image
- Provide a ready-to-use `android-x86.img` in the project root

---

## Troubleshooting

### `qemu-system-x86_64: command not found`
Install QEMU (see [Prerequisites](#prerequisites)).

### `Image file not found: android-x86.img`
Make sure you have downloaded and placed the image in the project root, or run
the setup script.

### KVM permission denied (`/dev/kvm: Permission denied`)
```bash
sudo usermod -aG kvm $USER
# Then log out and log back in
```

### Slow emulation (TCG fallback)
Enable KVM (Linux) or HAXM (Windows/macOS) to get hardware-accelerated
performance.

### Black screen on boot
Try adding `-vga std` instead of `-vga virtio` in `src/qemu_manager.cpp`
and rebuild. Some graphics configurations require the standard VGA adapter.

### SDL2 window does not open

- **Linux:** Ensure `libsdl2-dev` (or `libsdl2-2.0-0`) is installed and a
  display server is running (Wayland or X11). For headless servers, set
  `SDL_VIDEODRIVER=offscreen` to disable the window (console-only mode).
- **macOS:** Verify SDL2 is installed via Homebrew: `brew info sdl2`.
- **Windows:** Confirm `SDL2.dll` is on your `PATH` or in the same directory
  as `edu_emulator.exe`.

### SDL2: `No available video device`
The emulator can run without a display by setting the environment variable:
```bash
export SDL_VIDEODRIVER=offscreen
./build/edu_emulator
```
In this mode only console output is produced; the SDL2 window is suppressed.

---

## GApps (Google Play Store & Chrome)

Open GApps can be installed into the Android-x86 image to enable the Google Play Store and Chrome browser.

> **Legal notice:** Google's apps are proprietary software. By installing GApps you agree to [Google's Terms of Service](https://policies.google.com/terms) and the [Google Play Terms of Service](https://play.google.com/intl/en_us/about/play-terms/). This feature is intended for educational and personal development use only.

### Quick Install (Linux / macOS)

```bash
chmod +x scripts/install_gapps.sh
./scripts/install_gapps.sh --image android-x86.img --download --variant pico
```

### Quick Install (Windows)

```bat
scripts\install_gapps.bat --image android-x86.img --download --variant pico
```

For the full guide — including variant selection, manual installation, troubleshooting, rollback, and alternative app stores — see [docs/GAPPS_GUIDE.md](docs/GAPPS_GUIDE.md).

