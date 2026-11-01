#include "ChatController.h"
#include "core/AppConfig.h"
#include "core/IdentityManager.h"
#include "network/BluetoothService.h"
#include "network/FrameParser.h"
#include "network/LanTracker.h"
#include "network/PeerManager.h"
#include "network/TransferService.h"
#include "notifications/NotificationManager.h"
#include "platform/BackgroundService.h"
#include "storage/DatabaseManager.h"
#include "utils/DeviceInfo.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QStandardPaths>
#include <QUuid>

ChatController::ChatController(IdentityManager *identity, PeerManager *peers,
                               TransferService *transfer, BluetoothService *bluetooth,
                               DatabaseManager *db, NotificationManager *notifier, LanTracker *lan,
                               BackgroundService *background, QObject *parent)
    : QObject(parent)
    , m_identity(identity), m_peers(peers), m_transfer(transfer), m_bluetooth(bluetooth)
    , m_db(db), m_notifier(notifier), m_lan(lan), m_background(background)
{
    m_messages.setCurrentUserId(m_identity->userId());

    connect(m_transfer, &TransferService::textReceived, this, &ChatController::onIncoming);
    connect(m_transfer, &TransferService::sendFinished, this, &ChatController::onSendFinished);
    connect(m_transfer, &TransferService::fileReceived, this, &ChatController::onIncomingFile);
    connect(m_bluetooth, &BluetoothService::fileReceived, this,
            [this](const QString &from, const QString &alias, const QString &id, const QString &kind,
                   const QString &name, qint64 size, const QString &path, const QString &ts) {
        onIncomingFile(from, alias, id, kind, name, size, path, ts, {});
    });
    // Bluetooth file sends report completion asynchronously
    connect(m_bluetooth, &BluetoothService::sendFinished, this,
            [this](const QString &id, bool ok) { onSendFinished(id, ok, ok ? QString() : tr("Bluetooth transfer failed")); });

    connect(m_bluetooth, &BluetoothService::textReceived, this,
            [this](const QString &from, const QString &alias, const QString &id,
                   const QString &content, const QString &ts) {
        onIncoming(from, alias, id, content, ts, {});
    });
    connect(m_bluetooth, &BluetoothService::peerUp, m_peers, &PeerManager::onBluetoothPeerUp);
    connect(m_bluetooth, &BluetoothService::peerDown, m_peers, &PeerManager::onBluetoothPeerDown);

    // Keep the header of the open chat (online dot, transport) in sync with the peer list.
    connect(m_peers, &PeerManager::peerOnline, this, [this](const QString &id) {
        if (id == m_currentPeerId) emit currentPeerChanged();
    });
    connect(m_peers, &PeerManager::peerOffline, this, [this](const QString &id) {
        if (id == m_currentPeerId) emit currentPeerChanged();
    });

    connect(m_lan, &LanTracker::joinedNewLan, this, [this] {
        m_peers->handleNetworkChange();
        if (m_currentPeerId.size()) emit currentPeerChanged();
    });
    connect(m_lan, &LanTracker::networkChanged, this, &ChatController::networkChanged);

    connect(m_identity, &IdentityManager::aliasChanged, this, [this] { if (m_started) applyLocalIdentity(); });
}

void ChatController::applyLocalIdentity()
{
    m_peers->setLocalIdentity(m_identity->userId(), m_identity->alias(), m_identity->deviceName());
    m_bluetooth->setLocalInfo(m_identity->userId(), m_identity->alias(), m_identity->deviceName());
}

void ChatController::startServices()
{
    if (m_started)
        return;
    m_started = true;
    applyLocalIdentity();
    m_peers->loadKnownPeers();
    // Show the last message in the list for people we chatted with before.
    // (done lazily per peer when first loaded; cheap enough for a LAN-sized list)
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(m_peers->model());
    for (int i = 0; model && i < model->rowCount(); ++i) {
        const QString id = model->index(i, 0).data(PeerListModel::UserIdRole).toString();
        const QVariantMap last = m_db->lastMessage(DatabaseManager::conversationId(m_identity->userId(), id));
        if (!last.isEmpty())
            m_peers->peers()->setLastMessage(
                id, MessageModel::preview(last["messageType"].toInt(), last["content"].toString()),
                last["timestamp"].toString());
    }

    m_peers->startScanning();
    m_transfer->startServer();
    m_lan->start();
    m_bluetooth->start();
    m_background->start();
    emit networkChanged();
}

QString ChatController::currentPeerName() const
{
    const Peer *p = m_peers->peers()->find(m_currentPeerId);
    return p ? p->displayName() : QString();
}

bool ChatController::currentPeerOnline() const
{
    const Peer *p = m_peers->peers()->find(m_currentPeerId);
    return p && p->isOnline();
}

