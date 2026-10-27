#include "AppConfig.h"

AppConfig::AppConfig(QObject *parent)
    : QObject(parent)
{
}

QString AppConfig::appName() const
{
    return QStringLiteral("LetsChat");
}

QString AppConfig::appVersion() const
{
    return QStringLiteral("0.1");
}

QString AppConfig::organizationName() const
{
    return QStringLiteral("LetsChat");
}

QString AppConfig::organizationDomain() const
{
    return QStringLiteral("letschat.local");
}
