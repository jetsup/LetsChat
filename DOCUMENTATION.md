# LetsChat – Application Documentation

LetsChat is a Qt 6 / QML messenger for **people who are physically close to each other**. It needs no
internet connection and no accounts:

* **Wi-Fi / LAN** – devices on the same network find each other automatically (UDP broadcast) and exchange
  messages over TCP.
* **Bluetooth** – nearby devices connect directly over Bluetooth Classic (RFCOMM), useful when there is no
  shared Wi-Fi network.
* **Background operation** – on Android a foreground service keeps the app reachable; on desktop the app
  lives in the system tray.

Targets: Linux, Windows, macOS (desktop) and Android (Qt 6.12, tested on Android 16).

---

## 1. User guide

| Action | How |
|---|---|
| First start | Enter a display name (optional – the device name is used otherwise) and tap **Get started**. |
| Chat over Wi-Fi | Open LetsChat on two devices on the same network. They appear in the list within a few seconds; tap one to chat. |
| Chat over Bluetooth | Tap the Bluetooth icon (top bar) → **Nearby devices**. Turn Bluetooth on if asked, scan, then **Connect**. The other device must run LetsChat. Once linked, the peer shows up in the chat list with a *Bluetooth* badge. |
| Make this device visible | Nearby devices → **Make this device visible** (needed the first time two devices meet). |
| Settings | Gear icon: name, theme (System/Light/Dark), background running, notifications, Bluetooth switch. |
| Android Back button | Closes the open page/chat; from the chat list it moves the app to the background (the service keeps running). |
| Desktop close button | Hides the window to the tray (if a tray exists and *Keep running in background* is on). Use tray menu → **Quit** or Settings → **Quit LetsChat** to exit. |

Message status: *Sending…* → delivered (just the time) or *Not delivered* (red). LAN messages are
acknowledged by the receiver; if a LAN send fails and a Bluetooth link to the same peer exists, the message
is re-sent over Bluetooth automatically.

### Android background limitations (important)

The app does everything Android offers for staying alive: a foreground service
(`foregroundServiceType=connectedDevice`) with a persistent notification, a Wi-Fi multicast lock, a partial
wake lock, `android.app.background_running=true` so Qt keeps its event loop running, and a request to be
exempt from battery optimisation (Settings → *Unrestricted battery use*).

Some manufacturers add their **own** freezer on top. On the Infinix X6855 used for testing (XOS), the
system log shows `Usf_Hiber … freeze uid: … com.smalnets.letschat` a few seconds after the app leaves the
foreground, even with the service running and every standard battery exemption granted. While frozen the
whole process (including plain native threads) is halted, so peers see the device as offline. If you
see this, look for a vendor setting such as *Phone Master / Battery / App freeze / Auto-start / Lock app in
recents* and allow LetsChat there. Stock Android and most other vendors do not do this. This was **not**
resolved on the test device.

---

## 2. Architecture

```
main.cpp                    creates the backend objects, registers them to QML, starts the engine
src/
  core/        AppConfig        ports, timeouts, Bluetooth UUID, protocol version
               IdentityManager  persistent user id (UUID), alias, onboarding flag
               ThemeManager     system / light / dark
  chat/        ChatController   glue: transports <-> database <-> models <-> notifications
  network/     DiscoveryService UDP presence broadcast + peer expiry
               TransferService  TCP server/client, framed JSON, acknowledgements
               BluetoothService RFCOMM server, device scan, links, hello + text frames
               PeerManager      merges LAN + Bluetooth knowledge into the peer list
               LanTracker       notices Wi-Fi/network changes
  storage/     DatabaseManager  SQLite (peers, messages) – message bodies encrypted
               CryptoUtils      HMAC-SHA256 stream cipher + MAC
  models/      PeerListModel    peers with online state per transport, unread count, last message
               MessageModel     messages of the open conversation
  notifications/ NotificationManager  Android notification / desktop tray balloon
  platform/    BackgroundService      Android foreground service / desktop tray + hide-on-close
  utils/       DeviceInfo       device name, best local IPv4
qml/
  Main.qml                 window, responsive shell (side-by-side ≥ 820 px, single column below)
  themes/Theme.qml         colour/metric singleton (follows ThemeManager)
  views/                   Onboarding, Contacts (chat list), Chat, Settings, Bluetooth
  components/              Icon, IconButton, Avatar, PeerItem, MessageBubble, TransportBadge, Toast
  dialogs/AliasDialog.qml
android/
  AndroidManifest.xml      permissions, activity, foreground service, background_running flag
  src/.../LetsChatService.java   foreground service, notifications, permission helpers
  res/drawable-*/          launcher icon and notification icon
```

