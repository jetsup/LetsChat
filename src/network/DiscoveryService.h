#ifndef DISCOVERYSERVICE_H
#define DISCOVERYSERVICE_H

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

/**
 * UDP broadcast presence protocol.  Every DISCOVERY_INTERVAL_MS we announce
 * {userId, alias, deviceName} on every IPv4 broadcast address; peers that stay silent
 * for PEER_TIMEOUT_MS are reported as lost.
 */
class DiscoveryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)

public:
    explicit DiscoveryService(QObject *parent = nullptr);
    ~DiscoveryService() override;

    bool isRunning() const { return m_isRunning; }

    void startDiscovery();
    void stopDiscovery();
    /** Announce less often while the UI is in the background (saves battery). */
    void setBackground(bool background);
    void setLocalInfo(const QString &userId, const QString &alias, const QString &deviceName,
                      const QString &avatarHash = {});
    /** Forget every known peer (used when we move to another network) and announce again. */
    void reset();

signals:
    void peerDiscovered(const QString &userId, const QString &alias, const QString &deviceName,
                        const QString &ipAddress, const QString &avatarHash);
    void peerLost(const QString &userId);
    void runningChanged();

private:
    void sendDiscoveryBroadcast();
    void readPendingDatagrams();
    void expirePeers();
    void processDiscoveryMessage(const QJsonObject &message, const QString &senderIp);
    bool bindSocket();

    struct Known { QString ip; QString alias; QString deviceName; QString avatar; qint64 lastSeenMs; qint64 timeoutMs; };

    QUdpSocket *m_udpSocket;
    QTimer *m_broadcastTimer;
    QTimer *m_expireTimer;
    QTimer *m_retryTimer;
    QElapsedTimer m_clock;
    int m_intervalMs = 3000;
    bool m_isRunning = false;
    bool m_wanted = false;
    QString m_localUserId, m_localAlias, m_localDeviceName, m_localAvatar;
    QHash<QString, Known> m_known;
};

#endif // DISCOVERYSERVICE_H
