package io.github.decentbridge;

import android.os.Bundle;
import android.util.Log;

import org.qtproject.qt.android.bindings.QtActivity;

public class BridgeActivity extends QtActivity {
    private static final String TAG = "BridgeActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG, "BridgeActivity created");

        // Start foreground service to keep app running in background
        BridgeService.start(this);

        // Note: Don't auto-minimize. Qt's QML engine requires the Activity's
        // OpenGL surface. When the user presses Home, the foreground service
        // keeps the app alive. Auto-calling moveTaskToBack() destroys the
        // surface and causes Qt to fail, leading Android to kill the app.
    }

    @Override
    protected void onDestroy() {
        Log.d(TAG, "BridgeActivity destroyed");
        super.onDestroy();
    }
}
