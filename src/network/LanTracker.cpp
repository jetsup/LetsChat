#include "LanTracker.h"
#include "utils/DeviceInfo.h"
#include <QDebug>

LanTracker::LanTracker(QObject *parent)
    : QObject(parent), m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &LanTracker::check);
}

void LanTracker::start()
{
    m_timer->start(4000);
    check();
}

void LanTracker::check()
{
    const QString ip = DeviceInfo::getLocalIpAddress();
    // "Network id" = the /24 we are on. Good enough to notice Wi-Fi A -> Wi-Fi B.
    QString id;
    const QStringList parts = ip.split('.');
    if (parts.size() == 4)
        id = parts.mid(0, 3).join('.');

    if (ip != m_localIp) {
        m_localIp = ip;
        emit networkChanged();
    }
    if (id != m_networkId) {
        m_networkId = id;
        qInfo() << "LAN: network is now" << (id.isEmpty() ? "<none>" : id) << "local ip" << ip;
        if (!id.isEmpty())
            emit joinedNewLan(id);
    }
}
