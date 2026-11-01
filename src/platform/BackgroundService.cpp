#include "BackgroundService.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTimer>
#include <QDebug>
#include <QSettings>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#else
#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#endif

#ifdef Q_OS_ANDROID
static const char *kServiceClass = "com/smalnets/letschat/LetsChatService";
#endif

BackgroundService::BackgroundService(QObject *parent) : QObject(parent)
{
    m_runInBackground = QSettings().value("app/runInBackground", true).toBool();
}

BackgroundService::~BackgroundService()
{
#ifndef Q_OS_ANDROID
    delete m_tray;
#endif
}

void BackgroundService::start()
{
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            [](Qt::ApplicationState st) { qInfo() << "App state:" << st; });
#ifndef Q_OS_ANDROID
    if (!m_tray && QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray = new QSystemTrayIcon(QIcon(":/icons/app.png"), this);
        m_tray->setToolTip(QStringLiteral("LetsChat"));
        auto *menu = new QMenu();
        connect(menu->addAction(tr("Open LetsChat")), &QAction::triggered, this,
                &BackgroundService::showWindowRequested);
        menu->addSeparator();
        connect(menu->addAction(tr("Quit")), &QAction::triggered, this, &BackgroundService::quit);
        m_tray->setContextMenu(menu);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
            if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
                emit showWindowRequested();
        });
        connect(m_tray, &QSystemTrayIcon::messageClicked, this, &BackgroundService::showWindowRequested);
        m_tray->show();
    }
    // With a tray icon the app must survive its window being hidden.
    QApplication::setQuitOnLastWindowClosed(m_tray == nullptr);
#endif
    if (m_runInBackground)
        startService();
}

void BackgroundService::setRunInBackground(bool on)
{
    if (m_runInBackground == on)
        return;
    m_runInBackground = on;
    QSettings().setValue("app/runInBackground", on);
    if (on) startService(); else stopService();
    emit runInBackgroundChanged();
}

void BackgroundService::startService()
{
    if (m_serviceRunning)
        return;
#ifdef Q_OS_ANDROID
    requestNotificationPermission();
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid())
        QJniObject::callStaticMethod<void>(kServiceClass, "start", "(Landroid/content/Context;)V",
                                           context.object());
    qInfo() << "Background: Android foreground service requested";
#else
    qInfo() << "Background: desktop tray mode" << (m_tray ? "(tray icon active)" : "(no system tray)");
#endif
    m_serviceRunning = true;
    emit serviceRunningChanged();
}

void BackgroundService::stopService()
{
    if (!m_serviceRunning)
        return;
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid())
        QJniObject::callStaticMethod<void>(kServiceClass, "stop", "(Landroid/content/Context;)V",
                                           context.object());
#endif
    m_serviceRunning = false;
    emit serviceRunningChanged();
}

void BackgroundService::requestNotificationPermission()
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid())
            QJniObject::callStaticMethod<void>(kServiceClass, "requestNotificationPermission",
                                               "(Landroid/app/Activity;)V", activity.object());
    });
#endif
}

void BackgroundService::showTrayMessage(const QString &title, const QString &message)
{
#ifndef Q_OS_ANDROID
    if (m_tray)
        m_tray->showMessage(title, message, QSystemTrayIcon::Information, 5000);
    else
        qInfo() << "Notification:" << title << message;
#else
    Q_UNUSED(title) Q_UNUSED(message)
#endif
}

bool BackgroundService::shouldHideOnClose() const
{
#ifdef Q_OS_ANDROID
    return m_runInBackground;
#else
    return m_runInBackground && m_tray && !m_quitting;
#endif
}

void BackgroundService::moveToBackground()
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid())
            QJniObject::callStaticMethod<void>(kServiceClass, "moveToBackground",
                                               "(Landroid/app/Activity;)V", activity.object());
    });
#endif
}

bool BackgroundService::batteryUnrestricted() const
{
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    return context.isValid()
           && QJniObject::callStaticMethod<jboolean>(kServiceClass, "isIgnoringBatteryOptimizations",
                                                     "(Landroid/content/Context;)Z", context.object());
#else
    return true;
#endif
}

void BackgroundService::requestBatteryExemption()
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([this] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid())
            QJniObject::callStaticMethod<void>(kServiceClass, "requestIgnoreBatteryOptimizations",
                                               "(Landroid/app/Activity;)V", activity.object());
        emit batteryChanged();
    });
#endif
}

void BackgroundService::quit()
{
    m_quitting = true;
    stopService();
#ifndef Q_OS_ANDROID
    if (m_tray)
        m_tray->hide();
#endif
    QCoreApplication::quit();
}
