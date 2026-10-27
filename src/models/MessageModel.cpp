#include "MessageModel.h"

MessageModel::MessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_messages.count();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.size())
        return QVariant();
    
    const Message &message = m_messages[index.row()];
    
    switch (role) {
    case MessageIdRole:
        return message.messageId;
    case FromUserIdRole:
        return message.fromUserId;
    case ToUserIdRole:
        return message.toUserId;
    case ContentRole:
        return message.content;
    case MessageTypeRole:
        return message.messageType;
    case TimestampRole:
        return message.timestamp;
    case IsOwnMessageRole:
        return message.isOwnMessage;
    case TimeStringRole:
        return message.timestamp.toString("hh:mm");
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[MessageIdRole] = "messageId";
    roles[FromUserIdRole] = "fromUserId";
    roles[ToUserIdRole] = "toUserId";
    roles[ContentRole] = "content";
    roles[MessageTypeRole] = "messageType";
    roles[TimestampRole] = "timestamp";
    roles[IsOwnMessageRole] = "isOwnMessage";
    roles[TimeStringRole] = "timeString";
    return roles;
}

void MessageModel::addMessage(const QString &messageId, const QString &fromUserId,
                             const QString &toUserId, const QString &content,
                             int messageType, const QString &timestamp)
{
    beginInsertRows(QModelIndex(), m_messages.count(), m_messages.count());
    Message message;
    message.messageId = messageId;
    message.fromUserId = fromUserId;
    message.toUserId = toUserId;
    message.content = content;
    message.messageType = messageType;
    message.timestamp = QDateTime::fromString(timestamp, Qt::ISODate);
    if (message.timestamp.isNull()) {
        message.timestamp = QDateTime::currentDateTime();
    }
    message.isOwnMessage = (fromUserId == m_currentUserId);
    m_messages.append(message);
    endInsertRows();
}

void MessageModel::clear()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
}

void MessageModel::setCurrentUserId(const QString &userId)
{
    m_currentUserId = userId;
}

void MessageModel::loadConversation(const QString &conversationId)
{
    // Implementation would load from DatabaseManager
    Q_UNUSED(conversationId);
}
