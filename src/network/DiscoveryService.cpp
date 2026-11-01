#include "DiscoveryService.h"
#include "core/AppConfig.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QNetworkInterface>

DiscoveryService::DiscoveryService(QObject *parent)
    : QObject(parent)
    , m_udpSocket(new QUdpSocket(this))
    , m_broadcastTimer(new QTimer(this))
    , m_expireTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this))
{
    m_clock.start();
    connect(m_broadcastTimer, &QTimer::timeout, this, &DiscoveryService::sendDiscoveryBroadcast);
    connect(m_expireTimer, &QTimer::timeout, this, &DiscoveryService::expirePeers);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &DiscoveryService::readPendingDatagrams);
    // If the port was busy / the network was down at start-up, keep trying.
    m_retryTimer->setInterval(5000);
    connect(m_retryTimer, &QTimer::timeout, this, [this] {
        if (m_wanted && !m_isRunning)
            startDiscovery();
    });
}

DiscoveryService::~DiscoveryService() { stopDiscovery(); }

bool DiscoveryService::bindSocket()
{
    return m_udpSocket->bind(QHostAddress::AnyIPv4, AppConfig::DISCOVERY_PORT,
                             QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
}

void DiscoveryService::startDiscovery()
{
    m_wanted = true;
    if (m_isRunning)
        return;
    if (!bindSocket()) {
        qWarning() << "Discovery: cannot bind UDP port" << AppConfig::DISCOVERY_PORT << ":"
                   << m_udpSocket->errorString() << "- will retry";
        m_retryTimer->start();
        return;
    }
    m_retryTimer->stop();
    m_broadcastTimer->start(AppConfig::DISCOVERY_INTERVAL_MS);
    m_expireTimer->start(AppConfig::DISCOVERY_INTERVAL_MS);
    m_isRunning = true;
    emit runningChanged();
    qInfo() << "Discovery: listening on UDP" << AppConfig::DISCOVERY_PORT;
    sendDiscoveryBroadcast();
}

void DiscoveryService::stopDiscovery()
{
    m_wanted = false;
    m_retryTimer->stop();
    if (!m_isRunning)
        return;
    m_broadcastTimer->stop();
    m_expireTimer->stop();
    m_udpSocket->close();
    m_isRunning = false;
    emit runningChanged();
    qInfo() << "Discovery: stopped";
}

void DiscoveryService::setLocalInfo(const QString &userId, const QString &alias,
                                    const QString &deviceName)
{
    m_localUserId = userId;
    m_localAlias = alias;
    m_localDeviceName = deviceName;
    if (m_isRunning)
        sendDiscoveryBroadcast();   // push alias changes out immediately
}

void DiscoveryService::reset()
{
    const QStringList ids = m_known.keys();
    m_known.clear();
    for (const QString &id : ids)
        emit peerLost(id);
    if (m_isRunning)
        sendDiscoveryBroadcast();
}

void DiscoveryService::sendDiscoveryBroadcast()
{
    if (m_localUserId.isEmpty() || !m_isRunning)
        return;

    QJsonObject message;
    message["type"] = "discovery";
    message["v"] = AppConfig::PROTOCOL_VERSION;
    message["userId"] = m_localUserId;
    message["alias"] = m_localAlias;
    message["deviceName"] = m_localDeviceName;
    const QByteArray data = QJsonDocument(message).toJson(QJsonDocument::Compact);

    bool sent = false;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            const QHostAddress broadcast = entry.broadcast();
            if (broadcast.isNull())
                continue;
            if (m_udpSocket->writeDatagram(data, broadcast, AppConfig::DISCOVERY_PORT) >= 0)
                sent = true;
        }
    }
    if (!sent) // some systems report no broadcast address for tethering/hotspot interfaces
        m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, AppConfig::DISCOVERY_PORT);
}

void DiscoveryService::readPendingDatagrams()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_udpSocket->receiveDatagram();
        const QJsonDocument doc = QJsonDocument::fromJson(dg.data());
        if (!doc.isObject())
            continue;
        const QJsonObject msg = doc.object();
        if (msg["type"].toString() != "discovery" || msg["userId"].toString() == m_localUserId)
            continue;
        // Normalise "::ffff:192.168.1.5" style addresses
        bool ok = false;
        QHostAddress addr = dg.senderAddress();
        const quint32 v4 = addr.toIPv4Address(&ok);
        processDiscoveryMessage(msg, ok ? QHostAddress(v4).toString() : addr.toString());
    }
}

void DiscoveryService::processDiscoveryMessage(const QJsonObject &msg, const QString &senderIp)
{
    const QString userId = msg["userId"].toString();
    if (userId.isEmpty())
        return;
    const QString alias = msg["alias"].toString();
    const QString device = msg["deviceName"].toString();

    auto it = m_known.find(userId);
    const bool changed = it == m_known.end() || it->ip != senderIp || it->alias != alias
                         || it->deviceName != device;
    m_known[userId] = {senderIp, alias, device, m_clock.elapsed()};
    if (changed) {
        qInfo() << "Discovery: peer" << alias << device << senderIp;
        emit peerDiscovered(userId, alias, device, senderIp);
    }
}

void DiscoveryService::expirePeers()
{
    const qint64 now = m_clock.elapsed();
    QStringList gone;
    for (auto it = m_known.cbegin(); it != m_known.cend(); ++it)
        if (now - it->lastSeenMs > AppConfig::PEER_TIMEOUT_MS)
            gone << it.key();
    for (const QString &id : gone) {
        m_known.remove(id);
        qInfo() << "Discovery: lost peer" << id;
        emit peerLost(id);
    }
}
