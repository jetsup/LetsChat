#include "BluetoothService.h"
#include "FrameParser.h"
#include "core/AppConfig.h"
#include <QBluetoothLocalDevice>
#include <QBluetoothPermission>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTimer>

namespace { constexpr int ConnectTimeoutMs = 20000; }

BluetoothService::BluetoothService(const QString &receiveDir, QObject *parent)
    : QObject(parent), m_receiveDir(receiveDir)
{
    QSettings s;
    m_enabled = s.value("bluetooth/enabled", true).toBool();
    const QStringList known = s.value("bluetooth/known").toStringList();
    m_known = QSet<QString>(known.begin(), known.end());
}

BluetoothService::~BluetoothService() { stop(); }

bool BluetoothService::available() const
{
    return !QBluetoothLocalDevice::allDevices().isEmpty();
}

QBluetoothLocalDevice *BluetoothService::localDevice()
{
    if (!m_local && available()) {
        m_local = new QBluetoothLocalDevice(this);
        connect(m_local, &QBluetoothLocalDevice::hostModeStateChanged, this,
                [this](QBluetoothLocalDevice::HostMode mode) {
            if (mode == QBluetoothLocalDevice::HostPoweredOff) {
                setStatus(tr("Bluetooth is turned off"));
                if (m_server) {      // sockets are gone with the adapter
                    m_serviceInfo.unregisterService();
                    m_server->close();
                    m_server->deleteLater();
                    m_server = nullptr;
                }
            } else {
                setStatus({});
                start();
            }
            emit stateChanged();
        });
    }
    return m_local;
}

bool BluetoothService::poweredOff() const
{
    return m_local && m_local->hostMode() == QBluetoothLocalDevice::HostPoweredOff;
}

void BluetoothService::turnOn()
{
    ensurePermission([this] {
        if (QBluetoothLocalDevice *l = localDevice())
            l->powerOn();
        else
            setStatus(tr("No Bluetooth adapter found"));
    });
}

void BluetoothService::setStatus(const QString &text)
{
    if (m_status == text)
        return;
    m_status = text;
    qInfo() << "Bluetooth:" << (text.isEmpty() ? "<ok>" : text);
    emit stateChanged();
}

void BluetoothService::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    QSettings().setValue("bluetooth/enabled", on);
    if (on) start(); else stop();
    emit stateChanged();
}

void BluetoothService::setLocalInfo(const QString &userId, const QString &alias,
                                    const QString &deviceName, const QString &avatarHash,
                                    const QString &avatarPath)
{
    m_localAvatarHash = avatarHash;
    m_localAvatarPath = avatarPath;
    m_localUserId = userId;
    m_localAlias = alias;
    m_localDeviceName = deviceName;
}

void BluetoothService::ensurePermission(const std::function<void()> &granted)
{
    QBluetoothPermission permission;
    permission.setCommunicationModes(QBluetoothPermission::Access | QBluetoothPermission::Advertise);
    QCoreApplication *app = QCoreApplication::instance();
    switch (app->checkPermission(permission)) {
    case Qt::PermissionStatus::Granted:
        granted();
        break;
    case Qt::PermissionStatus::Undetermined:
        app->requestPermission(permission, this, [this, granted](const QPermission &result) {
            if (result.status() == Qt::PermissionStatus::Granted)
                granted();
            else
                setStatus(tr("Bluetooth permission denied. Allow “Nearby devices” in the system settings."));
        });
        break;
    case Qt::PermissionStatus::Denied:
        setStatus(tr("Bluetooth permission denied. Allow “Nearby devices” in the system settings."));
        break;
    }
}

// ------------------------------------------------------------------------------ server side

void BluetoothService::start()
{
    if (!m_enabled || listening())
        return;
    ensurePermission([this] { startListening(); });
}

void BluetoothService::startListening()
{
    if (m_server)
        return;
    if (!available()) {
        setStatus(tr("No Bluetooth adapter found"));
        return;
    }
    if (localDevice()->hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        setStatus(tr("Bluetooth is turned off"));
        emit stateChanged();
        return;
    }

    m_server = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
#ifdef Q_OS_ANDROID
    // Android listens with a "secure" socket by default, which only accepts devices that are paired.
    // LetsChat links are meant to work between devices that never met before.
    m_server->setSecurityFlags(QBluetooth::Security::NoSecurity);
#endif
    connect(m_server, &QBluetoothServer::newConnection, this, [this] {
        while (QBluetoothSocket *s = m_server->nextPendingConnection()) {
            qInfo() << "Bluetooth: incoming connection from" << s->peerAddress().toString();
            adopt(s);
        }
    });
    connect(m_server, &QBluetoothServer::errorOccurred, this,
            [this](QBluetoothServer::Error e) { setStatus(tr("Bluetooth server error (%1)").arg(int(e))); });

    m_serviceInfo = m_server->listen(QBluetoothUuid(AppConfig::BLUETOOTH_SERVICE_UUID),
                                     AppConfig::BLUETOOTH_SERVICE_NAME);
    if (!m_server->isListening()) {
        setStatus(tr("Could not start the Bluetooth service"));
        m_server->deleteLater();
        m_server = nullptr;
        return;
    }
    setStatus({});
    emit stateChanged();
}

