#ifndef PEERLISTMODEL_H
#define PEERLISTMODEL_H

#include <QAbstractListModel>
#include <QString>

struct Peer
{
    QString userId;
    QString alias;
    QString deviceName;
    QString ipAddress;
    bool isOnline;
    QString lastSeen;
};

class PeerListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum PeerRoles {
        UserIdRole = Qt::UserRole + 1,
        AliasRole,
        DeviceNameRole,
        IpAddressRole,
        IsOnlineRole,
        LastSeenRole,
        DisplayNameRole
    };

    explicit PeerListModel(QObject *parent = nullptr);
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    Q_INVOKABLE void addOrUpdatePeer(const QString &userId, const QString &alias,
                                    const QString &deviceName, const QString &ipAddress,
                                    bool isOnline);
    Q_INVOKABLE void removePeer(const QString &userId);
    Q_INVOKABLE void markAllOffline();
    Q_INVOKABLE void markPeerOffline(const QString &userId);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QVariantMap getPeer(int index) const;
    Q_INVOKABLE int getPeerIndex(const QString &userId) const;
    
    void loadFromDatabase();
    
private:
    QList<Peer> m_peers;
};

#endif // PEERLISTMODEL_H
