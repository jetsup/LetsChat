#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/** SQLite persistence for peers and chat history (message bodies are encrypted at rest). */
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    bool initialize();
    void close();

    static QString conversationId(const QString &userA, const QString &userB);

    // Messages. status: 0 = sending, 1 = sent / received, 2 = failed
    bool saveMessage(const QString &messageId, const QString &fromUserId, const QString &toUserId,
                     const QString &content, int messageType, const QString &timestamp, int status = 1);
    bool messageExists(const QString &messageId);
    bool updateMessageStatus(const QString &messageId, int status);
    QVariantList getMessages(const QString &conversationId, int limit = 200);
    QVariantMap lastMessage(const QString &conversationId);

    // Peers
    bool savePeer(const QString &userId, const QString &alias, const QString &deviceName,
                  const QString &ipAddress, const QString &btAddress);
    QVariantList getPeers();

signals:
    void error(const QString &message);

private:
    bool createTables();
    bool columnExists(const QString &table, const QString &column);
    QString encode(const QString &text) const;
    QString decode(const QString &stored) const;
    QString databasePath() const;

    QSqlDatabase m_db;
    QByteArray m_key;
};

#endif // DATABASEMANAGER_H
