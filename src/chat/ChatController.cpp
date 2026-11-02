#include "ChatController.h"
#include "core/AppConfig.h"
#include "core/IdentityManager.h"
#include "network/BluetoothService.h"
#include "network/FrameParser.h"
#include "models/PeerListModel.h"
#include "network/LanTracker.h"
#include "network/PeerManager.h"
#include "network/TransferService.h"
#include "notifications/NotificationManager.h"
#include "platform/BackgroundService.h"
#include "security/SecurityManager.h"
#include "storage/DatabaseManager.h"
#include "utils/DeviceInfo.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QClipboard>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

ChatController::ChatController(IdentityManager *identity, PeerManager *peers,
                               TransferService *transfer, BluetoothService *bluetooth,
                               DatabaseManager *db, NotificationManager *notifier, LanTracker *lan,
                               BackgroundService *background, SecurityManager *security,
                               QObject *parent)
    : QObject(parent)
    , m_identity(identity), m_peers(peers), m_transfer(transfer), m_bluetooth(bluetooth)
    , m_db(db), m_notifier(notifier), m_lan(lan), m_background(background), m_security(security)
{
    m_messages.setCurrentUserId(m_identity->userId());

    connect(m_transfer, &TransferService::textReceived, this, &ChatController::onIncoming);
    connect(m_transfer, &TransferService::sendFinished, this, &ChatController::onSendFinished);
    for (QObject *src : {static_cast<QObject *>(m_transfer), static_cast<QObject *>(m_bluetooth)})
        connect(src, SIGNAL(securityAlert(QString)), m_security, SLOT(noteAlert(QString)));
    connect(m_security, &SecurityManager::keyChanged, this, [this](const QString &id) {
        const Peer *p = m_peers->peers()->find(id);
        emit notice(tr("Security warning: the safety number of %1 changed. Messages are blocked until you review it in Contact info.")
                        .arg(p ? p->displayName() : tr("a contact")));
        if (id == m_currentPeerId)
            emit currentPeerChanged();
    });
    connect(m_security, &SecurityManager::peerSecurityChanged, this, [this](const QString &id) {
        if (id == m_currentPeerId)
            emit currentPeerChanged();
        else
            flushQueue(id);        // a key was accepted: deliver what was waiting
    });
    connect(m_transfer, &TransferService::fileReceived, this, &ChatController::onIncomingFile);
    connect(m_bluetooth, &BluetoothService::fileReceived, this,
            [this](const QString &path, const QJsonObject &header) { onIncomingFile(path, header, {}); });
    // Bluetooth file sends report completion asynchronously
    connect(m_bluetooth, &BluetoothService::sendFinished, this,
            [this](const QString &id, bool ok) { onSendFinished(id, ok, ok ? QString() : tr("Bluetooth transfer failed")); });

    connect(m_bluetooth, &BluetoothService::textReceived, this,
            [this](const QString &from, const QString &alias, const QString &id,
                   const QString &content, const QString &ts, const QString &reply) {
        onIncoming(from, alias, id, content, ts, {}, reply);
    });
    connect(m_bluetooth, &BluetoothService::peerUp, m_peers, &PeerManager::onBluetoothPeerUp);
    connect(m_bluetooth, &BluetoothService::deleteReceived, this, &ChatController::onDeleteReceived);
    connect(m_transfer, &TransferService::deleteReceived, this, &ChatController::onDeleteReceived);
    connect(m_bluetooth, &BluetoothService::avatarReceived, this, &ChatController::onAvatarReceived);
    connect(m_transfer, &TransferService::avatarReceived, this, &ChatController::onAvatarReceived);
    connect(m_peers, &PeerManager::avatarNeeded, this, &ChatController::requestAvatar);
    connect(m_identity, &IdentityManager::avatarChanged, this, &ChatController::publishLocalAvatar);
    connect(m_bluetooth, &BluetoothService::peerDown, m_peers, &PeerManager::onBluetoothPeerDown);

    // Keep the header of the open chat (online dot, transport) in sync with the peer list.
    connect(m_peers, &PeerManager::peerOnline, this, [this](const QString &id) {
        if (id == m_currentPeerId) emit currentPeerChanged();
        flushQueue(id);   // deliver everything that piled up while the peer was away/frozen
        flushDeletes(id);
    });
    connect(m_peers, &PeerManager::peerChanged, this, [this](const QString &id) {
        if (id == m_currentPeerId) emit currentPeerChanged();   // nickname / picture / state changed
    });
    connect(m_peers, &PeerManager::peerOffline, this, [this](const QString &id) {
        if (id == m_currentPeerId) emit currentPeerChanged();
    });

    connect(m_lan, &LanTracker::joinedNewLan, this, [this] {
        m_peers->handleNetworkChange();
        if (m_currentPeerId.size()) emit currentPeerChanged();
    });
    connect(m_lan, &LanTracker::networkChanged, this, &ChatController::networkChanged);

    // Retry queued messages when the app comes back to the foreground and every 20 s otherwise.
    m_retryTimer = new QTimer(this);
    m_retryTimer->setInterval(20000);
    connect(m_retryTimer, &QTimer::timeout, this, &ChatController::flushAll);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState st) {
        if (st == Qt::ApplicationActive && m_started)
            flushAll();
        m_lan->setBackground(st != Qt::ApplicationActive);
        m_peers->setBackground(st != Qt::ApplicationActive);
    });

    connect(m_identity, &IdentityManager::aliasChanged, this, [this] { if (m_started) applyLocalIdentity(); });
}