void BluetoothService::stop()
{
    stopScan();
    const auto sockets = m_links.keys();
    for (QBluetoothSocket *s : sockets)
        s->disconnectFromService();
    if (m_server) {
        m_serviceInfo.unregisterService();
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    emit stateChanged();
}

void BluetoothService::makeDiscoverable()
{
    ensurePermission([this] {
        QBluetoothLocalDevice *local = localDevice();
        if (!local || !local->isValid()) {
            setStatus(tr("No Bluetooth adapter found"));
            return;
        }
        local->setHostMode(QBluetoothLocalDevice::HostDiscoverable);
    });
}

// ------------------------------------------------------------------------------ discovery

QVariantList BluetoothService::devices() const
{
    QList<Seen> list = m_seen.values();
    auto rank = [](const Seen &s) { return s.kind == "other" ? 1 : 0; };
    std::sort(list.begin(), list.end(), [&](const Seen &a, const Seen &b) {
        if (rank(a) != rank(b)) return rank(a) < rank(b);
        if (a.name.isEmpty() != b.name.isEmpty()) return !a.name.isEmpty();
        return a.name.toLower() < b.name.toLower();
    });
    QVariantList out;
    for (const Seen &s : list) {
        out.append(QVariantMap{
            {"name", s.name.isEmpty() ? tr("Unknown device") : s.name},
            {"address", s.address},
            {"kind", s.kind},
            {"paired", s.paired},
            {"connected", socketForAddress(s.address) != nullptr},
            {"connecting", m_connecting.contains(s.address)},
            {"known", m_known.contains(s.address)},
        });
    }
    return out;
}

void BluetoothService::startScan()
{
    if (m_scanning)
        return;
    ensurePermission([this] {
        if (!available()) {
            setStatus(tr("No Bluetooth adapter found"));
            return;
        }
        if (localDevice()->hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
            setStatus(tr("Bluetooth is turned off"));
            emit stateChanged();
            return;
        }
        if (!m_agent) {
            m_agent = new QBluetoothDeviceDiscoveryAgent(this);
            connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this,
                    [this](const QBluetoothDeviceInfo &info) {
                if (!(info.coreConfigurations() & QBluetoothDeviceInfo::BaseRateCoreConfiguration))
                    return;   // LE-only devices cannot speak RFCOMM
                Seen s;
                s.name = info.name();
                s.address = info.address().toString();
                if (s.address.isEmpty()) // macOS-style opaque ids are not supported
                    return;
                const auto major = info.majorDeviceClass();
                s.kind = major == QBluetoothDeviceInfo::PhoneDevice ? "phone"
                         : major == QBluetoothDeviceInfo::ComputerDevice ? "computer" : "other";
                s.paired = localDevice()->pairingStatus(info.address())
                           != QBluetoothLocalDevice::Unpaired;
                m_seen.insert(s.address, s);
                emit devicesChanged();
                // Reconnect to devices we already talked to.
                if (m_known.contains(s.address) && !socketForAddress(s.address)
                    && !m_connecting.contains(s.address))
                    connectToDevice(s.address);
            });
            connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished, this, [this] {
                m_scanning = false;
                emit scanningChanged();
            });
            connect(m_agent, &QBluetoothDeviceDiscoveryAgent::canceled, this, [this] {
                m_scanning = false;
                emit scanningChanged();
            });
            connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
                    [this](QBluetoothDeviceDiscoveryAgent::Error) {
                setStatus(tr("Scan failed: %1").arg(m_agent->errorString()));
                m_scanning = false;
                emit scanningChanged();
            });
        }
        setStatus({});
        m_seen.clear();
        emit devicesChanged();
        m_agent->setLowEnergyDiscoveryTimeout(0);
        m_agent->start(QBluetoothDeviceDiscoveryAgent::ClassicMethod);
        m_scanning = m_agent->isActive();
        emit scanningChanged();
    });
}

void BluetoothService::stopScan()
{
    if (m_agent && m_agent->isActive())
        m_agent->stop();
}

// ------------------------------------------------------------------------------ links

QBluetoothSocket *BluetoothService::socketForAddress(const QString &address) const
{
    for (auto it = m_links.cbegin(); it != m_links.cend(); ++it)
        if (it->address == address)
            return it.key();
    return nullptr;
}

