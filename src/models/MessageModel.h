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
    int status = 1;           // 0: sending, 1: sent/received, 2: failed, 3: queued
    // type 0 text, 1 image, 2 file, 3 voice note, 4 deleted placeholder; for 1-3 `content` is
    // {"name","size","path"} JSON `content` is {"name","size","path"} JSON
    QString attachmentName, attachmentPath, caption;
    // quoted message (reply): empty replyId = not a reply
    QString replyId, replyText;
    bool replyFromMe = false;
    int replyType = 0;
    int durationMs = 0;
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
        AttachmentSizeRole,
        CaptionRole,
        DurationRole,
        DurationTextRole,
        ReplyIdRole,
        ReplyTextRole,
        ReplyFromMeRole,
        ReplyTypeRole
    };

    explicit MessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setCurrentUserId(const QString &userId) { m_currentUserId = userId; }
    void addMessage(const QString &messageId, const QString &fromUserId, const QString &toUserId,
                    const QString &content, int messageType, const QString &timestamp, int status = 1,
                    const QString &replyJson = {});
    void setStatus(const QString &messageId, int status);
    void removeMessage(const QString &messageId);
    /** Turns a message into the "deleted" placeholder (type 4). */
    void markDeleted(const QString &messageId);
    int rowOf(const QString &messageId) const;
    static QString replyJsonFor(const QString &id, const QString &fromUserId, const QString &text, int type);
    /** Rows whose text, caption or file name contains `query` (case-insensitive), oldest first. */
    QVariantList search(const QString &query) const;
    void setMessages(const QVariantList &rows);   // replaces the whole conversation
    void clear();

    /** Text shown in lists / notifications for a message of the given type. */
    static QString preview(int messageType, const QString &content);
    static QString attachmentJson(const QString &name, qint64 size, const QString &path,
                                  const QString &caption = {}, int durationMs = 0);

signals:
    void countChanged();

private:
    static Message make(const QString &id, const QString &from, const QString &to,
                        const QString &content, int type, const QString &timestamp, int status,
                        const QString &me, const QString &replyJson);

    QList<Message> m_messages;
    QString m_currentUserId;
};

#endif // MESSAGEMODEL_H
