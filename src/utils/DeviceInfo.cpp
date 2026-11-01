#include "DeviceInfo.h"
#include <QHostInfo>
#include <QNetworkInterface>
#include <QSysInfo>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

QString DeviceInfo::getDeviceName()
{
#ifdef Q_OS_ANDROID
    // QHostInfo::localHostName() is just "localhost" on Android; use the marketing model instead.
    const QJniObject model = QJniObject::getStaticObjectField<jstring>(
        "android/os/Build", "MODEL");
    if (model.isValid() && !model.toString().isEmpty())
        return model.toString();
#endif
    QString name = QHostInfo::localHostName();
    if (name.isEmpty())
        name = QSysInfo::machineHostName();
    return name.isEmpty() ? QStringLiteral("LetsChat device") : name;
}

QString DeviceInfo::getPlatformName() { return QSysInfo::productType(); }

bool DeviceInfo::isAndroid()
{
#ifdef Q_OS_ANDROID
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

static bool isPrivateV4(const QHostAddress &a)
{
    const quint32 ip = a.toIPv4Address();
    return (ip >> 24) == 10                       // 10.0.0.0/8
        || (ip >> 20) == 0xAC1                    // 172.16.0.0/12
        || (ip >> 16) == 0xC0A8;                  // 192.168.0.0/16
}

QString DeviceInfo::getLocalIpAddress()
{
    QString fallback;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack))
            continue;
        // Skip cellular / VPN / virtual bridges, they never carry our LAN.
        const QString n = iface.name();
        if (n.startsWith("rmnet") || n.startsWith("ccmni") || n.startsWith("tun")
            || n.startsWith("docker") || n.startsWith("veth") || n.startsWith("virbr")
            || n.startsWith("dummy"))
            continue;
        for (const QNetworkAddressEntry &e : iface.addressEntries()) {
            const QHostAddress ip = e.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLoopback())
                continue;
            if (isPrivateV4(ip))
                return ip.toString();
            if (fallback.isEmpty() && !ip.isLinkLocal())
                fallback = ip.toString();
        }
    }
    return fallback;
}
