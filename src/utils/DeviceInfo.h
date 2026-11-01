#ifndef DEVICEINFO_H
#define DEVICEINFO_H

#include <QObject>
#include <QString>

class DeviceInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isAndroid READ isAndroid CONSTANT)
    Q_PROPERTY(bool isDesktop READ isDesktop CONSTANT)
    Q_PROPERTY(QString platformName READ getPlatformName CONSTANT)

public:
    explicit DeviceInfo(QObject *parent = nullptr) : QObject(parent) {}

    static QString getDeviceName();
    static QString getPlatformName();
    static bool isAndroid();
    static bool isDesktop();
    /** Best private IPv4 address of this machine (Wi-Fi / Ethernet / hotspot), or empty. */
    static QString getLocalIpAddress();
};

#endif // DEVICEINFO_H
