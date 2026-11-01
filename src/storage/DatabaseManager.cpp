#include "DatabaseManager.h"
#include "CryptoUtils.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace { const char *kConnection = "letschat"; const char *kPrefix = "enc1:"; }

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
    // The key is generated once per installation and kept in the app's private settings.
    QSettings s;
    const QByteArray stored = QByteArray::fromBase64(s.value("storage/key").toByteArray());
    if (stored.size() == 32) {
        m_key = stored;
    } else {
        m_key = CryptoUtils::randomBytes(32);
        s.setValue("storage/key", m_key.toBase64());
    }
}

DatabaseManager::~DatabaseManager() { close(); }

QString DatabaseManager::databasePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/letschat.db";
}

bool DatabaseManager::initialize()
{
    const QString path = databasePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    m_db = QSqlDatabase::contains(kConnection) ? QSqlDatabase::database(kConnection)
                                               : QSqlDatabase::addDatabase("QSQLITE", kConnection);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        qWarning() << "Database open failed:" << m_db.lastError().text() << path;
        emit error(m_db.lastError().text());
        return false;
    }
    qInfo() << "Database opened at" << path;
    return createTables();
}

void DatabaseManager::close()
{
    if (m_db.isValid() && m_db.isOpen())
        m_db.close();
}

QString DatabaseManager::conversationId(const QString &a, const QString &b)
{
    return a < b ? a + "_" + b : b + "_" + a;
}

QString DatabaseManager::encode(const QString &text) const
{
    return QString::fromLatin1(kPrefix) + CryptoUtils::encryptString(text, m_key);
}

QString DatabaseManager::decode(const QString &stored) const
{
    if (!stored.startsWith(QLatin1String(kPrefix)))
        return stored;  // legacy plaintext row
    return CryptoUtils::decryptString(stored.mid(int(strlen(kPrefix))), m_key);
}

bool DatabaseManager::columnExists(const QString &table, const QString &column)
{
    QSqlQuery q(m_db);
    q.exec("PRAGMA table_info(" + table + ")");
    while (q.next())
        if (q.value(1).toString() == column)
            return true;
    return false;
}

bool DatabaseManager::createTables()
{
    QSqlQuery q(m_db);
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS messages ("
        "id TEXT PRIMARY KEY, from_user_id TEXT NOT NULL, to_user_id TEXT NOT NULL,"
        "content TEXT NOT NULL, message_type INTEGER DEFAULT 0, timestamp TEXT NOT NULL,"
        "conversation_id TEXT NOT NULL, status INTEGER DEFAULT 1)",
        "CREATE INDEX IF NOT EXISTS idx_messages_conv ON messages(conversation_id, timestamp)",
        "CREATE TABLE IF NOT EXISTS peers ("
        "user_id TEXT PRIMARY KEY, alias TEXT, device_name TEXT, ip_address TEXT,"
        "bt_address TEXT, is_online INTEGER DEFAULT 0, last_seen TEXT)",
        "CREATE TABLE IF NOT EXISTS pending_deletes (message_id TEXT PRIMARY KEY, peer_id TEXT NOT NULL)",
    };
    for (const QString &sql : statements) {
        if (!q.exec(sql)) {
            qWarning() << "DB schema error:" << q.lastError().text();
            emit error(q.lastError().text());
            return false;
        }
    }
    // Migrations for databases created by version 0.1
    if (!columnExists("messages", "status"))
        q.exec("ALTER TABLE messages ADD COLUMN status INTEGER DEFAULT 1");
    if (!columnExists("peers", "bt_address"))
        q.exec("ALTER TABLE peers ADD COLUMN bt_address TEXT");
    if (!columnExists("peers", "nickname"))
        q.exec("ALTER TABLE peers ADD COLUMN nickname TEXT");
    if (!columnExists("peers", "first_seen"))
        q.exec("ALTER TABLE peers ADD COLUMN first_seen TEXT");
    if (!columnExists("peers", "avatar_hash"))
        q.exec("ALTER TABLE peers ADD COLUMN avatar_hash TEXT");
    return true;
}

