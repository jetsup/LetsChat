# LetsChat – Security design

LetsChat is meant for people who are physically close to each other, on networks and radios nobody controls
(a café's Wi-Fi, a shared hotspot, Bluetooth in a crowd). So **every message, file, voice note, profile picture and
delete request is end-to-end encrypted and authenticated, on every transport**, and everything stored on the
device is encrypted. There is no plaintext mode and no downgrade path.

## 1. What is protected

| Data | Protection |
|---|---|
| Messages, replies, photos, files, voice notes, deletions, profile pictures **in transit** (Wi-Fi *and* Bluetooth) | Noise `XX` handshake (X25519) + ChaCha20-Poly1305 records. Mutual authentication, forward secrecy, tamper / replay / reorder detection. |
| Who you talk to | Contacts are pinned to their public key (see §3); an impostor cannot take over a contact's identity silently. |
| Message history on disk | Every message body and quote is encrypted with ChaCha20-Poly1305 (`enc2:` rows). Legacy rows from older versions are re-encrypted automatically on the first start. |
| The long-term identity key and the database key | Android: wrapped by a **non-exportable AES-256-GCM key in the Android Keystore** (hardware-backed where the phone has it). Desktop: stored in the user's settings file restricted to the owner (mode 0600). |

## 2. The secure channel

Everything runs inside `SecureChannel`, a `QIODevice` wrapper around the TCP or Bluetooth socket, so the
application protocol (JSON lines + raw file bytes) is unchanged *inside* the tunnel.

* **Handshake:** `Noise_XX_25519_ChaChaPoly_SHA256` (Noise Protocol Framework rev. 34), prologue `LetsChat/secure-channel/1`.

  ```
  -> e
  <- e, ee, s, es
  -> s, se
  ```
  Both static keys travel encrypted, so a passive observer learns neither identity. The three messages have fixed
  lengths (32 / 96 / 64 bytes); anything else – including an old plaintext client – is rejected immediately.
* **Records:** `[u16 length][ChaCha20-Poly1305(plaintext ≤ 16 KiB) + 16-byte tag]`, a 64-bit counter nonce per
  direction, empty associated data. A bad tag, a replayed or re-ordered record, or a truncated record closes the connection.
* **Identity check happens before any data is sent.** The side that dials checks the key it receives against the key
  pinned for the contact it wants to reach; messages written meanwhile are held back inside the channel and discarded
  if the check fails.
* **Who is speaking:** every frame names its sender (`fromUserId`). The receiver verifies that this id is really the
  owner of the channel's key (trust-on-first-use pinning) *before acting on the frame* – including before a file is
  written to disk, and for delete requests (so nobody can delete someone else's messages by claiming their id).
* **Limits:** handshake timeout 8 s, at most 24 simultaneous inbound TCP connections, records ≤ 64 KiB, files ≤ 25 MiB,
  file names sanitised.
* **Primitives:** OpenSSL 3 `libcrypto` (X25519, ChaCha20-Poly1305, CSPRNG; linked statically on Android from the SDK's
  `android_openssl` package) and Qt's HMAC-SHA256 / SHA-256. No home-made cryptography remains: the earlier
  HMAC-keystream construction is only used to *read* old rows before they are migrated.

## 3. Trust: pinning and safety numbers

* **Trust on first use (TOFU).** The first time a contact id is seen with a key, the pair is pinned (`peer_keys` table).
* **Key changed → blocked.** If the same id later presents another key (reinstall, new phone – or an impostor), the
  connection is refused, queued messages are held, a warning appears in the chat and in *Contact info*, and nothing is
  exchanged until the user presses **Accept new key**. Accepting also clears the *Verified* mark. A key already pinned to
  one id is never re-bound to another id.
* **Safety numbers.** *Contact info → Encryption* shows a 60-digit number derived from both public keys; it is identical on
  both devices. Compare it in person or over a call and switch **Verified** on. That upgrades "no one tampered since
  first contact" to "no one tampered at all". The chat header shows 🔒 *encrypted* or 🔒 *verified*.

## 4. Threat model

**Defended against:** eavesdroppers on Wi-Fi or Bluetooth; active attackers who inject, replay, reorder, modify or
truncate traffic; impostors who claim a known contact's id; plaintext/legacy clients; flooding of the TCP port; a lost or
stolen phone's *backup* or an extracted app-data folder (keys are Keystore-wrapped on Android; the backup is disabled);
recovering past traffic from a later key compromise (forward secrecy of the ephemeral keys).

**Not defended against – please read:**

1. **First contact over a hostile network.** TOFU cannot tell a real first contact from an attacker who is *already*
   in the middle of that very first handshake. Verify safety numbers with people who matter.
2. **Discovery metadata is public.** The UDP presence broadcast (user id, display name, device name, profile-picture hash)
   and Bluetooth device names are visible to anyone nearby, and can be spoofed (spoofing only redirects *where* we try
   to connect – the handshake then fails against the pinned key). Message *content* is never in it.
3. **Traffic analysis.** An observer can see that two devices talk, when, and roughly how much.
4. **A compromised device.** Malware or a rooted/unlocked phone can read the decrypted app. On Android the Keystore protects
   the key at rest, not from code running as the app. On desktop the keys sit in a 0600 file – use full-disk
   encryption; an OS keyring integration is future work.
5. **Files at rest.** Received/sent photos, files and voice notes are kept as ordinary files in the app's private folder
   (protected by the Android sandbox, or by your desktop file permissions). Only the message database is encrypted.
6. **Notifications and screenshots.** Notification text is shown by the OS and may appear on the lock screen; screenshots of the
   app are not blocked.
7. **Deleted-for-everyone** is a request: a modified client can ignore it.
8. **Denial of service** on the radio/LAN level (jamming, port flooding beyond the connection cap) is out of scope.
9. The cryptographic code has been tested (below) but **has not been independently audited**.

## 5. Verification

`tests/tst_security.cpp` (run: `cmake -DLETSCHAT_BUILD_TESTS=ON …; ctest --output-on-failure`) – 18 tests:

* X25519 against RFC 7748 §6.1; ChaCha20-Poly1305 against RFC 8439 §2.8.2; every single-bit flip, wrong key / nonce /
  associated data and truncation is rejected; low-order curve points are refused.
* **The Noise handshake reproduces, byte for byte, the messages, handshake hash and transport records of an independent
  implementation** (`tests/noise_reference.py`, written from the specification with Python's `cryptography` package).
* Random handshakes; replay, re-order and tamper of transport records; malformed / tampered handshake messages;
  a man-in-the-middle swapping the responder's static key.
* The channel over a real TCP socket: no plaintext (not even a recognisable phrase) on the wire, data held back until the
  identity check passes, refusal by the verifier, plaintext client rejected at once, forged record detected, 300 KiB bulk transfer.
* Pinning: first use, key change, no alert spam, another id claiming a pinned key, verification cleared on acceptance;
  symmetric safety numbers.
* At rest: the raw SQLite file contains neither message text nor quoted text; keys are not left in plain settings.

`tests/interop_probe.py <device-ip>` drives a *running* app with the independent implementation. Run on the Android test
phone it showed: first contact accepted with an encrypted acknowledgement; same id, different key → connection closed; legitimate
key still works; a plaintext client → connection closed. The fingerprint the phone displayed for the probe matched one
computed independently in Python.

## 6. Compatibility

The protocol version is now 2. **All devices must be updated together**: an old (plaintext) build can no longer exchange
messages with a new one – the new build refuses plaintext connections by design. Old devices still show up in the peer
list (their discovery broadcast is unchanged) but messages to them stay queued as "Waiting to send…".
