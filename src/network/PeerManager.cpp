#include "PeerManager.h"
#include <QDebug>
#include <QRegularExpression>

PeerManager::PeerManager(DatabaseManager *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_discovery(new DiscoveryService(this))
    , m_peerModel(new PeerListModel(this))
    , m_proxy(new QSortFilterProxyModel(this))
{
    m_proxy->setSourceModel(m_peerModel);
    m_proxy->setSortRole(PeerListModel::SortKeyRole);
    m_proxy->setFilterRole(PeerListModel::DisplayNameRole);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setDynamicSortFilter(true);
    m_proxy->sort(0, Qt::DescendingOrder);

    connect(m_discovery, &DiscoveryService::peerDiscovered, this, &PeerManager::onPeerDiscovered);
    connect(m_discovery, &DiscoveryService::peerLost, this, &PeerManager::onPeerLost);
    connect(m_discovery, &DiscoveryService::runningChanged, this, &PeerManager::scanningChanged);
    connect(m_peerModel, &PeerListModel::onlineCountChanged, this, &PeerManager::onlineCountChanged);
    connect(m_peerModel, &QAbstractItemModel::rowsInserted, this, &PeerManager::onlineCountChanged);
}

void PeerManager::setFilterText(const QString &text)
{
    if (m_filter == text)
        return;
    m_filter = text;
    m_proxy->setFilterFixedString(text.trimmed());
    emit filterTextChanged();
}

void PeerManager::setLocalIdentity(const QString &userId, const QString &alias,
                                   const QString &deviceName)
{
    m_localUserId = userId;
    m_discovery->setLocalInfo(userId, alias, deviceName);
}

void PeerManager::loadKnownPeers()
{
    const QVariantList rows = m_db->getPeers();
    for (const QVariant &v : rows) {
        const QVariantMap p = v.toMap();
        if (p["userId"].toString() == m_localUserId)
            continue;
        m_peerModel->loadKnown(p["userId"].toString(), p["alias"].toString(),
                               p["deviceName"].toString(), p["ipAddress"].toString(),
                               p["btAddress"].toString(), p["lastSeen"].toString());
    }
}

void PeerManager::handleNetworkChange()
{
    m_peerModel->markAllLanOffline();
    m_discovery->reset();
}

void PeerManager::onPeerDiscovered(const QString &userId, const QString &alias,
                                   const QString &deviceName, const QString &ipAddress)
{
    if (userId == m_localUserId)
        return;
    const Peer *before = m_peerModel->find(userId);
    const bool wasOnline = before && before->lanOnline;
    m_peerModel->upsertLan(userId, alias, deviceName, ipAddress);
    m_db->savePeer(userId, alias, deviceName, ipAddress, {});
    if (!wasOnline)
        emit peerOnline(userId);
}

void PeerManager::onPeerLost(const QString &userId)
{
    m_peerModel->setLanOffline(userId);
    emit peerOffline(userId);
}

void PeerManager::onLanPeerSeen(const QString &userId, const QString &alias, const QString &ip)
{
    if (userId == m_localUserId || userId.isEmpty())
        return;
    // Do not mark the peer online (discovery owns that state); just remember how to reach it.
    const Peer *p = m_peerModel->find(userId);
    if (!p) {
        m_peerModel->loadKnown(userId, alias, alias, ip, {}, {});
        m_db->savePeer(userId, alias, alias, ip, {});
    } else if (p->ipAddress != ip) {
        m_peerModel->loadKnown(userId, p->alias, p->deviceName, ip, p->btAddress, {});
        m_db->savePeer(userId, p->alias, p->deviceName, ip, {});
    }
}

void PeerManager::onBluetoothPeerUp(const QString &userId, const QString &alias,
                                    const QString &deviceName, const QString &address)
{
    if (userId == m_localUserId)
        return;
    const Peer *before = m_peerModel->find(userId);
    const bool wasOnline = before && before->isOnline();
    m_peerModel->upsertBluetooth(userId, alias, deviceName, address);
    m_db->savePeer(userId, alias, deviceName, {}, address);
    if (!wasOnline)
        emit peerOnline(userId);
}

void PeerManager::onBluetoothPeerDown(const QString &userId)
{
    m_peerModel->setBluetoothOffline(userId);
    const Peer *p = m_peerModel->find(userId);
    if (p && !p->isOnline())
        emit peerOffline(userId);
}