void BluetoothService::connectToDevice(const QString &address)
{
    if (address.isEmpty() || socketForAddress(address) || m_connecting.contains(address))
        return;
    ensurePermission([this, address] {
        m_connecting.insert(address);
        emit devicesChanged();

        auto *socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);
#ifdef Q_OS_ANDROID
        // The default on Android is a secure socket, which needs a pairing the other side (e.g. a
        // desktop that is not "pairable") may never accept. Use a plain RFCOMM link instead.
        socket->setPreferredSecurityFlags(QBluetooth::Security::NoSecurity);
#endif
        auto failed = [this, socket, address](const QString &why) {
            if (!m_connecting.contains(address))
                return;
            m_connecting.remove(address);
            qWarning() << "Bluetooth: connect to" << address << "failed:" << why;
            if (!m_known.contains(address))   // do not spam the status for background reconnects
                setStatus(tr("Could not connect: %1").arg(why));
            socket->abort();
            socket->deleteLater();
            emit devicesChanged();
        };
        connect(socket, &QBluetoothSocket::connected, this, [this, socket, address] {
            m_connecting.remove(address);
            qInfo() << "Bluetooth: connected to" << address;
            adopt(socket);
        });
        connect(socket, &QBluetoothSocket::errorOccurred, this,
                [socket, failed](QBluetoothSocket::SocketError) { failed(socket->errorString()); });
        QTimer::singleShot(ConnectTimeoutMs, socket, [socket, address, failed, this] {
            if (m_connecting.contains(address))
                failed(tr("timed out (is LetsChat open on the other device?)"));
        });
        socket->connectToService(QBluetoothAddress(address),
                                 QBluetoothUuid(AppConfig::BLUETOOTH_SERVICE_UUID));
    });
}

void BluetoothService::disconnectDevice(const QString &address)
{
    if (QBluetoothSocket *s = socketForAddress(address))
        s->disconnectFromService();
}

void BluetoothService::adopt(QBluetoothSocket *socket)
{
    Link link;
    link.address = socket->peerAddress().toString();
    m_links.insert(socket, link);
    socket->disconnect(this);   // drop the connect-phase handlers

    auto *parser = new FrameParser(m_receiveDir, socket);
    m_links[socket].parser = parser;
    connect(socket, &QBluetoothSocket::readyRead, parser, [socket, parser] { parser->feed(socket->readAll()); });
    connect(parser, &FrameParser::frame, this,
            [this, socket](const QJsonObject &msg) { if (m_links.contains(socket)) handleFrame(socket, msg); });
    connect(parser, &FrameParser::failed, this, [this, socket](const QString &why) {
        qWarning() << "Bluetooth: dropping link:" << why;
        dropLink(socket);
    });
    connect(parser, &FrameParser::fileReceived, this,
            [this, socket](const QJsonObject &h, const QString &path) {
        const auto it = m_links.constFind(socket);
        if (it == m_links.constEnd() || it->userId.isEmpty())
            return;
        if (h["kind"].toString() == "avatar") {
            emit avatarReceived(it->userId, path, h["id"].toString());
            return;
        }
        QJsonObject header = h;                   // trust the link, not the frame, for the sender
        header["fromUserId"] = it->userId;
        header["alias"] = it->alias;
        emit fileReceived(path, header);
    });
    connect(socket, &QBluetoothSocket::disconnected, this, [this, socket] { dropLink(socket); });
    connect(socket, &QBluetoothSocket::errorOccurred, this, [this, socket](QBluetoothSocket::SocketError) {
        qWarning() << "Bluetooth: socket error" << socket->errorString();
        dropLink(socket);
    });

    sendFrame(socket, {{"type", "hello"},
                       {"v", AppConfig::PROTOCOL_VERSION},
                       {"userId", m_localUserId},
                       {"alias", m_localAlias},
                       {"deviceName", m_localDeviceName},
                       {"av", m_localAvatarHash}});
    emit devicesChanged();
}

void BluetoothService::dropLink(QBluetoothSocket *socket)
{
    if (!m_links.contains(socket))
        return;
    const Link link = m_links.take(socket);
    if (!link.userId.isEmpty() && m_byUser.value(link.userId) == socket) {
        m_byUser.remove(link.userId);
        emit peerDown(link.userId);
    }
    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
    emit devicesChanged();
}

void BluetoothService::sendFrame(QBluetoothSocket *socket, const QJsonObject &msg)
{
    socket->write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n");
}

