#include "NotificationManager.h"
#include "platform/BackgroundService.h"
#include <QDebug>
#include <QSettings>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QCoreApplication>
#endif

NotificationManager::NotificationManager(QObject *parent) : QObject(parent)
{
    m_enabled = QSettings().value("app/enableNotifications", true).toBool();
}

void NotificationManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    QSettings().setValue("app/enableNotifications", enabled);
    if (enabled && m_background)
        m_background->requestNotificationPermission();
    emit enabledChanged();
}

void NotificationManager::showMessageNotification(const QString &senderName, const QString &message,
                                                  const QString &peerId)
{
    if (!m_enabled)
        return;
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        qWarning() << "Notification: no Android context";
        return;
    }
    QJniObject::callStaticMethod<void>(
        "com/smalnets/letschat/LetsChatService", "showMessageNotification",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;I)V",
        context.object(), QJniObject::fromString(senderName).object<jstring>(),
        QJniObject::fromString(message).object<jstring>(), jint(qHash(peerId)));
#else
    Q_UNUSED(peerId)
    if (m_background)
        m_background->showTrayMessage(senderName, message);
    else
        qInfo() << "Notification:" << senderName << message;
#endif
}
