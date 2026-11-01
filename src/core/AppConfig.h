#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QObject>
#include <QString>

class AppConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    explicit AppConfig(QObject *parent = nullptr) : QObject(parent) {}

    QString appName() const { return QStringLiteral("LetsChat"); }
    QString appVersion() const { return QStringLiteral("0.2"); }

    // LAN
    static constexpr quint16 DISCOVERY_PORT = 45454;   // UDP broadcast
    static constexpr quint16 TRANSFER_PORT = 45455;    // TCP messages
    static constexpr int DISCOVERY_INTERVAL_MS = 3000;
    static constexpr int DISCOVERY_INTERVAL_BG_MS = 8000;   // while the UI is in the background
    static constexpr int PEER_TIMEOUT_MS = 12000;      // peer is "offline" after this much silence
    static constexpr int SEND_TIMEOUT_MS = 5000;
    static constexpr int MAX_FRAME_BYTES = 1024 * 1024;
    static constexpr int AVATAR_SIZE = 256;               // profile pictures are stored as 256x256 JPEGs
    static constexpr qint64 MAX_FILE_BYTES = 25LL * 1024 * 1024;   // photos / documents

    // Wire protocol version (bump when messages change incompatibly)
    static constexpr int PROTOCOL_VERSION = 1;

    // Bluetooth (RFCOMM / SPP style custom service)
    static inline const QString BLUETOOTH_SERVICE_UUID =
        QStringLiteral("e3a1c5a4-7c6e-4b0a-9f55-1d2b7a0c9e31");
    static inline const QString BLUETOOTH_SERVICE_NAME = QStringLiteral("LetsChat");
};

#endif // APPCONFIG_H
