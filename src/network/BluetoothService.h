#ifndef BLUETOOTHSERVICE_H
#define BLUETOOTHSERVICE_H

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <functional>

class SecurityManager;
class SecureChannel;

/**
 * Bluetooth Classic (RFCOMM) transport for nearby devices.
 *
 * Every LetsChat instance exposes an RFCOMM service with a fixed UUID.  Devices found
 * by a scan can be connected to; right after the link comes up both sides exchange a
 * "hello" (userId / alias / deviceName) and then newline-delimited JSON text messages,
 * the same framing used on the LAN.
 */
class BluetoothService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool poweredOff READ poweredOff NOTIFY stateChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY stateChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    explicit BluetoothService(const QString &receiveDir, SecurityManager *security, QObject *parent = nullptr);
    ~BluetoothService() override;

    bool available() const;
    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    bool poweredOff() const;
    bool listening() const { return m_server && m_server->isListening(); }
    bool scanning() const { return m_scanning; }
    QString statusText() const { return m_status; }
    QVariantList devices() const;

    void setLocalInfo(const QString &userId, const QString &alias, const QString &deviceName,
                      const QString &avatarHash = {}, const QString &avatarPath = {});
    /** Starts listening for incoming connections (if the user enabled Bluetooth). */
    void start();
    void stop();

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void connectToDevice(const QString &address);
    Q_INVOKABLE void disconnectDevice(const QString &address);
    /** Asks the OS to make this device visible to other devices for a while. */
    Q_INVOKABLE void makeDiscoverable();
    /** Asks the OS to switch the Bluetooth adapter on (Android shows a system prompt). */
    Q_INVOKABLE void turnOn();

    bool isLinked(const QString &userId) const { return m_byUser.contains(userId); }
    bool sendText(const QString &userId, const QString &messageId, const QString &fromUserId,
                  const QString &alias, const QString &content, const QString &timestamp,
                  const QString &replyJson = {});

    /** Streams a file over the link; completion is reported through sendFinished(). */
    /** Asks the peer for its profile picture / tells it to delete one of our messages. */
    void requestAvatar(const QString &userId);
    bool sendDelete(const QString &userId, const QString &targetMessageId, const QString &fromUserId);
    bool sendFile(const QString &userId, const QString &messageId, const QString &fromUserId,
                  const QString &alias, const QString &kind, const QString &filePath,
                  const QString &fileName, qint64 size, const QString &timestamp,
                  const QString &caption = {}, int durationMs = 0,
                  const QString &replyJson = {});

signals:
    /** A device presented another identity key than the one pinned for it: the link was closed. */
    void securityAlert(const QString &userId);
    void sendFinished(const QString &messageId, bool ok);
    void fileReceived(const QString &savedPath, const QJsonObject &header);
    void stateChanged();
    void scanningChanged();
    void devicesChanged();
    void peerUp(const QString &userId, const QString &alias, const QString &deviceName,
                const QString &address, const QString &avatarHash);
    void deleteReceived(const QString &fromUserId, const QString &targetMessageId);
    void avatarReceived(const QString &peerUserId, const QString &path, const QString &hash);
    void peerDown(const QString &userId);
    void textReceived(const QString &fromUserId, const QString &alias, const QString &messageId,
                      const QString &content, const QString &timestamp, const QString &replyJson);

private:
    struct Link {
        QString address;
        QString userId;
        QString alias;
        class FrameParser *parser = nullptr;
        SecureChannel *chan = nullptr;     // all traffic of this link goes through it
    };
    struct Seen { QString name; QString address; QString kind; bool paired; };

    void ensurePermission(const std::function<void()> &granted);
    void startListening();
    QBluetoothLocalDevice *localDevice();
    void adopt(QBluetoothSocket *socket);
    void handleFrame(QBluetoothSocket *socket, const QJsonObject &msg);
    void sendFrame(QBluetoothSocket *socket, const QJsonObject &msg);
    QIODevice *io(QBluetoothSocket *socket) const;   // the encrypted channel of a link
    void dropLink(QBluetoothSocket *socket);
    void setStatus(const QString &text);
    void rememberAddress(const QString &address);
    QBluetoothSocket *socketForAddress(const QString &address) const;

    SecurityManager *m_security;
    QString m_receiveDir;
    bool m_enabled = true;
    bool m_scanning = false;
    QString m_status;
    QString m_localUserId, m_localAlias, m_localDeviceName, m_localAvatarHash, m_localAvatarPath;

    QBluetoothLocalDevice *m_local = nullptr;
    QBluetoothServer *m_server = nullptr;
    QBluetoothServiceInfo m_serviceInfo;
    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;

    QHash<QBluetoothSocket *, Link> m_links;
    QHash<QString, QBluetoothSocket *> m_byUser;
    QSet<QBluetoothSocket *> m_dialled;     // sockets we opened ourselves (Noise initiator side)
    QSet<QString> m_connecting;             // addresses with an outgoing attempt in flight
    QSet<QString> m_known;                  // addresses we linked with before (auto-reconnect)
    QHash<QString, Seen> m_seen;            // scan results by address
};

#endif // BLUETOOTHSERVICE_H
