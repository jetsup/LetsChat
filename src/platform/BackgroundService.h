#ifndef BACKGROUNDSERVICE_H
#define BACKGROUNDSERVICE_H

#include <QObject>
#include <QString>

class BackgroundService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRunningInBackground READ isRunningInBackground NOTIFY backgroundStateChanged)

public:
    explicit BackgroundService(QObject *parent = nullptr);
    
    bool isRunningInBackground() const;
    
    Q_INVOKABLE void startBackgroundService();
    Q_INVOKABLE void stopBackgroundService();
    Q_INVOKABLE void keepAlive();
    
signals:
    void backgroundStateChanged();
    void serviceStarted();
    void serviceStopped();
    
private:
    void setupDesktopBackground();
    void setupAndroidBackground();
    
    bool m_isRunningInBackground;
};

#endif // BACKGROUNDSERVICE_H
