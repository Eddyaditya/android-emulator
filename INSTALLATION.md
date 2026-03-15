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
4. Display boot status in the console

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
