#ifndef DEVICEINFO_H
#define DEVICEINFO_H

#include <QObject>
#include <QString>

class DeviceInfo : public QObject
{
    Q_OBJECT

public:
    explicit DeviceInfo(QObject *parent = nullptr);
    
    Q_INVOKABLE static QString getDeviceName();
    Q_INVOKABLE static QString getPlatformName();
    Q_INVOKABLE static QString getOSVersion();
    Q_INVOKABLE static bool isAndroid();
    Q_INVOKABLE static bool isIOS();
    Q_INVOKABLE static bool isDesktop();
    Q_INVOKABLE static QString getLocalIpAddress();
};

#endif // DEVICEINFO_H
