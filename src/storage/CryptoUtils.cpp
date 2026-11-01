#include "CryptoUtils.h"
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

namespace {
constexpr int NonceLen = 16;
constexpr int TagLen = 32;

QByteArray keystream(const QByteArray &key, const QByteArray &nonce, qsizetype length)
{
    QByteArray out;
    out.reserve(length + 32);
    quint32 counter = 0;
    while (out.size() < length) {
        QByteArray block = nonce;
        for (int s = 24; s >= 0; s -= 8)
            block.append(char((counter >> s) & 0xff));
        out += QMessageAuthenticationCode::hash(block, key + "enc", QCryptographicHash::Sha256);
        ++counter;
    }
    out.truncate(length);
    return out;
}

QByteArray macOf(const QByteArray &key, const QByteArray &nonceAndCipher)
{
    return QMessageAuthenticationCode::hash(nonceAndCipher, key + "mac", QCryptographicHash::Sha256);
}
} // namespace

QByteArray CryptoUtils::randomBytes(int length)
{
    QByteArray b(length, Qt::Uninitialized);
    QRandomGenerator::system()->generate(b.begin(), b.end());
    return b;
}

QByteArray CryptoUtils::encrypt(const QByteArray &plain, const QByteArray &key)
{
    const QByteArray nonce = randomBytes(NonceLen);
    QByteArray cipher = plain;
    const QByteArray ks = keystream(key, nonce, plain.size());
    for (qsizetype i = 0; i < cipher.size(); ++i)
        cipher[i] = char(cipher[i] ^ ks[i]);
    const QByteArray body = nonce + cipher;
    return body + macOf(key, body);
}

QByteArray CryptoUtils::decrypt(const QByteArray &blob, const QByteArray &key)
{
    if (blob.size() < NonceLen + TagLen)
        return {};
    const QByteArray body = blob.left(blob.size() - TagLen);
    const QByteArray tag = blob.right(TagLen);
    if (macOf(key, body) != tag)
        return {};
    const QByteArray nonce = body.left(NonceLen);
    QByteArray plain = body.mid(NonceLen);
    const QByteArray ks = keystream(key, nonce, plain.size());
    for (qsizetype i = 0; i < plain.size(); ++i)
        plain[i] = char(plain[i] ^ ks[i]);
    return plain.isNull() ? QByteArray("") : plain;
}

QString CryptoUtils::encryptString(const QString &text, const QByteArray &key)
{
    return QString::fromLatin1(encrypt(text.toUtf8(), key).toBase64());
}

QString CryptoUtils::decryptString(const QString &base64, const QByteArray &key)
{
    const QByteArray plain = decrypt(QByteArray::fromBase64(base64.toLatin1()), key);
    return plain.isNull() ? QString() : QString::fromUtf8(plain);
}