void BluetoothService::handleFrame(QBluetoothSocket *socket, const QJsonObject &msg)
{
    const QString type = msg["type"].toString();
    Link &link = m_links[socket];

    if (type == "hello") {
        const QString userId = msg["userId"].toString();
        if (userId.isEmpty() || userId == m_localUserId) {
            dropLink(socket);
            return;
        }
        // Both sides may have dialled each other at the same moment; keep the first link.
        if (m_byUser.contains(userId) && m_byUser.value(userId) != socket) {
            qInfo() << "Bluetooth: duplicate link for" << userId << "- closing the new one";
            dropLink(socket);
            return;
        }
        link.userId = userId;
        link.alias = msg["alias"].toString();
        m_byUser.insert(userId, socket);
        rememberAddress(link.address);
        qInfo() << "Bluetooth: peer up" << link.alias << link.address;
        emit peerUp(userId, link.alias, msg["deviceName"].toString(), link.address, msg["av"].toString());
    } else if (type == "delete") {
        if (!link.userId.isEmpty())
            emit deleteReceived(link.userId, msg["target"].toString());
    } else if (type == "avatar_get") {
        QFile f(m_localAvatarPath);
        if (m_localAvatarPath.isEmpty() || !f.open(QIODevice::ReadOnly))
            return;
        const QByteArray bytes = f.readAll();
        const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex().left(12));
        sendFrame(socket, {{"type", "file"}, {"id", hash}, {"fromUserId", m_localUserId},
                           {"kind", "avatar"}, {"name", "avatar.jpg"}, {"size", bytes.size()}});
        socket->write(bytes);
    } else if (type == "text") {
        if (link.userId.isEmpty())
            return;     // ignore data before the hello
        emit textReceived(link.userId, link.alias, msg["id"].toString(), msg["content"].toString(),
                          msg["timestamp"].toString(),
                          msg.contains("reply") ? QString::fromUtf8(QJsonDocument(msg["reply"].toObject()).toJson(QJsonDocument::Compact)) : QString());
    }
}

bool BluetoothService::sendText(const QString &userId, const QString &messageId,
                                const QString &fromUserId, const QString &alias,
                                const QString &content, const QString &timestamp,
                                const QString &replyJson)
{
    QBluetoothSocket *s = m_byUser.value(userId, nullptr);
    if (!s || s->state() != QBluetoothSocket::SocketState::ConnectedState)
        return false;
    QJsonObject frame{{"type", "text"}, {"v", AppConfig::PROTOCOL_VERSION}, {"id", messageId},
                      {"fromUserId", fromUserId}, {"alias", alias}, {"content", content},
                      {"timestamp", timestamp}};
    if (!replyJson.isEmpty())
        frame["reply"] = QJsonDocument::fromJson(replyJson.toUtf8()).object();
    const QByteArray data = QJsonDocument(frame).toJson(QJsonDocument::Compact) + "\n";
    return s->write(data) == data.size();
}

bool BluetoothService::sendFile(const QString &userId, const QString &messageId,
                                const QString &fromUserId, const QString &alias,
                                const QString &kind, const QString &filePath,
                                const QString &fileName, qint64 size, const QString &timestamp,
                                const QString &caption, int durationMs, const QString &replyJson)
{
    QBluetoothSocket *s = m_byUser.value(userId, nullptr);
    if (!s || s->state() != QBluetoothSocket::SocketState::ConnectedState)
        return false;
    auto *file = new QFile(filePath, s);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        return false;
    }
    QJsonObject frame{{"type", "file"}, {"v", AppConfig::PROTOCOL_VERSION}, {"id", messageId},
                      {"fromUserId", fromUserId}, {"alias", alias}, {"kind", kind}, {"name", fileName},
                      {"size", double(size)}, {"timestamp", timestamp}, {"caption", caption},
                      {"duration", durationMs}};
    if (!replyJson.isEmpty())
        frame["reply"] = QJsonDocument::fromJson(replyJson.toUtf8()).object();
    s->write(QJsonDocument(frame).toJson(QJsonDocument::Compact) + "\n");
    auto *pump = new FilePump(s, file, s);
    connect(pump, &FilePump::finished, this,
            [this, messageId, pump](bool ok) { emit sendFinished(messageId, ok); pump->deleteLater(); });
    pump->start();
    return true;
}

void BluetoothService::requestAvatar(const QString &userId)
{
    if (QBluetoothSocket *s = m_byUser.value(userId, nullptr))
        sendFrame(s, {{"type", "avatar_get"}});
}

bool BluetoothService::sendDelete(const QString &userId, const QString &target, const QString &from)
{
    QBluetoothSocket *s = m_byUser.value(userId, nullptr);
    if (!s || s->state() != QBluetoothSocket::SocketState::ConnectedState)
        return false;
    sendFrame(s, {{"type", "delete"}, {"target", target}, {"fromUserId", from}});
    return true;
}

void BluetoothService::rememberAddress(const QString &address)
{
    if (m_known.contains(address))
        return;
    m_known.insert(address);
    QSettings().setValue("bluetooth/known", QStringList(m_known.begin(), m_known.end()));
}
