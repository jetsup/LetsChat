#ifndef SECURITYMANAGER_H
#define SECURITYMANAGER_H

#include "Crypto.h"
#include <QHash>
#include <QObject>
#include <QVariantMap>

class DatabaseManager;

/**
 * Owns this installation's long-term identity key and decides whom to trust.
 *
 * Trust model: trust on first use.  The first time a contact id is seen together with a static public key,
 * the pair is pinned.  Later connections that present a different key are refused ("safety number changed")
 * until the user accepts the new key.  Users can compare safety numbers out-of-band and mark a contact verified.
 */
class SecurityManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString ownFingerprint READ ownFingerprint CONSTANT)
    Q_PROPERTY(QString protection READ protection CONSTANT)

public:
    enum class Trust { Trusted, New, Changed };

    explicit SecurityManager(DatabaseManager *db, QObject *parent = nullptr);

    Crypto::KeyPair identity() const { return m_identity; }
    QString ownFingerprint() const { return fingerprint(m_identity.pub); }
    QString protection() const;

    /** Checks (and on first sight pins) the key presented by `userId`. */
    Trust evaluate(const QString &userId, const QByteArray &key);
    /** True while a changed key is waiting for the user's decision: no traffic is exchanged meanwhile. */
    bool isBlocked(const QString &userId) const { return m_pendingKeys.contains(userId); }

    Q_INVOKABLE QVariantMap peerSecurity(const QString &userId) const;
    Q_INVOKABLE void setVerified(const QString &userId, bool verified);
    /** Forget everything about a contact (the pinned key itself is removed by DatabaseManager::forgetPeer). */
    void forget(const QString &userId);
    /** Pins the new key of a contact whose key changed (clears the "verified" mark). */
    Q_INVOKABLE void acceptNewKey(const QString &userId);

public slots:
    /** Transports report a refused connection here (the key itself is recorded by evaluate()). */
    void noteAlert(const QString &userId) { Q_UNUSED(userId) }

    static QString fingerprint(const QByteArray &key);
    /** 60-digit number derived from both public keys; identical on both devices. */
    QString safetyNumber(const QByteArray &theirKey) const;
    static QString safetyNumber(const QByteArray &keyA, const QByteArray &keyB);

signals:
    void keyChanged(const QString &userId);
    void peerSecurityChanged(const QString &userId);

private:
    DatabaseManager *m_db;
    Crypto::KeyPair m_identity;
    QHash<QString, QByteArray> m_pendingKeys;   // userId -> key that was refused
};

#endif // SECURITYMANAGER_H
