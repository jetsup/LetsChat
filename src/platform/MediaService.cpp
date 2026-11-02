#include "MediaService.h"
#include <QAudioDevice>
#include <QAudioInput>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QPermissions>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <private/qandroidextras_p.h>
#endif

MediaService::MediaService(const QString &workDir, QObject *parent)
    : QObject(parent), m_workDir(workDir)
{
    m_tick.setInterval(200);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        emit elapsedChanged();
        if (m_clock.elapsed() > 5 * 60 * 1000)   // voice notes are limited to five minutes
            stopRecording();
    });
}

MediaService::~MediaService()
{
    if (m_recording && m_recorder) {
        m_cancelled = true;
        m_recorder->stop();
    }
}

bool MediaService::cameraAvailable() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

void MediaService::startRecording()
{
    if (m_recording)
        return;
    QMicrophonePermission permission;
    QCoreApplication *app = QCoreApplication::instance();
    switch (app->checkPermission(permission)) {
    case Qt::PermissionStatus::Granted:
        beginRecording();
        break;
    case Qt::PermissionStatus::Undetermined:
        app->requestPermission(permission, this, [this](const QPermission &r) {
            emit problem(r.status() == Qt::PermissionStatus::Granted
                             ? tr("Microphone allowed – hold the microphone button again to record")
                             : tr("Microphone permission denied"));
        });
        break;
    case Qt::PermissionStatus::Denied:
        emit problem(tr("Microphone permission denied. Allow it in the system settings."));
        break;
    }
}

void MediaService::beginRecording()
{
    if (QMediaDevices::audioInputs().isEmpty()) {
        emit problem(tr("No microphone found"));
        return;
    }
    if (!m_recorder) {
        m_session = new QMediaCaptureSession(this);
        m_input = new QAudioInput(this);
        m_recorder = new QMediaRecorder(this);
        m_session->setAudioInput(m_input);
        m_session->setRecorder(m_recorder);
        QMediaFormat format;
        format.setFileFormat(QMediaFormat::MPEG4);
        format.setAudioCodec(QMediaFormat::AudioCodec::AAC);
        m_recorder->setMediaFormat(format);
        m_recorder->setQuality(QMediaRecorder::NormalQuality);
        m_recorder->setAudioChannelCount(1);
        connect(m_recorder, &QMediaRecorder::recorderStateChanged, this,
                [this](QMediaRecorder::RecorderState st) {
            if (st == QMediaRecorder::StoppedState && m_recording)
                onRecorderStopped();
        });
        connect(m_recorder, &QMediaRecorder::errorOccurred, this,
                [this](QMediaRecorder::Error, const QString &text) {
            qWarning() << "Recorder error:" << text;
            emit problem(tr("Recording failed: %1").arg(text));
            if (m_recording) {
                m_cancelled = true;
                onRecorderStopped();
            }
        });
    }
    QDir().mkpath(m_workDir);
    m_currentFile = m_workDir + "/voice_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + ".m4a";
    m_recorder->setOutputLocation(QUrl::fromLocalFile(m_currentFile));
    m_cancelled = false;
    m_recording = true;
    m_clock.start();
    m_tick.start();
    m_recorder->record();
    emit recordingChanged();
    emit elapsedChanged();
}

void MediaService::stopRecording()
{
    if (!m_recording || !m_recorder)
        return;
    m_durationMs = int(m_clock.elapsed());
    m_recorder->stop();     // finishes asynchronously, see onRecorderStopped()
}

void MediaService::cancelRecording()
{
    if (!m_recording || !m_recorder)
        return;
    m_cancelled = true;
    m_recorder->stop();
}

void MediaService::onRecorderStopped()
{
    m_tick.stop();
    m_recording = false;
    emit recordingChanged();
    emit elapsedChanged();
    const QString file = m_currentFile;
    if (m_cancelled || m_durationMs < 700 || !QFileInfo::exists(file) || QFileInfo(file).size() == 0) {
        QFile::remove(file);
        if (!m_cancelled)
            emit problem(tr("Hold the microphone button while you speak"));
        m_cancelled = false;
        return;
    }
    emit recordingFinished(QUrl::fromLocalFile(file), m_durationMs);
}

void MediaService::discardFile(const QUrl &file)
{
    if (file.isLocalFile())
        QFile::remove(file.toLocalFile());
}

QString MediaService::saveImage(const QUrl &file)
{
    const QString src = file.toLocalFile();
    if (src.isEmpty() || !QFileInfo::exists(src))
        return {};
    const QFileInfo info(src);
    QString name = info.fileName();
    if (info.suffix().isEmpty())
        name += ".jpg";
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    const QJniObject where = QJniObject::callStaticObjectMethod(
        "com/smalnets/letschat/MediaSaver", "saveImage",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
        context.object(), QJniObject::fromString(src).object<jstring>(),
        QJniObject::fromString(name).object<jstring>());
    return where.isValid() ? where.toString() : QString();
#else
    QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    dir += "/LetsChat";
    QDir().mkpath(dir);
    QString dest = dir + "/" + name;
    for (int n = 1; QFile::exists(dest); ++n)            // never overwrite an earlier save
        dest = dir + "/" + info.completeBaseName() + QString(" (%1).").arg(n) + (info.suffix().isEmpty() ? "jpg" : info.suffix());
    return QFile::copy(src, dest) ? dest : QString();
#endif
}

void MediaService::capturePhoto()
{
#ifdef Q_OS_ANDROID
    const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                         + "/captures/IMG_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".jpg";
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject intent = QJniObject::callStaticObjectMethod(
        "com/smalnets/letschat/LetsChatService", "captureIntent",
        "(Landroid/content/Context;Ljava/lang/String;)Landroid/content/Intent;", context.object(),
        QJniObject::fromString(path).object<jstring>());
    if (!intent.isValid()) {
        emit problem(tr("Could not start the camera"));
        return;
    }
    QtAndroidPrivate::startActivity(intent, 4712, [this, path](int, int resultCode, const QJniObject &) {
        constexpr int RESULT_OK = -1;
        QMetaObject::invokeMethod(this, [this, path, resultCode] {
            if (resultCode == RESULT_OK && QFileInfo(path).size() > 0)
                emit photoCaptured(QUrl::fromLocalFile(path));
            else
                QFile::remove(path);
        }, Qt::QueuedConnection);
    });
#else
    emit problem(tr("Taking pictures is available on Android"));
#endif
}
