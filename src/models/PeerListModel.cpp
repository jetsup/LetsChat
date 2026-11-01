#include "PeerListModel.h"

PeerListModel::PeerListModel(QObject *parent) : QAbstractListModel(parent) {}

int PeerListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_peers.size());
}

QVariant PeerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_peers.size())
        return {};
    const Peer &p = m_peers.at(index.row());
    switch (role) {
    case UserIdRole: return p.userId;
    case AliasRole: return p.alias;
    case DeviceNameRole: return p.deviceName;
    case DisplayNameRole: return p.displayName();
    case IpAddressRole: return p.ipAddress;
    case BtAddressRole: return p.btAddress;
    case IsOnlineRole: return p.isOnline();
    case TransportRole: return p.transport();
    case LanOnlineRole: return p.lanOnline;
    case BtOnlineRole: return p.btOnline;
    case LastMessageRole: return p.lastMessage;
    case LastActivityRole: return p.lastActivity;
    case UnreadRole: return p.unread;
    // online peers first, then most recent activity, then name
    case SortKeyRole:
        return QString(p.isOnline() ? "1" : "0") + p.lastActivity.rightJustified(25, '0')
               + p.displayName().toLower();
    default: return {};
    }
}

QHash<int, QByteArray> PeerListModel::roleNames() const
{
    return {
        {UserIdRole, "userId"},       {AliasRole, "alias"},
        {DeviceNameRole, "deviceName"}, {DisplayNameRole, "displayName"},
        {IpAddressRole, "ipAddress"}, {BtAddressRole, "btAddress"},
        {IsOnlineRole, "isOnline"},   {TransportRole, "transport"},
        {LanOnlineRole, "lanOnline"}, {BtOnlineRole, "btOnline"},
        {LastMessageRole, "lastMessage"}, {LastActivityRole, "lastActivity"},
        {UnreadRole, "unread"},       {SortKeyRole, "sortKey"},
    };
}

int PeerListModel::onlineCount() const
{
    int n = 0;
    for (const Peer &p : m_peers)
        n += p.isOnline() ? 1 : 0;
    return n;
}

int PeerListModel::indexOf(const QString &userId) const
{
    for (int i = 0; i < m_peers.size(); ++i)
        if (m_peers.at(i).userId == userId)
            return i;
    return -1;
}

const Peer *PeerListModel::find(const QString &userId) const
{
    const int i = indexOf(userId);
    return i < 0 ? nullptr : &m_peers.at(i);
}

Peer &PeerListModel::ensure(const QString &userId, bool *created)
{
    int i = indexOf(userId);
    if (i < 0) {
        beginInsertRows({}, int(m_peers.size()), int(m_peers.size()));
        Peer p;
        p.userId = userId;
        m_peers.append(p);
        endInsertRows();
        i = int(m_peers.size()) - 1;
        if (created) *created = true;
    }
    return m_peers[i];
}

void PeerListModel::touch(int row, const QList<int> &roles)
{
    emit dataChanged(index(row), index(row), roles);
    const int online = onlineCount();
    if (online != m_lastOnline) {
        m_lastOnline = online;
        emit onlineCountChanged();
    }
}

void PeerListModel::upsertLan(const QString &userId, const QString &alias, const QString &deviceName,
                              const QString &ip)
{
    Peer &p = ensure(userId);
    p.alias = alias;
    p.deviceName = deviceName;
    p.ipAddress = ip;
    p.lanOnline = true;
    touch(indexOf(userId));
}

void PeerListModel::upsertBluetooth(const QString &userId, const QString &alias,
                                    const QString &deviceName, const QString &btAddress)
{
    Peer &p = ensure(userId);
    p.alias = alias;
    if (!deviceName.isEmpty()) p.deviceName = deviceName;
    p.btAddress = btAddress;
    p.btOnline = true;
    touch(indexOf(userId));
}

void PeerListModel::loadKnown(const QString &userId, const QString &alias, const QString &deviceName,
                              const QString &ip, const QString &bt, const QString &lastSeen)
{
    Peer &p = ensure(userId);
    p.alias = alias;
    p.deviceName = deviceName;
    p.ipAddress = ip;
    p.btAddress = bt;
    if (p.lastActivity.isEmpty())
        p.lastActivity = lastSeen;
    touch(indexOf(userId));
}

void PeerListModel::setLanOffline(const QString &userId)
{
    const int i = indexOf(userId);
    if (i >= 0 && m_peers[i].lanOnline) {
        m_peers[i].lanOnline = false;
        touch(i);
    }
}

void PeerListModel::setBluetoothOffline(const QString &userId)
{
    const int i = indexOf(userId);
    if (i >= 0 && m_peers[i].btOnline) {
        m_peers[i].btOnline = false;
        touch(i);
    }
}

void PeerListModel::markAllLanOffline()
{
    for (int i = 0; i < m_peers.size(); ++i) {
        if (m_peers[i].lanOnline) {
            m_peers[i].lanOnline = false;
            touch(i);
        }
    }
}

void PeerListModel::setLastMessage(const QString &userId, const QString &text, const QString &timestamp)
{
    const int i = indexOf(userId);
    if (i < 0) return;
    m_peers[i].lastMessage = text;
    m_peers[i].lastActivity = timestamp;
    touch(i);
}

void PeerListModel::incrementUnread(const QString &userId)
{
    const int i = indexOf(userId);
    if (i < 0) return;
    ++m_peers[i].unread;
    touch(i);
}

void PeerListModel::clearUnread(const QString &userId)
{
    const int i = indexOf(userId);
    if (i >= 0 && m_peers[i].unread != 0) {
        m_peers[i].unread = 0;
        touch(i);
    }
}
