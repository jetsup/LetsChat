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
| Take a picture (Android) | Camera icon inside the message field (shown while the field is empty). The system camera opens; after confirming the shot a preview appears with an optional caption and Send / Cancel. |
| Voice note | Microphone icon inside the message field (shown while empty): **press and hold** to record, release to stop. A confirmation dialog lets you listen, then **Send** or **Discard**. Voice notes (up to 5 min) appear as a play button + progress bar. First use asks for microphone permission. |
| Send a photo or file | Tap the paperclip next to the message field → **Photo** or **File**, pick it in the file dialog (native picker on Android). Up to 25 MB. Images show inline (tap to view full screen); other files show as a card (desktop: click opens it with the default app). Received files are stored in the app data folder under `received/`. |
| Profile picture | Settings → **Change photo** (or tap your avatar). The picture is cropped to a square, shrunk to 256 × 256 and shared automatically with everyone you chat with (they fetch it when they see you; they see a new one within seconds). **Remove photo** goes back to initials. |
| Nickname for a contact | Tap a person's **picture** in the list (a dialog with a big picture or initials, a **Message** and an **Info** button), or the chat ⋮ menu → **Contact info** → *Nickname* → Save. Only you see it, and it is shown instead of the name the contact broadcasts (list, chat header, notifications). Clear the field to go back to their own name. |
| Contact info | Shows picture, nickname and broadcast name, status, **connection mode** (Wi-Fi / Bluetooth / both), **IP address** (Wi-Fi), Bluetooth address, **connected since** (or *last seen*), **first connected**, device name and user ID. |
| Search people | The *Search people* box at the top of the chat list (always shown) matches nickname, broadcast name, device name and IP address. |
| Search in a chat | Chat ⋮ menu (top right) → **Search**. Matches are outlined; the counter shows *current/total*; ▲ / ▼ (or Enter) jump between them, ✕ closes. |
| Delete a message | Long-press (touch) or right-click (desktop) a message → **Delete for me** (removed from this device only), or **Delete for everyone** for your own messages. The bubble is replaced on both sides by an italic placeholder, *"You deleted this message"* for the sender and *"This message was deleted"* for the contact; the text, file and any quote are erased. The request is queued if the contact is offline and delivered when they are back. A contact can only delete messages that *they* sent. |
| Forward a message | Long-press (touch) or right-click (desktop) a message → **Forward**, then pick a contact (the list can be searched). A copy is sent to that contact as a new message: text, photo (with caption), file or voice note; it is not linked to the original and carries no quote. Deleted messages cannot be forwarded. If the contact is offline it is queued like any other message. |
| Message options | The long-press menu is a **bottom sheet** on phones (anchored above the navigation bar) and a card on desktop: Reply, Forward, Copy, Delete for me, Delete for everyone. |
| Reply to a message | **Swipe a message to the left**, or long-press → **Reply**. A "Replying to …" bar appears above the message field (✕ cancels it) and the sent message shows the quoted text above it. Tapping the quote jumps to the original message. A reply bar is dropped when you switch chats. |
| Delete a conversation | Chat ⋮ menu → **Delete chat…**, or the same button on the Contact info page. Removes all messages and files with them from this device only. |
| Esc (desktop) | Closes the topmost thing: image viewer / dialog → page → the open chat (the right panel then asks you to select a conversation). |
| Settings | Gear icon: name, theme (System/Light/Dark), background running, notifications, Bluetooth switch. |
| Android Back button | Steps back one screen: dialog → page (Settings/Bluetooth) → open chat → chat list. Only from the chat list it moves the app to the background (the service keeps running). The first press while the keyboard is open just hides the keyboard. |
| Desktop close button | Hides the window to the tray (if a tray exists and *Keep running in background* is on). Use tray menu → **Quit** or Settings → **Quit LetsChat** to exit. |

