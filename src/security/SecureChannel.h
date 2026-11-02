#ifndef SECURECHANNEL_H
#define SECURECHANNEL_H

#include "NoiseXX.h"
#include <QBuffer>
#include <QIODevice>
#include <QTimer>
#include <functional>

/**
 * An end-to-end encrypted, authenticated stream on top of any QIODevice (QTcpSocket, QBluetoothSocket).
 *
 * The first bytes on the wire are the three Noise XX handshake messages; afterwards every record is
 *     [u16 length, big endian][ChaCha20-Poly1305 ciphertext + 16 byte tag]
 * carrying up to 16 KiB of plaintext.  Anything written before the handshake has finished is held back and
 * only released once the peer's identity was accepted by the `verifier`.
 *
 * To the code above this class behaves like a plain sequential QIODevice, so the existing newline-delimited
 * JSON / raw file framing runs unchanged inside the encrypted tunnel.
 */
class SecureChannel : public QIODevice
{
    Q_OBJECT

public:
    /** Called once the peer's static public key is known; returning false aborts before any data is sent. */
    using Verifier = std::function<bool(const QByteArray &remoteStaticKey)>;

    SecureChannel(QIODevice *transport, NoiseXX::Role role, const Crypto::KeyPair &identity,
                  Verifier verifier, QObject *parent = nullptr);

    void start();
    bool isEstablished() const { return m_established; }
    bool hasFailed() const { return m_failed; }
    QByteArray remoteKey() const { return m_remoteKey; }
    QByteArray channelBinding() const { return m_binding; }
    QIODevice *transport() const { return m_transport; }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_plain.size() + QIODevice::bytesAvailable(); }
    qint64 bytesToWrite() const override { return m_pendingPlain.size() + (m_transport ? m_transport->bytesToWrite() : 0); }

signals:
    void established();
    void failed(const QString &reason);

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    void onTransportReadyRead();
    bool processHandshake();
    bool processRecords();
    void sendRecord(const QByteArray &payload);
    void sendPlain(const QByteArray &data);
    void sendHandshakeIfDue();
    void fail(const QString &reason);
    void finishHandshake();

    QIODevice *m_transport;
    NoiseXX m_noise;
    Verifier m_verifier;
    QTimer m_handshakeTimer;
    QByteArray m_in;            // raw bytes from the transport
    QByteArray m_plain;         // decrypted bytes waiting to be read
    QByteArray m_pendingPlain;  // written by the application before the handshake completed
    QByteArray m_remoteKey;
    QByteArray m_binding;
    bool m_established = false;
    bool m_failed = false;
    bool m_started = false;
    int m_hsStep = 0;
};

#endif // SECURECHANNEL_H
