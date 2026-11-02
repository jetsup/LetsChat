#!/usr/bin/env python3
"""
Independent reference implementation of Noise_XX_25519_ChaChaPoly_SHA256 (spec revision 34), written directly
from the Noise specification using the `cryptography` package.  It is used to produce the fixed test vectors
checked by tst_security.cpp, so that the C++ implementation is validated against separate code, not only
against itself.

    python3 noise_reference.py            # prints the vectors as C++ initialisers
"""
import hashlib, hmac, struct, sys
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from cryptography.hazmat.primitives import serialization as ser

NAME = b"Noise_XX_25519_ChaChaPoly_SHA256"
PROLOGUE = b"LetsChat/secure-channel/1"

def pub_of(priv: bytes) -> bytes:
    return X25519PrivateKey.from_private_bytes(priv).public_key().public_bytes(ser.Encoding.Raw, ser.PublicFormat.Raw)

def dh(priv: bytes, pub: bytes) -> bytes:
    return X25519PrivateKey.from_private_bytes(priv).exchange(X25519PublicKey.from_public_bytes(pub))

def hmac256(k, d): return hmac.new(k, d, hashlib.sha256).digest()

def hkdf(ck, ikm, n=2):
    t = hmac256(ck, ikm)
    o1 = hmac256(t, b"\x01")
    o2 = hmac256(t, o1 + b"\x02")
    return (o1, o2) if n == 2 else (o1, o2, hmac256(t, o2 + b"\x03"))

def nonce(n): return b"\x00" * 4 + struct.pack("<Q", n)

class Cipher:
    def __init__(self, k=None): self.k, self.n = k, 0
    def enc(self, ad, pt):
        ct = ChaCha20Poly1305(self.k).encrypt(nonce(self.n), pt, ad); self.n += 1; return ct
    def dec(self, ad, ct):
        pt = ChaCha20Poly1305(self.k).decrypt(nonce(self.n), ct, ad); self.n += 1; return pt

class Symmetric:
    def __init__(self):
        self.h = NAME if len(NAME) == 32 else hashlib.sha256(NAME).digest()
        self.ck = self.h
        self.c = Cipher()
        self.mix_hash(PROLOGUE)
    def mix_hash(self, d): self.h = hashlib.sha256(self.h + d).digest()
    def mix_key(self, ikm):
        self.ck, k = hkdf(self.ck, ikm); self.c = Cipher(k)
    def enc_hash(self, pt):
        ct = self.c.enc(self.h, pt) if self.c.k else pt
        self.mix_hash(ct); return ct
    def dec_hash(self, ct):
        pt = self.c.dec(self.h, ct) if self.c.k else ct
        self.mix_hash(ct); return pt
    def split(self): return hkdf(self.ck, b"")

def run(i_s, i_e, r_s, r_e, ad_payload=b""):
    """Runs a whole handshake with fixed keys; returns every wire message and the resulting state."""
    i_s_pub, i_e_pub, r_s_pub, r_e_pub = pub_of(i_s), pub_of(i_e), pub_of(r_s), pub_of(r_e)
    I, R = Symmetric(), Symmetric()
    # -> e
    I.mix_hash(i_e_pub); m1 = i_e_pub + I.enc_hash(b"")
    R.mix_hash(m1[:32]); assert R.dec_hash(m1[32:]) == b""
    # <- e, ee, s, es
    R.mix_hash(r_e_pub); m2 = r_e_pub
    R.mix_key(dh(r_e, m1[:32]))
    m2 += R.enc_hash(r_s_pub)
    R.mix_key(dh(r_s, m1[:32]))
    m2 += R.enc_hash(b"")
    I.mix_hash(m2[:32]); I.mix_key(dh(i_e, m2[:32]))
    rs = I.dec_hash(m2[32:80]); assert rs == r_s_pub
    I.mix_key(dh(i_e, rs)); assert I.dec_hash(m2[80:]) == b""
    # -> s, se
    m3 = I.enc_hash(i_s_pub)
    I.mix_key(dh(i_s, m2[:32]))
    m3 += I.enc_hash(b"")
    rs2 = R.dec_hash(m3[:48]); assert rs2 == i_s_pub
    R.mix_key(dh(r_e, rs2)); assert R.dec_hash(m3[48:]) == b""
    assert I.h == R.h
    i_send, i_recv = I.split(); r_recv, r_send = R.split()
    assert i_send == r_recv and i_recv == r_send
    return dict(m1=m1, m2=m2, m3=m3, h=I.h, k_i2r=i_send, k_r2i=i_recv,
                i_s_pub=i_s_pub, r_s_pub=r_s_pub)

if __name__ == "__main__":
    # fixed, obviously-test-only keys (never used outside the tests)
    i_s = bytes(range(0x01, 0x21)); i_e = bytes(range(0x21, 0x41))
    r_s = bytes(range(0x41, 0x61)); r_e = bytes(range(0x61, 0x81))
    r = run(i_s, i_e, r_s, r_e)
    # first transport records, both directions
    pt1, pt2 = b"hello from the initiator", b"hello from the responder"
    c1 = ChaCha20Poly1305(r["k_i2r"]).encrypt(nonce(0), pt1, b"")
    c2 = ChaCha20Poly1305(r["k_r2i"]).encrypt(nonce(0), pt2, b"")
    c1b = ChaCha20Poly1305(r["k_i2r"]).encrypt(nonce(1), pt1, b"")
    def hx(b): return b.hex()
    for k, v in [("i_s", i_s), ("i_e", i_e), ("r_s", r_s), ("r_e", r_e), ("i_s_pub", r["i_s_pub"]),
                 ("r_s_pub", r["r_s_pub"]), ("msg1", r["m1"]), ("msg2", r["m2"]), ("msg3", r["m3"]),
                 ("handshake_hash", r["h"]), ("record_i2r_0", c1), ("record_r2i_0", c2), ("record_i2r_1", c1b)]:
        print(f'{k} = "{hx(v)}"')
