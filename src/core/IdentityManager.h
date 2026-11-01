#ifndef IDENTITYMANAGER_H
#define IDENTITYMANAGER_H

#include <QObject>
#include <QString>
#include <QUrl>

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
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY avatarChanged)
    Q_PROPERTY(bool onboarded READ onboarded NOTIFY onboardedChanged)

public:
    explicit IdentityManager(QObject *parent = nullptr);

    QString userId() const { return m_userId; }
    QString alias() const;              // alias, or the device name when none is set
    void setAlias(const QString &alias);
    QString deviceName() const { return m_deviceName; }
    bool onboarded() const { return m_onboarded; }

    QString avatarUrl() const;
    QString avatarPath() const { return m_avatarPath; }
    QString avatarHash() const { return m_avatarHash; }
    /** Crops the picture to a square, scales it down and stores it as the profile picture. */
    Q_INVOKABLE bool setAvatar(const QUrl &image);
    Q_INVOKABLE void clearAvatar();
    Q_INVOKABLE void completeOnboarding(const QString &alias);

signals:
    void aliasChanged();
    void onboardedChanged();
    void avatarChanged();

private:
    QString m_userId;
    QString m_alias;
    QString m_deviceName;
    QString m_avatarPath;
    QString m_avatarHash;
    bool m_onboarded = false;
};

#endif // IDENTITYMANAGER_H