void ChatController::applyLocalIdentity()
{
    m_peers->setLocalIdentity(m_identity->userId(), m_identity->alias(), m_identity->deviceName(),
                              m_identity->avatarHash());
    m_bluetooth->setLocalInfo(m_identity->userId(), m_identity->alias(), m_identity->deviceName(),
                              m_identity->avatarHash(), m_identity->avatarPath());
    m_transfer->setLocalUser(m_identity->userId());
    m_transfer->setLocalAvatar(m_identity->avatarPath());
}

void ChatController::startServices()
{
    if (m_started)
        return;
    m_started = true;
    applyLocalIdentity();
    m_peers->loadKnownPeers();
    m_db->requeueStale(m_identity->userId());
    m_retryTimer->start();
    // Show the last message in the list for people we chatted with before.
    // (done lazily per peer when first loaded; cheap enough for a LAN-sized list)
    // Iterate the *source* model: the sorted view re-orders itself every time a preview is set,
    // which used to make this loop skip people.
    PeerListModel *source = m_peers->peers();
    QStringList ids;
    for (int i = 0; i < source->rowCount(); ++i)
        ids << source->index(i, 0).data(PeerListModel::UserIdRole).toString();
    for (const QString &id : ids) {
        const QVariantMap last = m_db->lastMessage(DatabaseManager::conversationId(m_identity->userId(), id));
        if (!last.isEmpty())
            source->setLastMessage(
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
    if (userId != m_currentPeerId)
        clearReply();                       // a reply belongs to one conversation
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
    clearReply();
    m_currentPeerId.clear();
    m_messages.clear();
    emit currentPeerChanged();
}

// ---------------------------------------------------------------------------------- sending

bool ChatController::sendViaBluetooth(const QString &peerId, const QString &id, const QString &text,
                                      const QString &ts, const QString &reply)
{
    return m_bluetooth->sendText(peerId, id, m_identity->userId(), m_identity->alias(), text, ts, reply);
}

bool ChatController::resendViaBluetooth(const Outgoing &o, const QString &id)
{
    if (o.type == 0)
        return sendViaBluetooth(o.peerId, id, o.text, o.ts, o.reply);
    return m_bluetooth->sendFile(o.peerId, id, m_identity->userId(), m_identity->alias(), o.kind,
                                 o.path, o.name, o.size, o.ts, o.caption, o.durationMs, o.reply);
}

// Picks a transport for one outgoing message.  Wi-Fi is preferred; a peer that was seen on the LAN
// before may still be reachable after its last announcement expired, so its stored address is tried.
ChatController::Route ChatController::dispatch(const QString &id, Outgoing out)
{
    const Peer *peer = m_peers->peers()->find(out.peerId);
    if (!peer)
        return Route::Unreachable;
    const QString me = m_identity->userId();
    if (m_security->isBlocked(out.peerId))
        return Route::Blocked;

    const bool tryLan = !peer->ipAddress.isEmpty() && m_lan->isConnected()
                        && (peer->lanOnline || !peer->btOnline);
    if (tryLan) {
        m_inflight.insert(id, out);
        if (out.type == 0)
            m_transfer->sendText(peer->ipAddress, out.peerId, id, me, m_identity->alias(), out.text, out.ts, out.reply);
        else
            m_transfer->sendFile(peer->ipAddress, out.peerId, id, me, m_identity->alias(), out.kind, out.path,
                                 out.name, out.size, out.ts, out.caption, out.durationMs, out.reply);
        return Route::InFlight;
    }
    if (m_bluetooth->isLinked(out.peerId)) {
        if (out.type == 0)
            return sendViaBluetooth(out.peerId, id, out.text, out.ts, out.reply) ? Route::Sent : Route::Unreachable;
        out.viaBluetooth = true;
        if (m_bluetooth->sendFile(out.peerId, id, me, m_identity->alias(), out.kind, out.path,
                                  out.name, out.size, out.ts, out.caption, out.durationMs, out.reply)) {
            m_inflight.insert(id, out);
            return Route::InFlight;
        }
    }
    return Route::Unreachable;
}

void ChatController::sendMessage(const QString &rawText)
{
    sendTextTo(m_currentPeerId, rawText.trimmed(), true);
}

void ChatController::sendTextTo(const QString &peerId, const QString &text, bool quoteReply)
{
    const Peer *peer = m_peers->peers()->find(peerId);
    if (text.isEmpty() || !peer)
        return;

    const QString me = m_identity->userId();
    const QString name = peer->displayName();
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
    const bool visible = (peerId == m_currentPeerId);

    const QString reply = quoteReply ? takeReply() : QString();
    if (visible)
        m_messages.addMessage(id, me, peerId, text, 0, ts, 0, reply);
    m_db->saveMessage(id, me, peerId, text, 0, ts, 0, reply);
    m_peers->peers()->setLastMessage(peerId, text, ts);
    if (visible)
        emit messageAdded();

    Outgoing out;
    out.peerId = peerId;
    out.text = text;
    out.ts = ts;
    out.reply = reply;
    finishDispatch(id, name, dispatch(id, out));
}

// Updates the stored status after dispatch(): InFlight is resolved later by onSendFinished().
void ChatController::finishDispatch(const QString &id, const QString &peerName, Route route)
{
    if (route == Route::InFlight)
        return;
    if (route == Route::Blocked) {
        m_messages.setStatus(id, 2);
        m_db->updateMessageStatus(id, 2);
        emit notice(tr("Not sent: %1's safety number changed. Review it in Contact info.").arg(peerName));
        return;
    }
    const int status = route == Route::Sent ? 1 : 3;
    m_messages.setStatus(id, status);
    m_db->updateMessageStatus(id, status);
    if (status == 3)
        emit notice(tr("%1 is offline – the message will be sent when they are back").arg(peerName));
}

void ChatController::onSendFinished(const QString &messageId, bool ok, const QString &error)
{
    if (messageId.startsWith("del:")) {              // result of a "delete for everyone" request
        m_deleteInflight.remove(messageId);
        if (ok)
            m_db->removePendingDelete(messageId.mid(4));   // otherwise it stays queued
        return;
    }
    auto it = m_inflight.find(messageId);
    if (it == m_inflight.end())
        return;
    Outgoing out = it.value();
    m_inflight.erase(it);

    int status = ok ? 1 : 3;   // a failure is not final: the message stays queued and is retried
    if (!ok && !out.viaBluetooth && m_bluetooth->isLinked(out.peerId)) {
        out.viaBluetooth = true;
        if (out.type == 0 && sendViaBluetooth(out.peerId, messageId, out.text, out.ts, out.reply)) {
            status = 1;
        } else if (out.type != 0 && resendViaBluetooth(out, messageId)) {
            m_inflight.insert(messageId, out);   // completion is reported by BluetoothService
            return;
        }
    }
    if (!ok && status == 3)
        qInfo() << "Delivery postponed:" << error;
    m_messages.setStatus(messageId, status);
    m_db->updateMessageStatus(messageId, status);
}

void ChatController::sendAttachment(const QUrl &url, const QString &caption)
{
    if (!m_peers->peers()->find(m_currentPeerId) || !url.isValid())
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
    queueFile(src, name, size, isImage ? "image" : "file", isImage ? caption.trimmed() : QString(), 0);
}

void ChatController::sendVoice(const QUrl &url, int durationMs)
{
    if (!m_peers->peers()->find(m_currentPeerId) || !url.isLocalFile())
        return;
    const QString src = url.toLocalFile();
    const qint64 size = QFileInfo(src).size();
    if (size <= 0) {
        emit notice(tr("The recording is empty"));
        return;
    }
    queueFile(src, tr("Voice message") + ".m4a", size, "voice", {}, durationMs);
    QFile::remove(src);   // queueFile() keeps its own copy
}

// Copies the file into the app's "sent" folder, shows it in the chat and hands it to a transport.
void ChatController::queueFile(const QString &src, const QString &name, qint64 size, const QString &kind,
                               const QString &caption, int durationMs, const QString &targetPeerId,
                               bool quoteReply)
{
    const QString target = targetPeerId.isEmpty() ? m_currentPeerId : targetPeerId;
    const Peer *peer = m_peers->peers()->find(target);
    if (!peer)
        return;
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/sent";
    QDir().mkpath(dir);
    const QString copy = dir + "/" + id.left(8) + "_" + FrameParser::sanitizeFileName(name);
    if (!QFile::copy(src, copy)) {
        emit notice(tr("Could not read the selected file"));
        return;
    }
    const int type = kind == "image" ? 1 : kind == "voice" ? 3 : 2;
    const QString me = m_identity->userId();
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString content = MessageModel::attachmentJson(name, size, copy, caption, durationMs);
    const QString reply = quoteReply ? takeReply() : QString();
    const bool visible = (peer->userId == m_currentPeerId);
    if (visible)
        m_messages.addMessage(id, me, peer->userId, content, type, ts, 0, reply);
    m_db->saveMessage(id, me, peer->userId, content, type, ts, 0, reply);
    m_peers->peers()->setLastMessage(peer->userId, MessageModel::preview(type, content), ts);
    if (visible)
        emit messageAdded();

    Outgoing out{peer->userId, {}, ts, type, kind, copy, name, size, caption, durationMs, false, reply};
    finishDispatch(id, peer->displayName(), dispatch(id, out));
}

// ------------------------------------------------------------------------------ retry queue

void ChatController::flushQueue(const QString &peerId)
{
    const Peer *peer = m_peers->peers()->find(peerId);
    if (!peer || !peer->isOnline() || m_security->isBlocked(peerId))
        return;
    const QString me = m_identity->userId();
    const QVariantList rows = m_db->pendingMessages(me, peerId);
    for (const QVariant &v : rows) {
        const QVariantMap r = v.toMap();
        const QString id = r["id"].toString();
        if (m_inflight.contains(id))
            continue;
        const int type = r["messageType"].toInt();
        Outgoing out;
        out.peerId = peerId;
        out.ts = r["timestamp"].toString();
        out.type = type;
        out.reply = r["reply"].toString();
        if (type == 0) {
            out.text = r["content"].toString();
        } else {
            const QJsonObject o = QJsonDocument::fromJson(r["content"].toString().toUtf8()).object();
            out.kind = type == 1 ? "image" : type == 3 ? "voice" : "file";
            out.path = o["path"].toString();
            out.name = o["name"].toString();
            out.size = qint64(o["size"].toDouble());
            out.caption = o["caption"].toString();
            out.durationMs = o["duration"].toInt();
            if (!QFileInfo::exists(out.path)) {          // the stored copy is gone: give up
                m_db->updateMessageStatus(id, 2);
                m_messages.setStatus(id, 2);
                continue;
            }
        }
        m_messages.setStatus(id, 0);
        const Route route = dispatch(id, out);
        if (route == Route::Sent) {
            m_messages.setStatus(id, 1);
            m_db->updateMessageStatus(id, 1);
        } else if (route == Route::Unreachable) {
            m_messages.setStatus(id, 3);
        }
    }
}

void ChatController::flushAll()
{
    const QStringList peers = m_db->peersWithPending(m_identity->userId());
    for (const QString &id : peers)
        flushQueue(id);
    const QStringList deletes = m_db->peersWithPendingDeletes();
    for (const QString &id : deletes)
        flushDeletes(id);
}

// -------------------------------------------------------------------------------- receiving

void ChatController::onIncoming(const QString &fromUserId, const QString &alias,
                                const QString &messageId, const QString &content,
                                const QString &timestamp, const QString &viaIp,
                                const QString &replyJson)
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
    storeIncoming(fromUserId, alias, id, 0, content, ts, replyJson);
}

void ChatController::onIncomingFile(const QString &savedPath, const QJsonObject &h, const QString &viaIp)
{
    const QString fromUserId = h["fromUserId"].toString();
    const QString alias = h["alias"].toString();
    const QString messageId = h["id"].toString();
    if (fromUserId.isEmpty() || fromUserId == m_identity->userId())
        return;
    if (!messageId.isEmpty() && m_db->messageExists(messageId))
        return;
    if (!viaIp.isEmpty())
        m_peers->onLanPeerSeen(fromUserId, alias, viaIp);
    const QString id = messageId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : messageId;
    const QString timestamp = h["timestamp"].toString();
    const QString ts = QDateTime::fromString(timestamp, Qt::ISODate).isValid()
                           ? timestamp : QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString kind = h["kind"].toString();
    storeIncoming(fromUserId, alias, id, kind == "image" ? 1 : kind == "voice" ? 3 : 2,
                  MessageModel::attachmentJson(h["name"].toString(), qint64(h["size"].toDouble()),
                                               savedPath, h["caption"].toString(), h["duration"].toInt()),
                  ts,
                  h.contains("reply") ? QString::fromUtf8(QJsonDocument(h["reply"].toObject()).toJson(QJsonDocument::Compact))
                                      : QString());
}

void ChatController::storeIncoming(const QString &fromUserId, const QString &alias, const QString &id,
                                   int type, const QString &content, const QString &ts,
                                   const QString &replyJson)
{
    const QString me = m_identity->userId();
    const QString text = MessageModel::preview(type, content);
    m_db->saveMessage(id, fromUserId, me, content, type, ts, 1, replyJson);
    m_peers->peers()->setLastMessage(fromUserId, text, ts);

    const bool viewing = (fromUserId == m_currentPeerId);
    if (viewing) {
        m_messages.addMessage(id, fromUserId, me, content, type, ts, 1, replyJson);
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

// ------------------------------------------------------------------------- delete / search

void ChatController::removeFiles(const QStringList &paths)
{
    for (const QString &p : paths)
        if (!p.isEmpty())
            QFile::remove(p);
}

void ChatController::deleteMessage(const QString &messageId, bool forEveryone)
{
    const QVariantMap info = m_db->messageInfo(messageId);
    if (info.isEmpty())
        return;
    const QString me = m_identity->userId();
    const bool mine = info["from"].toString() == me;
    const QString peerId = mine ? info["to"].toString() : info["from"].toString();
    const int type = info["type"].toInt();

    if (type > 0 && type < 4)
        removeFiles({QJsonDocument::fromJson(info["content"].toString().toUtf8()).object()["path"].toString()});
    m_inflight.remove(messageId);
    if (m_replyJson.contains(messageId))     // do not keep quoting a message that is gone
        clearReply();

    if (forEveryone && mine && type != 4) {
        // WhatsApp style: the bubble stays, as a "you deleted this message" placeholder
        m_db->tombstoneMessage(messageId);
        m_messages.markDeleted(messageId);
        m_db->addPendingDelete(messageId, peerId);   // survives restarts; retried until acknowledged
        flushDeletes(peerId);
    } else {
        m_db->deleteMessage(messageId);
        m_messages.removeMessage(messageId);
    }
    refreshPreview(peerId);
}

void ChatController::refreshPreview(const QString &peerId)
{
    const QVariantMap last = m_db->lastMessage(DatabaseManager::conversationId(m_identity->userId(), peerId));
    m_peers->peers()->setLastMessage(peerId,
        last.isEmpty() ? QString() : MessageModel::preview(last["messageType"].toInt(), last["content"].toString()),
        last.isEmpty() ? QString() : last["timestamp"].toString());
}

void ChatController::flushDeletes(const QString &peerId)
{
    const Peer *peer = m_peers->peers()->find(peerId);
    if (!peer || !peer->isOnline() || m_security->isBlocked(peerId))
        return;
    const QString me = m_identity->userId();
    for (const QString &target : m_db->pendingDeletes(peerId)) {
        const QString key = "del:" + target;
        if (m_deleteInflight.contains(key))
            continue;
        const bool tryLan = !peer->ipAddress.isEmpty() && m_lan->isConnected()
                            && (peer->lanOnline || !peer->btOnline);
        if (tryLan) {
            m_deleteInflight.insert(key);
            m_transfer->sendDelete(peer->ipAddress, peerId, target, me);
        } else if (m_bluetooth->sendDelete(peerId, target, me)) {
            m_db->removePendingDelete(target);
        }
    }
}

void ChatController::onDeleteReceived(const QString &fromUserId, const QString &messageId)
{
    const QVariantMap info = m_db->messageInfo(messageId);
    // a peer may only delete messages that it sent itself
    if (info.isEmpty() || info["from"].toString() != fromUserId || info["type"].toInt() == 4)
        return;
    if (info["type"].toInt() > 0)
        removeFiles({QJsonDocument::fromJson(info["content"].toString().toUtf8()).object()["path"].toString()});
    m_db->tombstoneMessage(messageId);       // leave "this message was deleted" behind
    m_messages.markDeleted(messageId);
    if (m_replyJson.contains(messageId))
        clearReply();
    refreshPreview(fromUserId);
}

void ChatController::deleteConversation(const QString &peerId)
{
    const QString me = m_identity->userId();
    removeFiles(m_db->deleteConversation(DatabaseManager::conversationId(me, peerId)));
    if (peerId == m_currentPeerId)
        m_messages.clear();
    m_peers->peers()->setLastMessage(peerId, {}, {});
    m_peers->peers()->clearUnread(peerId);
}

QVariantList ChatController::searchMessages(const QString &text) const
{
    return m_messages.search(text);
}

void ChatController::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

// ------------------------------------------------------------------------- profile pictures

void ChatController::requestAvatar(const QString &peerId, const QString &ip)
{
    if (!ip.isEmpty())
        m_transfer->requestAvatar(ip, peerId);
    else
        m_bluetooth->requestAvatar(peerId);
}

void ChatController::onAvatarReceived(const QString &peerId, const QString &tmpPath, const QString &hash)
{
    if (!m_peers->peers()->contains(peerId))
        return;
    const QString dest = PeerListModel::avatarPath(peerId);
    QDir().mkpath(QFileInfo(dest).absolutePath());
    QFile::remove(dest);
    if (!QFile::rename(tmpPath, dest))
        QFile::copy(tmpPath, dest);
    QFile::remove(tmpPath);
    m_peers->setAvatar(peerId, hash);
}

// Our own picture changed: re-announce it (the hash travels in every discovery packet / BT hello).
void ChatController::publishLocalAvatar()
{
    if (m_started)
        applyLocalIdentity();
}

// ----------------------------------------------------------------------------------- replies

void ChatController::setReply(const QString &messageId)
{
    const QVariantMap info = m_db->messageInfo(messageId);
    if (info.isEmpty() || info["type"].toInt() == 4)
        return;
    const QString me = m_identity->userId();
    const bool mine = info["from"].toString() == me;
    m_replyText = MessageModel::preview(info["type"].toInt(), info["content"].toString());
    m_replyName = mine ? tr("You") : currentPeerName();
    m_replyJson = MessageModel::replyJsonFor(messageId, info["from"].toString(), m_replyText, info["type"].toInt());
    emit replyChanged();
}

void ChatController::clearReply()
{
    if (m_replyJson.isEmpty())
        return;
    m_replyJson.clear();
    m_replyName.clear();
    m_replyText.clear();
    emit replyChanged();
}

QString ChatController::takeReply()
{
    const QString json = m_replyJson;
    clearReply();
    return json;
}

int ChatController::rowOf(const QString &messageId) const
{
    return m_messages.rowOf(messageId);
}

// ---------------------------------------------------------------------------------- forwarding

bool ChatController::forwardMessage(const QString &messageId, const QString &peerId)
{
    const QVariantMap info = m_db->messageInfo(messageId);
    const Peer *peer = m_peers->peers()->find(peerId);
    if (info.isEmpty() || !peer || peerId == m_identity->userId()) {
        emit notice(tr("This message cannot be forwarded"));
        return false;
    }
    const int type = info["type"].toInt();
    const QString content = info["content"].toString();
    if (type == 4) {
        emit notice(tr("A deleted message cannot be forwarded"));
        return false;
    }
    if (type == 0) {
        sendTextTo(peerId, content, false);
    } else {
        const QJsonObject o = QJsonDocument::fromJson(content.toUtf8()).object();
        const QString path = o["path"].toString();
        if (!QFileInfo::exists(path)) {
            emit notice(tr("The file is no longer available"));
            return false;
        }
        queueFile(path, o["name"].toString(), qint64(o["size"].toDouble()),
                  type == 1 ? "image" : type == 3 ? "voice" : "file",
                  o["caption"].toString(), o["duration"].toInt(), peerId, false);
    }
    emit notice(tr("Forwarded to %1").arg(peer->displayName()));
    return true;
}
