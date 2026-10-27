#ifndef PEERMANAGER_H
#define PEERMANAGER_H

#include <QObject>
#include "DiscoveryService.h"
#include "models/PeerListModel.h"
#include "storage/DatabaseManager.h"

class PeerManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PeerListModel* peerModel READ peerModel CONSTANT)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY scanningChanged)

public:
    explicit PeerManager(QObject *parent = nullptr);
    
    PeerListModel* peerModel() const;
    bool isScanning() const;
    
    Q_INVOKABLE void startScanning();
    Q_INVOKABLE void stopScanning();
    Q_INVOKABLE void setLocalIdentity(const QString &userId, const QString &alias, 
                                     const QString &deviceName);
    Q_INVOKABLE void handleNetworkChange(); // Called when switching LANs
    
signals:
    void scanningChanged();
    void peerConnected(const QString &userId);
    void peerDisconnected(const QString &userId);
    
private slots:
    void onPeerDiscovered(const QString &userId, const QString &alias, 
                         const QString &deviceName, const QString &ipAddress);
    void onPeerLost(const QString &userId);
    
private:
    DiscoveryService *m_discoveryService;
    PeerListModel *m_peerModel;
    DatabaseManager *m_databaseManager;
    bool m_isScanning;
    QString m_localUserId;
};

#endif // PEERMANAGER_H
