#ifndef PEERMANAGER_H
#define PEERMANAGER_H

#include "DiscoveryService.h"
#include "models/PeerListModel.h"
#include "storage/DatabaseManager.h"
#include <QObject>
#include <QSortFilterProxyModel>

/** Owns the peer list: merges what LAN discovery and Bluetooth tell us and persists it. */
class PeerManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *model READ model CONSTANT)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY onlineCountChanged)
    Q_PROPERTY(int peerCount READ peerCount NOTIFY onlineCountChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit PeerManager(DatabaseManager *db, QObject *parent = nullptr);

    QObject *model() const { return m_proxy; }
    PeerListModel *peers() const { return m_peerModel; }
    bool isScanning() const { return m_discovery->isRunning(); }
    int onlineCount() const { return m_peerModel->onlineCount(); }
    int peerCount() const { return m_peerModel->rowCount(); }
    QString filterText() const { return m_filter; }
    void setFilterText(const QString &text);

    void setLocalIdentity(const QString &userId, const QString &alias, const QString &deviceName,
                          const QString &avatarHash = {});
    Q_INVOKABLE QVariantMap info(const QString &userId) const { return m_peerModel->info(userId); }
    /** The name *we* give a peer (nickname) wins over what the peer broadcasts. */
    Q_INVOKABLE void setNickname(const QString &userId, const QString &nickname);
    void setAvatar(const QString &userId, const QString &hash);
    void startScanning() { m_discovery->startDiscovery(); }
    void setBackground(bool background) { m_discovery->setBackground(background); }
    void stopScanning() { m_discovery->stopDiscovery(); }
    /** We joined a different LAN: everyone seen on LAN is unreachable until re-announced. */
    void handleNetworkChange();
    void loadKnownPeers();

public slots:
    void onBluetoothPeerUp(const QString &userId, const QString &alias, const QString &deviceName,
                           const QString &address, const QString &avatarHash);
    void onBluetoothPeerDown(const QString &userId);
    /** Used when a LAN message arrives from a peer we have not discovered (yet). */
    void onLanPeerSeen(const QString &userId, const QString &alias, const QString &ip);

signals:
    void scanningChanged();
    void onlineCountChanged();
    void filterTextChanged();
    void peerOnline(const QString &userId);
    void peerOffline(const QString &userId);
    void avatarNeeded(const QString &userId, const QString &ip);   // ip empty -> via Bluetooth
    void peerChanged(const QString &userId);

private:
    void onPeerDiscovered(const QString &userId, const QString &alias, const QString &deviceName,
                          const QString &ipAddress, const QString &avatarHash);
    /** The peer's picture differs from the one we have (empty ip = reachable over Bluetooth). */
    void checkAvatar(const QString &userId, const QString &advertisedHash, const QString &ip);
    void onPeerLost(const QString &userId);

    DatabaseManager *m_db;
    DiscoveryService *m_discovery;
    PeerListModel *m_peerModel;
    QSortFilterProxyModel *m_proxy;
    QString m_localUserId;
    QString m_filter;
};

#endif // PEERMANAGER_H
