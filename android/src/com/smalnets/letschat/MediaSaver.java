package com.smalnets.letschat;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/** Copies a picture into the public Pictures/LetsChat folder (shows up in the gallery). */
public final class MediaSaver {
    private static final String TAG = "LetsChatMediaSaver";

    private MediaSaver() {}

    /** @return a human readable location, or "" when saving failed */
    public static String saveImage(Context context, String path, String displayName) {
        try {
            String lower = displayName.toLowerCase();
            String mime = lower.endsWith(".png") ? "image/png" : lower.endsWith(".gif") ? "image/gif"
                        : lower.endsWith(".webp") ? "image/webp" : "image/jpeg";
            if (Build.VERSION.SDK_INT >= 29) {
                ContentResolver resolver = context.getContentResolver();
                ContentValues v = new ContentValues();
                v.put(MediaStore.MediaColumns.DISPLAY_NAME, displayName);
                v.put(MediaStore.MediaColumns.MIME_TYPE, mime);
                v.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_PICTURES + "/LetsChat");
                v.put(MediaStore.MediaColumns.IS_PENDING, 1);
                Uri uri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, v);
                if (uri == null) return "";
                try (InputStream in = new FileInputStream(path); OutputStream out = resolver.openOutputStream(uri)) {
                    if (out == null) return "";
                    copy(in, out);
                }
                v.clear();
                v.put(MediaStore.MediaColumns.IS_PENDING, 0);
                resolver.update(uri, v, null, null);
                return "Pictures/LetsChat";
            }
            // Android 9: public storage needs a permission, so use the app's own picture folder
            File dir = context.getExternalFilesDir(Environment.DIRECTORY_PICTURES);
            if (dir == null) return "";
            dir.mkdirs();
            File dest = new File(dir, displayName);
            try (InputStream in = new FileInputStream(path); OutputStream out = new FileOutputStream(dest)) {
                copy(in, out);
            }
            return dest.getAbsolutePath();
        } catch (Exception e) {
            Log.e(TAG, "saveImage failed", e);
            return "";
        }
    }

    private static void copy(InputStream in, OutputStream out) throws java.io.IOException {
        byte[] buf = new byte[64 * 1024];
        int n;
        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
    }
}
