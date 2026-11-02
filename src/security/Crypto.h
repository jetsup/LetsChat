#ifndef CRYPTO_H
#define CRYPTO_H

#include <QByteArray>
#include <QString>

/**
 * Thin wrappers around OpenSSL's libcrypto (X25519, ChaCha20-Poly1305, CSPRNG) plus HKDF/HMAC-SHA256 built on Qt.
 * Nothing in here is a home-made primitive.
 */
namespace Crypto {

constexpr int KeyLen = 32;
constexpr int TagLen = 16;
constexpr int NonceLen = 12;
constexpr int HashLen = 32;

struct KeyPair {
    QByteArray priv;   // 32 bytes
    QByteArray pub;    // 32 bytes
    bool isValid() const { return priv.size() == KeyLen && pub.size() == KeyLen; }
};

QByteArray randomBytes(int n);

KeyPair generateX25519();
/** Re-derives the public key; returns an invalid pair for a malformed private key. */
KeyPair keyPairFromPrivate(const QByteArray &priv);
/** X25519 shared secret; an empty array on failure (bad key or a low-order point). */
QByteArray dh(const QByteArray &priv, const QByteArray &pub);

QByteArray sha256(const QByteArray &data);
QByteArray hmacSha256(const QByteArray &key, const QByteArray &data);
/** HKDF as used by Noise: two 32-byte outputs from a chaining key and input key material. */
void hkdf2(const QByteArray &chainingKey, const QByteArray &ikm, QByteArray *out1, QByteArray *out2);

/** ChaCha20-Poly1305 (RFC 8439). Returns ciphertext || tag. */
QByteArray aeadEncrypt(const QByteArray &key, const QByteArray &nonce12, const QByteArray &ad,
                       const QByteArray &plain);
/** Returns a *null* QByteArray when authentication fails. An empty plaintext gives an empty (non-null) array. */
QByteArray aeadDecrypt(const QByteArray &key, const QByteArray &nonce12, const QByteArray &ad,
                       const QByteArray &ctAndTag);

bool constantTimeEqual(const QByteArray &a, const QByteArray &b);
void secureWipe(QByteArray &data);

} // namespace Crypto

#endif // CRYPTO_H