Message status: *Sending…* → delivered (just the time), *Waiting to send…* (queued) or *Not delivered* (red,
only after 24 h or if an attached file vanished). LAN messages are acknowledged by the receiver. A message
that cannot be delivered (peer offline, Wi-Fi gone, peer frozen by the phone's power manager) is **queued and
retried automatically** – when the peer shows up again, when the app returns to the foreground and every 20 s.
Retries reuse the message id, so a message that actually did arrive late is not duplicated: the receiver
ignores the duplicate but still acknowledges it, which flips the sender's status to delivered. If a LAN send
fails and a Bluetooth link to the same peer exists, the message is sent over Bluetooth instead.

### Android background limitations (important)

The app does what Android offers for staying alive *efficiently*: a foreground service
(`connectedDevice|remoteMessaging`) with a low-priority persistent notification, a Wi-Fi multicast lock (so
UDP broadcasts are received), `android.app.background_running=true` so Qt keeps its event loop running, and a
request to be exempt from battery optimisation (Settings → *Unrestricted battery use*). In the background
the app announces itself every 8 s instead of 3 s and polls the network every 10 s instead of 4 s; peers
adapt their timeouts to the announced interval. No wake lock is held (it would keep the CPU awake all day).

Some manufacturers add their **own** freezer on top. On the Infinix X6855 used for testing (XOS), the
system log shows `Usf_Hiber … freeze uid: … com.smalnets.letschat` a few seconds after the app leaves the
foreground, even with the service running and every standard battery exemption granted. While frozen the
whole process (including plain native threads) is halted, so peers see the device as offline. If you
see this, look for a vendor setting such as *Phone Master / Battery / App freeze / Auto-start / Lock app in
recents* and allow LetsChat there (Settings → *App settings* opens the system page for the app). Stock
Android and most other vendors do not do this. This was **not** resolved on the test device: neither
`background_running`, the foreground-service types, battery exemption, nor an established TCP connection
wakes the process (the vendor only unfreezes a hard-coded set of apps, e.g. WhatsApp via push). Consequently
a message sent to a frozen phone is **queued on the sender and delivered within seconds of the phone waking
up** (unlocking it, opening the app or a notification), not while it is frozen. Truly instant delivery to a
frozen device would need a push channel, which an offline LAN/Bluetooth app does not have.

---

## 2. Architecture

```
main.cpp                    creates the backend objects, registers them to QML, starts the engine
src/
  core/        AppConfig        ports, timeouts, Bluetooth UUID, protocol version
               IdentityManager  persistent user id (UUID), alias, onboarding flag
               ThemeManager     system / light / dark
  chat/        ChatController   glue: transports <-> database <-> models <-> notifications
  network/     FrameParser      line/file frame parser + FilePump (streams files into sockets)
               DiscoveryService UDP presence broadcast + peer expiry
               TransferService  TCP server/client, framed JSON, acknowledgements
               BluetoothService RFCOMM server, device scan, links, hello + text frames
               PeerManager      merges LAN + Bluetooth knowledge into the peer list
               LanTracker       notices Wi-Fi/network changes
  security/    Crypto            OpenSSL wrappers (X25519, ChaCha20-Poly1305), HKDF/HMAC
               NoiseXX           Noise_XX_25519_ChaChaPoly_SHA256 handshake + transport ciphers
               SecureChannel     encrypted QIODevice over TCP / Bluetooth sockets
               SecurityManager   identity key, contact pinning, safety numbers
               SecretStore       Keystore-wrapped (Android) / owner-only (desktop) secret storage
  storage/     DatabaseManager  SQLite (peers, messages) – message bodies encrypted
               CryptoUtils      HMAC-SHA256 stream cipher + MAC
  models/      PeerListModel    peers with online state per transport, unread count, last message
               MessageModel     messages of the open conversation
  notifications/ NotificationManager  Android notification / desktop tray balloon
  platform/    BackgroundService      Android foreground service / desktop tray + hide-on-close, Back-key bridge
               MediaService           voice-note recording (Qt Multimedia), camera capture via system camera (Android)
  utils/       DeviceInfo       device name, best local IPv4
qml/
  Main.qml                 window, responsive shell (side-by-side ≥ 820 px, single column below)
  themes/Theme.qml         colour/metric singleton (follows ThemeManager)
  views/                   Onboarding, Contacts (chat list), Chat, Settings, Bluetooth, Profile (contact info)
  components/              Icon, IconButton, Avatar, PeerItem, MessageBubble, TransportBadge, Toast
  dialogs/AliasDialog.qml
android/
  AndroidManifest.xml      permissions, activity, foreground service, background_running flag
  src/.../LetsChatService.java   foreground service, notifications, permission helpers, camera intent
  src/.../MainActivity.java      QtActivity subclass that owns the Back key (asks QML how many steps it can go back)
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

**Everything below travels inside the encrypted channel** (see SECURITY.md): a Noise XX handshake first, then ChaCha20-Poly1305
records `[u16 length][ciphertext+tag]`. The frames described here are the decrypted plaintext stream.
All frames are UTF-8 JSON objects terminated by `\n`.

### LAN discovery – UDP 45454 (broadcast)
Sent every 3 s to every interface's broadcast address:
```json
{"type":"discovery","v":1,"iv":3000,"userId":"<uuid>","alias":"Alice","deviceName":"Pixel 8","av":"<12-char hash of the profile picture>"}
```
A peer is *lost* after 12 s without announcement. Changing network (`LanTracker`) clears LAN state and
announces again immediately.

### LAN messages – TCP 45455 (one short connection per message)
```
→ {"type":"text","v":1,"id":"<uuid>","fromUserId":"…","alias":"…","content":"hi","timestamp":"ISO-8601"}
← {"type":"ack","id":"<uuid>"}
```
The sender fails the message after 5 s without an ack. Duplicate ids are ignored by the receiver.

### Profile pictures
Discovery packets and the Bluetooth `hello` carry only `av`, a hash of the sender's 256 × 256 JPEG. A receiver that sees a hash
different from the one it stored sends `{"type":"avatar_get","fromUserId"}` (TCP or the Bluetooth link) and gets back a normal file
frame with `"kind":"avatar"` followed by the JPEG bytes. Pictures are stored as `avatars/<userId>.jpg`; an empty hash means the
contact removed its picture.

### Replies
Text and file frames may carry `"reply":{"id","from","text","type"}`: the id of the quoted message, its author, a short preview (up to 160
characters, or "Photo" / "Voice message" / file name) and its type. Quotes are stored (encrypted) with the message in
`messages.reply`; the quoted text is a snapshot, so it stays readable if the original is later deleted.

### Deleting messages
`{"type":"delete","id":"del:<msgId>","target":"<msgId>","fromUserId"}` (acknowledged over TCP, a plain frame on Bluetooth). Receiving a delete turns the stored message into type 4 (placeholder, no content, no quote). Requests
that could not be delivered stay in the `pending_deletes` table and are retried like queued messages.

### Files, photos and voice notes
A file is a header frame followed by exactly `size` raw bytes (both on TCP and Bluetooth); the receiver streams them to disk
(`FrameParser`) and, on TCP, confirms with the usual `ack` after the last byte:
```
→ {"type":"file","v":1,"id","fromUserId","alias","kind":"image|file|voice","name":"photo.jpg","size":123456,"timestamp","caption":"","duration":0}\n<123456 bytes>
← {"type":"ack","id"}\n
```
Chat history stores such messages with type 1 (image) / 2 (file) / 3 (voice note) and JSON content `{name,size,path,caption,duration}` (`duration` in ms, only for voice notes).

### Bluetooth – RFCOMM service UUID `e3a1c5a4-7c6e-4b0a-9f55-1d2b7a0c9e31`
After connecting both sides send `{"type":"hello","userId","alias","deviceName"}`; afterwards the same
`text` frames as above are exchanged over the persistent link (no ack – RFCOMM is reliable). When two
devices dial each other simultaneously the first link wins. Addresses of devices that have linked once are
remembered and re-connected automatically when they show up in a scan.

---

## 4. Storage and security

Security is a first-class feature; the full design, threat model, limits and test evidence are in **[SECURITY.md](SECURITY.md)**.
In short:

* **In transit:** all traffic, on Wi-Fi *and* Bluetooth, runs inside an end-to-end encrypted, mutually authenticated
  Noise XX channel (X25519 + ChaCha20-Poly1305). There is no plaintext fallback.
* **Identity:** each install has a long-term X25519 key. Contacts are pinned on first use; a changed key blocks the
  conversation until the user accepts it; *Contact info* shows a safety number to verify.
* **At rest:** SQLite file `letschat.db` (app data directory) – `messages` (id, from, to, content, type, timestamp,
  conversation_id, status, reply) with `content` and `reply` stored as `enc2:` + base64(nonce ‖ ciphertext ‖ tag)
  (ChaCha20-Poly1305). `peers` (…, nickname, avatar_hash, first_seen), `peer_keys` (pinned public keys, verified flag),
  `pending_deletes`. Older databases are migrated in place (plaintext and `enc1:` rows are re-encrypted on first start).
* **Keys:** the database key and identity private key are kept in `SecretStore` – wrapped by the Android Keystore on phones,
  owner-only settings file on desktop.

## 5. Android specifics

* **Permissions** (see manifest): network + multicast, `POST_NOTIFICATIONS` (13+), `FOREGROUND_SERVICE` +
  `…_CONNECTED_DEVICE`/`…_REMOTE_MESSAGING`, `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS`, `BLUETOOTH_SCAN/CONNECT/ADVERTISE`
  (12+; legacy `BLUETOOTH*`/location for ≤ 11). The Bluetooth runtime permission is requested through
  `QBluetoothPermission` the first time it is needed; the notification permission when the service starts.
* **Service**: `LetsChatService` (same process). Started by `BackgroundService::start()` when
  *Keep running in background* is on. It owns the Wi-Fi multicast lock.
* **Background event loop**: Qt blocks its event loop when the activity is suspended unless the manifest has
  `android.app.background_running=true` (Qt's loader turns that into `QT_BLOCK_EVENT_LOOPS_WHEN_SUSPENDED=0`).
  (An earlier workaround that hid the window on `Suspended` was removed: after the process was thawed it left a black
  window because stale state events arrive out of order.)
* **Back key**: Qt's own back callback never reached QML on current Android, so Back used to leave the app from any
  screen. `MainActivity` registers its own `OnBackInvokedCallback`, reads the number of back steps QML published
  (`Background.backDepth`) and either asks QML to take one step or calls `moveTaskToBack`.
* **Camera**: `MediaStore.ACTION_IMAGE_CAPTURE` with an output URI from `androidx.core` `FileProvider`
  (`res/xml/file_paths.xml`, cache dir), so no CAMERA permission is needed. Result delivery uses
  `QtAndroidPrivate::startActivity` (private Qt API, needs `Qt6::CorePrivate`).
* **Voice notes**: `RECORD_AUDIO` runtime permission via `QMicrophonePermission`; recording and playback use Qt Multimedia
  (FFmpeg backend), AAC in an `.m4a` container.
* **Bluetooth security**: Android's default RFCOMM sockets are *secure* and require the two devices to be paired first. A desktop
  that is not "pairable" (BlueZ `Pairable: no`, the usual default) never accepts that pairing, so phone → desktop
  connections failed while desktop → phone worked. LetsChat therefore uses non-secure RFCOMM on Android
  (`setPreferredSecurityFlags(NoSecurity)` for the client, `setSecurityFlags(NoSecurity)` for the server); no pairing
  dialog, and BT messages are not end-to-end encrypted (same as LAN).
* **Notification tap**: the notification's intent carries the sender's id (`letschat.peer`); `MainActivity` stores it, the
  native side picks it up (`Background.openChatRequested`) and QML leaves any page and opens that conversation. On desktop,
  clicking a tray message balloon shows the window and opens the conversation too.
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
Requires Qt 6.5+ with Quick, QuickControls2, Sql, Network, Bluetooth, Multimedia and Widgets (desktop only, for the tray) and the
OpenSSL 3 development files (`libssl-dev` / `openssl-devel`). Android links OpenSSL statically from the SDK's `android_openssl` package
(`ANDROID_SDK_ROOT/android_openssl/ssl_3`).
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
| Composer | Material text field drew its floating label across the border and clipped the text; no way to attach media | Custom growing message field (Enter sends on desktop, Shift+Enter newline), attach menu, photo/file transfer |
| Delivery | A timeout was treated as final failure although the message often arrived later; status never recovered | Queue + automatic retry with idempotent ids (see §1), status *Waiting to send…* |
| Back key | Back left the app from every screen | `MainActivity` back bridge (see §5) |
| Build | `-Wdeprecated`: implicit `this` capture in `[=]` lambdas (TransferService) | Explicit `[=, this]`; a full `-Wall -Wextra` build is now warning-free for project sources |
| Voice notes | Pause restarted the note; the play/pause icon never updated for notes whose file name has a space (URL strings differ by percent-encoding) | The player is keyed on the message id instead of the URL |
| Chat list | Last-message previews could be skipped at start-up (the sorted view re-ordered while being iterated) | Iterate the source model |
| Bluetooth | Android → desktop connect failed (secure socket needs pairing the desktop refuses) | Non-secure RFCOMM on Android |
| Notifications | Tapping a notification only opened the app | Opens the sender's conversation |
| Messages | A deleted message disappeared without a trace | Placeholder bubble (type 4); replies added |
| Message menu | Long-press menu (a QML `Menu`) opened at an odd position on phones | Bottom-sheet popup parented to the chat page, so it also stays clear of the system navigation bar |
| Keyboard | Closing any popup in a chat (options, dialogs, viewer) re-opened the on-screen keyboard | Focus is parked on the page and the keyboard is hidden again for a moment afterwards (Android re-shows it asynchronously) |
| Security | Traffic was plaintext JSON (readable and spoofable on the LAN / Bluetooth); home-made at-rest cipher; any peer could claim any user id (even to delete messages) | Noise XX channel, pinned identities, safety numbers, ChaCha20-Poly1305 at rest, Keystore-wrapped keys – see SECURITY.md |
| Dead code | `sendFile` stub, unused `SettingsManager`, duplicated settings | Removed (file transfer was reimplemented properly, see §3) |

## 8. Known limitations / ideas

* Attachments are limited to 25 MB and have no progress bar. On Android, non-image files can be received but not opened from the chat yet (needs a FileProvider share/open intent); images can be viewed in the app.
* Security limits (trust-on-first-use, public discovery metadata, unencrypted media files at rest, no audit) are listed in SECURITY.md §4.
* Android background reliability depends on vendor battery management (see §1).
* **Bluetooth was implemented and builds/starts on Android and desktop, but a two-device Bluetooth chat has not
  been verified end-to-end** (only one phone and a desktop without a second Bluetooth peer were available).
* Bluetooth Classic only; BLE advertising for zero-touch discovery would be a natural next step.
* Peers on other subnets/VLANs are not found by broadcast discovery.
* The `forms/*.ui` Designer files and `importedcontent/` are leftovers of the original template and unused.