bool DatabaseManager::saveMessage(const QString &messageId, const QString &fromUserId,
                                  const QString &toUserId, const QString &content, int messageType,
                                  const QString &timestamp, int status)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO messages "
              "(id, from_user_id, to_user_id, content, message_type, timestamp, conversation_id, status) "
              "VALUES (?,?,?,?,?,?,?,?)");
    q.addBindValue(messageId);
    q.addBindValue(fromUserId);
    q.addBindValue(toUserId);
    q.addBindValue(encode(content));
    q.addBindValue(messageType);
    q.addBindValue(timestamp);
    q.addBindValue(conversationId(fromUserId, toUserId));
    q.addBindValue(status);
    if (!q.exec()) {
        qWarning() << "saveMessage failed:" << q.lastError().text();
        emit error(q.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::messageExists(const QString &messageId)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM messages WHERE id = ?");
    q.addBindValue(messageId);
    return q.exec() && q.next();
}

bool DatabaseManager::updateMessageStatus(const QString &messageId, int status)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE messages SET status = ? WHERE id = ?");
    q.addBindValue(status);
    q.addBindValue(messageId);
    return q.exec();
}

QVariantList DatabaseManager::getMessages(const QString &conversationId, int limit)
{
    QVariantList out;
    QSqlQuery q(m_db);
    // newest `limit` rows, returned oldest-first
    q.prepare("SELECT id, from_user_id, to_user_id, content, message_type, timestamp, status FROM ("
              "SELECT * FROM messages WHERE conversation_id = ? ORDER BY timestamp DESC LIMIT ?) "
              "ORDER BY timestamp ASC");
    q.addBindValue(conversationId);
    q.addBindValue(limit);
    if (!q.exec()) {
        qWarning() << "getMessages failed:" << q.lastError().text();
        return out;
    }
    while (q.next()) {
        QVariantMap m;
        m["id"] = q.value(0).toString();
        m["fromUserId"] = q.value(1).toString();
        m["toUserId"] = q.value(2).toString();
        m["content"] = decode(q.value(3).toString());
        m["messageType"] = q.value(4).toInt();
        m["timestamp"] = q.value(5).toString();
        m["status"] = q.value(6).toInt();
        out.append(m);
    }
    return out;
}

QVariantMap DatabaseManager::lastMessage(const QString &conversationId)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT content, timestamp, message_type FROM messages WHERE conversation_id = ? "
              "ORDER BY timestamp DESC LIMIT 1");
    q.addBindValue(conversationId);
    QVariantMap m;
    if (q.exec() && q.next()) {
        m["content"] = decode(q.value(0).toString());
        m["timestamp"] = q.value(1).toString();
        m["messageType"] = q.value(2).toInt();
    }
    return m;
}

bool DatabaseManager::savePeer(const QString &userId, const QString &alias, const QString &deviceName,
                               const QString &ipAddress, const QString &btAddress)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO peers (user_id, alias, device_name, ip_address, bt_address, last_seen, first_seen) "
              "VALUES (?,?,?,?,?,?,?) "
              "ON CONFLICT(user_id) DO UPDATE SET alias = excluded.alias, "
              "device_name = excluded.device_name, "
              "ip_address = CASE WHEN excluded.ip_address != '' THEN excluded.ip_address ELSE ip_address END, "
              "bt_address = CASE WHEN excluded.bt_address != '' THEN excluded.bt_address ELSE bt_address END, "
              "last_seen = excluded.last_seen");
    q.addBindValue(userId);
    q.addBindValue(alias);
    q.addBindValue(deviceName);
    q.addBindValue(ipAddress);
    q.addBindValue(btAddress);
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    q.addBindValue(now);
    q.addBindValue(now);   // first_seen: only used by the INSERT, the upsert keeps the original
    if (!q.exec()) {
        qWarning() << "savePeer failed:" << q.lastError().text();
        emit error(q.lastError().text());
        return false;
    }
    return true;
}

QVariantList DatabaseManager::getPeers()
{
    QVariantList out;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT user_id, alias, device_name, ip_address, bt_address, last_seen, nickname, first_seen, avatar_hash FROM peers"))
        return out;
    while (q.next()) {
        QVariantMap p;
        p["userId"] = q.value(0).toString();
        p["alias"] = q.value(1).toString();
        p["deviceName"] = q.value(2).toString();
        p["ipAddress"] = q.value(3).toString();
        p["btAddress"] = q.value(4).toString();
        p["lastSeen"] = q.value(5).toString();
        p["nickname"] = q.value(6).toString();
        p["firstSeen"] = q.value(7).toString();
        p["avatarHash"] = q.value(8).toString();
        out.append(p);
    }
    return out;
}

