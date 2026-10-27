#ifndef IDENTITYMANAGER_H
#define IDENTITYMANAGER_H

#include <QObject>
#include <QString>
#include <QUuid>

class IdentityManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString userId READ userId NOTIFY identityChanged)
    Q_PROPERTY(QString alias READ alias WRITE setAlias NOTIFY aliasChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY identityChanged)
    Q_PROPERTY(bool isFirstRun READ isFirstRun NOTIFY identityChanged)

public:
    explicit IdentityManager(QObject *parent = nullptr);
    
    QString userId() const;
    QString alias() const;
    void setAlias(const QString &alias);
    QString deviceName() const;
    bool isFirstRun() const;
    
    Q_INVOKABLE void generateIfNeeded();
    Q_INVOKABLE void resetIdentity();
    
signals:
    void identityChanged();
    void aliasChanged();
    
private:
    void loadIdentity();
    void saveIdentity();
    QString generateUniqueId() const;
    
    QString m_userId;
    QString m_alias;
    QString m_deviceName;
    bool m_isFirstRun;
};

#endif // IDENTITYMANAGER_H
