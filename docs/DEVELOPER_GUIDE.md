# Developer Guide

This guide covers developer-specific features of the EDU Android Emulator, including ADB integration, root access, security best practices, and connection management.

---

## Table of Contents

1. [ADB Overview](#adb-overview)
2. [Enabling ADB](#enabling-adb)
3. [Root Access](#root-access)
4. [Security Best Practices](#security-best-practices)
5. [Trusted Hosts Configuration](#trusted-hosts-configuration)
6. [Connection Logging](#connection-logging)
7. [Firewall Rules](#firewall-rules)
8. [Error Recovery](#error-recovery)
9. [Graceful Shutdown](#graceful-shutdown)

---

## ADB Overview

The Android Debug Bridge (ADB) provides a communication channel between your development machine (host) and the Android emulator (guest). The emulator exposes an ADB daemon (`adbd`) over TCP on port **5555** by default.

**Architecture:**

```
Host                              Emulator (QEMU guest)
┌─────────────────┐               ┌────────────────────┐
│  adb client     │◄──── TCP ────►│  adbd (port 5555)  │
│  (your shell)   │               │  Android-x86       │
│       │         │               └────────────────────┘
│  ADB server     │
│  (port 5037)    │
└─────────────────┘
```

---

## Enabling ADB

### Automatic (via setup script)

**Linux / macOS:**
```bash
chmod +x scripts/enable_adb.sh
./scripts/enable_adb.sh
```

**Windows:**
```bat
scripts\enable_adb.bat
```

### Manual

```bash
# Start the local ADB server
adb start-server

# Connect to the emulator over TCP
adb connect localhost:5555

# Verify
adb devices -l
```

For a full command reference, see [ADB_GUIDE.md](ADB_GUIDE.md).

---

## Root Access

> **⚠ Security Warning**
>
> Root access grants unrestricted control over the entire Android system.  
> Misuse can corrupt the disk image, expose private data, or introduce security vulnerabilities.  
> **Only enable root access in an isolated, trusted development environment.**

### How Root Works on Android-x86

The emulator uses the `AdbManager` and `RootManager` C++ classes (see `src/root_manager.h`) to manage root access:

1. `adb root` restarts `adbd` with `uid=0` (available on `userdebug`/`eng` builds).
2. The `su` binary at `/system/xbin/su` or `/system/bin/su` enables per-command escalation.
3. `RootManager::verifyRoot()` confirms root is active by checking the output of `id`.

### Enabling Root

**Via the setup script:**
```bash
./scripts/enable_adb.sh --root
```

**Manually:**
```bash
adb -s localhost:5555 root
adb -s localhost:5555 shell
# id → uid=0(root)
```

### Disabling Root

```bash
adb -s localhost:5555 unroot
```

Or use the `RootManager` API in C++:
```cpp
RootManager rootMgr(adbManager);
rootMgr.disableRoot();
```

### Safe Mode

Safe mode disables third-party apps and restricts `su` access, providing a safer environment for testing:

**Enable safe mode:**
```cpp
rootMgr.enableSafeMode();
```

**Disable safe mode:**
```cpp
rootMgr.disableSafeMode();
```

---

## Security Best Practices

### 1. Keep ADB Off by Default

Do not start the ADB server unless you are actively debugging. Use `AdbConfig::enableTcpAdb = false` in production configurations.

### 2. Restrict ADB to Localhost

Never expose port 5555 to external networks. Use a firewall rule to block inbound connections from outside `127.0.0.1` (see [Firewall Rules](#firewall-rules)).

### 3. Use ADB Authentication

Android enforces RSA-key-based authentication for ADB connections. The host's public key (`~/.android/adbkey.pub`) must be accepted on the device. Never disable authentication (`ADB_VENDOR_KEYS` should not be set to bypass keys).

### 4. Rotate ADB Keys

Generate a fresh ADB key pair periodically:
```bash
# Remove old key
rm ~/.android/adbkey ~/.android/adbkey.pub

# Generate a new one (adb will create it on next connection)
adb kill-server
adb start-server
```

### 5. Disable Root When Not Needed

Always call `adb unroot` (or `RootManager::disableRoot()`) when you have finished debugging to reduce the attack surface.

### 6. Monitor Connection Logs

Enable connection logging (`AdbConfig::logDir`) and review the logs regularly for unexpected connections.

### 7. Use Encrypted Storage

Avoid storing sensitive data in `/sdcard` (unencrypted). Use the Android Keystore system for credentials.

---

## Trusted Hosts Configuration

The `AdbConfig::trustedHostsFile` (default: `~/.android/adb_trusted_hosts`) lists host fingerprints that are pre-authorized to connect without an interactive prompt.

### Format

One SHA-256 fingerprint per line (same format as SSH `known_hosts`):
```
# Trusted ADB host fingerprints
SHA256:AbCdEfGhIjKlMnOpQrStUvWxYz1234567890ABCDEFGH development-laptop
SHA256:ZyXwVuTsRqPoNmLkJiHgFeEdCbA0987654321zyxwvuts ci-server
```

### Viewing the Current Host Key

```bash
adb keygen /tmp/adb_test_key
cat /tmp/adb_test_key.pub
```

The public key fingerprint is shown when a new device first connects.

---

## Connection Logging

ADB and logcat logs are written to `AdbConfig::logDir` (default: `/tmp/adb_logs` on Linux/macOS, `%TEMP%\adb_logs` on Windows).

### Log Files

| File | Contents |
|------|----------|
| `logcat.log` | Android system log (all priorities) |

### Viewing Logs

```bash
# Tail the logcat log
tail -f /tmp/adb_logs/logcat.log

# Filter errors only
grep " E " /tmp/adb_logs/logcat.log
```

### Starting Logcat via Script

```bash
./scripts/enable_adb.sh --logcat
```

---

## Firewall Rules

### Linux (iptables)

Allow ADB connections from localhost only:
```bash
# Allow localhost
sudo iptables -A INPUT -p tcp --dport 5555 -s 127.0.0.1 -j ACCEPT

# Block all other sources
sudo iptables -A INPUT -p tcp --dport 5555 -j DROP
```

Persist the rules (Debian/Ubuntu):
```bash
sudo apt-get install -y iptables-persistent
sudo netfilter-persistent save
```

### macOS (pf)

Add to `/etc/pf.conf`:
```
# ADB: allow only localhost
pass in on lo0 proto tcp from 127.0.0.1 to any port 5555
block in proto tcp to any port 5555
```

Apply:
```bash
sudo pfctl -f /etc/pf.conf
sudo pfctl -e
```

### Windows (Windows Firewall)

```powershell
# Block all inbound connections to port 5555 except from localhost
New-NetFirewallRule -DisplayName "ADB Block External" `
    -Direction Inbound -Protocol TCP -LocalPort 5555 `
    -RemoteAddress "0.0.0.0/0" -Action Block

New-NetFirewallRule -DisplayName "ADB Allow Localhost" `
    -Direction Inbound -Protocol TCP -LocalPort 5555 `
    -RemoteAddress "127.0.0.1" -Action Allow
```

---

## Error Recovery

### ADB Server Crashes

If the ADB server becomes unresponsive:
```bash
adb kill-server
adb start-server
adb connect localhost:5555
```

In C++:
```cpp
adbManager.stopServer();
adbManager.startServer();
adbManager.connect();
```

### Device Appears Offline

```bash
adb disconnect localhost:5555
adb connect localhost:5555
adb -s localhost:5555 wait-for-device
```

### Root Access Lost After Reboot

`adb root` does not persist across reboots. Re-run:
```bash
adb root
adb connect localhost:5555
```

Or use the `--root` flag when running the setup script.

### adbd Keeps Crashing

Check logcat for crash details:
```bash
adb -s localhost:5555 logcat -d | grep -i "adbd\|crash\|fatal"
```

---

## Graceful Shutdown

Always disconnect ADB before shutting down the emulator to avoid dangling TCP connections:

```bash
# Stop logcat (if running)
pkill -f "adb logcat"   # Linux/macOS
taskkill /F /IM adb.exe  # Windows

# Disconnect
adb disconnect localhost:5555

# Stop ADB server
adb kill-server

# Shut down the emulator (via QEMU monitor)
adb -s localhost:5555 reboot -p
```

In C++:
```cpp
rootMgr.disableRoot();
adbManager.stopLogcat();
adbManager.disconnect();
adbManager.stopServer();
```

---

*For ADB command reference and file transfer instructions, see [ADB_GUIDE.md](ADB_GUIDE.md).*
