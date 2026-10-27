#ifndef MESSAGEMODEL_H
#define MESSAGEMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QDateTime>

struct Message
{
    QString messageId;
    QString fromUserId;
    QString toUserId;
    QString content;
    int messageType; // 0: text, 1: image, 2: file, 3: status
    QDateTime timestamp;
    bool isOwnMessage;
};

class MessageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum MessageRoles {
        MessageIdRole = Qt::UserRole + 1,
        FromUserIdRole,
        ToUserIdRole,
        ContentRole,
        MessageTypeRole,
        TimestampRole,
        IsOwnMessageRole,
        TimeStringRole
    };

    explicit MessageModel(QObject *parent = nullptr);
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    Q_INVOKABLE void addMessage(const QString &messageId, const QString &fromUserId,
                              const QString &toUserId, const QString &content,
                              int messageType, const QString &timestamp);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setCurrentUserId(const QString &userId);
    
    void loadConversation(const QString &conversationId);
    
private:
    QList<Message> m_messages;
    QString m_currentUserId;
};

#endif // MESSAGEMODEL_H
