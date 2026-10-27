# LetsChat

A cross-platform Qt 6.12 LAN chat application for communicating within local networks. Works on Desktop (Windows, Linux, macOS) and Android.

## Features

- **LAN Discovery**: Automatically finds other devices running LetsChat on the same network
- **Peer Management**: Tracks peers across network changes - marks previous LAN peers as offline when switching networks
- **Unique Identity**: Each device gets a unique UUID on first run, with customizable alias
- **Text & Media**: Exchange text messages and media files
- **Message Persistence**: Messages are stored locally in encrypted SQLite database
- **Encrypted Storage**: User data is encrypted before storage
- **Background Operation**: Continues scanning for peers in background
- **Notifications**: Platform-specific notifications for messages and files (Desktop & Android)
- **Theme Support**: Light, Dark, and System theme modes
- **Qt Designer Compatible**: UI files (.ui) are provided for Qt Designer

## Project Structure

```
LetsChat/
├── src/                    # C++ source code
│   ├── core/              # Core functionality (Identity, Theme, Settings, AppConfig)
│   ├── network/           # LAN discovery, peer management, file transfer
│   ├── storage/           # Database and encryption
│   ├── notifications/     # Notification system
│   ├── models/            # QML models
│   ├── platform/          # Platform-specific background service
│   └── utils/             # Utilities
├── qml/                   # QML UI files
│   ├── components/        # Reusable components
│   ├── views/            # Main views
│   ├── dialogs/          # Dialogs
│   └── themes/           # Theme definitions
├── forms/                 # Qt Designer UI files (.ui)
├── resources/             # Resources (icons, etc.)
└── android/              # Android-specific configuration
```

## Building

### Prerequisites

- Qt 6.12 or later
- CMake 3.16+
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
adb install -r build/Qt_6_12_0_for_Android_arm64_v8a_Debug/android-build/app-debug.apk
```

- **Release**: Install the signed APK
```bash
adb install -r your_app_release.apk
```

## First Run

1. Launch the app - you'll see the onboarding screen
2. Your device name is shown automatically
3. Optionally set an alias (others will see this if set, otherwise device name)
4. Click "Get Started"
5. The app will start scanning for peers on your LAN

## Usage

- **Switching Networks**: When you join a different LAN, peers from the previous network are automatically marked offline
- **Changing Alias**: Use the menu (...) > Change Alias
- **Settings**: Access theme, notifications, and other settings
- **Background Mode**: The app continues scanning even when minimized

## Package Information

- **Android Package Name**: `com.smalnets.letschat`
- **Organization**: SmalNets
- **Version**: 0.1

## Network Ports

- Discovery: UDP 45454
- File Transfer: TCP 45455
