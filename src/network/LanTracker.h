#ifndef LANTRACKER_H
#define LANTRACKER_H

#include <QObject>
#include <QString>
#include <QTimer>

/** Watches the local IPv4 network and tells us when we join a different LAN. */
class LanTracker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString localIp READ localIp NOTIFY networkChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY networkChanged)

public:
    explicit LanTracker(QObject *parent = nullptr);

    QString localIp() const { return m_localIp; }
    bool isConnected() const { return !m_localIp.isEmpty(); }
    void start();

signals:
    void networkChanged();
    void joinedNewLan(const QString &networkId);

private:
    void check();

    QTimer *m_timer;
    QString m_localIp;
    QString m_networkId;
};

#endif // LANTRACKER_H