Backend objects are exposed to QML as singletons of the `LetsChat` module:
`Identity`, `Peers`, `Chat`, `Bluetooth`, `Background`, `Notifier`, `ThemeManager`, `Device`, `AppConfig`.

### Data flow

```
 incoming:  TransferService / BluetoothService ──► ChatController ──► DatabaseManager
                                                        │
                                                        ├──► MessageModel (if that chat is open)
                                                        ├──► PeerListModel (last message, unread)
                                                        └──► NotificationManager (not viewing / app inactive)
 outgoing:  QML Chat.sendMessage() ► ChatController ► TransferService (LAN, ack) or BluetoothService
```

---

## 3. Protocols

All frames are UTF-8 JSON objects terminated by `\n`.

### LAN discovery – UDP 45454 (broadcast)
Sent every 3 s to every interface's broadcast address:
```json
{"type":"discovery","v":1,"userId":"<uuid>","alias":"Alice","deviceName":"Pixel 8"}
```
A peer is *lost* after 12 s without announcement. Changing network (`LanTracker`) clears LAN state and
announces again immediately.

### LAN messages – TCP 45455 (one short connection per message)
```
→ {"type":"text","v":1,"id":"<uuid>","fromUserId":"…","alias":"…","content":"hi","timestamp":"ISO-8601"}
← {"type":"ack","id":"<uuid>"}
```
The sender fails the message after 5 s without an ack. Duplicate ids are ignored by the receiver.

### Bluetooth – RFCOMM service UUID `e3a1c5a4-7c6e-4b0a-9f55-1d2b7a0c9e31`
After connecting both sides send `{"type":"hello","userId","alias","deviceName"}`; afterwards the same
`text` frames as above are exchanged over the persistent link (no ack – RFCOMM is reliable). When two
devices dial each other simultaneously the first link wins. Addresses of devices that have linked once are
remembered and re-connected automatically when they show up in a scan.

---

## 4. Storage and security

* SQLite file `letschat.db` in the app data directory (`QStandardPaths::AppDataLocation`).
* `messages` (id, from, to, content, type, timestamp, conversation_id, status) – `content` is stored as
  `enc1:` + base64(nonce ‖ ciphertext ‖ MAC) using `CryptoUtils`. The 32-byte key is generated per
  installation and kept in the app's private settings.
* `peers` (user_id, alias, device_name, ip_address, bt_address, last_seen).
* Databases from version 0.1 are migrated in place; legacy plaintext rows are still readable.

**Limits – please read:** LAN traffic is **not encrypted or authenticated**; anyone on the network can read
or spoof messages. The at-rest encryption uses a home-made construction built from HMAC-SHA256 and a key
stored next to the data, so it only deters casual inspection. For real security use TLS (e.g. `QSslSocket`
with pinned device certificates) and a vetted AEAD cipher.

---

## 5. Android specifics

* **Permissions** (see manifest): network + multicast, `POST_NOTIFICATIONS` (13+), `FOREGROUND_SERVICE` +
  `…_CONNECTED_DEVICE`, `WAKE_LOCK`, `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS`, `BLUETOOTH_SCAN/CONNECT/ADVERTISE`
  (12+; legacy `BLUETOOTH*`/location for ≤ 11). The Bluetooth runtime permission is requested through
  `QBluetoothPermission` the first time it is needed; the notification permission when the service starts.
* **Service**: `LetsChatService` (same process). Started by `BackgroundService::start()` when
  *Keep running in background* is on. It owns the multicast lock and wake lock.
