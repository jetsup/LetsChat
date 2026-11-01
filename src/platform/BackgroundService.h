#ifndef BACKGROUNDSERVICE_H
#define BACKGROUNDSERVICE_H

#include <QDebug>
#include <QObject>
#include <atomic>
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
    Q_PROPERTY(int backDepth READ depth WRITE setBackDepth NOTIFY backDepthChanged)
    Q_PROPERTY(QString manufacturer READ manufacturer CONSTANT)
    Q_PROPERTY(bool batteryUnrestricted READ batteryUnrestricted NOTIFY batteryChanged)

public:
    explicit BackgroundService(QObject *parent = nullptr);
    ~BackgroundService() override;

    bool runInBackground() const { return m_runInBackground; }
    void setRunInBackground(bool on);
    bool serviceRunning() const { return m_serviceRunning; }
    bool trayAvailable() const { return m_tray != nullptr; }
    bool batteryUnrestricted() const;
    QString manufacturer() const;

    /** Called once the app is up. Starts the foreground service / tray as configured. */
    void start();
    void requestNotificationPermission();
    void showTrayMessage(const QString &title, const QString &message, const QString &peerId = {});
    static void requestPendingChat();   // a notification was tapped (any thread)

    /** True when closing the main window should only hide it. */
    Q_INVOKABLE bool shouldHideOnClose() const;
    /** QML tells us how many back steps it can take (page / open chat / dialog); read by the Activity. */
    void setBackDepth(int depth) { if (s_backDepth.exchange(depth) != depth) emit backDepthChanged(); }
    int depth() const { return s_backDepth; }
    static int backDepth() { return s_backDepth; }
    static void requestBack();
    Q_INVOKABLE void moveToBackground();
    Q_INVOKABLE void requestBatteryExemption();
    Q_INVOKABLE void openAppSettings();
    Q_INVOKABLE void quit();

signals:
    void runInBackgroundChanged();
    void serviceRunningChanged();
    void batteryChanged();
    void showWindowRequested();
    void backRequested();
    /** The user tapped a notification: show this conversation. */
    void openChatRequested(const QString &peerId);
    void backDepthChanged();

private:
    void startService();
    void pullPendingChat();
    void stopService();

    bool m_runInBackground = true;
    bool m_serviceRunning = false;
    bool m_quitting = false;
    static inline std::atomic<int> s_backDepth{0};
    static inline BackgroundService *s_instance = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QString m_lastTrayPeer;
};

#endif // BACKGROUNDSERVICE_H
