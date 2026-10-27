#include "IdentityManager.h"
#include <QSettings>
#include <QHostInfo>
#include <QSysInfo>
#include <QUuid>
#include <QDebug>

IdentityManager::IdentityManager(QObject *parent)
    : QObject(parent)
    , m_isFirstRun(true)
{
    loadIdentity();
}

QString IdentityManager::userId() const
{
    return m_userId;
}

QString IdentityManager::alias() const
{
    return m_alias.isEmpty() ? m_deviceName : m_alias;
}

void IdentityManager::setAlias(const QString &alias)
{
    if (m_alias != alias) {
        m_alias = alias;
        saveIdentity();
        emit aliasChanged();
    }
}

QString IdentityManager::deviceName() const
{
    return m_deviceName;
}

bool IdentityManager::isFirstRun() const
{
    return m_isFirstRun;
}

void IdentityManager::generateIfNeeded()
{
    if (m_userId.isEmpty()) {
        m_userId = generateUniqueId();
        m_isFirstRun = true;
        saveIdentity();
        emit identityChanged();
    }
}

void IdentityManager::resetIdentity()
{
    QSettings settings;
    settings.remove("identity/userId");
    settings.remove("identity/alias");
    loadIdentity();
    emit identityChanged();
}

void IdentityManager::loadIdentity()
{
    QSettings settings;
    m_userId = settings.value("identity/userId", QString()).toString();
    m_alias = settings.value("identity/alias", QString()).toString();
    m_deviceName = QHostInfo::localHostName();
    if (m_deviceName.isEmpty()) {
        m_deviceName = QSysInfo::machineHostName();
    }
    m_isFirstRun = m_userId.isEmpty();
}

void IdentityManager::saveIdentity()
{
    QSettings settings;
    settings.setValue("identity/userId", m_userId);
    settings.setValue("identity/alias", m_alias);
    settings.sync();
}

QString IdentityManager::generateUniqueId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
