#ifndef CHATCONTROLLER_H
#define CHATCONTROLLER_H

#include "models/MessageModel.h"
#include <QObject>
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
    QString localIp() const;
    bool lanConnected() const;

    /** Load persisted state and start LAN discovery, the TCP server and Bluetooth. */
    void startServices();

    Q_INVOKABLE void selectPeer(const QString &userId);
    Q_INVOKABLE void closeChat();
    Q_INVOKABLE void sendMessage(const QString &text);
    /** Sends a photo or any other file (a file:// or content:// URL from the file dialog). */
    Q_INVOKABLE void sendAttachment(const QUrl &url);

signals:
    void currentPeerChanged();
    void networkChanged();
    /** Short user-facing message (shown as a toast). */
    void notice(const QString &text);
    void messageAdded();

private:
    struct Outgoing {
        QString peerId, text, ts;
        // attachments
        int type = 0;
        QString kind, path, name;
        qint64 size = 0;
        bool viaBluetooth = false;
    };

    void onIncoming(const QString &fromUserId, const QString &alias, const QString &messageId,
                    const QString &content, const QString &timestamp, const QString &viaIp);
    void onSendFinished(const QString &messageId, bool ok, const QString &error);
    void onIncomingFile(const QString &fromUserId, const QString &alias, const QString &messageId,
                        const QString &kind, const QString &fileName, qint64 size,
                        const QString &savedPath, const QString &timestamp, const QString &viaIp);
    void storeIncoming(const QString &fromUserId, const QString &alias, const QString &id, int type,
                       const QString &content, const QString &ts);
    void applyLocalIdentity();
    void refreshCurrent();
    bool resendViaBluetooth(const Outgoing &o, const QString &id);
    bool sendViaBluetooth(const QString &peerId, const QString &id, const QString &text,
                          const QString &ts);


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
    QHash<QString, Outgoing> m_inflight;   // LAN sends awaiting an ack
    bool m_started = false;
};

#endif // CHATCONTROLLER_H
