#ifndef CRYPTOUTILS_H
#define CRYPTOUTILS_H

#include <QObject>
#include <QString>
#include <QByteArray>

class CryptoUtils : public QObject
{
    Q_OBJECT

public:
    explicit CryptoUtils(QObject *parent = nullptr);
    
    Q_INVOKABLE static QByteArray encrypt(const QByteArray &data, const QString &key);
    Q_INVOKABLE static QByteArray decrypt(const QByteArray &data, const QString &key);
    Q_INVOKABLE static QString encryptString(const QString &data, const QString &key);
    Q_INVOKABLE static QString decryptString(const QString &data, const QString &key);
    Q_INVOKABLE static QByteArray generateKey(int length = 32);
    Q_INVOKABLE static QString generateKeyString();
    
private:
    static QByteArray deriveKey(const QString &key);
};

#endif // CRYPTOUTILS_H
