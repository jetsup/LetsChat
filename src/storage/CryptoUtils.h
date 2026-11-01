#ifndef CRYPTOUTILS_H
#define CRYPTOUTILS_H

#include <QByteArray>
#include <QString>

/**
 * Small authenticated stream cipher built from Qt's HMAC-SHA256 (keystream in counter
 * mode + encrypt-then-MAC).  It protects the local message database against casual
 * inspection; it is not a replacement for a vetted AEAD such as AES-GCM.
 */
class CryptoUtils
{
public:
    static QByteArray encrypt(const QByteArray &plain, const QByteArray &key);
    /** Returns a null QByteArray when the data was tampered with or the key is wrong. */
    static QByteArray decrypt(const QByteArray &blob, const QByteArray &key);

    static QString encryptString(const QString &text, const QByteArray &key);
    static QString decryptString(const QString &base64, const QByteArray &key);

    static QByteArray randomBytes(int length);
};

#endif // CRYPTOUTILS_H
