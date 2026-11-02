#ifndef PEERLISTMODEL_H
#define PEERLISTMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>

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
    QString nickname;       // what *we* call them; wins over everything they broadcast
    QString firstSeen;
    QString lastSeen;
    QString connectedSince; // runtime only: when they last became reachable
    QString avatarHash;

    bool isOnline() const { return lanOnline || btOnline; }
    QString broadcastName() const { return alias.isEmpty() ? deviceName : alias; }
    QString displayName() const { return nickname.isEmpty() ? broadcastName() : nickname; }
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
        SortKeyRole,
        NicknameRole,
        BroadcastNameRole,
        AvatarUrlRole,
        SearchTextRole
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
                   const QString &ip, const QString &bt, const QString &lastSeen,
                   const QString &nickname = {}, const QString &firstSeen = {},
                   const QString &avatarHash = {});
    void setNickname(const QString &userId, const QString &nickname);
    void remove(const QString &userId);
    void setAvatarHash(const QString &userId, const QString &hash);
    QVariantMap info(const QString &userId) const;
    static void setAvatarDir(const QString &dir) { s_avatarDir = dir; }
    static QString avatarPath(const QString &userId) { return s_avatarDir + "/" + userId + ".jpg"; }
    void setLanOffline(const QString &userId);
    void setBluetoothOffline(const QString &userId);
    void markAllLanOffline();

    void setLastMessage(const QString &userId, const QString &text, const QString &timestamp);
    void incrementUnread(const QString &userId);
    void clearUnread(const QString &userId);

signals:
    void onlineCountChanged();
    void peerChanged(const QString &userId);

private:
    int indexOf(const QString &userId) const;
    Peer &ensure(const QString &userId, bool *created = nullptr);
    void touch(int row, const QList<int> &roles = {});

    QList<Peer> m_peers;
    static inline QString s_avatarDir;
    int m_lastOnline = 0;
};

#endif // PEERLISTMODEL_H
