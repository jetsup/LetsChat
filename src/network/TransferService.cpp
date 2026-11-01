#include "TransferService.h"
#include "FrameParser.h"
#include "core/AppConfig.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QSharedPointer>
#include <QCryptographicHash>

namespace { constexpr int IdleTimeoutMs = 10000; }

static QString ipOf(const QHostAddress &a)
{
    bool ok = false;
    const quint32 v4 = a.toIPv4Address(&ok);
    return ok ? QHostAddress(v4).toString() : a.toString();
}

TransferService::TransferService(const QString &receiveDir, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_receiveDir(receiveDir)
    , m_retryTimer(new QTimer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &TransferService::onNewConnection);
    m_retryTimer->setInterval(5000);
    connect(m_retryTimer, &QTimer::timeout, this, &TransferService::startServer);
}

TransferService::~TransferService() { stopServer(); }

void TransferService::startServer()
{
    if (m_server->isListening())
        return;
    if (m_server->listen(QHostAddress::AnyIPv4, AppConfig::TRANSFER_PORT)) {
        m_retryTimer->stop();
        qInfo() << "Transfer: listening on TCP" << AppConfig::TRANSFER_PORT;
    } else {
        qWarning() << "Transfer: cannot listen on" << AppConfig::TRANSFER_PORT << ":"
                   << m_server->errorString() << "- will retry";
        m_retryTimer->start();
    }
}

void TransferService::stopServer()
{
    m_retryTimer->stop();
    if (m_server->isListening())
        m_server->close();
    const auto sockets = m_incoming;
    for (QTcpSocket *s : sockets)
        s->abort();
}

// ------------------------------------------------------------------------------- receiving

void TransferService::onNewConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        m_incoming.append(socket);
        auto *parser = new FrameParser(m_receiveDir, socket);
        auto *idle = new QTimer(socket);
        idle->setSingleShot(true);
        idle->setInterval(IdleTimeoutMs);
        connect(idle, &QTimer::timeout, socket, &QTcpSocket::abort);
        idle->start();

        auto ack = [socket](const QString &id) {
            socket->write(QJsonDocument(QJsonObject{{"type", "ack"}, {"id", id}})
                              .toJson(QJsonDocument::Compact) + "\n");
            socket->disconnectFromHost();
        };

        connect(socket, &QTcpSocket::readyRead, parser, [socket, parser, idle] {
            idle->start();
            parser->feed(socket->readAll());
        });
        connect(parser, &FrameParser::failed, socket, [socket](const QString &why) {
            qWarning() << "Transfer: dropping connection:" << why;
            socket->abort();
        });
        connect(parser, &FrameParser::frame, this, [this, socket, ack](const QJsonObject &msg) {
            const QString type = msg["type"].toString();
            if (type == "delete") {
                emit deleteReceived(msg["fromUserId"].toString(), msg["target"].toString());
                ack(msg["id"].toString());
                return;
            }
            if (type == "avatar_get") {          // a peer wants our profile picture
                auto *file = new QFile(m_avatarPath, socket);
                if (m_avatarPath.isEmpty() || !file->open(QIODevice::ReadOnly)) {
                    socket->disconnectFromHost();
                    return;
                }
                const QString hash = QString::fromLatin1(
                    QCryptographicHash::hash(file->readAll(), QCryptographicHash::Sha1).toHex().left(12));
                file->seek(0);
                socket->write(QJsonDocument(QJsonObject{{"type", "file"}, {"id", hash},
                                                        {"fromUserId", m_localUserId}, {"kind", "avatar"},
                                                        {"name", "avatar.jpg"}, {"size", double(file->size())}})
                                  .toJson(QJsonDocument::Compact) + "\n");
                auto *pump = new FilePump(socket, file, socket);
                connect(pump, &FilePump::finished, socket, [socket] { socket->disconnectFromHost(); });
                pump->start();
                return;
            }
            if (type != "text")
                return;
            const QString id = msg["id"].toString();
            emit textReceived(msg["fromUserId"].toString(), msg["alias"].toString(), id,
                              msg["content"].toString(), msg["timestamp"].toString(),
                              ipOf(socket->peerAddress()),
                              msg.contains("reply") ? QString::fromUtf8(QJsonDocument(msg["reply"].toObject()).toJson(QJsonDocument::Compact)) : QString());
            ack(id);
        });
        connect(parser, &FrameParser::fileReceived, this,
                [this, socket, ack](const QJsonObject &h, const QString &path) {
            emit fileReceived(path, h, ipOf(socket->peerAddress()));
            ack(h["id"].toString());
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            m_incoming.removeAll(socket);
            socket->deleteLater();
        });
    }
}

// --------------------------------------------------------------------------------- sending

static QJsonObject withReply(QJsonObject frame, const QString &replyJson)
{
    if (!replyJson.isEmpty())
        frame["reply"] = QJsonDocument::fromJson(replyJson.toUtf8()).object();
    return frame;
}

void TransferService::sendText(const QString &ip, const QString &id, const QString &from,
                               const QString &alias, const QString &content, const QString &ts,
                               const QString &replyJson)
{
    startSend(ip, id, withReply({{"type", "text"}, {"v", AppConfig::PROTOCOL_VERSION}, {"id", id},
                                 {"fromUserId", from}, {"alias", alias}, {"content", content},
                                 {"timestamp", ts}}, replyJson), {});
}

