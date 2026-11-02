#include "NoiseXX.h"

namespace {
const QByteArray kProtocolName = QByteArrayLiteral("Noise_XX_25519_ChaChaPoly_SHA256");
const QByteArray kPrologue = QByteArrayLiteral("LetsChat/secure-channel/1");
}

NoiseXX::NoiseXX(Role role, const Crypto::KeyPair &staticKey, const Crypto::KeyPair &ephemeralForTest)
    : m_role(role), m_s(staticKey), m_e(ephemeralForTest)
{
    // The protocol name is exactly HASHLEN (32) bytes, so it is used as h directly.
    m_h = kProtocolName;
    m_ck = m_h;
    mixHash(kPrologue);
    if (!m_s.isValid())
        m_failed = true;
}

int NoiseXX::expectedLength(Role role, int step)
{
    // message 1: e (32) | message 2: e (32) + enc(s) (48) + payload tag (16) | message 3: enc(s) (48) + tag (16)
    const int len[3] = {32, 96, 64};
    Q_UNUSED(role)
    return step >= 0 && step < 3 ? len[step] : -1;
}

bool NoiseXX::fail()
{
    m_failed = true;
    return false;
}

void NoiseXX::mixHash(const QByteArray &data) { m_h = Crypto::sha256(m_h + data); }

void NoiseXX::mixKey(const QByteArray &ikm)
{
    QByteArray tempK;
    Crypto::hkdf2(m_ck, ikm, &m_ck, &tempK);
    m_symmetric.k = tempK;
    m_symmetric.n = 0;
}

QByteArray NoiseXX::nonceFor(quint64 n)
{
    // 32 bits of zeros followed by the 64-bit little-endian counter
    QByteArray nonce(Crypto::NonceLen, '\0');
    for (int i = 0; i < 8; ++i)
        nonce[4 + i] = char((n >> (8 * i)) & 0xff);
    return nonce;
}

QByteArray NoiseXX::cipherEncrypt(CipherState &cs, const QByteArray &ad, const QByteArray &plain)
{
    if (cs.n == std::numeric_limits<quint64>::max())
        return {};
    const QByteArray out = Crypto::aeadEncrypt(cs.k, nonceFor(cs.n), ad, plain);
    if (!out.isNull())
        ++cs.n;
    return out;
}

QByteArray NoiseXX::cipherDecrypt(CipherState &cs, const QByteArray &ad, const QByteArray &cipher)
{
    if (cs.n == std::numeric_limits<quint64>::max())
        return {};
    const QByteArray out = Crypto::aeadDecrypt(cs.k, nonceFor(cs.n), ad, cipher);
    if (!out.isNull())
        ++cs.n;
    return out;
}

QByteArray NoiseXX::encryptAndHash(const QByteArray &plain)
{
    QByteArray out = plain;
    if (m_symmetric.hasKey()) {
        out = cipherEncrypt(m_symmetric, m_h, plain);
        if (out.isNull()) { fail(); return {}; }
    }
    mixHash(out);
    return out;
}

QByteArray NoiseXX::decryptAndHash(const QByteArray &cipher)
{
    QByteArray out = cipher;
    if (m_symmetric.hasKey()) {
        out = cipherDecrypt(m_symmetric, m_h, cipher);
        if (out.isNull()) { fail(); return {}; }
    }
    mixHash(cipher);
    return out.isNull() ? QByteArray("") : out;
}

void NoiseXX::split()
{
    QByteArray k1, k2;
    Crypto::hkdf2(m_ck, QByteArray(), &k1, &k2);
    CipherState c1, c2;
    c1.k = k1;
    c2.k = k2;
    if (m_role == Initiator) { m_send = c1; m_recv = c2; }
    else                     { m_send = c2; m_recv = c1; }
    m_symmetric = {};
    m_ck.fill('\0');
    m_ck.clear();
    m_e = {};
    m_done = true;
}