* **Background event loop**: Qt blocks its event loop when the activity is suspended unless the manifest has
  `android.app.background_running=true` (Qt's loader turns that into `QT_BLOCK_EVENT_LOOPS_WHEN_SUSPENDED=0`).
  Because Android destroys the window surface in the background, `main.cpp` hides the Quick window while the
  app is `Suspended/Hidden` so Qt does not draw into a dead surface.
* **Device name**: `Build.MODEL` (the usual hostname is just “localhost” on Android).
* **Icons**: `android/res/drawable-*` (launcher + notification) and `resources/icons/app.png` (desktop/tray).

---

## 6. Building

### Desktop
```bash
cmake -S . -B build/desktop -G Ninja -DCMAKE_PREFIX_PATH=~/Qt/6.12.0/gcc_64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/desktop
./build/desktop/appLetsChat
```
Requires Qt 6.5+ with Quick, QuickControls2, Sql, Network, Bluetooth and Widgets (desktop only, for the tray).
Linux Bluetooth needs BlueZ (`bluetoothd`) running.

### Android
Use the *Qt for Android arm64-v8a* kit in Qt Creator, or on the command line configure with the Qt Android
prefix, `QT_HOST_PATH`, the NDK toolchain file and `ANDROID_SDK_ROOT`, then build the `apk` target:
```bash
cmake --build build/android --target apk
adb install -r build/android/android-build-appLetsChat/build/outputs/apk/debug/android-build-appLetsChat-debug.apk
```
The first start of a debug build takes ~10 s (library extraction) before the first frame appears.

### Debugging on a device
```bash
adb logcat -c && adb shell am start -n com.smalnets.letschat/org.qtproject.qt.android.bindings.QtActivity
adb logcat --pid=$(adb shell pidof com.smalnets.letschat) | grep " default :"   # Qt qInfo/qWarning output
adb shell dumpsys activity services com.smalnets.letschat | grep isForeground    # service running?
adb logcat | grep -i "freeze uid"                                                # vendor freezer (see §1)
```
Useful log lines: `Discovery: listening…`, `Transfer: listening…`, `LAN: network is now…`,
`Background: Android foreground service requested`, `Bluetooth: …`.

---

## 7. Issues found and fixed in this revision

| Area | Problem | Fix |
|---|---|---|
| Android build | Manifest referenced a missing `@drawable/icon` → resource linking failed | Added launcher/notification icons |
| Identity | `generateIfNeeded()` never cleared `isFirstRun`; id generated before onboarding finished | Id created once at start; explicit `onboarded` flag |
| QML wiring | Theme singleton and sub-directory imports never registered; every backend access guarded by `typeof` | Proper QML singleton + module singletons |
| Messaging | Chat view never sent anything; no selected peer; received messages saved twice with different ids | New `ChatController`, one id per message, de-duplication |
| TCP | `readAll()` assumed one complete JSON per read; no ack, no timeout, errors silent | Newline framing, ack, 5 s timeout, delivery status |
| Discovery | Peers were never reported as lost; re-appearing peers ignored | Expiry timer, IP/alias change detection, retry when the port is busy |
| Database | Two `DatabaseManager`s fought over the same default connection; fixed hard-coded key; plain XOR | Single named connection, per-install key, authenticated cipher, migration |
| Network id | `getLocalIpAddress()` returned the first IPv4 (often mobile data on Android) | Prefers private LAN addresses, skips cellular/VPN/virtual interfaces |
| Android | Foreground service, notifications and background execution were stubs | `LetsChatService`, JNI helpers, `background_running`, runtime permissions |
| Android UI | Window already sits between the system bars; extra insets doubled the header | Insets removed |
| Dead code | `sendFile` stub, unused `SettingsManager`, duplicated settings | Removed (file transfer is *not* implemented) |

## 8. Known limitations / ideas

* Text messages only – no file/media transfer yet.
* LAN traffic unencrypted (see §4). Add TLS + device pairing.
* Android background reliability depends on vendor battery management (see §1).
* **Bluetooth was implemented and builds/starts on Android and desktop, but a two-device Bluetooth chat has not
  been verified end-to-end** (only one phone and a desktop without a second Bluetooth peer were available).
* Bluetooth Classic only; BLE advertising for zero-touch discovery would be a natural next step.
* Peers on other subnets/VLANs are not found by broadcast discovery.
* The `forms/*.ui` Designer files and `importedcontent/` are leftovers of the original template and unused.
