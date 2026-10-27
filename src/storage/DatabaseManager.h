#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();
    
    bool initialize();
    void close();
    
    // Messages
    Q_INVOKABLE bool saveMessage(const QString &messageId, const QString &fromUserId, 
                                const QString &toUserId, const QString &content, 
                                int messageType, const QString &timestamp);
    Q_INVOKABLE QVariantList getMessages(const QString &conversationId, int limit = 50);
    Q_INVOKABLE bool deleteMessage(const QString &messageId);
    
    // Peers
    Q_INVOKABLE bool savePeer(const QString &userId, const QString &alias, 
                             const QString &deviceName, const QString &ipAddress,
                             bool isOnline);
    Q_INVOKABLE QVariantList getPeers();
    Q_INVOKABLE bool updatePeerStatus(const QString &userId, bool isOnline);
    Q_INVOKABLE bool markAllPeersOffline();
    
    // Settings/Encrypted storage
    Q_INVOKABLE bool saveEncryptedData(const QString &key, const QString &data);
    Q_INVOKABLE QString loadEncryptedData(const QString &key);
    
signals:
    void databaseReady();
    void error(const QString &message);
    
private:
    bool createTables();
    QString getDatabasePath() const;
    
    QSqlDatabase m_db;
    QString m_encryptionKey;
};

#endif // DATABASEMANAGER_H
