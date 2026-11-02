#include "SecretStore.h"
#include <QDebug>
#include <QFile>
#include <QSettings>

#ifdef Q_OS_ANDROID
#include <QJniArray>
#include <QJniObject>
#endif

namespace {

QString key(const QString &name) { return QStringLiteral("secrets/") + name; }

void restrictSettingsFile(QSettings &s)
{
#ifdef Q_OS_UNIX
    s.sync();
    QFile::setPermissions(s.fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#else
    Q_UNUSED(s)
#endif
}

#ifdef Q_OS_ANDROID
const char *kVault = "com/smalnets/letschat/SecretVault";

QByteArray vault(const char *method, const QByteArray &in)
{
    const QJniArray<jbyte> arg{QByteArray(in)};
    const QJniArray<jbyte> out = QJniObject::callStaticMethod<jbyte[]>(kVault, method, arg);
    if (!out.isValid())
        return {};                       // the Keystore refused (Java returned null)
    const QByteArray bytes = out.toContainer();
    return bytes.isNull() ? QByteArray("") : bytes;
}
#endif

} // namespace

QByteArray SecretStore::load(const QString &name)
{
    QSettings s;
    const QByteArray stored = QByteArray::fromBase64(s.value(key(name)).toByteArray());
    if (stored.isEmpty())
        return {};
#ifdef Q_OS_ANDROID
    return vault("unwrap", stored);
#else
    return stored;
#endif
}

bool SecretStore::save(const QString &name, const QByteArray &secret)
{
    QByteArray blob = secret;
#ifdef Q_OS_ANDROID
    blob = vault("wrap", secret);
    if (blob.isEmpty()) {
        qCritical() << "SecretStore: the Android Keystore refused to wrap" << name;
        return false;
    }
#endif
    QSettings s;
    s.setValue(key(name), blob.toBase64());
    restrictSettingsFile(s);
    return true;
}

void SecretStore::remove(const QString &name)
{
    QSettings s;
    s.remove(key(name));
}

QString SecretStore::protection()
{
#ifdef Q_OS_ANDROID
    return QStringLiteral("Android Keystore (hardware-backed where available)");
#else
    return QStringLiteral("owner-only settings file");
#endif
}
