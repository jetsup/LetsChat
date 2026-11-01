# LetsChat

A Qt 6 / QML messenger for talking to people **nearby – no internet needed**. Works on Linux, Windows,
macOS and Android.

## Features

- **Wi-Fi / LAN chat**: devices on the same network discover each other automatically (UDP broadcast) and
  exchange messages over TCP with delivery confirmation
- **Bluetooth chat**: scan for nearby devices, connect over Bluetooth Classic (RFCOMM) and chat when there is
  no shared network; messages fall back to Bluetooth if Wi-Fi delivery fails
- **Background service**: Android foreground service (persistent notification) and desktop system tray, so
  messages keep arriving when the window is closed (some vendors' own freezers can still suspend the app –
  see the documentation)
- **Notifications** for new messages (Android notification channel / tray balloon)
- **Persistent history**: SQLite database, message bodies encrypted at rest; unread counters, last-message preview
- **Responsive Material UI**: side-by-side layout on wide windows, single column on phones; light, dark or
  system theme
- Unique device identity (UUID) with an editable display name

> Security note: LAN/Bluetooth traffic is not end-to-end encrypted. See [DOCUMENTATION.md](DOCUMENTATION.md#4-storage-and-security).

## Documentation

Full documentation (user guide, architecture, protocols, Android notes, debugging, fixed issues):
**[DOCUMENTATION.md](DOCUMENTATION.md)**

## Project structure

```
LetsChat/
├── main.cpp            # wiring
├── src/
│   ├── core/           # AppConfig, IdentityManager, ThemeManager
│   ├── chat/           # ChatController (transports <-> storage <-> UI)
│   ├── network/        # Discovery (UDP), Transfer (TCP), Bluetooth (RFCOMM), PeerManager, LanTracker
│   ├── storage/        # SQLite + encryption
│   ├── models/         # PeerListModel, MessageModel
│   ├── notifications/  # NotificationManager
│   ├── platform/       # BackgroundService (Android service / desktop tray)
│   └── utils/          # DeviceInfo
├── qml/                # Main.qml, views/, components/, dialogs/, themes/
├── android/            # manifest, foreground service (Java), icons
└── resources/          # app icon
```

## Building

### Prerequisites

- Qt 6.5 or later (developed with 6.12) incl. Bluetooth, Quick Controls 2, SQL (+ Widgets on desktop)
- CMake 3.21+
- Ninja (recommended)
- For Android: Android SDK/NDK, Qt for Android

### Debug Build (Desktop)

```bash
cd LetsChat
mkdir -p build/Desktop_Qt_6_12_0_Debug
cd build/Desktop_Qt_6_12_0_Debug
cmake ../.. -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja
./appLetsChat
```

### Release Build (Desktop)

```bash
cd LetsChat
mkdir -p build/Desktop_Qt_6_12_0_Release
cd build/Desktop_Qt_6_12_0_Release
cmake ../.. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
./appLetsChat
```

### Android Build

#### Using Qt Creator
1. Open the project in Qt Creator
2. Select an Android kit (arm64-v8a recommended)
3. Build in Debug or Release mode
4. Deploy to connected device

#### Using Command Line

**Debug Build:**
```bash
cd LetsChat
mkdir -p build/Android_arm64_v8a_Debug
cd build/Android_arm64_v8a_Debug
cmake ../.. -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DQT_ANDROID_ABIS=arm64-v8a
ninja
androiddeployqt --input android-deployment-settings.json --output <output_dir> --android-platform android-28 --jdk $JAVA_HOME --gradle
```

**Release Build:**
```bash
cd LetsChat
mkdir -p build/Android_arm64_v8a_Release
cd build/Android_arm64_v8a_Release
cmake ../.. -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DQT_ANDROID_ABIS=arm64-v8a
ninja
androiddeployqt --input android-deployment-settings.json --output <output_dir> --android-platform android-28 --jdk $JAVA_HOME --gradle --release --sign <keystore> <alias>
```

## Running

### Desktop
After building, run the executable:
```bash
./appLetsChat
```

### Android
- **Debug**: Deploy via Qt Creator or adb
```bash
adb install -r build/<android-kit>/android-build-appLetsChat/build/outputs/apk/debug/android-build-appLetsChat-debug.apk
```

- **Release**: Install the signed APK
```bash
adb install -r your_app_release.apk
```

## First Run

1. Launch the app and enter a display name (optional), then tap **Get started**
2. Allow the notification and "nearby devices" (Bluetooth) permissions on Android
3. People on the same Wi-Fi appear automatically; for Bluetooth tap the Bluetooth icon > scan > Connect

## Usage

- **Switching networks**: peers of the previous network are marked offline automatically
- **Settings** (gear icon): display name, theme, background running, notifications, Bluetooth
- **Background mode**: Android keeps a foreground service running; on desktop closing the window hides it to the tray

## Package Information

- **Android package**: `com.smalnets.letschat` (min. Android 9 / API 28 with Qt 6.12)
- **Organization**: SmalNets
- **Version**: 0.2

## Network Ports

- Discovery: UDP 45454
- Messages: TCP 45455
- Bluetooth: RFCOMM service UUID `e3a1c5a4-7c6e-4b0a-9f55-1d2b7a0c9e31`
