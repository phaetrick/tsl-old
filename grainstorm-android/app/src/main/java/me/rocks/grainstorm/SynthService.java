package me.rocks.grainstorm;

import static android.app.Notification.FOREGROUND_SERVICE_IMMEDIATE;

import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import static me.rocks.grainstorm.MyApplication.CHANNEL_ID;

public class SynthService extends Service {
    private static final int NOTIFICATION_ID = 101;

    public static final String ACTION_START_FOREGROUND_SERVICE = "ACTION_START_FOREGROUND_SERVICE";
    public static final String ACTION_STOP_FOREGROUND_SERVICE = "ACTION_STOP_FOREGROUND_SERVICE";

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {

        // A null intent means the system restarted this service by itself after
        // the process was killed. The restart happens with the app in the
        // background, and on Android 12+ a foreground-service start from the
        // background throws ForegroundServiceStartNotAllowedException.
        //
        // It was also wrong on its own terms — the synth died with the process,
        // so the notification would have advertised an engine that is not there.
        if (intent == null) {
            stopSelf();
            return START_NOT_STICKY;
        }

        if (ACTION_STOP_FOREGROUND_SERVICE.equals(intent.getAction())) {
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
            return START_NOT_STICKY;
        }

        if (!showNotification()) stopSelf();

        // Was START_STICKY, which is what asked for the restart above. This
        // service exists to hold the process up while the synth runs, so it has
        // no purpose once the process is gone.
        return START_NOT_STICKY;
    }

    /** @return false if the system refused the foreground start. */
    private boolean showNotification() {
        Intent openIntent = new Intent(this, MainActivity.class);
        openIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        PendingIntent openPi = PendingIntent.getActivity(this, 0, openIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);

        Intent stopIntent = new Intent(this, MyBroadcastReceiver.class);
        stopIntent.setAction(MyBroadcastReceiver.poweroffString);
        PendingIntent stopPi = PendingIntent.getBroadcast(this, 1, stopIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setSmallIcon(R.mipmap.ic_launcher_foreground)
                .setContentTitle(getString(R.string.app_name))
                .setContentText("Synth engine running")
                .setPriority(NotificationCompat.PRIORITY_LOW)
                .setContentIntent(openPi)
                .setOngoing(true)
                .addAction(0, "Stop", stopPi);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            builder.setForegroundServiceBehavior(FOREGROUND_SERVICE_IMMEDIATE);

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                startForeground(NOTIFICATION_ID, builder.build(),
                        ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
            } else {
                startForeground(NOTIFICATION_ID, builder.build());
            }
            return true;
        } catch (IllegalStateException | SecurityException e) {
            // ForegroundServiceStartNotAllowedException (API 31+) extends
            // IllegalStateException; API 34+ can raise SecurityException over the
            // declared foregroundServiceType. Catching the supertypes keeps this
            // compiling and loading back to minSdk.
            //
            // Degrade rather than crash: the synth is already running and the
            // user hears no difference. All that is lost is the protection from
            // being killed in the background, which the system just declined.
            Log.w("SynthService", "Foreground service refused; running without it: " + e);
            return false;
        }
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
