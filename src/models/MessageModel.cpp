#include "MessageModel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QUrl>

MessageModel::MessageModel(QObject *parent) : QAbstractListModel(parent) {}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_messages.size());
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.size())
        return {};
    const Message &m = m_messages.at(index.row());
    switch (role) {
    case MessageIdRole: return m.messageId;
    case FromUserIdRole: return m.fromUserId;
    case ToUserIdRole: return m.toUserId;
    case ContentRole: return m.content;
    case MessageTypeRole: return m.messageType;
    case TimestampRole: return m.timestamp;
    case IsOwnMessageRole: return m.isOwnMessage;
    case TimeStringRole: return QLocale().toString(m.timestamp.time(), QLocale::ShortFormat);
    case StatusRole: return m.status;
    case AttachmentNameRole: return m.attachmentName;
    case AttachmentPathRole: return m.attachmentPath;
    case AttachmentUrlRole:
        return m.attachmentPath.isEmpty() ? QString() : QUrl::fromLocalFile(m.attachmentPath).toString();
    case CaptionRole: return m.caption;
    case DurationRole: return m.durationMs;
    case DurationTextRole: {
        const int sec = (m.durationMs + 500) / 1000;
        return QStringLiteral("%1:%2").arg(sec / 60).arg(sec % 60, 2, 10, QLatin1Char('0'));
    }
    case AttachmentSizeRole:
        return m.messageType > 0 ? QLocale().formattedDataSize(m.attachmentSize, 1) : QString();
    case DayStringRole: {
        // Used by the view to draw a date separator when the day changes.
        const QDate d = m.timestamp.date();
        if (d == QDate::currentDate()) return tr("Today");
        if (d == QDate::currentDate().addDays(-1)) return tr("Yesterday");
        return QLocale().toString(d, QLocale::LongFormat);
    }
    default: return {};
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const
{
    return {
        {MessageIdRole, "messageId"},   {FromUserIdRole, "fromUserId"},
        {ToUserIdRole, "toUserId"},     {ContentRole, "content"},
        {MessageTypeRole, "messageType"}, {TimestampRole, "timestamp"},
        {IsOwnMessageRole, "isOwnMessage"}, {TimeStringRole, "timeString"},
        {DayStringRole, "dayString"},   {StatusRole, "status"},
        {AttachmentNameRole, "attachmentName"}, {AttachmentPathRole, "attachmentPath"},
        {AttachmentUrlRole, "attachmentUrl"},   {AttachmentSizeRole, "attachmentSize"},
        {CaptionRole, "caption"}, {DurationRole, "duration"}, {DurationTextRole, "durationText"},
    };
}

Message MessageModel::make(const QString &id, const QString &from, const QString &to,
                           const QString &content, int type, const QString &timestamp, int status,
                           const QString &me)
{
    Message m;
    m.messageId = id;
    m.fromUserId = from;
    m.toUserId = to;
    m.content = content;
    m.messageType = type;
    m.timestamp = QDateTime::fromString(timestamp, Qt::ISODate);
    if (!m.timestamp.isValid())
        m.timestamp = QDateTime::currentDateTime();
    m.isOwnMessage = (from == me);
    m.status = status;
    if (type > 0) {
        const QJsonObject o = QJsonDocument::fromJson(content.toUtf8()).object();
        m.attachmentName = o["name"].toString();
        m.attachmentPath = o["path"].toString();
        m.attachmentSize = qint64(o["size"].toDouble());
        m.caption = o["caption"].toString();
        m.durationMs = o["duration"].toInt();
    }
    return m;
}

void MessageModel::addMessage(const QString &id, const QString &from, const QString &to,
                              const QString &content, int type, const QString &timestamp, int status)
{
    const int row = int(m_messages.size());
    beginInsertRows({}, row, row);
    m_messages.append(make(id, from, to, content, type, timestamp, status, m_currentUserId));
    endInsertRows();
    emit countChanged();
}

void MessageModel::setStatus(const QString &messageId, int status)
{
    for (int i = int(m_messages.size()) - 1; i >= 0; --i) {
        if (m_messages[i].messageId == messageId) {
            m_messages[i].status = status;
            emit dataChanged(index(i), index(i), {StatusRole});
            return;
        }
    }
}

void MessageModel::setMessages(const QVariantList &rows)
{
    beginResetModel();
    m_messages.clear();
    for (const QVariant &v : rows) {
        const QVariantMap r = v.toMap();
        m_messages.append(make(r["id"].toString(), r["fromUserId"].toString(),
                               r["toUserId"].toString(), r["content"].toString(),
                               r["messageType"].toInt(), r["timestamp"].toString(),
                               r["status"].toInt(), m_currentUserId));
    }
    endResetModel();
    emit countChanged();
}

void MessageModel::clear()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
    emit countChanged();
}

QString MessageModel::attachmentJson(const QString &name, qint64 size, const QString &path,
                                     const QString &caption, int durationMs)
{
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"name", name}, {"size", double(size)},
                                                       {"path", path}, {"caption", caption},
                                                       {"duration", durationMs}})
                                 .toJson(QJsonDocument::Compact));
}

QString MessageModel::preview(int messageType, const QString &content)
{
    if (messageType == 0)
        return content;
    const QJsonObject o = QJsonDocument::fromJson(content.toUtf8()).object();
    const QString name = o["name"].toString();
    if (messageType == 1) {
        const QString caption = o["caption"].toString();
        return caption.isEmpty() ? tr("Photo") : tr("Photo: %1").arg(caption);
    }
    if (messageType == 3)
        return tr("Voice message");
    return name.isEmpty() ? tr("File") : name;
}

void MessageModel::removeMessage(const QString &messageId)
{
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages[i].messageId == messageId) {
            beginRemoveRows({}, i, i);
            m_messages.removeAt(i);
            endRemoveRows();
            emit countChanged();
            return;
        }
    }
}

QVariantList MessageModel::search(const QString &query) const
{
    QVariantList rows;
    const QString q = query.trimmed();
    if (q.isEmpty())
        return rows;
    for (int i = 0; i < m_messages.size(); ++i) {
        const Message &m = m_messages.at(i);
        const QString haystack = m.messageType == 0 ? m.content : (m.caption + " " + m.attachmentName);
        if (haystack.contains(q, Qt::CaseInsensitive))
            rows.append(i);
    }
    return rows;
}
