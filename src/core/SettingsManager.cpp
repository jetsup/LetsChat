#include "SettingsManager.h"

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
}

bool SettingsManager::startInBackground() const
{
    return m_settings.value("app/startInBackground", true).toBool();
}

void SettingsManager::setStartInBackground(bool enabled)
{
    if (startInBackground() != enabled) {
        m_settings.setValue("app/startInBackground", enabled);
        emit settingsChanged();
    }
}

bool SettingsManager::enableNotifications() const
{
    return m_settings.value("app/enableNotifications", true).toBool();
}

void SettingsManager::setEnableNotifications(bool enabled)
{
    if (enableNotifications() != enabled) {
        m_settings.setValue("app/enableNotifications", enabled);
        emit settingsChanged();
    }
}

bool SettingsManager::autoAcceptFiles() const
{
    return m_settings.value("app/autoAcceptFiles", false).toBool();
}

void SettingsManager::setAutoAcceptFiles(bool enabled)
{
    if (autoAcceptFiles() != enabled) {
        m_settings.setValue("app/autoAcceptFiles", enabled);
        emit settingsChanged();
    }
}

int SettingsManager::discoveryInterval() const
{
    return m_settings.value("network/discoveryInterval", 3000).toInt();
}

void SettingsManager::setDiscoveryInterval(int ms)
{
    if (discoveryInterval() != ms) {
        m_settings.setValue("network/discoveryInterval", ms);
        emit settingsChanged();
    }
}

void SettingsManager::save()
{
    m_settings.sync();
}

void SettingsManager::restoreDefaults()
{
    m_settings.clear();
    emit settingsChanged();
}
