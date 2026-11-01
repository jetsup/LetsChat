#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QObject>
#include <QString>

class BackgroundService;

/** Shows message notifications: Android notification channel / desktop tray balloon. */
class NotificationManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit NotificationManager(QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    void setBackgroundService(BackgroundService *bg) { m_background = bg; }

    void showMessageNotification(const QString &senderName, const QString &message, const QString &peerId);

signals:
    void enabledChanged();

private:
    bool m_enabled = true;
    BackgroundService *m_background = nullptr;
};

#endif // NOTIFICATIONMANAGER_H
