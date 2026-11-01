#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <memory>

#ifndef Q_OS_ANDROID
#include <QApplication>
#endif

#include "chat/ChatController.h"
#include "core/AppConfig.h"
#include "core/IdentityManager.h"
#include "core/ThemeManager.h"
#include "network/BluetoothService.h"
#include "network/LanTracker.h"
#include "network/PeerManager.h"
#include "network/TransferService.h"
#include "notifications/NotificationManager.h"
#include "models/PeerListModel.h"
#include "platform/BackgroundService.h"
#include "platform/MediaService.h"
#include "storage/DatabaseManager.h"
#include "utils/DeviceInfo.h"

int main(int argc, char *argv[])
{
    // Desktop needs QApplication for the system-tray icon; Android keeps the lighter QGuiApplication.
#ifdef Q_OS_ANDROID
    QGuiApplication app(argc, argv);
#else
    QApplication app(argc, argv);
#endif
    app.setOrganizationName("SmalNets");
    app.setOrganizationDomain("smalnets.com");
    app.setApplicationName("LetsChat");
    app.setApplicationVersion("0.2");
    app.setWindowIcon(QIcon(":/icons/app.png"));

    QQuickStyle::setStyle("Material");

    // Backend objects. They live on the stack for the whole run, the engine only borrows them.
    AppConfig appConfig;
    DatabaseManager database;
    if (!database.initialize())
        qWarning() << "The message database could not be opened - history will not be saved";

    IdentityManager identity;
    ThemeManager themeManager;
    DeviceInfo device;
    LanTracker lanTracker;
    const QString receiveDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/received";
    TransferService transfer(receiveDir);
    BluetoothService bluetooth(receiveDir);
    BackgroundService background;
    MediaService media(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/voice");
    NotificationManager notifier;
    notifier.setBackgroundService(&background);
    PeerListModel::setAvatarDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatars");
    PeerManager peers(&database);
    ChatController chat(&identity, &peers, &transfer, &bluetooth, &database, &notifier, &lanTracker,
                        &background);

    // Start everything as soon as the user has an identity (immediately, or after onboarding).
    if (identity.onboarded())
        QMetaObject::invokeMethod(&chat, &ChatController::startServices, Qt::QueuedConnection);
    QObject::connect(&identity, &IdentityManager::onboardedChanged, &chat,
                     &ChatController::startServices);

    qmlRegisterSingletonInstance("LetsChat", 1, 0, "AppConfig", &appConfig);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "Identity", &identity);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "ThemeManager", &themeManager);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "Device", &device);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "Peers", &peers);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "Chat", &chat);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "Bluetooth", &bluetooth);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "Background", &background);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "Notifier", &notifier);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "Media", &media);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("LetsChat", "Main");

#ifdef Q_OS_ANDROID
    // android.app.background_running=true (AndroidManifest.xml) keeps Qt's event loop alive while the
    // activity is in the background, which discovery, the TCP server and Bluetooth need.
    app.setQuitOnLastWindowClosed(false);
#endif

    return app.exec();
}
