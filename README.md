# DecentBridge

A bridge server that connects your DE1 espresso machine and Bluetooth scale to the network, letting you control them from any app or browser.

## What Does It Do?

DecentBridge runs on your DE1 tablet (or any computer) and:

1. **Connects to your DE1** via Bluetooth
2. **Connects to your scale** via Bluetooth (supports 14 different scales)
3. **Provides a web API** so other apps can control your machine

This means you can:
- Control your DE1 from a custom app
- Monitor shots in real-time from your phone or computer
- Build your own coffee automation tools
- Use any scale with any app (the bridge handles the translation)

## Supported Scales

- Acaia (Lunar, Pearl, Pyxis)
- Atomheart Eclair
- Bookoo
- Decent Scale
- DiFluid
- Eureka Precisa
- Felicita (Arc, Parallel)
- Hiroia Jimmy
- Skale
- SmartChef
- SoloBarista
- Varia Aku

## Quick Start

### Option 1: Install Pre-built APK (Easiest)

1. Download the latest APK from Releases
2. Connect to your DE1 tablet via ADB:
   ```
   adb connect 192.168.1.208
   ```
3. Install the APK:
   ```
   adb install DecentBridge.apk
   ```
4. Open "DecentBridge" from the app drawer
5. The server starts automatically on ports 8080 (HTTP) and 8081 (WebSocket)

### Option 2: Build From Source

See [Building From Source](#building-from-source) below.

## Using the API

Once DecentBridge is running, you can access your DE1 from any device on the same network.

### Check if it's working

Open a browser and go to:
```
http://192.168.1.208:8080/api/v1/devices
```

You should see a list of connected devices (DE1 and/or scale).

### Example: Get machine state

```bash
curl http://192.168.1.208:8080/api/v1/machine/state
```

Returns:
```json
{
  "state": {"state": "Idle", "substate": "Ready"},
  "pressure": 0.0,
  "flow": 0.0,
  "groupTemperature": 92.5,
  "steamTemperature": 140.0
}
```

### Example: Tare the scale

```bash
curl -X PUT http://192.168.1.208:8080/api/v1/scale/tare
```

### Example: Start espresso

```bash
curl -X PUT http://192.168.1.208:8080/api/v1/machine/state/espresso
```

### Real-time data via WebSocket

Connect to `ws://192.168.1.208:8081/ws/v1/machine/snapshot` to receive live shot data:

```javascript
const ws = new WebSocket('ws://192.168.1.208:8081/ws/v1/machine/snapshot');
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log(`Pressure: ${data.pressure} bar, Flow: ${data.flow} ml/s`);
};
```

## Building From Source

### Requirements

- **Qt 6.10 or later** - Download from [qt.io](https://www.qt.io/download)
  - Select these components: Core, Bluetooth, Network, WebSockets, Gui
- **CMake 3.21 or later** - Usually included with Qt
- **For Android builds**: Android SDK and NDK

### Building for Windows

1. Open a terminal (Command Prompt or PowerShell)

2. Navigate to the project:
   ```
   cd C:\code\DecentBridge
   ```

3. Configure the build (adjust the Qt path to match your installation):
   ```
   cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64"
   ```

4. Build:
   ```
   cmake --build build --config Release
   ```

5. Run:
   ```
   build\Release\DecentBridge.exe
   ```

### Building for Android

1. Make sure you have Android SDK and NDK installed (Qt Maintenance Tool can install these)

2. Configure the build:
   ```bash
   cmake -S . -B build/android \
     -G Ninja \
     -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe" \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_TOOLCHAIN_FILE="C:/Users/YOUR_USERNAME/AppData/Local/Android/Sdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" \
     -DANDROID_ABI=arm64-v8a \
     -DANDROID_PLATFORM=android-28 \
     -DANDROID_SDK_ROOT="C:/Users/YOUR_USERNAME/AppData/Local/Android/Sdk" \
     -DQT_HOST_PATH="C:/Qt/6.10.1/msvc2022_64" \
     -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/android_arm64_v8a" \
     -DCMAKE_FIND_ROOT_PATH="C:/Qt/6.10.1/android_arm64_v8a"
   ```

   Replace `YOUR_USERNAME` with your Windows username.

3. Build:
   ```
   cmake --build build/android
   ```

4. The APK will be at:
   ```
   build/android/android-build/build/outputs/apk/debug/android-build-debug.apk
   ```

### Building with Qt Creator (Easiest)

1. Open Qt Creator
2. File → Open File or Project
3. Select `CMakeLists.txt` from the DecentBridge folder
4. Select your kit (Desktop for PC, Android for tablet)
5. Click Build (Ctrl+B)
6. Click Run (Ctrl+R) or deploy to device

## API Reference

### HTTP Endpoints (Port 8080)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/devices` | List connected devices |
| GET | `/api/v1/devices/scan` | Start scanning for devices |
| GET | `/api/v1/machine/info` | Get machine info (model, firmware) |
| GET | `/api/v1/machine/state` | Get current state and sensor readings |
| PUT | `/api/v1/machine/state/{state}` | Change state (idle, espresso, steam, water, flush) |
| GET | `/api/v1/machine/settings` | Get machine settings |
| POST | `/api/v1/machine/settings` | Update machine settings |
| POST | `/api/v1/machine/profile` | Upload a profile |
| PUT | `/api/v1/scale/tare` | Tare the scale |

### WebSocket Channels (Port 8081)

| Channel | Description |
|---------|-------------|
| `/ws/v1/machine/snapshot` | Real-time pressure, flow, temperature |
| `/ws/v1/scale/snapshot` | Real-time weight and flow rate |
| `/ws/v1/machine/waterLevels` | Water tank levels |
| `/ws/v1/machine/shotSettings` | Shot settings updates |

## Troubleshooting

### "DE1 not connected"

- Make sure Bluetooth is enabled on the device running DecentBridge
- The DE1 must be powered on and not connected to another app
- Check that you're on the same network as the DE1 tablet

### "Scale not found"

- Make sure the scale is powered on and in pairing mode
- Some scales need to be woken up first (step on them or press a button)
- Check that no other app is connected to the scale

### Can't connect to the API

- Verify the IP address of the device running DecentBridge
- Make sure ports 8080 and 8081 are not blocked by a firewall
- Check that both devices are on the same WiFi network

### Android build fails

- Make sure Android SDK and NDK paths are correct
- Try cleaning the build: `rm -rf build/android` and rebuild
- Check that Qt for Android is installed via Qt Maintenance Tool

## License

MIT License - See LICENSE file for details.

## Contributing

Contributions are welcome! Please open an issue or pull request on GitHub.

## Acknowledgments

- Scale implementations based on [Decenza](https://github.com/kulitorum/de1-qt)
- API design inspired by [ReaPrime](https://github.com/tadelv/reaprime)
