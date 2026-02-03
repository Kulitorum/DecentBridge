# DecentBridge

A headless BLE-to-HTTP/WebSocket bridge server for DE1 espresso machines and scales. Written in C++17 with Qt 6.

## Project Overview

DecentBridge connects to DE1 espresso machines and various Bluetooth scales via BLE, exposing their functionality through REST and WebSocket APIs. It's designed to run on the DE1's Android tablet or any platform with Qt 6 and Bluetooth support.

### Origin

This project combines:
- **ReaPrime's** clean REST/WebSocket API design (the "REA" server concept)
- **Decenza's** robust C++ BLE implementations for DE1 and 14 different scales

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      DecentBridge                           │
├─────────────────────────────────────────────────────────────┤
│  HTTP Server (8080)          WebSocket Server (8081)        │
│  - REST API                  - Real-time streaming          │
│  - Device control            - Shot samples                 │
│  - Settings                  - Scale weight                 │
├─────────────────────────────────────────────────────────────┤
│                         Bridge                              │
│  - Orchestrates BLE ↔ Network                              │
│  - Auto-connects to DE1 and scales                         │
├─────────────────────────────────────────────────────────────┤
│  BLEManager        DE1Device           ScaleDevice          │
│  - Discovery       - DE1 protocol      - Base class         │
│  - Connection      - State machine     - 14 implementations │
├─────────────────────────────────────────────────────────────┤
│              ScaleBleTransport (Platform Abstraction)       │
│  - QtScaleBleTransport (Desktop)                           │
│  - AndroidScaleBleTransport (Android - fixes Qt BLE bugs)  │
│  - CoreBluetoothScaleBleTransport (iOS)                    │
└─────────────────────────────────────────────────────────────┘
```

## Supported Scales

All 14 scales from Decenza are supported:
- Acaia (Lunar, Pearl, Pyxis)
- Atomheart Eclair
- Bookoo
- Decent Scale
- DiFluid
- Eureka Precisa
- Felicita (Arc, Parallel)
- Flow (virtual scale using DE1 flow data)
- Hiroia Jimmy
- Skale
- SmartChef
- SoloBarista
- Varia Aku

## Directory Structure

```
DecentBridge/
├── CMakeLists.txt              # Build configuration
├── CLAUDE.md                   # This file
├── android/                    # Android-specific files
│   ├── AndroidManifest.xml     # Permissions & app config
│   └── res/drawable-*/         # App icons
└── src/
    ├── main.cpp                # Entry point
    ├── core/
    │   ├── bridge.cpp/h        # Main orchestrator
    │   └── settings.cpp/h      # Configuration
    ├── ble/
    │   ├── blemanager.cpp/h    # BLE discovery
    │   ├── de1device.cpp/h     # DE1 protocol
    │   ├── scaledevice.cpp/h   # Scale base class
    │   ├── protocol/
    │   │   ├── de1characteristics.h  # DE1 BLE UUIDs
    │   │   └── binarycodec.cpp/h     # Binary encoding
    │   ├── scales/             # Scale implementations
    │   │   ├── scalefactory.cpp/h    # Auto-detection
    │   │   ├── acaiascale.cpp/h
    │   │   ├── decentscale.cpp/h
    │   │   └── ... (12 more)
    │   └── transport/          # Platform-specific BLE
    │       ├── scalebletransport.h        # Interface
    │       ├── qtscalebletransport.cpp/h  # Desktop
    │       ├── androidscalebletransport.cpp/h
    │       └── corebluetooth/             # iOS
    └── network/
        ├── httpserver.cpp/h    # REST API
        └── websocketserver.cpp/h
```

## Building

### Prerequisites

- Qt 6.10+ with:
  - Core, Bluetooth, Network, WebSockets, Gui (Gui needed for Android)
- CMake 3.21+
- For Android: Android SDK & NDK

### Windows (Desktop)

```bash
cd C:/code/DecentBridge
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64"
cmake --build build
```

### Android

```bash
cd C:/code/DecentBridge
cmake -S . -B build/android-debug \
  -G Ninja \
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="C:/Users/Micro/AppData/Local/Android/Sdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_SDK_ROOT="C:/Users/Micro/AppData/Local/Android/Sdk" \
  -DQT_HOST_PATH="C:/Qt/6.10.1/msvc2022_64" \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/android_arm64_v8a" \
  -DCMAKE_FIND_ROOT_PATH="C:/Qt/6.10.1/android_arm64_v8a"

cmake --build build/android-debug
```

APK output: `build/android-debug/android-build/build/outputs/apk/debug/android-build-debug.apk`

### Install on DE1 Tablet

```bash
adb connect 192.168.1.208
adb -s 192.168.1.208:5555 install -r build/android-debug/android-build/build/outputs/apk/debug/android-build-debug.apk
```

## API Reference

### HTTP Endpoints (Port 8080)

#### Devices
- `GET /api/v1/devices` - List connected devices
- `GET /api/v1/devices/scan` - Start BLE scan
- `PUT /api/v1/devices/connect?deviceId=<id>` - Connect to device

#### Machine
- `GET /api/v1/machine/info` - Firmware version, model, serial
- `GET /api/v1/machine/state` - Current state, temps, pressures
- `PUT /api/v1/machine/state/<newState>` - Change state (idle, espresso, steam, etc.)
- `GET /api/v1/machine/settings` - Machine settings
- `POST /api/v1/machine/settings` - Update settings
- `POST /api/v1/machine/profile` - Upload profile

#### Scale
- `PUT /api/v1/scale/tare` - Tare the scale

#### Settings
- `GET /api/v1/settings` - Bridge settings
- `POST /api/v1/settings` - Update bridge settings

### WebSocket Channels (Port 8081)

- `/ws/v1/machine/snapshot` - Real-time shot data (pressure, flow, temp)
- `/ws/v1/machine/shotSettings` - Shot settings updates
- `/ws/v1/machine/waterLevels` - Water tank levels
- `/ws/v1/scale/snapshot` - Scale weight and flow rate
- `/ws/v1/machine/raw` - Raw DE1 commands

## Platform-Specific Notes

### ScaleBleTransport Abstraction

Qt's Bluetooth implementation has issues with certain scales on Android. The `ScaleBleTransport` abstraction layer provides platform-specific implementations:

- **Desktop (Windows, macOS, Linux)**: Uses `QtScaleBleTransport` - standard Qt Bluetooth
- **Android**: Uses `AndroidScaleBleTransport` - native Android Bluetooth via JNI
- **iOS**: Uses `CoreBluetoothScaleBleTransport` - native CoreBluetooth via Objective-C++

### Android Permissions

The AndroidManifest.xml includes:
- `BLUETOOTH`, `BLUETOOTH_ADMIN`, `BLUETOOTH_CONNECT`, `BLUETOOTH_SCAN`
- `ACCESS_FINE_LOCATION`, `ACCESS_COARSE_LOCATION` (required for BLE scanning)
- `INTERNET` (for HTTP/WebSocket servers)
- `FOREGROUND_SERVICE` (to keep running in background)

## Related Projects

- **Decenza** (`C:/code/de1-qt`) - Full DE1 app with GUI, source of scale implementations
- **ReaPrime** (`C:/code/reaprime`) - Flutter app with embedded JS plugin system

## Development Notes

- Uses Qt's signal/slot mechanism for async BLE operations
- `ScaleFactory::createScale()` auto-detects scale type from BLE device name
- DE1 uses BLE Service UUID `0000A000-0000-1000-8000-00805F9B34FB`
- Scale implementations handle manufacturer-specific protocols (checksums, encoding, etc.)
