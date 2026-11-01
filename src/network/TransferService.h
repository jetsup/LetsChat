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
                  const QString &alias, const QString &content, const QString &timestamp);
    void sendFile(const QString &targetIp, const QString &messageId, const QString &fromUserId,
                  const QString &alias, const QString &kind, const QString &filePath,
                  const QString &fileName, qint64 size, const QString &timestamp);

signals:
    void textReceived(const QString &fromUserId, const QString &alias, const QString &messageId,
                      const QString &content, const QString &timestamp, const QString &senderIp);
    void fileReceived(const QString &fromUserId, const QString &alias, const QString &messageId,
                      const QString &kind, const QString &fileName, qint64 size,
                      const QString &savedPath, const QString &timestamp, const QString &senderIp);
    void sendFinished(const QString &messageId, bool ok, const QString &error);

private:
    void onNewConnection();
    void startSend(const QString &ip, const QString &messageId, const QJsonObject &header,
                   const QString &filePath);

    QTcpServer *m_server;
    QString m_receiveDir;
    QTimer *m_retryTimer;
    QList<QTcpSocket *> m_incoming;
};

#endif // TRANSFERSERVICE_H
