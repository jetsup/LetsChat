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
    private PowerManager.WakeLock wakeLock;

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

    public static void showMessageNotification(Context context, String title, String text, int id) {
        try {
            ensureChannels(context);
            Notification.Builder b = builder(context, CHANNEL_MESSAGES)
                    .setContentTitle(title)
                    .setContentText(text)
                    .setStyle(new Notification.BigTextStyle().bigText(text))
                    .setAutoCancel(true)
                    .setContentIntent(openAppIntent(context));
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
                .setContentIntent(openAppIntent(this))
                .build();
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                int type = Build.VERSION.SDK_INT >= 30 ? ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE : 0;
                startForeground(SERVICE_NOTIFICATION_ID, n, type);
            } else {
                startForeground(SERVICE_NOTIFICATION_ID, n);
            }
        } catch (Exception e) {
            Log.e(TAG, "startForeground failed", e);
        }
        acquireMulticastLock();
        acquireWakeLock();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        if (multicastLock != null && multicastLock.isHeld()) {
            multicastLock.release();
        }
        if (wakeLock != null && wakeLock.isHeld()) {
            wakeLock.release();
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    // ---------------------------------------------------------------- helpers

    /** Partial wake lock: keeps the CPU running so sockets keep being served with the screen off. */
    private void acquireWakeLock() {
        try {
            if (wakeLock == null) {
                PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
                wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "letschat:service");
                wakeLock.setReferenceCounted(false);
            }
            if (!wakeLock.isHeld()) wakeLock.acquire();
        } catch (Exception e) {
            Log.e(TAG, "Wake lock failed", e);
        }
    }

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

    private static Notification.Builder builder(Context context, String channel) {
        Notification.Builder b = Build.VERSION.SDK_INT >= 26
                ? new Notification.Builder(context, channel)
                : new Notification.Builder(context);
        int icon = context.getResources().getIdentifier("ic_stat_letschat", "drawable", context.getPackageName());
        b.setSmallIcon(icon != 0 ? icon : android.R.drawable.ic_dialog_info);
        return b;
    }

    private static PendingIntent openAppIntent(Context context) {
        Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        if (launch != null) {
            launch.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        }
        int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 23 ? PendingIntent.FLAG_IMMUTABLE : 0);
        return PendingIntent.getActivity(context, 0, launch, flags);
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
