#include "TransferService.h"
#include "core/AppConfig.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>
#include <QDateTime>

TransferService::TransferService(QObject *parent)
    : QObject(parent)
    , m_tcpServer(new QTcpServer(this))
{
    connect(m_tcpServer, &QTcpServer::newConnection, this, &TransferService::onNewConnection);
}

TransferService::~TransferService()
{
    stopServer();
}

void TransferService::startServer()
{
    if (!m_tcpServer->isListening()) {
        if (m_tcpServer->listen(QHostAddress::Any, AppConfig::TRANSFER_PORT)) {
            qDebug() << "Transfer server started on port" << AppConfig::TRANSFER_PORT;
        } else {
            qDebug() << "Failed to start transfer server:" << m_tcpServer->errorString();
        }
    }
}

void TransferService::stopServer()
{
    if (m_tcpServer->isListening()) {
        m_tcpServer->close();
    }
    
    for (auto it = m_transfers.begin(); it != m_transfers.end(); ++it) {
        if (it->socket) {
            it->socket->disconnectFromHost();
        }
        if (it->file) {
            it->file->close();
            delete it->file;
        }
    }
    m_transfers.clear();
}

void TransferService::sendTextMessage(const QString &targetIp, quint16 port,
                                     const QString &message, const QString &fromUserId)
{
    QTcpSocket *socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, [this, socket, message, fromUserId]() {
        QJsonObject msg;
        msg["type"] = "text";
        msg["fromUserId"] = fromUserId;
        msg["content"] = message;
        msg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        QJsonDocument doc(msg);
        QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
        socket->write(data);
    });
    
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    
    socket->connectToHost(targetIp, port);
}

void TransferService::sendFile(const QString &targetIp, quint16 port,
                              const QString &filePath, const QString &fromUserId)
{
    Q_UNUSED(targetIp);
    Q_UNUSED(port);
    Q_UNUSED(filePath);
    Q_UNUSED(fromUserId);
    // File transfer implementation
    qDebug() << "File transfer requested";
}

void TransferService::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &TransferService::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &TransferService::onDisconnected);
    }
}

void TransferService::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    QByteArray data = socket->readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject message = doc.object();
        QString type = message["type"].toString();
        
        if (type == "text") {
            QString fromUserId = message["fromUserId"].toString();
            QString content = message["content"].toString();
            QString senderIp = socket->peerAddress().toString();
            emit textMessageReceived(fromUserId, content, senderIp);
            socket->disconnectFromHost();
        }
    }
}

void TransferService::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        m_transfers.remove(socket);
        socket->deleteLater();
    }
}
