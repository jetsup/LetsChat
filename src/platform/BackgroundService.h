#ifndef BACKGROUNDSERVICE_H
#define BACKGROUNDSERVICE_H

#include <QObject>
#include <QString>

class QSystemTrayIcon;

/**
 * Keeps LetsChat reachable while its window is not visible.
 *  - Android: starts/stops a foreground service (see LetsChatService.java) so the process
 *    and its sockets are not frozen, and moves the task to the background on "back".
 *  - Desktop: system-tray icon; closing the window hides it instead of quitting.
 */
class BackgroundService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool runInBackground READ runInBackground WRITE setRunInBackground NOTIFY runInBackgroundChanged)
    Q_PROPERTY(bool serviceRunning READ serviceRunning NOTIFY serviceRunningChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
    Q_PROPERTY(bool batteryUnrestricted READ batteryUnrestricted NOTIFY batteryChanged)

public:
    explicit BackgroundService(QObject *parent = nullptr);
    ~BackgroundService() override;

    bool runInBackground() const { return m_runInBackground; }
    void setRunInBackground(bool on);
    bool serviceRunning() const { return m_serviceRunning; }
    bool trayAvailable() const { return m_tray != nullptr; }
    bool batteryUnrestricted() const;

    /** Called once the app is up. Starts the foreground service / tray as configured. */
    void start();
    void requestNotificationPermission();
    void showTrayMessage(const QString &title, const QString &message);

    /** True when closing the main window should only hide it. */
    Q_INVOKABLE bool shouldHideOnClose() const;
    Q_INVOKABLE void moveToBackground();
    Q_INVOKABLE void requestBatteryExemption();
    Q_INVOKABLE void quit();

signals:
    void runInBackgroundChanged();
    void serviceRunningChanged();
    void batteryChanged();
    void showWindowRequested();

private:
    void startService();
    void stopService();

    bool m_runInBackground = true;
    bool m_serviceRunning = false;
    bool m_quitting = false;
    QSystemTrayIcon *m_tray = nullptr;
};

#endif // BACKGROUNDSERVICE_H