bool NoiseXX::wantsToWrite() const
{
    if (m_done || m_failed)
        return false;
    return m_role == Initiator ? (m_step == 0 || m_step == 2) : (m_step == 1);
}

QByteArray NoiseXX::writeMessage()
{
    if (!wantsToWrite())
        return {};
    QByteArray out;
    if (m_role == Initiator && m_step == 0) {                  // -> e
        if (!m_e.isValid()) m_e = Crypto::generateX25519();
        if (!m_e.isValid()) { fail(); return {}; }
        out = m_e.pub;
        mixHash(m_e.pub);
        out += encryptAndHash(QByteArray());                   // empty payload (no key yet: plaintext)
    } else if (m_role == Responder && m_step == 1) {           // <- e, ee, s, es
        if (!m_e.isValid()) m_e = Crypto::generateX25519();
        if (!m_e.isValid()) { fail(); return {}; }
        out = m_e.pub;
        mixHash(m_e.pub);
        const QByteArray ee = Crypto::dh(m_e.priv, m_re);
        if (ee.isEmpty()) { fail(); return {}; }
        mixKey(ee);
        out += encryptAndHash(m_s.pub);
        const QByteArray es = Crypto::dh(m_s.priv, m_re);      // responder: DH(s, re)
        if (es.isEmpty()) { fail(); return {}; }
        mixKey(es);
        out += encryptAndHash(QByteArray());
    } else {                                                   // initiator step 2: -> s, se
        out = encryptAndHash(m_s.pub);
        const QByteArray se = Crypto::dh(m_s.priv, m_re);      // initiator: DH(s, re)
        if (se.isEmpty()) { fail(); return {}; }
        mixKey(se);
        out += encryptAndHash(QByteArray());
    }
    if (m_failed)
        return {};
    ++m_step;
    if (m_step == 3)
        split();
    return out;
}

bool NoiseXX::readMessage(const QByteArray &msg)
{
    if (m_failed || m_done || wantsToWrite())
        return false;
    if (msg.size() != expectedLength(m_role, nextReadIndex()))
        return fail();

    if (m_role == Responder && m_step == 0) {                  // -> e
        m_re = msg.left(32);
        mixHash(m_re);
        const QByteArray payload = decryptAndHash(msg.mid(32));
        if (m_failed || !payload.isEmpty()) return fail();
    } else if (m_role == Initiator && m_step == 1) {           // <- e, ee, s, es
        m_re = msg.left(32);
        mixHash(m_re);
        const QByteArray ee = Crypto::dh(m_e.priv, m_re);
        if (ee.isEmpty()) return fail();
        mixKey(ee);
        m_rs = decryptAndHash(msg.mid(32, 48));
        if (m_failed || m_rs.size() != 32) return fail();
        const QByteArray es = Crypto::dh(m_e.priv, m_rs);      // initiator: DH(e, rs)
        if (es.isEmpty()) return fail();
        mixKey(es);
        const QByteArray payload = decryptAndHash(msg.mid(80));
        if (m_failed || !payload.isEmpty()) return fail();
    } else {                                                   // responder step 2: -> s, se
        m_rs = decryptAndHash(msg.left(48));
        if (m_failed || m_rs.size() != 32) return fail();
        const QByteArray se = Crypto::dh(m_e.priv, m_rs);      // responder: DH(e, rs)
        if (se.isEmpty()) return fail();
        mixKey(se);
        const QByteArray payload = decryptAndHash(msg.mid(48));
        if (m_failed || !payload.isEmpty()) return fail();
    }
    ++m_step;
    if (m_step == 3)
        split();
    return true;
}

QByteArray NoiseXX::encrypt(const QByteArray &plain)
{
    if (!m_done)
        return {};
    return cipherEncrypt(m_send, QByteArray(), plain);
}

QByteArray NoiseXX::decrypt(const QByteArray &cipher)
{
    if (!m_done)
        return {};
    return cipherDecrypt(m_recv, QByteArray(), cipher);
}
