#include "SecureChannel.h"
#include <QAbstractSocket>
#include <QDebug>

namespace {
constexpr int MaxPlainChunk = 16 * 1024;
constexpr int HandshakeTimeoutMs = 8000;
constexpr qint64 MaxPendingBeforeHandshake = 8 * 1024 * 1024;
}

SecureChannel::SecureChannel(QIODevice *transport, NoiseXX::Role role, const Crypto::KeyPair &identity,
                             Verifier verifier, QObject *parent)
    : QIODevice(parent)
    , m_transport(transport)
    , m_noise(role, identity)
    , m_verifier(std::move(verifier))
{
    open(QIODevice::ReadWrite);
    m_handshakeTimer.setSingleShot(true);
    m_handshakeTimer.setInterval(HandshakeTimeoutMs);
    connect(&m_handshakeTimer, &QTimer::timeout, this, [this] { fail(tr("Secure handshake timed out")); });
    connect(m_transport, &QIODevice::readyRead, this, &SecureChannel::onTransportReadyRead);
    connect(m_transport, &QIODevice::bytesWritten, this, [this](qint64 n) { emit bytesWritten(n); });
}

void SecureChannel::start()
{
    if (m_started || m_failed)
        return;
    m_started = true;
    m_handshakeTimer.start();
    sendHandshakeIfDue();     // the initiator speaks first
    onTransportReadyRead();   // the transport may already hold bytes
}

void SecureChannel::fail(const QString &reason)
{
    if (m_failed)
        return;
    m_failed = true;
    m_handshakeTimer.stop();
    m_pendingPlain.clear();
    qWarning() << "SecureChannel:" << reason;
    if (m_transport) {
        if (auto *sock = qobject_cast<QAbstractSocket *>(m_transport))
            sock->abort();
        else
            m_transport->close();
    }
    emit failed(reason);
}

void SecureChannel::sendRecord(const QByteArray &payload)
{
    if (payload.size() > 0xffff) { fail(tr("Record too large")); return; }
    QByteArray rec(2, '\0');
    rec[0] = char((payload.size() >> 8) & 0xff);
    rec[1] = char(payload.size() & 0xff);
    rec += payload;
    m_transport->write(rec);
}

void SecureChannel::sendHandshakeIfDue()
{
    while (!m_failed && m_noise.wantsToWrite()) {
        const QByteArray msg = m_noise.writeMessage();
        if (msg.isEmpty() || m_noise.hasFailed()) { fail(tr("Handshake failed")); return; }
        sendRecord(msg);
        ++m_hsStep;
        if (m_noise.isDone()) { finishHandshake(); return; }
    }
}

void SecureChannel::finishHandshake()
{
    m_handshakeTimer.stop();
    m_established = true;
    m_binding = m_noise.handshakeHash();
    emit established();
    // release everything the application queued while we were negotiating
    const QByteArray pending = m_pendingPlain;
    m_pendingPlain.clear();
    if (!pending.isEmpty())
        sendPlain(pending);
}

void SecureChannel::onTransportReadyRead()
{
    if (m_failed || !m_started)
        return;
    m_in += m_transport->readAll();
    if (!m_established) {
        if (!processHandshake())
            return;
    }
    if (m_established && !m_failed)
        processRecords();
}

// Handshake records: [u16 len][message].  Lengths are fixed by the pattern, so a plaintext client (or any
// other protocol) is rejected on its first bytes instead of being waited for.
bool SecureChannel::processHandshake()
{
    while (!m_failed && !m_established && m_noise.wantsToRead()) {
        if (m_in.size() < 2)
            return true;
        const int len = (uchar(m_in[0]) << 8) | uchar(m_in[1]);
        // Reject a wrong length immediately (before waiting for it to "arrive"): a plaintext peer's first
        // two bytes would otherwise be read as a huge length and stall until the timeout.
        if (len != NoiseXX::expectedLength(m_noise.role(), m_noise.nextReadIndex())) {
            fail(tr("Unexpected handshake message"));
            return false;
        }
        if (m_in.size() < 2 + len)
            return true;
        const QByteArray msg = m_in.mid(2, len);
        m_in.remove(0, 2 + len);
        if (!m_noise.readMessage(msg)) { fail(tr("Peer failed authentication")); return false; }
        ++m_hsStep;
        // As soon as the peer's static key is known, decide whether to continue.
        if (!m_noise.remoteStatic().isEmpty() && m_remoteKey.isEmpty()) {
            m_remoteKey = m_noise.remoteStatic();
            if (m_verifier && !m_verifier(m_remoteKey)) { fail(tr("Peer identity rejected")); return false; }
        }
        if (m_noise.isDone()) { finishHandshake(); return true; }
        sendHandshakeIfDue();
    }
    return !m_failed;
}

bool SecureChannel::processRecords()
{
    bool gotData = false;
    while (!m_failed) {
        if (m_in.size() < 2)
            break;
        const int len = (uchar(m_in[0]) << 8) | uchar(m_in[1]);
        if (len < Crypto::TagLen) { fail(tr("Malformed record")); return false; }
        if (m_in.size() < 2 + len)
            break;
        const QByteArray ct = m_in.mid(2, len);
        m_in.remove(0, 2 + len);
        const QByteArray plain = m_noise.decrypt(ct);
        if (plain.isNull()) { fail(tr("Message authentication failed")); return false; }
        m_plain += plain;
        gotData = true;
    }
    if (gotData)
        emit readyRead();
    return !m_failed;
}

qint64 SecureChannel::readData(char *data, qint64 maxSize)
{
    const qint64 n = qMin<qint64>(maxSize, m_plain.size());
    if (n <= 0)
        return 0;
    memcpy(data, m_plain.constData(), size_t(n));
    m_plain.remove(0, int(n));
    return n;
}

void SecureChannel::sendPlain(const QByteArray &data)
{
    for (qsizetype off = 0; off < data.size() && !m_failed; off += MaxPlainChunk) {
        const QByteArray ct = m_noise.encrypt(data.mid(off, MaxPlainChunk));
        if (ct.isNull()) { fail(tr("Encryption failed")); return; }
        sendRecord(ct);
    }
}

qint64 SecureChannel::writeData(const char *data, qint64 maxSize)
{
    if (m_failed)
        return -1;
    if (!m_established) {
        if (m_pendingPlain.size() + maxSize > MaxPendingBeforeHandshake)
            return -1;
        m_pendingPlain.append(data, int(maxSize));
        return maxSize;
    }
    sendPlain(QByteArray(data, int(maxSize)));
    return m_failed ? -1 : maxSize;
}
