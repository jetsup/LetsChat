#!/usr/bin/env python3
"""Interoperability probe: talks to a running LetsChat (TCP 45455) using the independent Noise implementation in
noise_reference.py and checks that (1) a first contact is accepted, (2) the same id with another key is refused,
(3) a plaintext client is dropped.   usage: python3 -I tests/interop_probe.py <device-ip>"""
import os, socket, struct, json, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import noise_reference as nr
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey

HOST = sys.argv[1]; PORT = 45455

def rec(sock, payload): sock.sendall(struct.pack(">H", len(payload)) + payload)
def read_rec(sock, timeout=6):
    sock.settimeout(timeout)
    hdr = b""
    while len(hdr) < 2:
        c = sock.recv(2 - len(hdr)); 
        if not c: return None
        hdr += c
    n = struct.unpack(">H", hdr)[0]; data = b""
    while len(data) < n:
        c = sock.recv(n - len(data))
        if not c: return None
        data += c
    return data

def connect_secure(static_priv):
    """Noise XX initiator written independently of the app (noise_reference.py)."""
    s = socket.create_connection((HOST, PORT), timeout=5)
    i_e = os.urandom(32); i_s = static_priv
    I = nr.Symmetric()
    I.mix_hash(nr.pub_of(i_e)); m1 = nr.pub_of(i_e) + I.enc_hash(b"")
    rec(s, m1)
    m2 = read_rec(s)
    if m2 is None: return s, None, "closed after message 1"
    I.mix_hash(m2[:32]); I.mix_key(nr.dh(i_e, m2[:32]))
    rs = I.dec_hash(m2[32:80]); I.mix_key(nr.dh(i_e, rs)); assert I.dec_hash(m2[80:]) == b""
    m3 = I.enc_hash(nr.pub_of(i_s)); I.mix_key(nr.dh(i_s, m2[:32])); m3 += I.enc_hash(b"")
    rec(s, m3)
    k_send, k_recv = I.split()
    return s, (k_send, k_recv, rs), None

def send_json(s, keys, obj, counter):
    ct = ChaCha20Poly1305(keys[0]).encrypt(nr.nonce(counter), (json.dumps(obj) + "\n").encode(), b"")
    rec(s, ct)

def read_ack(s, keys):
    r = read_rec(s)
    if r is None: return None
    return ChaCha20Poly1305(keys[1]).decrypt(nr.nonce(0), r, b"")

def deliver(static_priv, user_id, text, mid):
    s, keys, err = connect_secure(static_priv)
    if err: return err
    send_json(s, keys, {"type": "text", "id": mid, "fromUserId": user_id, "alias": "SecureProbe",
                        "content": text, "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S")}, 0)
    try:
        ack = read_ack(s, keys)
    except Exception as e:
        return "no ack (%s)" % type(e).__name__
    return ack

if __name__ == "__main__":
    real = bytes(range(0x90, 0xb0)); other = bytes(range(0xb0, 0xd0))
    print("1) first contact, key A        ->", deliver(real, "probe-user-1", "Hello over Noise from an independent implementation", "probe-1"))
    print("2) same id + same key again    ->", deliver(real, "probe-user-1", "second message", "probe-2"))
    print("3) same id, DIFFERENT key (MITM/impostor) ->", deliver(other, "probe-user-1", "I am an impostor", "probe-3"))
    print("4) legit key still works       ->", deliver(real, "probe-user-1", "third message", "probe-4"))
    # plaintext client
    s = socket.create_connection((HOST, PORT), timeout=5)
    s.sendall(b'{"type":"text","id":"x","fromUserId":"old","content":"plaintext"}\n'); s.settimeout(4)
    try: print("5) old plaintext client        ->", s.recv(100) or "connection closed")
    except Exception as e: print("5) old plaintext client        ->", type(e).__name__)