void TransferService::sendFile(const QString &ip, const QString &id, const QString &from,
                               const QString &alias, const QString &kind, const QString &filePath,
                               const QString &fileName, qint64 size, const QString &ts,
                               const QString &caption, int durationMs, const QString &replyJson)
{
    startSend(ip, id, withReply({{"type", "file"}, {"v", AppConfig::PROTOCOL_VERSION}, {"id", id},
                                 {"fromUserId", from}, {"alias", alias}, {"kind", kind},
                                 {"name", fileName}, {"size", double(size)}, {"timestamp", ts},
                                 {"caption", caption}, {"duration", durationMs}}, replyJson), filePath);
}

void TransferService::startSend(const QString &ip, const QString &id, const QJsonObject &header,
                                const QString &filePath)
{
    auto *socket = new QTcpSocket(this);
    auto done = QSharedPointer<bool>::create(false);
    auto buffer = QSharedPointer<QByteArray>::create();
    auto *watchdog = new QTimer(socket);   // fires if nothing moves for IdleTimeoutMs
    watchdog->setSingleShot(true);
    watchdog->setInterval(AppConfig::SEND_TIMEOUT_MS * 2);

    auto finish = [this, socket, done, id](bool ok, const QString &err) {
        if (*done)
            return;
        *done = true;
        if (!ok)
            qWarning() << "Transfer: send failed:" << err;
        emit sendFinished(id, ok, err);
        socket->abort();
        socket->deleteLater();
    };

    QFile *file = nullptr;
    if (!filePath.isEmpty()) {
        file = new QFile(filePath, socket);
        if (!file->open(QIODevice::ReadOnly)) {
            QTimer::singleShot(0, this, [finish] { finish(false, QObject::tr("Cannot read the file")); });
            return;
        }
    }

    connect(socket, &QTcpSocket::connected, this, [=, this] {
        watchdog->start();
        socket->write(QJsonDocument(header).toJson(QJsonDocument::Compact) + "\n");
        if (file) {
            auto *pump = new FilePump(socket, file, socket);
            connect(pump, &FilePump::progress, socket, [watchdog] { watchdog->start(); });
            connect(pump, &FilePump::finished, this, [finish](bool ok) {
                if (!ok) finish(false, QObject::tr("Read error"));
            });
            pump->start();
        }
    });
    connect(socket, &QTcpSocket::readyRead, this, [=, this] {
        watchdog->start();
        *buffer += socket->readAll();
        const int nl = buffer->indexOf('\n');
        if (nl < 0)
            return;
        const QJsonObject ack = QJsonDocument::fromJson(buffer->left(nl)).object();
        if (ack["type"].toString() == "ack" && ack["id"].toString() == id)
            finish(true, {});
    });
    connect(socket, &QTcpSocket::errorOccurred, this,
            [=, this](QAbstractSocket::SocketError) { finish(false, socket->errorString()); });
    connect(socket, &QTcpSocket::disconnected, this,
            [=, this] { finish(false, tr("Connection closed before delivery was confirmed")); });
    connect(watchdog, &QTimer::timeout, this, [=, this] { finish(false, tr("Timed out")); });
    // connecting to an unreachable host must not wait for the long idle timeout
    QTimer::singleShot(AppConfig::SEND_TIMEOUT_MS, socket, [=, this] {
        if (socket->state() != QAbstractSocket::ConnectedState)
            finish(false, tr("Timed out"));
    });

    socket->connectToHost(ip, AppConfig::TRANSFER_PORT);
}

void TransferService::sendDelete(const QString &ip, const QString &target, const QString &from)
{
    startSend(ip, "del:" + target,
              {{"type", "delete"}, {"v", AppConfig::PROTOCOL_VERSION}, {"id", "del:" + target},
               {"target", target}, {"fromUserId", from}}, {});
}

void TransferService::requestAvatar(const QString &ip, const QString &peerUserId)
{
    auto *socket = new QTcpSocket(this);
    auto *parser = new FrameParser(m_receiveDir, socket);
    auto *timeout = new QTimer(socket);
    timeout->setSingleShot(true);
    timeout->start(IdleTimeoutMs);
    connect(timeout, &QTimer::timeout, socket, &QTcpSocket::abort);
    connect(socket, &QTcpSocket::connected, socket, [this, socket] {
        socket->write(QJsonDocument(QJsonObject{{"type", "avatar_get"}, {"fromUserId", m_localUserId}})
                          .toJson(QJsonDocument::Compact) + "\n");
    });
    connect(socket, &QTcpSocket::readyRead, parser, [socket, parser] { parser->feed(socket->readAll()); });
    connect(parser, &FrameParser::fileReceived, this,
            [this, socket, peerUserId](const QJsonObject &h, const QString &path) {
        if (h["kind"].toString() == "avatar")
            emit avatarReceived(peerUserId, path, h["id"].toString());
        socket->disconnectFromHost();
    });
    connect(parser, &FrameParser::failed, socket, &QTcpSocket::abort);
    connect(socket, &QTcpSocket::errorOccurred, socket, [socket](QAbstractSocket::SocketError) { socket->abort(); });
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    socket->connectToHost(ip, AppConfig::TRANSFER_PORT);
}
