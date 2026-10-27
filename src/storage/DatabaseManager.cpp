#include "DatabaseManager.h"
#include "CryptoUtils.h"
#include "core/IdentityManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
    m_encryptionKey = "LetsChatDefaultKey2024";
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::initialize()
{
    QString dbPath = getDatabasePath();
    QDir dir = QFileInfo(dbPath).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Remove existing connection if any
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        QSqlDatabase::removeDatabase("qt_sql_default_connection");
    }
    
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);
    
    if (!m_db.open()) {
        emit error(m_db.lastError().text());
        return false;
    }
    
    if (!createTables()) {
        return false;
    }
    
    emit databaseReady();
    return true;
}

void DatabaseManager::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

QString DatabaseManager::getDatabasePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataPath + "/letschat.db";
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_db);
    
    // Messages table
    QString createMessagesTable = 
        "CREATE TABLE IF NOT EXISTS messages ("
        "id TEXT PRIMARY KEY,"
        "from_user_id TEXT NOT NULL,"
        "to_user_id TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "message_type INTEGER DEFAULT 0,"
        "timestamp TEXT NOT NULL,"
        "conversation_id TEXT NOT NULL"
        ")";
    
    if (!query.exec(createMessagesTable)) {
        emit error(query.lastError().text());
        return false;
    }
    
    // Peers table
    QString createPeersTable =
        "CREATE TABLE IF NOT EXISTS peers ("
        "user_id TEXT PRIMARY KEY,"
        "alias TEXT,"
        "device_name TEXT,"
        "ip_address TEXT,"
        "is_online INTEGER DEFAULT 0,"
        "last_seen TEXT"
        ")";
    
    if (!query.exec(createPeersTable)) {
        emit error(query.lastError().text());
        return false;
    }
    
    // Encrypted storage table
    QString createEncryptedTable =
        "CREATE TABLE IF NOT EXISTS encrypted_storage ("
        "key_name TEXT PRIMARY KEY,"
        "encrypted_data TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ")";
    
    if (!query.exec(createEncryptedTable)) {
        emit error(query.lastError().text());
        return false;
    }
    
    return true;
}

bool DatabaseManager::saveMessage(const QString &messageId, const QString &fromUserId,
                                 const QString &toUserId, const QString &content,
                                 int messageType, const QString &timestamp)
{
    QString conversationId = fromUserId < toUserId ? fromUserId + "_" + toUserId : toUserId + "_" + fromUserId;
    
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO messages (id, from_user_id, to_user_id, content, message_type, timestamp, conversation_id) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(messageId);
    query.addBindValue(fromUserId);
    query.addBindValue(toUserId);
    query.addBindValue(content);
    query.addBindValue(messageType);
    query.addBindValue(timestamp);
    query.addBindValue(conversationId);
    
    if (!query.exec()) {
        emit error(query.lastError().text());
        return false;
    }
    return true;
}

QVariantList DatabaseManager::getMessages(const QString &conversationId, int limit)
{
    QVariantList messages;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, from_user_id, to_user_id, content, message_type, timestamp FROM messages "
                  "WHERE conversation_id = ? ORDER BY timestamp ASC LIMIT ?");
    query.addBindValue(conversationId);
    query.addBindValue(limit);
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap msg;
            msg["id"] = query.value(0).toString();
            msg["fromUserId"] = query.value(1).toString();
            msg["toUserId"] = query.value(2).toString();
            msg["content"] = query.value(3).toString();
            msg["messageType"] = query.value(4).toInt();
            msg["timestamp"] = query.value(5).toString();
            messages.append(msg);
        }
    }
    
    return messages;
}

bool DatabaseManager::deleteMessage(const QString &messageId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM messages WHERE id = ?");
    query.addBindValue(messageId);
    
    if (!query.exec()) {
        emit error(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::savePeer(const QString &userId, const QString &alias,
                              const QString &deviceName, const QString &ipAddress,
                              bool isOnline)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO peers (user_id, alias, device_name, ip_address, is_online, last_seen) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(userId);
    query.addBindValue(alias);
    query.addBindValue(deviceName);
    query.addBindValue(ipAddress);
    query.addBindValue(isOnline ? 1 : 0);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    
    if (!query.exec()) {
        emit error(query.lastError().text());
        return false;
    }
    return true;
}

QVariantList DatabaseManager::getPeers()
{
    QVariantList peers;
    QSqlQuery query(m_db);
    query.prepare("SELECT user_id, alias, device_name, ip_address, is_online, last_seen FROM peers");
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap peer;
            peer["userId"] = query.value(0).toString();
            peer["alias"] = query.value(1).toString();
            peer["deviceName"] = query.value(2).toString();
            peer["ipAddress"] = query.value(3).toString();
            peer["isOnline"] = query.value(4).toInt() == 1;
            peer["lastSeen"] = query.value(5).toString();
            peers.append(peer);
        }
    }
    
    return peers;
}

bool DatabaseManager::updatePeerStatus(const QString &userId, bool isOnline)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE peers SET is_online = ?, last_seen = ? WHERE user_id = ?");
    query.addBindValue(isOnline ? 1 : 0);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(userId);
    
    if (!query.exec()) {
        emit error(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::markAllPeersOffline()
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE peers SET is_online = 0");
    
    if (!query.exec()) {
        emit error(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::saveEncryptedData(const QString &key, const QString &data)
{
    QString encrypted = CryptoUtils::encryptString(data, m_encryptionKey);
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO encrypted_storage (key_name, encrypted_data, updated_at) VALUES (?, ?, ?)");
    query.addBindValue(key);
    query.addBindValue(encrypted);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    
    if (!query.exec()) {
        emit error(query.lastError().text());
        return false;
    }
    return true;
}

QString DatabaseManager::loadEncryptedData(const QString &key)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT encrypted_data FROM encrypted_storage WHERE key_name = ?");
    query.addBindValue(key);
    
    if (query.exec() && query.next()) {
        QString encrypted = query.value(0).toString();
        return CryptoUtils::decryptString(encrypted, m_encryptionKey);
    }
    
    return QString();
}
