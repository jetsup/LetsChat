#include "PeerListModel.h"
#include "storage/DatabaseManager.h"
#include <QDebug>

PeerListModel::PeerListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PeerListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_peers.count();
}

QVariant PeerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_peers.size())
        return QVariant();
    
    const Peer &peer = m_peers[index.row()];
    
    switch (role) {
    case UserIdRole:
        return peer.userId;
    case AliasRole:
        return peer.alias;
    case DeviceNameRole:
        return peer.deviceName;
    case IpAddressRole:
        return peer.ipAddress;
    case IsOnlineRole:
        return peer.isOnline;
    case LastSeenRole:
        return peer.lastSeen;
    case DisplayNameRole:
        return peer.alias.isEmpty() ? peer.deviceName : peer.alias;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PeerListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[UserIdRole] = "userId";
    roles[AliasRole] = "alias";
    roles[DeviceNameRole] = "deviceName";
    roles[IpAddressRole] = "ipAddress";
    roles[IsOnlineRole] = "isOnline";
    roles[LastSeenRole] = "lastSeen";
    roles[DisplayNameRole] = "displayName";
    return roles;
}

void PeerListModel::addOrUpdatePeer(const QString &userId, const QString &alias,
                                   const QString &deviceName, const QString &ipAddress,
                                   bool isOnline)
{
    int index = getPeerIndex(userId);
    if (index >= 0) {
        // Update existing peer
        m_peers[index].alias = alias;
        m_peers[index].deviceName = deviceName;
        m_peers[index].ipAddress = ipAddress;
        m_peers[index].isOnline = isOnline;
        emit dataChanged(this->index(index), this->index(index));
    } else {
        // Add new peer
        beginInsertRows(QModelIndex(), m_peers.count(), m_peers.count());
        Peer peer;
        peer.userId = userId;
        peer.alias = alias;
        peer.deviceName = deviceName;
        peer.ipAddress = ipAddress;
        peer.isOnline = isOnline;
        m_peers.append(peer);
        endInsertRows();
    }
}

void PeerListModel::removePeer(const QString &userId)
{
    int index = getPeerIndex(userId);
    if (index >= 0) {
        beginRemoveRows(QModelIndex(), index, index);
        m_peers.removeAt(index);
        endRemoveRows();
    }
}

void PeerListModel::markAllOffline()
{
    for (int i = 0; i < m_peers.size(); ++i) {
        if (m_peers[i].isOnline) {
            m_peers[i].isOnline = false;
            emit dataChanged(this->index(i), this->index(i), {IsOnlineRole});
        }
    }
}

void PeerListModel::markPeerOffline(const QString &userId)
{
    int index = getPeerIndex(userId);
    if (index >= 0 && m_peers[index].isOnline) {
        m_peers[index].isOnline = false;
        emit dataChanged(this->index(index), this->index(index), {IsOnlineRole});
    }
}

void PeerListModel::clear()
{
    beginResetModel();
    m_peers.clear();
    endResetModel();
}

QVariantMap PeerListModel::getPeer(int index) const
{
    QVariantMap result;
    if (index >= 0 && index < m_peers.size()) {
        const Peer &peer = m_peers[index];
        result["userId"] = peer.userId;
        result["alias"] = peer.alias;
        result["deviceName"] = peer.deviceName;
        result["ipAddress"] = peer.ipAddress;
        result["isOnline"] = peer.isOnline;
        result["displayName"] = peer.alias.isEmpty() ? peer.deviceName : peer.alias;
    }
    return result;
}

int PeerListModel::getPeerIndex(const QString &userId) const
{
    for (int i = 0; i < m_peers.size(); ++i) {
        if (m_peers[i].userId == userId) {
            return i;
        }
    }
    return -1;
}

void PeerListModel::loadFromDatabase()
{
    // This would load from DatabaseManager
    // Implementation would need access to DB instance
}
