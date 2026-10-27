#ifndef DISCOVERYSERVICE_H
#define DISCOVERYSERVICE_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QMap>
#include <QString>

class DiscoveryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)

public:
    explicit DiscoveryService(QObject *parent = nullptr);
    ~DiscoveryService();
    
    bool isRunning() const;
    
    Q_INVOKABLE void startDiscovery();
    Q_INVOKABLE void stopDiscovery();
    Q_INVOKABLE void setLocalInfo(const QString &userId, const QString &alias, 
                                  const QString &deviceName);
    
signals:
    void peerDiscovered(const QString &userId, const QString &alias, 
                       const QString &deviceName, const QString &ipAddress);
    void peerLost(const QString &userId);
    void runningChanged();
    
private slots:
    void sendDiscoveryBroadcast();
    void readPendingDatagrams();
    
private:
    void processDiscoveryMessage(const QJsonObject &message, const QString &senderIp);
    
    QUdpSocket *m_udpSocket;
    QTimer *m_broadcastTimer;
    bool m_isRunning;
    QString m_localUserId;
    QString m_localAlias;
    QString m_localDeviceName;
    QMap<QString, QString> m_discoveredPeers; // userId -> ipAddress
};

#endif // DISCOVERYSERVICE_H
