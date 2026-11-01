#include "DatabaseManager.h"
#include "CryptoUtils.h"
#include <QDateTime>
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
    q.prepare("INSERT INTO peers (user_id, alias, device_name, ip_address, bt_address, last_seen) "
              "VALUES (?,?,?,?,?,?) "
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
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
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
    if (!q.exec("SELECT user_id, alias, device_name, ip_address, bt_address, last_seen FROM peers"))
        return out;
    while (q.next()) {
        QVariantMap p;
        p["userId"] = q.value(0).toString();
        p["alias"] = q.value(1).toString();
        p["deviceName"] = q.value(2).toString();
        p["ipAddress"] = q.value(3).toString();
        p["btAddress"] = q.value(4).toString();
        p["lastSeen"] = q.value(5).toString();
        out.append(p);
    }
    return out;
}
