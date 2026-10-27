#ifndef TRANSFERSERVICE_H
#define TRANSFERSERVICE_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QMap>
#include <QDateTime>

class TransferService : public QObject
{
    Q_OBJECT

public:
    explicit TransferService(QObject *parent = nullptr);
    ~TransferService();
    
    Q_INVOKABLE void startServer();
    Q_INVOKABLE void stopServer();
    Q_INVOKABLE void sendTextMessage(const QString &targetIp, quint16 port, 
                                    const QString &message, const QString &fromUserId);
    Q_INVOKABLE void sendFile(const QString &targetIp, quint16 port,
                             const QString &filePath, const QString &fromUserId);
    
signals:
    void textMessageReceived(const QString &fromUserId, const QString &message, const QString &senderIp);
    void fileTransferRequest(const QString &fromUserId, const QString &fileName, 
                            qint64 fileSize, const QString &transferId);
    void fileTransferProgress(const QString &transferId, qint64 bytesTransferred, qint64 totalBytes);
    void fileTransferComplete(const QString &transferId, const QString &filePath);
    void transferError(const QString &error);
    
private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    
private:
    struct TransferInfo {
        QTcpSocket *socket;
        QString transferId;
        QString fileName;
        qint64 fileSize;
        qint64 bytesReceived;
        QFile *file;
        bool isReceiving;
    };
    
    QTcpServer *m_tcpServer;
    QMap<QTcpSocket*, TransferInfo> m_transfers;
};

#endif // TRANSFERSERVICE_H
