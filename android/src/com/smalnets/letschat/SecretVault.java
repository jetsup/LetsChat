package com.smalnets.letschat;

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Log;

import java.security.KeyStore;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/**
 * Wraps small secrets (the identity private key, the database key) with an AES-256-GCM key that lives in
 * the Android Keystore and can never be exported, not even by this app. The wrapped blobs are stored in
 * the app's settings; copying them to another device (or reading a backup) yields nothing usable.
 */
public final class SecretVault {
    private static final String TAG = "SecretVault";
    private static final String ALIAS = "letschat_master_v1";
    private static final String PROVIDER = "AndroidKeyStore";

    private SecretVault() {}

    private static SecretKey masterKey() throws Exception {
        KeyStore ks = KeyStore.getInstance(PROVIDER);
        ks.load(null);
        if (!ks.containsAlias(ALIAS)) {
            KeyGenerator kg = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, PROVIDER);
            kg.init(new KeyGenParameterSpec.Builder(ALIAS,
                    KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setKeySize(256)
                    .build());
            kg.generateKey();
        }
        return (SecretKey) ks.getKey(ALIAS, null);
    }

    /** Returns iv || ciphertext || tag, or null if the Keystore is unavailable. */
    public static byte[] wrap(byte[] plain) {
        try {
            Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
            c.init(Cipher.ENCRYPT_MODE, masterKey());
            byte[] iv = c.getIV();
            byte[] ct = c.doFinal(plain);
            byte[] out = new byte[iv.length + ct.length];
            System.arraycopy(iv, 0, out, 0, iv.length);
            System.arraycopy(ct, 0, out, iv.length, ct.length);
            return out;
        } catch (Exception e) {
            Log.e(TAG, "wrap failed", e);
            return null;
        }
    }

    /** Inverse of {@link #wrap}; null when the blob is damaged or the Keystore key is gone. */
    public static byte[] unwrap(byte[] blob) {
        try {
            if (blob == null || blob.length < 13 + 16) return null;
            Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
            c.init(Cipher.DECRYPT_MODE, masterKey(), new GCMParameterSpec(128, blob, 0, 12));
            return c.doFinal(blob, 12, blob.length - 12);
        } catch (Exception e) {
            Log.e(TAG, "unwrap failed", e);
            return null;
        }
    }
}
