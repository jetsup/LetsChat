#include "IdentityManager.h"
#include "utils/DeviceInfo.h"
#include <QSettings>
#include <QUuid>

IdentityManager::IdentityManager(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    m_userId = s.value("identity/userId").toString();
    if (m_userId.isEmpty()) {
        m_userId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue("identity/userId", m_userId);
    }
    m_alias = s.value("identity/alias").toString();
    m_onboarded = s.value("identity/onboarded", false).toBool();
    m_deviceName = DeviceInfo::getDeviceName();
}

QString IdentityManager::alias() const
{
    return m_alias.isEmpty() ? m_deviceName : m_alias;
}

void IdentityManager::setAlias(const QString &alias)
{
    const QString trimmed = alias.trimmed();
    // Setting the alias to the device name (or nothing) means "use the default".
    const QString stored = (trimmed == m_deviceName) ? QString() : trimmed;
    if (m_alias == stored)
        return;
    m_alias = stored;
    QSettings().setValue("identity/alias", m_alias);
    emit aliasChanged();
}

void IdentityManager::completeOnboarding(const QString &alias)
{
    setAlias(alias);
    if (!m_onboarded) {
        m_onboarded = true;
        QSettings().setValue("identity/onboarded", true);
        emit onboardedChanged();
    }
}
