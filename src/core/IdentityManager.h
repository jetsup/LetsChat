#ifndef IDENTITYMANAGER_H
#define IDENTITYMANAGER_H

#include <QObject>
#include <QString>

/**
 * Persistent identity of this installation: a stable UUID, an optional alias and
 * whether the onboarding screen has been completed.
 */
class IdentityManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString userId READ userId CONSTANT)
    Q_PROPERTY(QString alias READ alias WRITE setAlias NOTIFY aliasChanged)
    Q_PROPERTY(QString deviceName READ deviceName CONSTANT)
    Q_PROPERTY(bool onboarded READ onboarded NOTIFY onboardedChanged)

public:
    explicit IdentityManager(QObject *parent = nullptr);

    QString userId() const { return m_userId; }
    QString alias() const;              // alias, or the device name when none is set
    void setAlias(const QString &alias);
    QString deviceName() const { return m_deviceName; }
    bool onboarded() const { return m_onboarded; }

    Q_INVOKABLE void completeOnboarding(const QString &alias);

signals:
    void aliasChanged();
    void onboardedChanged();

private:
    QString m_userId;
    QString m_alias;
    QString m_deviceName;
    bool m_onboarded = false;
};

#endif // IDENTITYMANAGER_H
