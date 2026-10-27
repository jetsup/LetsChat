#include "LanTracker.h"
#include "utils/DeviceInfo.h"
#include <QNetworkInterface>
#include <QCryptographicHash>
#include <QDebug>

LanTracker::LanTracker(QObject *parent)
    : QObject(parent)
    , m_checkTimer(new QTimer(this))
    , m_isConnected(false)
{
    connect(m_checkTimer, &QTimer::timeout, this, &LanTracker::checkNetwork);
    m_checkTimer->start(5000);
}

QString LanTracker::currentNetworkId() const
{
    return m_currentNetworkId;
}

bool LanTracker::isConnected() const
{
    return m_isConnected;
}

void LanTracker::monitorNetwork()
{
    checkNetwork();
}

void LanTracker::checkNetwork()
{
    QString localIp = DeviceInfo::getLocalIpAddress();
    m_isConnected = !localIp.isEmpty() && localIp != "127.0.0.1";
    
    QString networkId = generateNetworkId();
    if (networkId != m_currentNetworkId && !m_currentNetworkId.isEmpty() && !networkId.isEmpty()) {
        // Joined a new LAN
        m_lastNetworkId = m_currentNetworkId;
        m_currentNetworkId = networkId;
        emit joinedNewLan(networkId);
        qDebug() << "Joined new LAN, old:" << m_lastNetworkId << "new:" << m_currentNetworkId;
    } else if (m_currentNetworkId != networkId) {
        m_currentNetworkId = networkId;
    }
    
    emit networkChanged(m_currentNetworkId);
}

QString LanTracker::generateNetworkId() const
{
    // Generate network ID based on local IP
    QString localIp = DeviceInfo::getLocalIpAddress();
    QStringList parts = localIp.split('.');
    if (parts.size() == 4) {
        QString networkPrefix = parts[0] + "." + parts[1] + "." + parts[2];
        QByteArray hash = QCryptographicHash::hash(networkPrefix.toUtf8(), QCryptographicHash::Sha256);
        return hash.toHex().left(16);
    }
    return QString();
}
