#ifndef NOISEXX_H
#define NOISEXX_H

#include "Crypto.h"

/**
 * Noise_XX_25519_ChaChaPoly_SHA256 (Noise Protocol Framework, revision 34).
 *
 *   -> e
 *   <- e, ee, s, es
 *   -> s, se
 *
 * Both sides authenticate with a static X25519 key that is transmitted encrypted, giving mutual
 * authentication, forward secrecy and identity hiding from passive observers.  The application decides
 * whether the learned remote static key is acceptable (trust-on-first-use pinning).
 */
class NoiseXX
{
public:
    enum Role { Initiator, Responder };

    NoiseXX(Role role, const Crypto::KeyPair &staticKey, const Crypto::KeyPair &ephemeralForTest = {});

    /** Next outgoing handshake message (empty array if it is not our turn or on failure). */
    QByteArray writeMessage();
    /** Consumes the peer's handshake message; false on any malformation or authentication failure. */
    bool readMessage(const QByteArray &msg);

    bool wantsToWrite() const;
    bool wantsToRead() const { return !m_done && !m_failed && !wantsToWrite(); }
    bool isDone() const { return m_done; }
    bool hasFailed() const { return m_failed; }
    /** Known once the peer's static key has been decrypted (initiator: after message 2). */
    QByteArray remoteStatic() const { return m_rs; }
    /** Hash of the whole handshake: identical on both sides, usable as channel binding. */
    QByteArray handshakeHash() const { return m_h; }

    // Transport phase (after isDone()).
    QByteArray encrypt(const QByteArray &plain);
    /** Null array on authentication failure. */
    QByteArray decrypt(const QByteArray &cipher);

    static int expectedLength(Role role, int index);  // wire size of handshake message `index` (0..2)
    Role role() const { return m_role; }
    /** Index (0..2) of the handshake message this side expects to read next. */
    int nextReadIndex() const { return m_role == Initiator ? 1 : (m_step == 0 ? 0 : 2); }

private:
    struct CipherState {
        QByteArray k;
        quint64 n = 0;
        bool hasKey() const { return !k.isEmpty(); }
    };

    void mixHash(const QByteArray &data);
    void mixKey(const QByteArray &ikm);
    QByteArray encryptAndHash(const QByteArray &plain);
    QByteArray decryptAndHash(const QByteArray &cipher);
    static QByteArray nonceFor(quint64 n);
    static QByteArray cipherEncrypt(CipherState &cs, const QByteArray &ad, const QByteArray &plain);
    static QByteArray cipherDecrypt(CipherState &cs, const QByteArray &ad, const QByteArray &cipher);
    void split();
    bool fail();

    Role m_role;
    Crypto::KeyPair m_s, m_e;
    QByteArray m_rs, m_re;
    QByteArray m_h, m_ck;
    CipherState m_symmetric;
    CipherState m_send, m_recv;
    int m_step = 0;
    bool m_done = false;
    bool m_failed = false;
};

#endif // NOISEXX_H
