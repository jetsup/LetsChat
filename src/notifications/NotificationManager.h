#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QObject>
#include <QString>

class NotificationManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit NotificationManager(QObject *parent = nullptr);
    
    bool enabled() const;
    void setEnabled(bool enabled);
    
    Q_INVOKABLE void showNotification(const QString &title, const QString &message);
    Q_INVOKABLE void showMessageNotification(const QString &senderName, const QString &message);
    Q_INVOKABLE void showFileNotification(const QString &senderName, const QString &fileName);
    
signals:
    void enabledChanged();
    void notificationClicked(const QString &data);
    
private:
    void showDesktopNotification(const QString &title, const QString &message);
    void showAndroidNotification(const QString &title, const QString &message);
    
    bool m_enabled;
};

#endif // NOTIFICATIONMANAGER_H
