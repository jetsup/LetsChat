#ifndef FRAMEPARSER_H
#define FRAMEPARSER_H

#include <QByteArray>
#include <QFile>
#include <QJsonObject>
#include <QObject>

/**
 * Incremental parser for the wire format shared by LAN and Bluetooth:
 * newline-delimited JSON frames, where a {"type":"file", "size":N, ...} frame is
 * immediately followed by N raw bytes which are streamed to a file in `destDir`.
 */
class FrameParser : public QObject
{
    Q_OBJECT
public:
    FrameParser(const QString &destDir, QObject *parent = nullptr);
    ~FrameParser() override;

    void feed(const QByteArray &data);

    static QString sanitizeFileName(const QString &name);

signals:
    void frame(const QJsonObject &frame);                       // any non-file frame
    void fileReceived(const QJsonObject &header, const QString &path);
    void failed(const QString &reason);

private:
    bool beginFile(const QJsonObject &header);
    void finishFile();

    QString m_destDir;
    QByteArray m_buffer;
    QFile *m_file = nullptr;
    qint64 m_remaining = 0;
    QJsonObject m_header;
    QString m_path;
};

/** Streams a file into a socket without buffering all of it in memory. */
class FilePump : public QObject
{
    Q_OBJECT
public:
    FilePump(QIODevice *device, QFile *file, QObject *parent = nullptr);
    void start();

signals:
    void progress(qint64 sent, qint64 total);
    void finished(bool ok);

private:
    void pump();

    QIODevice *m_device;
    QFile *m_file;
    qint64 m_total;
    bool m_done = false;
};

#endif // FRAMEPARSER_H
