// Security test-suite: primitives against RFC vectors, the Noise handshake against an independent reference
// implementation (tests/noise_reference.py), the encrypted channel, identity pinning and at-rest encryption.
#include "security/Crypto.h"
#include "security/NoiseXX.h"
#include "security/SecretStore.h"
#include "security/SecureChannel.h"
#include "security/SecurityManager.h"
#include "storage/DatabaseManager.h"
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

static QByteArray H(const char *hex) { return QByteArray::fromHex(hex); }

class TstSecurity : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("LetsChatTest");
        QCoreApplication::setApplicationName("security-tests");
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QDir::tempPath() + "/letschat-test-settings");
        QSettings().clear();
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
    }

    void cleanupTestCase()
    {
        QSettings().clear();
        QDir(QDir::tempPath() + "/letschat-test-settings").removeRecursively();
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
    }

    // ------------------------------------------------------------------------ primitives

    void x25519_rfc7748()
    {
        const QByteArray alicePriv = H("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
        const QByteArray alicePub = H("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
        const QByteArray bobPriv = H("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb");
        const QByteArray bobPub = H("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
        const QByteArray shared = H("4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
        QCOMPARE(Crypto::keyPairFromPrivate(alicePriv).pub, alicePub);
        QCOMPARE(Crypto::keyPairFromPrivate(bobPriv).pub, bobPub);
        QCOMPARE(Crypto::dh(alicePriv, bobPub), shared);
        QCOMPARE(Crypto::dh(bobPriv, alicePub), shared);
    }

    void x25519_rejectsLowOrderPoint()
    {
        const Crypto::KeyPair k = Crypto::generateX25519();
        QVERIFY(Crypto::dh(k.priv, QByteArray(32, '\0')).isEmpty());   // all-zero point
        QVERIFY(Crypto::dh(k.priv, QByteArray(31, 'x')).isEmpty());    // wrong length
    }

    void chacha20poly1305_rfc8439()
    {
        const QByteArray key = H("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
        const QByteArray nonce = H("070000004041424344454647");
        const QByteArray aad = H("50515253c0c1c2c3c4c5c6c7");
        const QByteArray pt = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the "
                              "future, sunscreen would be it.";
        const QByteArray expected = H(
            "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b"
            "1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc"
            "3ff4def08e4b7a9de576d26586cec64b61161ae10b594f09e26a7e902ecbd0600691");
        const QByteArray ct = Crypto::aeadEncrypt(key, nonce, aad, pt);
        QCOMPARE(ct, expected);
        QCOMPARE(Crypto::aeadDecrypt(key, nonce, aad, ct), pt);
    }

    void aead_rejectsTampering()
    {
        const QByteArray key = Crypto::randomBytes(32), nonce = Crypto::randomBytes(12);
        const QByteArray ct = Crypto::aeadEncrypt(key, nonce, "ad", "secret message");
        QVERIFY(!Crypto::aeadDecrypt(key, nonce, "ad", ct).isNull());
        for (int i = 0; i < ct.size(); ++i) {               // flipping any single bit must be detected
            QByteArray bad = ct;
            bad[i] = char(bad[i] ^ 0x01);
            QVERIFY2(Crypto::aeadDecrypt(key, nonce, "ad", bad).isNull(), qPrintable(QString::number(i)));
        }
        QVERIFY(Crypto::aeadDecrypt(key, nonce, "other", ct).isNull());                      // wrong AD
        QVERIFY(Crypto::aeadDecrypt(Crypto::randomBytes(32), nonce, "ad", ct).isNull());     // wrong key
        QVERIFY(Crypto::aeadDecrypt(key, Crypto::randomBytes(12), "ad", ct).isNull());       // wrong nonce
        QVERIFY(Crypto::aeadDecrypt(key, nonce, "ad", ct.left(ct.size() - 1)).isNull());     // truncated
        // empty plaintext is legal and authenticated
        const QByteArray e = Crypto::aeadEncrypt(key, nonce, "", "");
        QCOMPARE(e.size(), 16);
        QVERIFY(!Crypto::aeadDecrypt(key, nonce, "", e).isNull());
    }

    // ------------------------------------------------------------------------ Noise handshake

    void noise_matchesIndependentReference()
    {
        // vectors produced by tests/noise_reference.py (separate implementation, different library)
        const Crypto::KeyPair is = Crypto::keyPairFromPrivate(H("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20"));
        const Crypto::KeyPair ie = Crypto::keyPairFromPrivate(H("2122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f40"));
        const Crypto::KeyPair rs = Crypto::keyPairFromPrivate(H("4142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f60"));
        const Crypto::KeyPair re = Crypto::keyPairFromPrivate(H("6162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f80"));
        NoiseXX init(NoiseXX::Initiator, is, ie), resp(NoiseXX::Responder, rs, re);

        const QByteArray m1 = init.writeMessage();
        QCOMPARE(m1, H("5869aff450549732cbaaed5e5df9b30a6da31cb0e5742bad5ad4a1a768f1a67b"));
        QVERIFY(resp.readMessage(m1));
        const QByteArray m2 = resp.writeMessage();
        QCOMPARE(m2, H("244fe3b963e899dd295baffce248d3530f3a9a7479ba063002680ebfe7adad49f197d6be61b51372854b4947bb7968e9"
                       "0c719fe79550e48ecf696cc4e7b51e169808a773dedf2dfcf9f513723eccc074734532620728124b2257e33775449b53"));
        QVERIFY(init.readMessage(m2));
        QCOMPARE(init.remoteStatic(), rs.pub);
        const QByteArray m3 = init.writeMessage();
        QCOMPARE(m3, H("bb393122a9dd7e3e19dd4433a6c6743ff2454a8d4e165786a5f1235a87aaf27ef1b43f0eaabf016b5a146634143dc747"
                       "a41948e343138cfc62dc15dcccab8e76"));
        QVERIFY(resp.readMessage(m3));
        QCOMPARE(resp.remoteStatic(), is.pub);
        QVERIFY(init.isDone() && resp.isDone());
        QCOMPARE(init.handshakeHash(), resp.handshakeHash());
        QCOMPARE(init.handshakeHash(), H("90ff3cc2e54fd7f36e21d67b88bf41d6db956e7323c91f0079a4d12365f1f3a7"));

        QCOMPARE(init.encrypt("hello from the initiator"),
                 H("e1e26b9369b34c31aaf24d1e2b004dc13806b08abc1e44007e6bff0700fce20c9233933a318ad8d4"));
        QCOMPARE(resp.encrypt("hello from the responder"),
                 H("67875e48608c57d5d65d5d9fef39547f0ca89e91b7776bab9e4927f344c37468c917d600fefc433a"));
        QCOMPARE(init.encrypt("hello from the initiator"),     // second record: counter advanced
                 H("47e77600967136a100452d4b3cb98c111f91e08c75b166e8f120b1ae0b899cd01c1b7e620e8df8d6"));
    }

    void noise_randomHandshakeAndTransport()
    {
        NoiseXX a(NoiseXX::Initiator, Crypto::generateX25519()), b(NoiseXX::Responder, Crypto::generateX25519());
        QVERIFY(b.readMessage(a.writeMessage()));
        QVERIFY(a.readMessage(b.writeMessage()));
        QVERIFY(b.readMessage(a.writeMessage()));
        QVERIFY(a.isDone() && b.isDone());
        for (int i = 0; i < 50; ++i) {
            const QByteArray msg = Crypto::randomBytes(1 + i * 37);
            QCOMPARE(b.decrypt(a.encrypt(msg)), msg);
            QCOMPARE(a.decrypt(b.encrypt(msg)), msg);
        }
    }

    void noise_transportRejectsReplayReorderAndTamper()
    {
        NoiseXX a(NoiseXX::Initiator, Crypto::generateX25519()), b(NoiseXX::Responder, Crypto::generateX25519());
        QVERIFY(b.readMessage(a.writeMessage()));
        QVERIFY(a.readMessage(b.writeMessage()));
        QVERIFY(b.readMessage(a.writeMessage()));
        const QByteArray r0 = a.encrypt("first"), r1 = a.encrypt("second");
        QVERIFY(b.decrypt(r1).isNull());                    // out of order: nonce mismatch
        QCOMPARE(b.decrypt(r0), QByteArray("first"));
        QVERIFY(b.decrypt(r0).isNull());                    // replay
        QByteArray bad = r1; bad[0] = char(bad[0] ^ 1);
        QVERIFY(b.decrypt(bad).isNull());                   // bit flip
        QCOMPARE(b.decrypt(r1), QByteArray("second"));      // a failed attempt must not burn the counter
    }

    void noise_rejectsMalformedHandshakes()
    {
        const Crypto::KeyPair rs = Crypto::generateX25519();
        {   // wrong length
            NoiseXX r(NoiseXX::Responder, rs);
            QVERIFY(!r.readMessage(QByteArray(31, 'a')));
            QVERIFY(r.hasFailed());
        }
        {   // low-order ephemeral key
            NoiseXX r(NoiseXX::Responder, rs);
            QVERIFY(r.readMessage(QByteArray(32, '\0')));   // reading e itself is fine ...
            QVERIFY(r.writeMessage().isEmpty());            // ... but the DH with it is refused
            QVERIFY(r.hasFailed());
        }
        {   // tampered message 2
            NoiseXX a(NoiseXX::Initiator, Crypto::generateX25519()), b(NoiseXX::Responder, rs);
            QVERIFY(b.readMessage(a.writeMessage()));
            QByteArray m2 = b.writeMessage();
            m2[40] = char(m2[40] ^ 0x80);
            QVERIFY(!a.readMessage(m2));
        }
        {   // tampered message 3
            NoiseXX a(NoiseXX::Initiator, Crypto::generateX25519()), b(NoiseXX::Responder, rs);
            QVERIFY(b.readMessage(a.writeMessage()));
            QVERIFY(a.readMessage(b.writeMessage()));
            QByteArray m3 = a.writeMessage();
            m3[5] = char(m3[5] ^ 1);
            QVERIFY(!b.readMessage(m3));
            QVERIFY(!b.isDone());
        }
        {   // a man in the middle that swaps the responder's static key cannot complete the handshake
            NoiseXX a(NoiseXX::Initiator, Crypto::generateX25519());
            NoiseXX mitm(NoiseXX::Responder, Crypto::generateX25519());
            NoiseXX real(NoiseXX::Responder, rs);
            QVERIFY(mitm.readMessage(a.writeMessage()));
            QVERIFY(a.readMessage(mitm.writeMessage()));
            QVERIFY(a.remoteStatic() != rs.pub);            // the initiator sees a *different* identity
            QVERIFY(!real.readMessage(a.writeMessage()));   // and the real server rejects the forged transcript
        }
    }

    // ------------------------------------------------------------------------ encrypted channel

    void channel_roundTripAndConfidentiality()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QVERIFY(server.waitForNewConnection(3000));
        QTcpSocket *accepted = server.nextPendingConnection();
        QVERIFY(client.waitForConnected(3000));

        const Crypto::KeyPair ck = Crypto::generateX25519(), sk = Crypto::generateX25519();
        SecureChannel cc(&client, NoiseXX::Initiator, ck, nullptr);
        SecureChannel sc(accepted, NoiseXX::Responder, sk, nullptr);
        QSignalSpy upC(&cc, &SecureChannel::established), upS(&sc, &SecureChannel::established);

        // what the "network" sees: record all raw bytes arriving at the server socket
        QByteArray wire;
        connect(accepted, &QIODevice::readyRead, this, [&] { wire += accepted->peek(accepted->bytesAvailable()); });

        const QByteArray secret = "the quick brown fox jumps over the lazy dog - TOP SECRET";
        cc.write(secret);             // written before the handshake: must be held back, then delivered
        sc.start();
        cc.start();
        QTRY_VERIFY(upC.count() == 1 && upS.count() == 1);
        QCOMPARE(cc.remoteKey(), sk.pub);
        QCOMPARE(sc.remoteKey(), ck.pub);
        QCOMPARE(cc.channelBinding(), sc.channelBinding());
        QTRY_COMPARE(sc.bytesAvailable(), secret.size());
        QCOMPARE(sc.readAll(), secret);
        QVERIFY2(!wire.contains("TOP SECRET") && !wire.contains("quick brown fox"), "plaintext visible on the wire");

        // bulk data in both directions, several records
        const QByteArray blob = Crypto::randomBytes(300 * 1024);
        sc.write(blob);
        QByteArray got;
        QTRY_VERIFY2((got += cc.readAll()).size() >= blob.size(), "bulk data did not arrive");
        QCOMPARE(got, blob);
    }

    void channel_verifierCanRefuseThePeer()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QVERIFY(server.waitForNewConnection(3000));
        QTcpSocket *accepted = server.nextPendingConnection();
        QVERIFY(client.waitForConnected(3000));

        const Crypto::KeyPair sk = Crypto::generateX25519();
        // the client only trusts one particular server key - this server presents another one
        SecureChannel cc(&client, NoiseXX::Initiator, Crypto::generateX25519(),
                         [](const QByteArray &) { return false; });
        SecureChannel sc(accepted, NoiseXX::Responder, sk, nullptr);
        QSignalSpy failedC(&cc, &SecureChannel::failed);
        cc.write("must never leave this device");
        sc.start();
        cc.start();
        QTRY_VERIFY(failedC.count() == 1);
        QVERIFY(!cc.isEstablished());
        QVERIFY(sc.bytesAvailable() == 0);
        QVERIFY(!sc.isEstablished());
    }

    void channel_rejectsPlaintextClient()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QVERIFY(server.waitForNewConnection(3000));
        QTcpSocket *accepted = server.nextPendingConnection();
        SecureChannel sc(accepted, NoiseXX::Responder, Crypto::generateX25519(), nullptr);
        QSignalSpy failed(&sc, &SecureChannel::failed);
        sc.start();
        QVERIFY(client.waitForConnected(3000));
        client.write("{\"type\":\"text\",\"content\":\"I am an old plaintext client\"}\n");
        QTRY_VERIFY_WITH_TIMEOUT(failed.count() == 1, 2000);   // refused at once, not after a timeout
    }

    void channel_detectsTamperingOnTheWire()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QVERIFY(server.waitForNewConnection(3000));
        QTcpSocket *accepted = server.nextPendingConnection();
        QVERIFY(client.waitForConnected(3000));
        SecureChannel cc(&client, NoiseXX::Initiator, Crypto::generateX25519(), nullptr);
        SecureChannel sc(accepted, NoiseXX::Responder, Crypto::generateX25519(), nullptr);
        QSignalSpy up(&sc, &SecureChannel::established), failed(&sc, &SecureChannel::failed);
        sc.start();
        cc.start();
        QTRY_VERIFY(up.count() == 1);
        // an attacker injects a forged record directly into the TCP stream
        QByteArray forged(2, '\0');
        forged[1] = 40;
        forged += Crypto::randomBytes(40);
        client.write(forged);
        QTRY_VERIFY(failed.count() == 1);
        QVERIFY(sc.bytesAvailable() == 0);
    }

    // ------------------------------------------------------------------------ trust (pinning)

    void trust_pinsAndDetectsKeyChange()
    {
        DatabaseManager db;
        QVERIFY(db.initialize());
        SecurityManager sec(&db);
        QSignalSpy changed(&sec, &SecurityManager::keyChanged);
        const QByteArray k1 = Crypto::generateX25519().pub, k2 = Crypto::generateX25519().pub;

        QVERIFY(sec.evaluate("alice", k1) == SecurityManager::Trust::New);       // first use: pinned
        QVERIFY(sec.evaluate("alice", k1) == SecurityManager::Trust::Trusted);
        QVERIFY(!sec.isBlocked("alice"));
        QVERIFY(sec.evaluate("alice", k2) == SecurityManager::Trust::Changed);   // impostor / reinstall
        QVERIFY(sec.isBlocked("alice"));
        QCOMPARE(changed.count(), 1);
        QVERIFY(sec.evaluate("alice", k2) == SecurityManager::Trust::Changed);   // still refused ...
        QCOMPARE(changed.count(), 1);                                            // ... without alert spam
        QVERIFY(sec.evaluate("alice", k1) == SecurityManager::Trust::Trusted);   // the real one still works
        QVERIFY(!sec.isBlocked("alice"));

        // another id claiming alice's key is refused as well
        QVERIFY(sec.evaluate("mallory", k1) == SecurityManager::Trust::Changed);

        // verification mark is dropped when a new key is accepted
        sec.setVerified("alice", true);
        QVERIFY(sec.peerSecurity("alice")["verified"].toBool());
        QVERIFY(sec.evaluate("alice", k2) == SecurityManager::Trust::Changed);
        sec.acceptNewKey("alice");
        QVERIFY(sec.evaluate("alice", k2) == SecurityManager::Trust::Trusted);
        QVERIFY(!sec.peerSecurity("alice")["verified"].toBool());
        QVERIFY(sec.evaluate("alice", k1) == SecurityManager::Trust::Changed);   // the old key is now the odd one out
    }

    void trust_safetyNumberIsSymmetric()
    {
        const QByteArray a = Crypto::generateX25519().pub, b = Crypto::generateX25519().pub, c = Crypto::generateX25519().pub;
        const QString n1 = SecurityManager::safetyNumber(a, b);
        QCOMPARE(n1, SecurityManager::safetyNumber(b, a));
        QVERIFY(n1 != SecurityManager::safetyNumber(a, c));
        QCOMPARE(n1.split(' ').size(), 12);
        QCOMPARE(QString(n1).remove(' ').size(), 60);
    }

    // ------------------------------------------------------------------------ data at rest

    void storage_messagesAreEncryptedAtRest()
    {
        const QString secret = "MY-VERY-SECRET-PLAINTEXT-424242";
        QString dbPath;
        {
            DatabaseManager db;
            QVERIFY(db.initialize());
            QVERIFY(db.saveMessage("m1", "alice", "bob", secret, 0, "2026-01-01T10:00:00", 1, "{\"text\":\"QUOTED-SECRET-77\"}"));
            const QVariantList rows = db.getMessages(DatabaseManager::conversationId("alice", "bob"));
            QCOMPARE(rows.size(), 1);
            QCOMPARE(rows[0].toMap()["content"].toString(), secret);
            QVERIFY(rows[0].toMap()["reply"].toString().contains("QUOTED-SECRET-77"));
            dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/letschat.db";
        }
        QFile f(dbPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray raw = f.readAll();
        QVERIFY2(!raw.contains("MY-VERY-SECRET"), "message text is stored in plaintext");
        QVERIFY2(!raw.contains("QUOTED-SECRET"), "quoted text is stored in plaintext");
        QVERIFY(raw.contains("enc2:"));
    }

    void storage_secretsAreNotKeptInPlainSettings()
    {
        DatabaseManager db;                         // creates / loads the database key
        QSettings s;
        QVERIFY(!s.contains("storage/key"));        // the old plaintext location must be gone
        SecurityManager sec(&db);                   // creates / loads the identity key
        QCOMPARE(sec.identity().pub.size(), 32);
        QCOMPARE(SecretStore::load("identity_key"), sec.identity().priv);
    }
};

QTEST_MAIN(TstSecurity)
#include "tst_security.moc"
