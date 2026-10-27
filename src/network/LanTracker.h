#ifndef LANTRACKER_H
#define LANTRACKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QTimer>

class LanTracker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentNetworkId READ currentNetworkId NOTIFY networkChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY networkChanged)

public:
    explicit LanTracker(QObject *parent = nullptr);
    
    QString currentNetworkId() const;
    bool isConnected() const;
    
    Q_INVOKABLE void monitorNetwork();
    
signals:
    void networkChanged(const QString &newNetworkId);
    void joinedNewLan(const QString &networkId);
    
private slots:
    void checkNetwork();
    
private:
    QString generateNetworkId() const;
    
    QTimer *m_checkTimer;
    QString m_currentNetworkId;
    QString m_lastNetworkId;
    bool m_isConnected;
};

#endif // LANTRACKER_H
