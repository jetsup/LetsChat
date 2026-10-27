#include "CryptoUtils.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDebug>

CryptoUtils::CryptoUtils(QObject *parent)
    : QObject(parent)
{
}

QByteArray CryptoUtils::deriveKey(const QString &key)
{
    return QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256);
}

QByteArray CryptoUtils::encrypt(const QByteArray &data, const QString &key)
{
    QByteArray derived = deriveKey(key);
    QByteArray encrypted = data;
    // Simple XOR encryption with key (can be replaced with AES in production)
    for (int i = 0; i < encrypted.size(); ++i) {
        encrypted[i] = encrypted[i] ^ derived[i % derived.size()];
    }
    return encrypted;
}

QByteArray CryptoUtils::decrypt(const QByteArray &data, const QString &key)
{
    // XOR is symmetric
    return encrypt(data, key);
}

QString CryptoUtils::encryptString(const QString &data, const QString &key)
{
    QByteArray encrypted = encrypt(data.toUtf8(), key);
    return QString::fromLatin1(encrypted.toBase64());
}

QString CryptoUtils::decryptString(const QString &data, const QString &key)
{
    QByteArray decoded = QByteArray::fromBase64(data.toLatin1());
    QByteArray decrypted = decrypt(decoded, key);
    return QString::fromUtf8(decrypted);
}

QByteArray CryptoUtils::generateKey(int length)
{
    QByteArray key;
    key.resize(length);
    for (int i = 0; i < length; ++i) {
        key[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return key;
}

QString CryptoUtils::generateKeyString()
{
    return QString::fromLatin1(generateKey(32).toBase64());
}
