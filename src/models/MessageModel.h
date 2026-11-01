#ifndef MESSAGEMODEL_H
#define MESSAGEMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVariantList>

struct Message
{
    QString messageId;
    QString fromUserId;
    QString toUserId;
    QString content;
    int messageType = 0;      // 0: text
    QDateTime timestamp;
    bool isOwnMessage = false;
    int status = 1;           // 0: sending, 1: sent/received, 2: failed
    // for messageType 1 (image) and 2 (file) `content` is {"name","size","path"} JSON
    QString attachmentName, attachmentPath;
    qint64 attachmentSize = 0;
};

class MessageModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum MessageRoles {
        MessageIdRole = Qt::UserRole + 1,
        FromUserIdRole,
        ToUserIdRole,
        ContentRole,
        MessageTypeRole,
        TimestampRole,
        IsOwnMessageRole,
        TimeStringRole,
        DayStringRole,
        StatusRole,
        AttachmentNameRole,
        AttachmentPathRole,
        AttachmentUrlRole,
        AttachmentSizeRole
    };

    explicit MessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setCurrentUserId(const QString &userId) { m_currentUserId = userId; }
    void addMessage(const QString &messageId, const QString &fromUserId, const QString &toUserId,
                    const QString &content, int messageType, const QString &timestamp, int status = 1);
    void setStatus(const QString &messageId, int status);
    void setMessages(const QVariantList &rows);   // replaces the whole conversation
    void clear();

    /** Text shown in lists / notifications for a message of the given type. */
    static QString preview(int messageType, const QString &content);
    static QString attachmentJson(const QString &name, qint64 size, const QString &path);

signals:
    void countChanged();

private:
    static Message make(const QString &id, const QString &from, const QString &to,
                        const QString &content, int type, const QString &timestamp, int status,
                        const QString &me);

    QList<Message> m_messages;
    QString m_currentUserId;
};

#endif // MESSAGEMODEL_H
