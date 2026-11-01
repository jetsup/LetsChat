package com.smalnets.letschat;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.provider.Settings;
import android.util.Log;

/**
 * Foreground service that keeps the LetsChat process alive while the UI is in the
 * background, so the LAN discovery / TCP / Bluetooth sockets owned by the native
 * (Qt) side keep running.  It also owns the Wi-Fi multicast lock (needed to receive
 * UDP broadcasts reliably on many devices) and the notification helpers used by C++.
 */
public class LetsChatService extends Service {
    private static final String TAG = "LetsChatService";
    private static final String CHANNEL_SERVICE = "letschat_service";
    private static final String CHANNEL_MESSAGES = "letschat_messages";
    private static final int SERVICE_NOTIFICATION_ID = 1;

    private WifiManager.MulticastLock multicastLock;

    // ---------------------------------------------------------------- static API (called from C++ via JNI)

    public static void start(Context context) {
        Intent intent = new Intent(context, LetsChatService.class);
        try {
            if (Build.VERSION.SDK_INT >= 26) {
                context.startForegroundService(intent);
            } else {
                context.startService(intent);
            }
        } catch (Exception e) {
            Log.e(TAG, "Unable to start foreground service", e);
        }
    }

    public static void stop(Context context) {
        context.stopService(new Intent(context, LetsChatService.class));
    }

    @SuppressWarnings("deprecation")   // priority/defaults are only used below API 26
    public static void showMessageNotification(Context context, String title, String text, int id, String peerId) {
        try {
            ensureChannels(context);
            Notification.Builder b = builder(context, CHANNEL_MESSAGES)
                    .setContentTitle(title)
                    .setContentText(text)
                    .setStyle(new Notification.BigTextStyle().bigText(text))
                    .setAutoCancel(true)
                    .setContentIntent(openAppIntent(context, peerId));
            if (Build.VERSION.SDK_INT < 26) {
                b.setPriority(Notification.PRIORITY_HIGH).setDefaults(Notification.DEFAULT_ALL);
            }
            NotificationManager nm = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            nm.notify(100 + (id & 0xffff), b.build());
        } catch (Exception e) {
            Log.e(TAG, "Unable to show notification", e);
        }
    }

    /** Ask for POST_NOTIFICATIONS on Android 13+. */
    public static void requestNotificationPermission(Activity activity) {
        if (Build.VERSION.SDK_INT >= 33
                && activity.checkSelfPermission("android.permission.POST_NOTIFICATIONS")
                   != PackageManager.PERMISSION_GRANTED) {
            activity.requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"}, 4711);
        }
    }

    public static boolean isIgnoringBatteryOptimizations(Context context) {
        PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        return pm != null && pm.isIgnoringBatteryOptimizations(context.getPackageName());
    }

