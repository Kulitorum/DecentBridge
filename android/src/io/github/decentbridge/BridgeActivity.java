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

        // Start foreground service
        BridgeService.start(this);

        // Move to background after Qt initializes
        getWindow().getDecorView().postDelayed(new Runnable() {
            @Override
            public void run() {
                Log.d(TAG, "Moving to background");
                moveTaskToBack(true);
            }
        }, 3000);
    }

    @Override
    protected void onDestroy() {
        Log.d(TAG, "BridgeActivity destroyed");
        super.onDestroy();
    }
}
