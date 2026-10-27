#include "NotificationManager.h"
#include "utils/DeviceInfo.h"
#include <QDebug>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
    , m_enabled(true)
{
}

bool NotificationManager::enabled() const
{
    return m_enabled;
}

void NotificationManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged();
    }
}

void NotificationManager::showNotification(const QString &title, const QString &message)
{
    if (!m_enabled) {
        return;
    }
    
    if (DeviceInfo::isAndroid()) {
        showAndroidNotification(title, message);
    } else {
        showDesktopNotification(title, message);
    }
}

void NotificationManager::showMessageNotification(const QString &senderName, const QString &message)
{
    showNotification(QStringLiteral("New message from %1").arg(senderName), message);
}

void NotificationManager::showFileNotification(const QString &senderName, const QString &fileName)
{
    showNotification(QStringLiteral("File from %1").arg(senderName), 
                    QStringLiteral("Received: %1").arg(fileName));
}

void NotificationManager::showDesktopNotification(const QString &title, const QString &message)
{
    qDebug() << "Desktop notification:" << title << "-" << message;
    // Platform-specific implementations would go here
    // For Linux: libnotify, for macOS: NSUserNotification, for Windows: Toast notifications
}

void NotificationManager::showAndroidNotification(const QString &title, const QString &message)
{
    qDebug() << "Android notification:" << title << "-" << message;
#ifdef Q_OS_ANDROID
    // Android notification implementation would go here using JNI
#endif
}