QVariantList DatabaseManager::pendingMessages(const QString &from, const QString &to, int maxAgeHours)
{
    QVariantList out;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, content, message_type, timestamp FROM messages "
              "WHERE from_user_id = ? AND to_user_id = ? AND status IN (0, 3) AND timestamp > ? "
              "ORDER BY timestamp ASC LIMIT 50");
    q.addBindValue(from);
    q.addBindValue(to);
    q.addBindValue(QDateTime::currentDateTime().addSecs(-3600 * maxAgeHours).toString(Qt::ISODate));
    if (!q.exec())
        return out;
    while (q.next()) {
        out.append(QVariantMap{{"id", q.value(0).toString()},
                               {"content", decode(q.value(1).toString())},
                               {"messageType", q.value(2).toInt()},
                               {"timestamp", q.value(3).toString()}});
    }
    return out;
}

QStringList DatabaseManager::peersWithPending(const QString &from)
{
    QStringList out;
    QSqlQuery q(m_db);
    q.prepare("SELECT DISTINCT to_user_id FROM messages WHERE from_user_id = ? AND status IN (0, 3)");
    q.addBindValue(from);
    if (q.exec())
        while (q.next())
            out << q.value(0).toString();
    return out;
}

void DatabaseManager::requeueStale(const QString &from, int maxAgeHours)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE messages SET status = 2 WHERE from_user_id = ? AND status IN (0, 3) AND timestamp <= ?");
    q.addBindValue(from);
    q.addBindValue(QDateTime::currentDateTime().addSecs(-3600 * maxAgeHours).toString(Qt::ISODate));
    q.exec();
    q.prepare("UPDATE messages SET status = 3 WHERE from_user_id = ? AND status = 0");
    q.addBindValue(from);
    q.exec();
}

void DatabaseManager::setNickname(const QString &userId, const QString &nickname)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE peers SET nickname = ? WHERE user_id = ?");
    q.addBindValue(nickname);
    q.addBindValue(userId);
    q.exec();
}

void DatabaseManager::setAvatarHash(const QString &userId, const QString &hash)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE peers SET avatar_hash = ? WHERE user_id = ?");
    q.addBindValue(hash);
    q.addBindValue(userId);
    q.exec();
}

QVariantMap DatabaseManager::messageInfo(const QString &messageId)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT from_user_id, to_user_id, message_type, content FROM messages WHERE id = ?");
    q.addBindValue(messageId);
    QVariantMap m;
    if (q.exec() && q.next()) {
        m["from"] = q.value(0).toString();
        m["to"] = q.value(1).toString();
        m["type"] = q.value(2).toInt();
        m["content"] = decode(q.value(3).toString());
    }
    return m;
}

bool DatabaseManager::deleteMessage(const QString &messageId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM messages WHERE id = ?");
    q.addBindValue(messageId);
    return q.exec();
}

QStringList DatabaseManager::deleteConversation(const QString &conversationId)
{
    QStringList files;
    QSqlQuery q(m_db);
    q.prepare("SELECT message_type, content FROM messages WHERE conversation_id = ? AND message_type > 0");
    q.addBindValue(conversationId);
    if (q.exec()) {
        while (q.next()) {
            const QJsonObject o = QJsonDocument::fromJson(decode(q.value(1).toString()).toUtf8()).object();
            if (!o["path"].toString().isEmpty())
                files << o["path"].toString();
        }
    }
    q.prepare("DELETE FROM messages WHERE conversation_id = ?");
    q.addBindValue(conversationId);
    q.exec();
    return files;
}

void DatabaseManager::addPendingDelete(const QString &messageId, const QString &peerId)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO pending_deletes (message_id, peer_id) VALUES (?,?)");
    q.addBindValue(messageId);
    q.addBindValue(peerId);
    q.exec();
}

void DatabaseManager::removePendingDelete(const QString &messageId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM pending_deletes WHERE message_id = ?");
    q.addBindValue(messageId);
    q.exec();
}

QStringList DatabaseManager::pendingDeletes(const QString &peerId)
{
    QStringList out;
    QSqlQuery q(m_db);
    q.prepare("SELECT message_id FROM pending_deletes WHERE peer_id = ?");
    q.addBindValue(peerId);
    if (q.exec())
        while (q.next())
            out << q.value(0).toString();
    return out;
}

QStringList DatabaseManager::peersWithPendingDeletes()
{
    QStringList out;
    QSqlQuery q(m_db);
    if (q.exec("SELECT DISTINCT peer_id FROM pending_deletes"))
        while (q.next())
            out << q.value(0).toString();
    return out;
}
