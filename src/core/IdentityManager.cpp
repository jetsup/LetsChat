#include "IdentityManager.h"
#include "AppConfig.h"
#include "utils/DeviceInfo.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QStandardPaths>
#include <QSettings>
#include <QUuid>

IdentityManager::IdentityManager(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    m_userId = s.value("identity/userId").toString();
    if (m_userId.isEmpty()) {
        m_userId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue("identity/userId", m_userId);
    }
    m_alias = s.value("identity/alias").toString();
    m_onboarded = s.value("identity/onboarded", false).toBool();
    m_deviceName = DeviceInfo::getDeviceName();

    m_avatarPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatar.jpg";
    QFile f(m_avatarPath);
    if (f.open(QIODevice::ReadOnly))
        m_avatarHash = QString::fromLatin1(QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1).toHex().left(12));
}

QString IdentityManager::avatarUrl() const
{
    return m_avatarHash.isEmpty() ? QString()
                                  : QUrl::fromLocalFile(m_avatarPath).toString() + "?v=" + m_avatarHash;
}

bool IdentityManager::setAvatar(const QUrl &image)
{
    // content:// URIs (Android picker) cannot be opened by QImageReader directly: read the bytes first.
    QFile in(image.isLocalFile() ? image.toLocalFile() : image.toString());
    if (!in.open(QIODevice::ReadOnly))
        return false;
    QImageReader reader(&in);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull())
        return false;
    const int side = qMin(img.width(), img.height());
    img = img.copy((img.width() - side) / 2, (img.height() - side) / 2, side, side)
              .scaled(AppConfig::AVATAR_SIZE, AppConfig::AVATAR_SIZE, Qt::IgnoreAspectRatio,
                      Qt::SmoothTransformation)
              .convertToFormat(QImage::Format_RGB32);
    QDir().mkpath(QFileInfo(m_avatarPath).absolutePath());
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 85);
    QFile out(m_avatarPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(bytes) != bytes.size())
        return false;
    out.close();
    m_avatarHash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex().left(12));
    emit avatarChanged();
    return true;
}

void IdentityManager::clearAvatar()
{
    QFile::remove(m_avatarPath);
    m_avatarHash.clear();
    emit avatarChanged();
}

QString IdentityManager::alias() const
{
    return m_alias.isEmpty() ? m_deviceName : m_alias;
}

void IdentityManager::setAlias(const QString &alias)
{
    const QString trimmed = alias.trimmed();
    // Setting the alias to the device name (or nothing) means "use the default".
    const QString stored = (trimmed == m_deviceName) ? QString() : trimmed;
    if (m_alias == stored)
        return;
    m_alias = stored;
    QSettings().setValue("identity/alias", m_alias);
    emit aliasChanged();
}

void IdentityManager::completeOnboarding(const QString &alias)
{
    setAlias(alias);
    if (!m_onboarded) {
        m_onboarded = true;
        QSettings().setValue("identity/onboarded", true);
        emit onboardedChanged();
    }
}
