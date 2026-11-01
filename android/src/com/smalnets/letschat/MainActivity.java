package com.smalnets.letschat;

import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

/**
 * QtActivity with its own Back handling.  Qt's built-in back callback does not forward the key
 * to QML on current Android versions, so Back used to leave the app from any screen.
 * Here Back asks the UI (via JNI) whether there is something to close (dialog, page, open chat);
 * if so QML handles it, otherwise the task is moved to the background.
 */
public class MainActivity extends org.qtproject.qt.android.bindings.QtActivity {
    private static final String TAG = "LetsChatActivity";

    /** Number of "back steps" the UI can take right now (0 = nothing to close). */
    private static native int nativeBackDepth();
    /** Asks the UI to take one back step. */
    private static native void nativeBackRequested();
    /** Tells the native side that a new notification tap is waiting in {@link #takePendingPeer()}. */
    private static native void nativeIntentArrived();

    /** Extra put on notification intents: the conversation to open. */
    public static final String EXTRA_PEER = "letschat.peer";
    private static volatile String pendingPeer = "";

    /** Called from C++ once the UI is ready; returns (and forgets) the conversation to open, or "". */
    public static String takePendingPeer() {
        String p = pendingPeer;
        pendingPeer = "";
        return p == null ? "" : p;
    }

    private void readPeerFrom(android.content.Intent intent) {
        if (intent != null && intent.hasExtra(EXTRA_PEER)) {
            pendingPeer = intent.getStringExtra(EXTRA_PEER);
            intent.removeExtra(EXTRA_PEER);
            try {
                nativeIntentArrived();
            } catch (UnsatisfiedLinkError e) {
                // native side not up yet: it asks for the pending peer when it starts
            }
        }
    }

    @Override
    protected void onNewIntent(android.content.Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        readPeerFrom(intent);
    }

    private OnBackInvokedCallback backCallback;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        registerBackCallback();
        readPeerFrom(getIntent());
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Qt registers its own callback; registering ours last gives it priority.
        reclaimBack();
        // Qt may register its callback a moment later; take priority again afterwards.
        new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(this::reclaimBack, 400);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) reclaimBack();
    }

    private void reclaimBack() {
        unregisterBackCallback();
        registerBackCallback();
    }

    @Override
    protected void onDestroy() {
        unregisterBackCallback();
        super.onDestroy();
    }

    private void registerBackCallback() {
        if (Build.VERSION.SDK_INT >= 33 && backCallback == null) {
            backCallback = this::handleBack;
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    OnBackInvokedDispatcher.PRIORITY_DEFAULT, backCallback);
        }
    }

    private void unregisterBackCallback() {
        if (Build.VERSION.SDK_INT >= 33 && backCallback != null) {
            getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(backCallback);
            backCallback = null;
        }
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {   // Android < 13
        handleBack();
    }

    private void handleBack() {
        int depth = 0;
        try {
            depth = nativeBackDepth();
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "native back bridge not ready");
        }
        if (depth > 0) {
            try {
                nativeBackRequested();
                return;
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "native back bridge not ready");
            }
        }
        moveTaskToBack(true);   // nothing to close: keep running in the background
    }
}
