#ifndef SECRETSTORE_H
#define SECRETSTORE_H

#include <QByteArray>
#include <QString>

/**
 * Storage for the app's long-term secrets (identity private key, database key).
 *  - Android: wrapped with a non-exportable AES-GCM key in the Android Keystore (see SecretVault.java).
 *  - Desktop: kept in the user's settings file, which is restricted to the owner (mode 0600).
 */
class SecretStore
{
public:
    static QByteArray load(const QString &name);
    static bool save(const QString &name, const QByteArray &secret);
    static void remove(const QString &name);
    /** Human-readable description of how secrets are protected on this platform. */
    static QString protection();
};

#endif // SECRETSTORE_H
