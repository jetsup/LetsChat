#ifndef TRANSFERSERVICE_H
#define TRANSFERSERVICE_H

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class FrameParser;

/**
 * Newline-delimited JSON over short-lived TCP connections.
 *   -> {"type":"text", id, fromUserId, alias, content, timestamp}\n
 *   -> {"type":"file", id, kind, name, size, ...}\n<size raw bytes>
 *   <- {"type":"ack","id"}\n
 */
class TransferService : public QObject
{
    Q_OBJECT

public:
    explicit TransferService(const QString &receiveDir, QObject *parent = nullptr);
    ~TransferService() override;

    void startServer();
    void stopServer();
    bool isListening() const { return m_server->isListening(); }

    /** Results are reported through sendFinished(). */
    void sendText(const QString &targetIp, const QString &messageId, const QString &fromUserId,
                  const QString &alias, const QString &content, const QString &timestamp,
                  const QString &replyJson = {});
    void sendFile(const QString &targetIp, const QString &messageId, const QString &fromUserId,
                  const QString &alias, const QString &kind, const QString &filePath,
                  const QString &fileName, qint64 size, const QString &timestamp,
                  const QString &caption = {}, int durationMs = 0,
                  const QString &replyJson = {});

    void sendDelete(const QString &targetIp, const QString &targetMessageId, const QString &fromUserId);

    // profile pictures
    void setLocalUser(const QString &userId) { m_localUserId = userId; }
    void setLocalAvatar(const QString &path) { m_avatarPath = path; }
    void requestAvatar(const QString &targetIp, const QString &peerUserId);

signals:
    /** The peer asked us to delete one of *its* messages from our history. */
    void deleteReceived(const QString &fromUserId, const QString &targetMessageId);
    void avatarReceived(const QString &peerUserId, const QString &path, const QString &hash);
    void textReceived(const QString &fromUserId, const QString &alias, const QString &messageId,
                      const QString &content, const QString &timestamp, const QString &senderIp,
                      const QString &replyJson);
    /** `header` is the sender's file frame (id, fromUserId, alias, kind, name, size, caption, duration…). */
    void fileReceived(const QString &savedPath, const QJsonObject &header, const QString &senderIp);
    void sendFinished(const QString &messageId, bool ok, const QString &error);

private:
    void onNewConnection();
    void startSend(const QString &ip, const QString &messageId, const QJsonObject &header,
                   const QString &filePath);

    QTcpServer *m_server;
    QString m_receiveDir;
    QString m_localUserId;
    QString m_avatarPath;
    QTimer *m_retryTimer;
    QList<QTcpSocket *> m_incoming;
};

#endif // TRANSFERSERVICE_H
