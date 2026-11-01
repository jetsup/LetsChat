#include "BackgroundService.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTimer>
#include <QDebug>
#include <QSettings>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
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
    s_instance = this;
    m_runInBackground = QSettings().value("app/runInBackground", true).toBool();
}

BackgroundService::~BackgroundService()
{
#ifndef Q_OS_ANDROID
    delete m_tray;
#endif
}

void BackgroundService::requestBack()
{
    if (s_instance)
        QMetaObject::invokeMethod(s_instance, &BackgroundService::backRequested, Qt::QueuedConnection);
}

#ifdef Q_OS_ANDROID
// JNI bridge for MainActivity (called on the Android UI thread, so only touch atomics/queued calls)
static jint jniBackDepth(JNIEnv *, jclass) { return BackgroundService::backDepth(); }
static void jniBackRequested(JNIEnv *, jclass) { BackgroundService::requestBack(); }
static void jniIntentArrived(JNIEnv *, jclass) { BackgroundService::requestPendingChat(); }
#endif

void BackgroundService::requestPendingChat()
{
    if (s_instance)
        QMetaObject::invokeMethod(s_instance, [] { s_instance->pullPendingChat(); }, Qt::QueuedConnection);
}

// Picks up the conversation of a tapped notification (Android) once the UI can show it.
void BackgroundService::pullPendingChat()
{
#ifdef Q_OS_ANDROID
    const QString peer = QJniObject::callStaticObjectMethod(
        "com/smalnets/letschat/MainActivity", "takePendingPeer", "()Ljava/lang/String;").toString();
    if (!peer.isEmpty())
        emit openChatRequested(peer);
#endif
}

void BackgroundService::start()
{
#ifdef Q_OS_ANDROID
    static bool registered = false;
    if (!registered) {
        registered = true;
        const JNINativeMethod methods[] = {
            {"nativeBackDepth", "()I", reinterpret_cast<void *>(jniBackDepth)},
            {"nativeBackRequested", "()V", reinterpret_cast<void *>(jniBackRequested)},
            {"nativeIntentArrived", "()V", reinterpret_cast<void *>(jniIntentArrived)},
        };
        QJniEnvironment env;
        if (!env.registerNativeMethods("com/smalnets/letschat/MainActivity", methods, 3))
            qWarning() << "Could not register the Android back bridge";
    }
    QTimer::singleShot(800, this, [this] { pullPendingChat(); });   // app started by a notification tap
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState st) {
        if (st == Qt::ApplicationActive)
            pullPendingChat();
    });
#endif
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
        // clicking a message balloon shows the window *and* the conversation it was about
        connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
            emit showWindowRequested();
            if (!m_lastTrayPeer.isEmpty())
                emit openChatRequested(m_lastTrayPeer);
        });
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

void BackgroundService::showTrayMessage(const QString &title, const QString &message, const QString &peerId)
{
#ifndef Q_OS_ANDROID
    m_lastTrayPeer = peerId;
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

QString BackgroundService::manufacturer() const
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticObjectMethod(kServiceClass, "manufacturer", "()Ljava/lang/String;").toString();
#else
    return {};
#endif
}

void BackgroundService::openAppSettings()
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid())
            QJniObject::callStaticMethod<void>(kServiceClass, "openAppSettings",
                                               "(Landroid/app/Activity;)V", activity.object());
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
