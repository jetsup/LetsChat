#include "DiscoveryService.h"
#include "core/AppConfig.h"
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QDateTime>
#include <QDebug>

DiscoveryService::DiscoveryService(QObject *parent)
    : QObject(parent)
    , m_udpSocket(new QUdpSocket(this))
    , m_broadcastTimer(new QTimer(this))
    , m_isRunning(false)
{
    connect(m_broadcastTimer, &QTimer::timeout, this, &DiscoveryService::sendDiscoveryBroadcast);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &DiscoveryService::readPendingDatagrams);
}

DiscoveryService::~DiscoveryService()
{
    stopDiscovery();
}

bool DiscoveryService::isRunning() const
{
    return m_isRunning;
}

void DiscoveryService::startDiscovery()
{
    if (m_isRunning) {
        return;
    }
    
    if (m_udpSocket->bind(QHostAddress::Any, AppConfig::DISCOVERY_PORT, 
                          QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        m_broadcastTimer->start(AppConfig::DISCOVERY_INTERVAL_MS);
        m_isRunning = true;
        emit runningChanged();
        qDebug() << "Discovery service started on port" << AppConfig::DISCOVERY_PORT;
    } else {
        qDebug() << "Failed to bind discovery socket:" << m_udpSocket->errorString();
    }
}

void DiscoveryService::stopDiscovery()
{
    if (!m_isRunning) {
        return;
    }
    
    m_broadcastTimer->stop();
    m_udpSocket->close();
    m_isRunning = false;
    emit runningChanged();
    qDebug() << "Discovery service stopped";
}

void DiscoveryService::setLocalInfo(const QString &userId, const QString &alias, 
                                    const QString &deviceName)
{
    m_localUserId = userId;
    m_localAlias = alias;
    m_localDeviceName = deviceName;
}

void DiscoveryService::sendDiscoveryBroadcast()
{
    if (m_localUserId.isEmpty()) {
        return;
    }
    
    QJsonObject message;
    message["type"] = "discovery";
    message["userId"] = m_localUserId;
    message["alias"] = m_localAlias;
    message["deviceName"] = m_localDeviceName;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    // Broadcast to all network interfaces
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        if (interface.flags() & QNetworkInterface::IsUp && 
            interface.flags() & QNetworkInterface::IsRunning &&
            !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            
            const QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    QHostAddress broadcast = entry.broadcast();
                    if (!broadcast.isNull()) {
                        m_udpSocket->writeDatagram(data, broadcast, AppConfig::DISCOVERY_PORT);
                    }
                }
            }
        }
    }
}

void DiscoveryService::readPendingDatagrams()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;
        
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(datagram, &error);
        if (error.error != QJsonParseError::NoError) {
            continue;
        }
        
        if (doc.isObject()) {
            QJsonObject message = doc.object();
            if (message["type"].toString() == "discovery") {
                // Ignore our own broadcasts
                if (message["userId"].toString() == m_localUserId) {
                    continue;
                }
                processDiscoveryMessage(message, senderAddress.toString());
            }
        }
    }
}

void DiscoveryService::processDiscoveryMessage(const QJsonObject &message, const QString &senderIp)
{
    QString userId = message["userId"].toString();
    QString alias = message["alias"].toString();
    QString deviceName = message["deviceName"].toString();
    
    if (!userId.isEmpty()) {
        bool isNew = !m_discoveredPeers.contains(userId);
        m_discoveredPeers[userId] = senderIp;
        
        if (isNew) {
            emit peerDiscovered(userId, alias, deviceName, senderIp);
            qDebug() << "Discovered peer:" << userId << alias << deviceName << senderIp;
        }
    }
}
