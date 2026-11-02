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
                     const QString &content, int messageType, const QString &timestamp, int status = 1,
                     const QString &replyJson = {});
    bool messageExists(const QString &messageId);
    bool updateMessageStatus(const QString &messageId, int status);
    QVariantList getMessages(const QString &conversationId, int limit = 200);
    /** Every photo, file and voice note of a conversation, newest first. */
    QVariantList mediaMessages(const QString &conversationId);
    QVariantMap lastMessage(const QString &conversationId);
    /** Own messages that still have to be delivered (status 0/3, younger than maxAgeHours). */
    QVariantList pendingMessages(const QString &fromUserId, const QString &toUserId, int maxAgeHours = 24);
    QStringList peersWithPending(const QString &fromUserId);
    /** Marks messages left in "sending" by a previous run as queued, and expires very old ones. */
    void requeueStale(const QString &fromUserId, int maxAgeHours = 24);

    // Peers
    // Pinned identity keys (trust on first use)
    QByteArray peerKey(const QString &userId);
    QString userForKey(const QByteArray &key);
    void savePeerKey(const QString &userId, const QByteArray &key, bool verified);
    bool isPeerVerified(const QString &userId);
    void setPeerVerified(const QString &userId, bool verified);
    QString peerKeySince(const QString &userId);

    bool savePeer(const QString &userId, const QString &alias, const QString &deviceName,
                  const QString &ipAddress, const QString &btAddress);
    QVariantList getPeers();
    void setNickname(const QString &userId, const QString &nickname);
    void setAvatarHash(const QString &userId, const QString &hash);

    // Deleting
    QVariantMap messageInfo(const QString &messageId);        // {from, to, type, content}
    bool deleteMessage(const QString &messageId);
    /** "Deleted for everyone": keeps the row as a placeholder (type 4) without content, files or quote. */
    bool tombstoneMessage(const QString &messageId);
    /** Removes the whole conversation; returns the attachment files that belonged to it. */
    QStringList deleteConversation(const QString &conversationId);
    /** Forget a contact completely: the peer row, its nickname, its pinned key and queued deletes. */
    void forgetPeer(const QString &userId);
    // "delete for everyone" requests that still have to reach the peer
    void addPendingDelete(const QString &messageId, const QString &peerId);
    void removePendingDelete(const QString &messageId);
    QStringList pendingDeletes(const QString &peerId);
    QStringList peersWithPendingDeletes();

signals:
    void error(const QString &message);

private:
    bool createTables();
    bool columnExists(const QString &table, const QString &column);
    QString encode(const QString &text, const QByteArray &ad = {}) const;
    QString decode(const QString &stored, const QByteArray &ad = {}) const;
    void migrateEncryption();
    QString databasePath() const;

    QSqlDatabase m_db;
    QByteArray m_key;
};

#endif // DATABASEMANAGER_H
