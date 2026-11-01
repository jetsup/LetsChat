#ifndef CHATCONTROLLER_H
#define CHATCONTROLLER_H

#include "models/MessageModel.h"
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>

class IdentityManager;
class PeerManager;
class TransferService;
class BluetoothService;
class DatabaseManager;
class NotificationManager;
class LanTracker;
class BackgroundService;

/**
 * Glue between the transports (LAN, Bluetooth), persistence, the peer list and the UI.
 * Owns the message list of the conversation that is currently open.
 */
class ChatController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *messages READ messages CONSTANT)
    Q_PROPERTY(bool hasCurrentPeer READ hasCurrentPeer NOTIFY currentPeerChanged)
    Q_PROPERTY(QString currentPeerId READ currentPeerId NOTIFY currentPeerChanged)
    Q_PROPERTY(QString currentPeerName READ currentPeerName NOTIFY currentPeerChanged)
    Q_PROPERTY(bool currentPeerOnline READ currentPeerOnline NOTIFY currentPeerChanged)
    Q_PROPERTY(QString currentPeerTransport READ currentPeerTransport NOTIFY currentPeerChanged)
    Q_PROPERTY(bool replyActive READ replyActive NOTIFY replyChanged)
    Q_PROPERTY(QString replyName READ replyName NOTIFY replyChanged)
    Q_PROPERTY(QString replyText READ replyText NOTIFY replyChanged)
    Q_PROPERTY(QString localIp READ localIp NOTIFY networkChanged)
    Q_PROPERTY(bool lanConnected READ lanConnected NOTIFY networkChanged)

public:
    ChatController(IdentityManager *identity, PeerManager *peers, TransferService *transfer,
                   BluetoothService *bluetooth, DatabaseManager *db, NotificationManager *notifier,
                   LanTracker *lan, BackgroundService *background, QObject *parent = nullptr);

    QObject *messages() { return &m_messages; }
    bool hasCurrentPeer() const { return !m_currentPeerId.isEmpty(); }
    QString currentPeerId() const { return m_currentPeerId; }
    QString currentPeerName() const;
    bool currentPeerOnline() const;
    QString currentPeerTransport() const;
    bool replyActive() const { return !m_replyJson.isEmpty(); }
    QString replyName() const { return m_replyName; }
    QString replyText() const { return m_replyText; }
    QString localIp() const;
    bool lanConnected() const;

    /** Load persisted state and start LAN discovery, the TCP server and Bluetooth. */
    void startServices();

    Q_INVOKABLE void selectPeer(const QString &userId);
    Q_INVOKABLE void closeChat();
    Q_INVOKABLE void sendMessage(const QString &text);
    /** Sends a photo or any other file (a file:// or content:// URL from the file dialog). */
    Q_INVOKABLE void sendAttachment(const QUrl &url, const QString &caption = QString());
    /** Sends a recorded voice note (m4a) of the given length. */
    Q_INVOKABLE void sendVoice(const QUrl &url, int durationMs);

    /** Removes a message here; with forEveryone also from the peer's device (own messages only). */
    Q_INVOKABLE void deleteMessage(const QString &messageId, bool forEveryone);
    /** Deletes the whole conversation with a peer from this device. */
    Q_INVOKABLE void deleteConversation(const QString &peerId);
    /** Row numbers (in the open conversation) of messages matching `text`. */
    Q_INVOKABLE QVariantList searchMessages(const QString &text) const;
    Q_INVOKABLE void copyText(const QString &text);
    /** Sends a copy of a message (text, photo, file or voice note) to another contact. */
    Q_INVOKABLE bool forwardMessage(const QString &messageId, const QString &peerId);

    /** The next message sent is a reply to this one (shown as a quote). */
    Q_INVOKABLE void setReply(const QString &messageId);
    Q_INVOKABLE void clearReply();
    /** Row of a message in the open conversation, or -1 (used to jump to a quoted message). */
    Q_INVOKABLE int rowOf(const QString &messageId) const;

signals:
    void currentPeerChanged();
    void networkChanged();
    /** Short user-facing message (shown as a toast). */
    void notice(const QString &text);
    void messageAdded();
    void replyChanged();

private:
    struct Outgoing {
        QString peerId, text, ts;
        // attachments
        int type = 0;
        QString kind, path, name;
        qint64 size = 0;
        QString caption;
        int durationMs = 0;
        bool viaBluetooth = false;
        QString reply;          // JSON of the quoted message, empty if not a reply
    };

    void onIncoming(const QString &fromUserId, const QString &alias, const QString &messageId,
                    const QString &content, const QString &timestamp, const QString &viaIp,
                    const QString &replyJson);
    enum class Route { Unreachable, Sent, InFlight };
    Route dispatch(const QString &id, Outgoing out);
    void finishDispatch(const QString &id, const QString &peerName, Route route);
    void flushQueue(const QString &peerId);
    void flushAll();
    void flushDeletes(const QString &peerId);
    void refreshPreview(const QString &peerId);
    void onDeleteReceived(const QString &fromUserId, const QString &messageId);
    void removeFiles(const QStringList &paths);
    void onAvatarReceived(const QString &peerId, const QString &tmpPath, const QString &hash);
    void requestAvatar(const QString &peerId, const QString &ip);
    void publishLocalAvatar();
    void onSendFinished(const QString &messageId, bool ok, const QString &error);
    void onIncomingFile(const QString &savedPath, const QJsonObject &header, const QString &viaIp);
    // peerId empty = the open conversation; quoteReply false for forwards (no quote is attached)
    void queueFile(const QString &path, const QString &name, qint64 size, const QString &kind,
                   const QString &caption, int durationMs, const QString &peerId = {}, bool quoteReply = true);
    void sendTextTo(const QString &peerId, const QString &text, bool quoteReply);
    void storeIncoming(const QString &fromUserId, const QString &alias, const QString &id, int type,
                       const QString &content, const QString &ts, const QString &replyJson = {});
    QString takeReply();
    void applyLocalIdentity();
    void refreshCurrent();
    bool resendViaBluetooth(const Outgoing &o, const QString &id);
    bool sendViaBluetooth(const QString &peerId, const QString &id, const QString &text,
                          const QString &ts, const QString &reply = {});


    IdentityManager *m_identity;
    PeerManager *m_peers;
    TransferService *m_transfer;
    BluetoothService *m_bluetooth;
    DatabaseManager *m_db;
    NotificationManager *m_notifier;
    LanTracker *m_lan;
    BackgroundService *m_background;

    MessageModel m_messages;
    QString m_currentPeerId;
    QHash<QString, Outgoing> m_inflight;
    QString m_replyJson, m_replyName, m_replyText;
    QSet<QString> m_deleteInflight;       // "del:<id>" requests waiting for an ack   // LAN sends awaiting an ack
    QTimer *m_retryTimer = nullptr;
    bool m_started = false;
};

#endif // CHATCONTROLLER_H
