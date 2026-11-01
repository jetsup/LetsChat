#ifndef MEDIASERVICE_H
#define MEDIASERVICE_H

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>

class QAudioInput;
class QMediaCaptureSession;
class QMediaRecorder;

/**
 * Voice-note recording (all platforms, Qt Multimedia) and camera capture (Android: the system
 * camera app is launched through an intent, so no camera permission is needed).
 */
class MediaService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(int elapsedMs READ elapsedMs NOTIFY elapsedChanged)
    Q_PROPERTY(bool cameraAvailable READ cameraAvailable CONSTANT)

public:
    explicit MediaService(const QString &workDir, QObject *parent = nullptr);
    ~MediaService() override;

    bool recording() const { return m_recording; }
    int elapsedMs() const { return m_recording ? int(m_clock.elapsed()) : 0; }
    bool cameraAvailable() const;

    /** Starts recording (asks for the microphone permission first). */
    Q_INVOKABLE void startRecording();
    /** Stops and delivers the file through recordingFinished(). */
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void cancelRecording();
    Q_INVOKABLE void capturePhoto();
    Q_INVOKABLE void discardFile(const QUrl &file);

signals:
    void recordingChanged();
    void elapsedChanged();
    void recordingFinished(const QUrl &file, int durationMs);
    void photoCaptured(const QUrl &file);
    void problem(const QString &text);

private:
    void beginRecording();
    void onRecorderStopped();

    QString m_workDir;
    QMediaCaptureSession *m_session = nullptr;
    QAudioInput *m_input = nullptr;
    QMediaRecorder *m_recorder = nullptr;
    QTimer m_tick;
    QElapsedTimer m_clock;
    QString m_currentFile;
    int m_durationMs = 0;
    bool m_recording = false;
    bool m_cancelled = false;
};

#endif // MEDIASERVICE_H