QString ChatController::currentPeerTransport() const
{
    const Peer *p = m_peers->peers()->find(m_currentPeerId);
    return p ? p->transport() : QString();
}

QString ChatController::localIp() const { return m_lan->localIp(); }
bool ChatController::lanConnected() const { return m_lan->isConnected(); }

void ChatController::selectPeer(const QString &userId)
{
    if (userId.isEmpty() || !m_peers->peers()->contains(userId))
        return;
    m_currentPeerId = userId;
    m_peers->peers()->clearUnread(userId);
    m_messages.setMessages(m_db->getMessages(
        DatabaseManager::conversationId(m_identity->userId(), userId)));
    emit currentPeerChanged();
    emit messageAdded();
}

void ChatController::closeChat()
{
    if (m_currentPeerId.isEmpty())
        return;
    m_currentPeerId.clear();
    m_messages.clear();
    emit currentPeerChanged();
}

// ---------------------------------------------------------------------------------- sending

bool ChatController::sendViaBluetooth(const QString &peerId, const QString &id, const QString &text,
                                      const QString &ts)
{
    return m_bluetooth->sendText(peerId, id, m_identity->userId(), m_identity->alias(), text, ts);
}

void ChatController::sendMessage(const QString &rawText)
{
    const QString text = rawText.trimmed();
    const Peer *peer = m_peers->peers()->find(m_currentPeerId);
    if (text.isEmpty() || !peer)
        return;

    const QString me = m_identity->userId();
    const QString peerId = peer->userId;
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Prefer Wi-Fi/LAN; fall back to Bluetooth.  A peer that was seen on the LAN before may
    // be reachable even if its last announcement has expired, so try the stored address too.
    const bool tryLan = !peer->ipAddress.isEmpty() && m_lan->isConnected()
                        && (peer->lanOnline || !peer->btOnline);
    int status = 0;
    if (!tryLan) {
        if (sendViaBluetooth(peerId, id, text, ts)) {
            status = 1;
        } else {
            status = 2;
            emit notice(tr("%1 is not reachable right now").arg(peer->displayName()));
        }
    }

    m_messages.addMessage(id, me, peerId, text, 0, ts, status);
    m_db->saveMessage(id, me, peerId, text, 0, ts, status);
    m_peers->peers()->setLastMessage(peerId, text, ts);
    emit messageAdded();

    if (tryLan) {
        Outgoing out;
        out.peerId = peerId;
        out.text = text;
        out.ts = ts;
        m_inflight.insert(id, out);
        m_transfer->sendText(peer->ipAddress, id, me, m_identity->alias(), text, ts);
    }
}

bool ChatController::resendViaBluetooth(const Outgoing &o, const QString &id)
{
    if (o.type == 0)
        return sendViaBluetooth(o.peerId, id, o.text, o.ts);
    return m_bluetooth->sendFile(o.peerId, id, m_identity->userId(), m_identity->alias(), o.kind,
                                 o.path, o.name, o.size, o.ts);
}

void ChatController::onSendFinished(const QString &messageId, bool ok, const QString &error)
{
    auto it = m_inflight.find(messageId);
    if (it == m_inflight.end())
        return;
    Outgoing out = it.value();
    m_inflight.erase(it);
    int status = ok ? 1 : 2;
    if (!ok && !out.viaBluetooth && resendViaBluetooth(out, messageId)) {
        // LAN failed but a Bluetooth link was up. Text is written immediately; files report
        // their completion through BluetoothService::sendFinished.
        if (out.type != 0) {
            out.viaBluetooth = true;
            m_inflight.insert(messageId, out);
            return;
        }
        status = 1;
    } else if (!ok) {
        status = 2;
    }
    if (status == 2) {
        const Peer *p = m_peers->peers()->find(out.peerId);
        emit notice(tr("Message to %1 was not delivered").arg(p ? p->displayName() : tr("peer")));
        qWarning() << "Delivery failed:" << error;
    }
    m_messages.setStatus(messageId, status);
    m_db->updateMessageStatus(messageId, status);
}

