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
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
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
#include "security/SecurityManager.h"
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

#ifndef Q_OS_ANDROID
    // Only one copy may run per user (two copies would fight over the ports and the database).
    // Android's launchMode="singleTask" already guarantees this there.
    QString runDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runDir.isEmpty())
        runDir = QDir::tempPath();
    const QString userTag = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME", "user"));
    const QString serverName = "letschat-single-instance-" + userTag;
    QLockFile instanceLock(runDir + "/letschat-" + userTag + ".lock");
    instanceLock.setStaleLockTime(0);          // a crashed copy leaves a lock that is detected by its dead pid
    if (!instanceLock.tryLock(0)) {
        // Already running: ask that copy to show its window, then quit.
        QLocalSocket other;
        other.connectToServer(serverName);
        if (other.waitForConnected(1500)) {
            other.write("show");
            other.waitForBytesWritten(1500);
        }
        qInfo() << "LetsChat is already running - bringing that window to the front.";
        return 0;
    }
    QLocalServer::removeServer(serverName);
    QLocalServer instanceServer;
    instanceServer.setSocketOptions(QLocalServer::UserAccessOption);   // nobody else may poke it
    if (!instanceServer.listen(serverName))
        qWarning() << "Single-instance server:" << instanceServer.errorString();
#endif

    // Backend objects. They live on the stack for the whole run, the engine only borrows them.
    AppConfig appConfig;
    DatabaseManager database;
    if (!database.initialize())
        qWarning() << "The message database could not be opened - history will not be saved";

    SecurityManager security(&database);
    IdentityManager identity;
    ThemeManager themeManager;
    DeviceInfo device;
    LanTracker lanTracker;
    const QString receiveDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/received";
    TransferService transfer(receiveDir, &security);
    BluetoothService bluetooth(receiveDir, &security);
    BackgroundService background;
#ifndef Q_OS_ANDROID
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &background, [&]() {
        while (QLocalSocket *c = instanceServer.nextPendingConnection()) {
            c->deleteLater();
            emit background.showWindowRequested();
        }
    });
#endif
    MediaService media(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/voice");
    NotificationManager notifier;
    notifier.setBackgroundService(&background);
    PeerListModel::setAvatarDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatars");
    PeerManager peers(&database);
    ChatController chat(&identity, &peers, &transfer, &bluetooth, &database, &notifier, &lanTracker,
                        &background, &security);

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
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "Security", &security);

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
