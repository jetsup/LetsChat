#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QObject>
#include <QString>

class AppConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString organizationName READ organizationName CONSTANT)
    Q_PROPERTY(QString organizationDomain READ organizationDomain CONSTANT)

public:
    explicit AppConfig(QObject *parent = nullptr);
    
    QString appName() const;
    QString appVersion() const;
    QString organizationName() const;
    QString organizationDomain() const;
    
    static const quint16 DISCOVERY_PORT = 45454;
    static const quint16 TRANSFER_PORT = 45455;
    static const int DISCOVERY_INTERVAL_MS = 3000;
    static const int PEER_TIMEOUT_MS = 15000;
};

#endif // APPCONFIG_H