void ChatController::sendAttachment(const QUrl &url)
{
    const Peer *peer = m_peers->peers()->find(m_currentPeerId);
    if (!peer || !url.isValid())
        return;
    const QString src = url.isLocalFile() ? url.toLocalFile() : url.toString();

    QFile in(src);
    if (!in.open(QIODevice::ReadOnly)) {
        emit notice(tr("Could not open the selected file"));
        return;
    }
    const qint64 size = in.size();
    if (size <= 0 || size > AppConfig::MAX_FILE_BYTES) {
        emit notice(size <= 0 ? tr("The file is empty")
                              : tr("Files larger than %1 MB are not supported").arg(AppConfig::MAX_FILE_BYTES / 1024 / 1024));
        return;
    }
    QString name = QFileInfo(url.isLocalFile() ? src : url.fileName()).fileName();
    if (name.isEmpty())
        name = tr("file");
    QImageReader reader(&in);
    const bool isImage = reader.canRead();
    in.close();

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/sent";
    QDir().mkpath(dir);
    const QString copy = dir + "/" + id.left(8) + "_" + FrameParser::sanitizeFileName(name);
    if (!QFile::copy(src, copy)) {
        emit notice(tr("Could not read the selected file"));
        return;
    }
    // Images picked from Android providers can lack an extension; give the copy one for previews.
    const QString kind = isImage ? "image" : "file";
    const int type = isImage ? 1 : 2;

    const QString me = m_identity->userId();
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
    Outgoing out{peer->userId, {}, ts, type, kind, copy, name, size};
    const bool tryLan = !peer->ipAddress.isEmpty() && m_lan->isConnected()
                        && (peer->lanOnline || !peer->btOnline);
    int status = 0;
    if (!tryLan) {
        if (resendViaBluetooth(out, id)) {
            out.viaBluetooth = true;
        } else {
            status = 2;
            emit notice(tr("%1 is not reachable right now").arg(peer->displayName()));
        }
    }
    const QString content = MessageModel::attachmentJson(name, size, copy);
    m_messages.addMessage(id, me, peer->userId, content, type, ts, status);
    m_db->saveMessage(id, me, peer->userId, content, type, ts, status);
    m_peers->peers()->setLastMessage(peer->userId, MessageModel::preview(type, content), ts);
    emit messageAdded();

    if (status == 0) {
        m_inflight.insert(id, out);
        if (tryLan)
            m_transfer->sendFile(peer->ipAddress, id, me, m_identity->alias(), kind, copy, name, size, ts);
    }
}

// -------------------------------------------------------------------------------- receiving

void ChatController::onIncoming(const QString &fromUserId, const QString &alias,
                                const QString &messageId, const QString &content,
                                const QString &timestamp, const QString &viaIp)
{
    if (fromUserId.isEmpty() || fromUserId == m_identity->userId() || content.isEmpty())
        return;
    if (!messageId.isEmpty() && m_db->messageExists(messageId))
        return;   // duplicate (e.g. sender retried over the other transport)
    if (!viaIp.isEmpty())
        m_peers->onLanPeerSeen(fromUserId, alias, viaIp);
    const QString id = messageId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : messageId;
    const QString ts = QDateTime::fromString(timestamp, Qt::ISODate).isValid()
                           ? timestamp : QDateTime::currentDateTime().toString(Qt::ISODate);
    storeIncoming(fromUserId, alias, id, 0, content, ts);
}

void ChatController::onIncomingFile(const QString &fromUserId, const QString &alias,
                                    const QString &messageId, const QString &kind,
                                    const QString &fileName, qint64 size, const QString &savedPath,
                                    const QString &timestamp, const QString &viaIp)
{
    if (fromUserId.isEmpty() || fromUserId == m_identity->userId())
        return;
    if (!messageId.isEmpty() && m_db->messageExists(messageId))
        return;
    if (!viaIp.isEmpty())
        m_peers->onLanPeerSeen(fromUserId, alias, viaIp);
    const QString id = messageId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : messageId;
    const QString ts = QDateTime::fromString(timestamp, Qt::ISODate).isValid()
                           ? timestamp : QDateTime::currentDateTime().toString(Qt::ISODate);
    storeIncoming(fromUserId, alias, id, kind == "image" ? 1 : 2,
                  MessageModel::attachmentJson(fileName, size, savedPath), ts);
}

void ChatController::storeIncoming(const QString &fromUserId, const QString &alias, const QString &id,
                                   int type, const QString &content, const QString &ts)
{
    const QString me = m_identity->userId();
    const QString text = MessageModel::preview(type, content);
    m_db->saveMessage(id, fromUserId, me, content, type, ts, 1);
    m_peers->peers()->setLastMessage(fromUserId, text, ts);

    const bool viewing = (fromUserId == m_currentPeerId);
    if (viewing) {
        m_messages.addMessage(id, fromUserId, me, content, type, ts, 1);
        emit messageAdded();
    }
    const bool appActive = QGuiApplication::applicationState() == Qt::ApplicationActive;
    if (!viewing || !appActive) {
        if (!viewing)
            m_peers->peers()->incrementUnread(fromUserId);
        const Peer *p = m_peers->peers()->find(fromUserId);
        m_notifier->showMessageNotification(p ? p->displayName() : alias, text, fromUserId);
    }
}
