#include "FrameParser.h"
#include "core/AppConfig.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

FrameParser::FrameParser(const QString &destDir, QObject *parent)
    : QObject(parent), m_destDir(destDir)
{
}

FrameParser::~FrameParser()
{
    if (m_file) {
        m_file->close();
        m_file->remove();   // incomplete download
        delete m_file;
    }
}

QString FrameParser::sanitizeFileName(const QString &name)
{
    QString n = QFileInfo(name).fileName();
    n.replace(QRegularExpression("[^\\w .()\\-]"), "_");
    n = n.trimmed();
    if (n.isEmpty() || n.startsWith('.'))
        n.prepend("file_");
    return n.left(120);
}

void FrameParser::feed(const QByteArray &data)
{
    m_buffer += data;
    for (;;) {
        if (m_file) {
            const qint64 take = qMin<qint64>(m_remaining, m_buffer.size());
            if (take > 0) {
                if (m_file->write(m_buffer.constData(), take) != take) {
                    emit failed(tr("Could not write the received file"));
                    return;
                }
                m_buffer.remove(0, take);
                m_remaining -= take;
            }
            if (m_remaining == 0)
                finishFile();
            else
                return;       // need more bytes
            continue;
        }
        const int nl = m_buffer.indexOf('\n');
        if (nl < 0) {
            if (m_buffer.size() > AppConfig::MAX_FRAME_BYTES) {
                m_buffer.clear();
                emit failed(tr("Oversized frame"));
            }
            return;
        }
        const QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject())
            continue;
        const QJsonObject obj = doc.object();
        if (obj["type"].toString() == "file") {
            if (!beginFile(obj))
                return;
            if (m_remaining == 0)
                finishFile();
        } else {
            emit frame(obj);
        }
    }
}

bool FrameParser::beginFile(const QJsonObject &header)
{
    const qint64 size = qint64(header["size"].toDouble(-1));
    if (size < 0 || size > AppConfig::MAX_FILE_BYTES) {
        emit failed(tr("File too large"));
        return false;
    }
    QDir().mkpath(m_destDir);
    const QString id = header["id"].toString().left(8);
    m_path = QDir(m_destDir).filePath(id + "_" + sanitizeFileName(header["name"].toString()));
    m_file = new QFile(m_path);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(tr("Cannot create %1").arg(m_path));
        delete m_file;
        m_file = nullptr;
        return false;
    }
    m_header = header;
    m_remaining = size;
    return true;
}

void FrameParser::finishFile()
{
    m_file->close();
    delete m_file;
    m_file = nullptr;
    const QJsonObject header = m_header;
    const QString path = m_path;
    m_header = {};
    emit fileReceived(header, path);
}

// ------------------------------------------------------------------------------------------

FilePump::FilePump(QIODevice *device, QFile *file, QObject *parent)
    : QObject(parent), m_device(device), m_file(file), m_total(file->size())
{
    connect(device, &QIODevice::bytesWritten, this, &FilePump::pump);
}

void FilePump::start() { pump(); }

void FilePump::pump()
{
    if (m_done)
        return;
    constexpr qint64 Chunk = 64 * 1024, HighWater = 256 * 1024;
    while (!m_file->atEnd() && m_device->bytesToWrite() < HighWater) {
        const QByteArray chunk = m_file->read(Chunk);
        if (chunk.isEmpty()) {
            m_done = true;
            emit finished(false);
            return;
        }
        m_device->write(chunk);
    }
    emit progress(m_file->pos(), m_total);
    if (m_file->atEnd() && m_device->bytesToWrite() == 0) {
        m_done = true;
        emit finished(true);
    }
}
