#include "DeviceInfo.h"
#include <QHostInfo>
#include <QSysInfo>
#include <QNetworkInterface>
#include <QDebug>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

DeviceInfo::DeviceInfo(QObject *parent)
    : QObject(parent)
{
}

QString DeviceInfo::getDeviceName()
{
    QString name = QHostInfo::localHostName();
    if (name.isEmpty()) {
        name = QSysInfo::machineHostName();
    }
    return name;
}

QString DeviceInfo::getPlatformName()
{
    return QSysInfo::productType();
}

QString DeviceInfo::getOSVersion()
{
    return QSysInfo::productVersion();
}

bool DeviceInfo::isAndroid()
{
#if defined(Q_OS_ANDROID)
    return true;
#else
    return false;
#endif
}

bool DeviceInfo::isIOS()
{
#if defined(Q_OS_IOS)
    return true;
#else
    return false;
#endif
}

bool DeviceInfo::isDesktop()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return false;
#else
    return true;
#endif
}

QString DeviceInfo::getLocalIpAddress()
{
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol &&
            address != QHostAddress::LocalHost) {
            return address.toString();
        }
    }
    return QHostAddress(QHostAddress::LocalHost).toString();
}
