#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>

class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool startInBackground READ startInBackground WRITE setStartInBackground NOTIFY settingsChanged)
    Q_PROPERTY(bool enableNotifications READ enableNotifications WRITE setEnableNotifications NOTIFY settingsChanged)
    Q_PROPERTY(bool autoAcceptFiles READ autoAcceptFiles WRITE setAutoAcceptFiles NOTIFY settingsChanged)
    Q_PROPERTY(int discoveryInterval READ discoveryInterval WRITE setDiscoveryInterval NOTIFY settingsChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);
    
    bool startInBackground() const;
    void setStartInBackground(bool enabled);
    
    bool enableNotifications() const;
    void setEnableNotifications(bool enabled);
    
    bool autoAcceptFiles() const;
    void setAutoAcceptFiles(bool enabled);
    
    int discoveryInterval() const;
    void setDiscoveryInterval(int ms);
    
    Q_INVOKABLE void save();
    Q_INVOKABLE void restoreDefaults();
    
signals:
    void settingsChanged();
    
private:
    QSettings m_settings;
};

#endif // SETTINGSMANAGER_H
