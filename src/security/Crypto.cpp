#include "Crypto.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QMessageAuthenticationCode>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace Crypto {

namespace {
struct PkeyDeleter { void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); } };
struct CtxDeleter { void operator()(EVP_PKEY_CTX *p) const { EVP_PKEY_CTX_free(p); } };
struct CipherDeleter { void operator()(EVP_CIPHER_CTX *p) const { EVP_CIPHER_CTX_free(p); } };
using Pkey = std::unique_ptr<EVP_PKEY, PkeyDeleter>;
using PkeyCtx = std::unique_ptr<EVP_PKEY_CTX, CtxDeleter>;
using CipherCtx = std::unique_ptr<EVP_CIPHER_CTX, CipherDeleter>;

const unsigned char *u(const QByteArray &b) { return reinterpret_cast<const unsigned char *>(b.constData()); }
unsigned char *u(QByteArray &b) { return reinterpret_cast<unsigned char *>(b.data()); }
} // namespace

QByteArray randomBytes(int n)
{
    QByteArray b(n, Qt::Uninitialized);
    if (RAND_bytes(u(b), n) != 1) {
        qCritical() << "RAND_bytes failed";
        return {};
    }
    return b;
}

KeyPair keyPairFromPrivate(const QByteArray &priv)
{
    KeyPair kp;
    if (priv.size() != KeyLen)
        return kp;
    Pkey key(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, u(priv), size_t(priv.size())));
    if (!key)
        return kp;
    size_t len = KeyLen;
    QByteArray pub(KeyLen, Qt::Uninitialized);
    if (EVP_PKEY_get_raw_public_key(key.get(), u(pub), &len) != 1 || len != KeyLen)
        return kp;
    kp.priv = priv;
    kp.pub = pub;
    return kp;
}

KeyPair generateX25519()
{
    PkeyCtx ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr));
    EVP_PKEY *raw = nullptr;
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) != 1 || EVP_PKEY_keygen(ctx.get(), &raw) != 1)
        return {};
    Pkey key(raw);
    size_t privLen = KeyLen, pubLen = KeyLen;
    KeyPair kp;
    kp.priv.resize(KeyLen);
    kp.pub.resize(KeyLen);
    if (EVP_PKEY_get_raw_private_key(key.get(), u(kp.priv), &privLen) != 1
        || EVP_PKEY_get_raw_public_key(key.get(), u(kp.pub), &pubLen) != 1)
        return {};
    return kp;
}

QByteArray dh(const QByteArray &priv, const QByteArray &pub)
{
    if (priv.size() != KeyLen || pub.size() != KeyLen)
        return {};
    Pkey mine(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, u(priv), size_t(priv.size())));
    Pkey theirs(EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, u(pub), size_t(pub.size())));
    if (!mine || !theirs)
        return {};
    PkeyCtx ctx(EVP_PKEY_CTX_new(mine.get(), nullptr));
    if (!ctx || EVP_PKEY_derive_init(ctx.get()) != 1 || EVP_PKEY_derive_set_peer(ctx.get(), theirs.get()) != 1)
        return {};
    size_t len = KeyLen;
    QByteArray secret(KeyLen, Qt::Uninitialized);
    if (EVP_PKEY_derive(ctx.get(), u(secret), &len) != 1 || len != KeyLen)
        return {};   // OpenSSL rejects an all-zero result (low-order input point)
    return secret;
}

QByteArray sha256(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

QByteArray hmacSha256(const QByteArray &key, const QByteArray &data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

void hkdf2(const QByteArray &ck, const QByteArray &ikm, QByteArray *out1, QByteArray *out2)
{
    const QByteArray temp = hmacSha256(ck, ikm);
    const QByteArray o1 = hmacSha256(temp, QByteArray(1, '\x01'));
    const QByteArray o2 = hmacSha256(temp, o1 + QByteArray(1, '\x02'));
    if (out1) *out1 = o1;
    if (out2) *out2 = o2;
}

QByteArray aeadEncrypt(const QByteArray &key, const QByteArray &nonce, const QByteArray &ad, const QByteArray &plain)
{
    if (key.size() != KeyLen || nonce.size() != NonceLen)
        return {};
    CipherCtx ctx(EVP_CIPHER_CTX_new());
    QByteArray out(plain.size() + TagLen, Qt::Uninitialized);
    int len = 0, total = 0;
    if (!ctx
        || EVP_EncryptInit_ex(ctx.get(), EVP_chacha20_poly1305(), nullptr, nullptr, nullptr) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, NonceLen, nullptr) != 1
        || EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, u(key), u(nonce)) != 1)
        return {};
    if (!ad.isEmpty() && EVP_EncryptUpdate(ctx.get(), nullptr, &len, u(ad), int(ad.size())) != 1)
        return {};
    if (!plain.isEmpty()) {
        if (EVP_EncryptUpdate(ctx.get(), u(out), &len, u(plain), int(plain.size())) != 1)
            return {};
        total = len;
    }
    if (EVP_EncryptFinal_ex(ctx.get(), u(out) + total, &len) != 1)
        return {};
    total += len;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG, TagLen, u(out) + total) != 1)
        return {};
    out.resize(total + TagLen);
    return out;
}

QByteArray aeadDecrypt(const QByteArray &key, const QByteArray &nonce, const QByteArray &ad, const QByteArray &ct)
{
    if (key.size() != KeyLen || nonce.size() != NonceLen || ct.size() < TagLen)
        return {};
    const int bodyLen = int(ct.size()) - TagLen;
    QByteArray tag = ct.right(TagLen);
    QByteArray out(bodyLen, Qt::Uninitialized);
    CipherCtx ctx(EVP_CIPHER_CTX_new());
    int len = 0, total = 0;
    if (!ctx
        || EVP_DecryptInit_ex(ctx.get(), EVP_chacha20_poly1305(), nullptr, nullptr, nullptr) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, NonceLen, nullptr) != 1
        || EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, u(key), u(nonce)) != 1)
        return {};
    if (!ad.isEmpty() && EVP_DecryptUpdate(ctx.get(), nullptr, &len, u(ad), int(ad.size())) != 1)
        return {};
    if (bodyLen > 0) {
        if (EVP_DecryptUpdate(ctx.get(), u(out), &len, u(ct), bodyLen) != 1)
            return {};
        total = len;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG, TagLen, u(tag)) != 1
        || EVP_DecryptFinal_ex(ctx.get(), u(out) + total, &len) <= 0) {
        OPENSSL_cleanse(out.data(), size_t(out.size()));
        return {};   // authentication failed
    }
    out.resize(total + len);
    return out.isNull() ? QByteArray("") : out;
}

bool constantTimeEqual(const QByteArray &a, const QByteArray &b)
{
    return a.size() == b.size() && CRYPTO_memcmp(a.constData(), b.constData(), size_t(a.size())) == 0;
}

void secureWipe(QByteArray &data)
{
    if (!data.isEmpty())
        OPENSSL_cleanse(data.data(), size_t(data.size()));
    data.clear();
}

} // namespace Crypto
