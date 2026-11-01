#ifndef PEERLISTMODEL_H
#define PEERLISTMODEL_H

#include <QAbstractListModel>
#include <QString>

struct Peer
{
    QString userId;
    QString alias;
    QString deviceName;
    QString ipAddress;      // last known LAN address
    QString btAddress;      // last known Bluetooth address
    bool lanOnline = false;
    bool btOnline = false;
    QString lastMessage;
    QString lastActivity;   // ISO timestamp of the last message (or last seen)
    int unread = 0;

    bool isOnline() const { return lanOnline || btOnline; }
    QString displayName() const { return alias.isEmpty() ? deviceName : alias; }
    QString transport() const { return lanOnline ? "lan" : (btOnline ? "bluetooth" : ""); }
};

class PeerListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY onlineCountChanged)

public:
    enum PeerRoles {
        UserIdRole = Qt::UserRole + 1,
        AliasRole,
        DeviceNameRole,
        DisplayNameRole,
        IpAddressRole,
        BtAddressRole,
        IsOnlineRole,
        TransportRole,
        LanOnlineRole,
        BtOnlineRole,
        LastMessageRole,
        LastActivityRole,
        UnreadRole,
        SortKeyRole
    };

    explicit PeerListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int onlineCount() const;
    bool contains(const QString &userId) const { return indexOf(userId) >= 0; }
    const Peer *find(const QString &userId) const;

    void upsertLan(const QString &userId, const QString &alias, const QString &deviceName,
                   const QString &ip);
    void upsertBluetooth(const QString &userId, const QString &alias, const QString &deviceName,
                         const QString &btAddress);
    void loadKnown(const QString &userId, const QString &alias, const QString &deviceName,
                   const QString &ip, const QString &bt, const QString &lastSeen);
    void setLanOffline(const QString &userId);
    void setBluetoothOffline(const QString &userId);
    void markAllLanOffline();

    void setLastMessage(const QString &userId, const QString &text, const QString &timestamp);
    void incrementUnread(const QString &userId);
    void clearUnread(const QString &userId);

signals:
    void onlineCountChanged();

private:
    int indexOf(const QString &userId) const;
    Peer &ensure(const QString &userId, bool *created = nullptr);
    void touch(int row, const QList<int> &roles = {});

    QList<Peer> m_peers;
    int m_lastOnline = 0;
};

#endif // PEERLISTMODEL_H
