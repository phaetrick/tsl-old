package me.rocks.pocketanalog;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class MyBroadcastReceiver extends BroadcastReceiver {
    static String poweroffString = "Power Off";
    static String poweronString = "Power On";
    static String micoffString = "Stop";
    static String foregroundString = "Foreground";

    private static final String TAG = "MyBroadcastReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null || intent.getAction() == null) return;
        String action = intent.getAction();
        if (action.equals(poweroffString) || action.equals(poweronString)) {
            if (MyApplication.getInstance() != null) {
                // Stop only. A receiver runs with the app in the background, and
                // java_control(0) *toggles* — against a stale notification that
                // would start the synth from the background, which on Android
                // 12+ is a foreground-service start and therefore a crash.
                MainActivity.powerOff();
            } else {
                android.app.NotificationManager nm =
                    (android.app.NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
                if (nm != null) nm.cancel(101);
            }
        } else if (action.equals(micoffString)) {
            MyApplication.java_control(1);
        } else if (action.equals(foregroundString)) {
            Intent i = new Intent(context, MainActivity.class);
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(i);
        }
    }
}


