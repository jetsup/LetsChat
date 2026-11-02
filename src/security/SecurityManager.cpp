#include "SecurityManager.h"
#include "SecretStore.h"
#include "storage/DatabaseManager.h"
#include <QDebug>
#include <QSettings>

SecurityManager::SecurityManager(DatabaseManager *db, QObject *parent)
    : QObject(parent), m_db(db)
{
    m_identity = Crypto::keyPairFromPrivate(SecretStore::load("identity_key"));
    if (!m_identity.isValid()) {
        m_identity = Crypto::generateX25519();
        if (!m_identity.isValid() || !SecretStore::save("identity_key", m_identity.priv))
            qCritical() << "Could not create or store the identity key - secure messaging is unavailable";
        else
            qInfo() << "Security: generated a new identity key" << fingerprint(m_identity.pub);
    }
}

QString SecurityManager::protection() const { return SecretStore::protection(); }

QString SecurityManager::fingerprint(const QByteArray &key)
{
    const QByteArray hex = Crypto::sha256(key).left(16).toHex();
    QStringList groups;
    for (int i = 0; i < hex.size(); i += 4)
        groups << QString::fromLatin1(hex.mid(i, 4));
    return groups.join(' ');
}

QString SecurityManager::safetyNumber(const QByteArray &theirKey) const
{
    if (theirKey.size() != Crypto::KeyLen || !m_identity.isValid())
        return {};
    return safetyNumber(m_identity.pub, theirKey);
}

QString SecurityManager::safetyNumber(const QByteArray &keyA, const QByteArray &keyB)
{
    // order-independent so both devices show the same digits
    const QByteArray joined = keyA < keyB ? keyA + keyB : keyB + keyA;
    const QByteArray d1 = Crypto::sha256(QByteArrayLiteral("LetsChat safety number 1") + joined);
    const QByteArray d2 = Crypto::sha256(QByteArrayLiteral("LetsChat safety number 2") + joined);
    const QByteArray bytes = d1 + d2;                 // 64 bytes -> 12 groups of 5 digits (5 bytes each)
    QStringList groups;
    for (int g = 0; g < 12; ++g) {
        quint64 v = 0;
        for (int i = 0; i < 5; ++i)
            v = (v << 8) | quint8(bytes[g * 5 + i]);
        groups << QStringLiteral("%1").arg(v % 100000, 5, 10, QLatin1Char('0'));
    }
    return groups.join(' ');
}

SecurityManager::Trust SecurityManager::evaluate(const QString &userId, const QByteArray &key)
{
    if (userId.isEmpty() || key.size() != Crypto::KeyLen)
        return Trust::Changed;
    const QByteArray pinned = m_db->peerKey(userId);
    if (pinned.isEmpty()) {
        const QString other = m_db->userForKey(key);
        if (!other.isEmpty() && other != userId) {
            // the same device key is already known under a different id: do not let an id be re-bound silently
            m_pendingKeys.insert(userId, key);
            emit keyChanged(userId);
            return Trust::Changed;
        }
        m_db->savePeerKey(userId, key, false);
        emit peerSecurityChanged(userId);
        return Trust::New;
    }
    if (Crypto::constantTimeEqual(pinned, key)) {
        if (m_pendingKeys.remove(userId))
            emit peerSecurityChanged(userId);
        return Trust::Trusted;
    }
    qWarning() << "Security: key of" << userId << "changed - refusing the connection";
    const bool fresh = !m_pendingKeys.contains(userId);
    m_pendingKeys.insert(userId, key);
    if (fresh)
        emit keyChanged(userId);
    return Trust::Changed;
}

QVariantMap SecurityManager::peerSecurity(const QString &userId) const
{
    QVariantMap m;
    const QByteArray key = m_db->peerKey(userId);
    m["encrypted"] = true;             // there is no plaintext mode
    m["hasKey"] = !key.isEmpty();
    m["verified"] = m_db->isPeerVerified(userId);
    m["changed"] = m_pendingKeys.contains(userId);
    m["fingerprint"] = key.isEmpty() ? QString() : fingerprint(key);
    m["safetyNumber"] = key.isEmpty() ? QString() : safetyNumber(key);
    m["newFingerprint"] = m_pendingKeys.contains(userId) ? fingerprint(m_pendingKeys.value(userId)) : QString();
    m["since"] = m_db->peerKeySince(userId);
    return m;
}

void SecurityManager::setVerified(const QString &userId, bool verified)
{
    m_db->setPeerVerified(userId, verified);
    emit peerSecurityChanged(userId);
}

void SecurityManager::forget(const QString &userId)
{
    m_pendingKeys.remove(userId);
}

void SecurityManager::acceptNewKey(const QString &userId)
{
    const auto it = m_pendingKeys.constFind(userId);
    if (it == m_pendingKeys.constEnd())
        return;
    const QByteArray key = it.value();
    m_db->savePeerKey(userId, key, false);       // pinning a new key always clears "verified"
    m_pendingKeys.remove(userId);
    emit peerSecurityChanged(userId);
}