    public static void requestIgnoreBatteryOptimizations(Activity activity) {
        try {
            Intent i = new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS);
            i.setData(Uri.parse("package:" + activity.getPackageName()));
            activity.startActivity(i);
        } catch (Exception e) {
            Log.e(TAG, "Battery optimisation request failed", e);
        }
    }

    /** Intent for the system camera app that stores the picture at {@code outPath}. */
    public static Intent captureIntent(Context context, String outPath) {
        java.io.File file = new java.io.File(outPath);
        if (file.getParentFile() != null) file.getParentFile().mkdirs();
        Uri uri = androidx.core.content.FileProvider.getUriForFile(
                context, context.getPackageName() + ".fileprovider", file);
        Intent intent = new Intent(android.provider.MediaStore.ACTION_IMAGE_CAPTURE);
        intent.putExtra(android.provider.MediaStore.EXTRA_OUTPUT, uri);
        intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.setClipData(android.content.ClipData.newRawUri("", uri));
        return intent;
    }

    public static String manufacturer() {
        return Build.MANUFACTURER == null ? "" : Build.MANUFACTURER;
    }

    public static void openAppSettings(Activity activity) {
        try {
            Intent i = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            i.setData(Uri.parse("package:" + activity.getPackageName()));
            activity.startActivity(i);
        } catch (Exception e) {
            Log.e(TAG, "Cannot open app settings", e);
        }
    }

    /** Back button behaviour: hide the UI instead of destroying it so sockets stay alive. */
    public static void moveToBackground(Activity activity) {
        activity.moveTaskToBack(true);
    }

    // ---------------------------------------------------------------- Service lifecycle

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        ensureChannels(this);
        Notification n = builder(this, CHANNEL_SERVICE)
                .setContentTitle("LetsChat is running")
                .setContentText("Listening for nearby devices on Wi-Fi and Bluetooth")
                .setOngoing(true)
                .setContentIntent(openAppIntent(this, null))
                .build();
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                int type = Build.VERSION.SDK_INT >= 30 ? ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE : 0;
                if (Build.VERSION.SDK_INT >= 34) {
                    type |= ServiceInfo.FOREGROUND_SERVICE_TYPE_REMOTE_MESSAGING;
                }
                startForeground(SERVICE_NOTIFICATION_ID, n, type);
            } else {
                startForeground(SERVICE_NOTIFICATION_ID, n);
            }
        } catch (Exception e) {
            Log.e(TAG, "startForeground failed", e);
        }
        acquireMulticastLock();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        if (multicastLock != null && multicastLock.isHeld()) {
            multicastLock.release();
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    // ---------------------------------------------------------------- helpers

    private void acquireMulticastLock() {
        try {
            if (multicastLock == null) {
                WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
                if (wifi != null) {
                    multicastLock = wifi.createMulticastLock("letschat-discovery");
                    multicastLock.setReferenceCounted(false);
                }
            }
            if (multicastLock != null && !multicastLock.isHeld()) {
                multicastLock.acquire();
            }
        } catch (Exception e) {
            Log.e(TAG, "Multicast lock failed", e);
        }
    }

    @SuppressWarnings("deprecation")   // Builder(Context) is only used below API 26
    private static Notification.Builder builder(Context context, String channel) {
        Notification.Builder b = Build.VERSION.SDK_INT >= 26
                ? new Notification.Builder(context, channel)
                : new Notification.Builder(context);
        int icon = context.getResources().getIdentifier("ic_stat_letschat", "drawable", context.getPackageName());
        b.setSmallIcon(icon != 0 ? icon : android.R.drawable.ic_dialog_info);
        return b;
    }

    /** Opens the app; with a peer id it also asks MainActivity to open that conversation. */
    private static PendingIntent openAppIntent(Context context, String peerId) {
        Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        if (launch != null) {
            launch.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
            if (peerId != null && !peerId.isEmpty()) {
                launch.putExtra(MainActivity.EXTRA_PEER, peerId);
            }
        }
        // one PendingIntent per conversation, so each keeps its own extra
        int request = peerId == null ? 0 : (peerId.hashCode() & 0x7fffffff);
        int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 23 ? PendingIntent.FLAG_IMMUTABLE : 0);
        return PendingIntent.getActivity(context, request, launch, flags);
    }

    private static void ensureChannels(Context context) {
        if (Build.VERSION.SDK_INT < 26) return;
        NotificationManager nm = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm.getNotificationChannel(CHANNEL_SERVICE) == null) {
            NotificationChannel c = new NotificationChannel(CHANNEL_SERVICE, "Background service",
                    NotificationManager.IMPORTANCE_MIN);
            c.setDescription("Keeps LetsChat connected to nearby devices");
            nm.createNotificationChannel(c);
        }
        if (nm.getNotificationChannel(CHANNEL_MESSAGES) == null) {
            NotificationChannel c = new NotificationChannel(CHANNEL_MESSAGES, "Messages",
                    NotificationManager.IMPORTANCE_HIGH);
            c.setDescription("New chat messages");
            nm.createNotificationChannel(c);
        }
    }
}
