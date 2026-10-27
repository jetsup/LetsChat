#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QUuid>
#include <QDateTime>

#include "core/AppConfig.h"
#include "core/IdentityManager.h"
#include "core/ThemeManager.h"
#include "core/SettingsManager.h"
#include "network/DiscoveryService.h"
#include "network/PeerManager.h"
#include "network/TransferService.h"
#include "network/LanTracker.h"
#include "storage/DatabaseManager.h"
#include "storage/CryptoUtils.h"
#include "notifications/NotificationManager.h"
#include "models/PeerListModel.h"
#include "models/MessageModel.h"
#include "platform/BackgroundService.h"
#include "utils/DeviceInfo.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Set application info
    app.setOrganizationName("SmalNets");
    app.setOrganizationDomain("smalnets.com");
    app.setApplicationName("LetsChat");
    app.setApplicationVersion("0.1");

    QQmlApplicationEngine engine;
    
    // Create instances
    AppConfig appConfig;
    IdentityManager identityManager;
    ThemeManager themeManager;
    SettingsManager settingsManager;
    PeerManager peerManager;
    TransferService transferService;
    LanTracker lanTracker;
    DatabaseManager databaseManager;
    CryptoUtils cryptoUtils;
    NotificationManager notificationManager;
    MessageModel messageModel;
    BackgroundService backgroundService;
    DeviceInfo deviceInfo;
    
    // Initialize database
    databaseManager.initialize();
    
    // Set message model current user
    messageModel.setCurrentUserId(identityManager.userId());
    
    // Register to QML
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "AppConfig", &appConfig);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "ThemeManager", &themeManager);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "SettingsManager", &settingsManager);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "NotificationManager", &notificationManager);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "CryptoUtils", &cryptoUtils);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "DeviceInfo", &deviceInfo);
    qmlRegisterSingletonInstance("LetsChat", 1, 0, "BackgroundService", &backgroundService);
    
    engine.rootContext()->setContextProperty("appConfig", &appConfig);
    engine.rootContext()->setContextProperty("identityManager", &identityManager);
    engine.rootContext()->setContextProperty("themeManager", &themeManager);
    engine.rootContext()->setContextProperty("settingsManager", &settingsManager);
    engine.rootContext()->setContextProperty("peerManager", &peerManager);
    engine.rootContext()->setContextProperty("transferService", &transferService);
    engine.rootContext()->setContextProperty("lanTracker", &lanTracker);
    engine.rootContext()->setContextProperty("databaseManager", &databaseManager);
    engine.rootContext()->setContextProperty("notificationManager", &notificationManager);
    engine.rootContext()->setContextProperty("messageModel", &messageModel);
    engine.rootContext()->setContextProperty("backgroundService", &backgroundService);
    engine.rootContext()->setContextProperty("deviceInfo", &deviceInfo);
    
    // Connect signals
    QObject::connect(&identityManager, &IdentityManager::identityChanged, [&]() {
        messageModel.setCurrentUserId(identityManager.userId());
        peerManager.setLocalIdentity(identityManager.userId(), identityManager.alias(), deviceInfo.getDeviceName());
    });
    
    QObject::connect(&transferService, &TransferService::textMessageReceived, 
                     [&](const QString &fromUserId, const QString &message, const QString &senderIp) {
        notificationManager.showMessageNotification(fromUserId, message);
        messageModel.addMessage(QUuid::createUuid().toString(QUuid::WithoutBraces), 
                               fromUserId, identityManager.userId(), message, 0, 
                               QDateTime::currentDateTime().toString(Qt::ISODate));
        databaseManager.saveMessage(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                   fromUserId, identityManager.userId(), message, 0,
                                   QDateTime::currentDateTime().toString(Qt::ISODate));
    });
    
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    
    engine.loadFromModule("LetsChat", "Main");

    return QGuiApplication::exec();
}
