package me.rocks.pocketanalog;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

public class SynthService extends Service {
    private static final int NOTIFICATION_ID = 101;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {

        // A null intent means the system restarted this service by itself after
        // the process was killed. That was the crash reported from the field:
        // the restart happens with the app in the background, and on Android 12+
        // a foreground-service start from the background throws
        // ForegroundServiceStartNotAllowedException.
        //
        // It was also wrong on its own terms — the synth died with the process,
        // so the notification would have advertised an engine that is not there.
        if (intent == null) {
            stopSelf();
            return START_NOT_STICKY;
        }

        if ("STOP".equals(intent.getAction())) {
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

        Notification notification = new NotificationCompat.Builder(this, MyApplication.CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_stat_v)
                // Orange shows THROUGH the badge's V-shaped hole (see
                // ic_stat_v.xml) - the shade paints the mask black over a disc
                // of this color, which inverted reads as an orange V on black.
                .setColor(0xFFFFA500)
                // The real artwork - the neon V on black - as the card's large
                // icon, where full color IS allowed. Rendered via Canvas:
                // ic_launcher is an adaptive-icon XML, which BitmapFactory
                // cannot decode (it returns null and the icon silently
                // disappears).
                .setLargeIcon(launcherBitmap())
                .setContentTitle(getString(R.string.app_name))
                .setContentText("Synth engine running")
                .setPriority(NotificationCompat.PRIORITY_LOW)
                .setContentIntent(openPi)
                .setOngoing(true)
                .addAction(0, "Stop", stopPi)
                .build();

        try {
            startForeground(NOTIFICATION_ID, notification);
            return true;
        } catch (IllegalStateException | SecurityException e) {
            // ForegroundServiceStartNotAllowedException (API 31+) extends
            // IllegalStateException; API 34+ can raise SecurityException over the
            // declared foregroundServiceType. Catching the supertypes keeps this
            // compiling and loading back to minSdk 24.
            //
            // Degrade rather than crash: the synth is already running and the
            // user hears no difference. All that is lost is the protection from
            // being killed in the background, which the system just declined.
            Log.w("SynthService", "Foreground service refused; running without it: " + e);
            return false;
        }
    }

    // Swipe-away semantics depend on whether the user can SEE this service.
    //
    // The service always runs with the synth (powerControl, targetSdk 37), and
    // on 13+ its notification is invisible until POST_NOTIFICATIONS is granted.
    // In that state a swiped-away app would keep sounding with no handle to
    // stop it -- so the swipe is honored as STOP. With the notification visible
    // (or below 13, where it always shows), swiping away deliberately keeps
    // playback running: that is background-playback mode, and the Stop button
    // is in the shade.
    //
    // powerOff() is the same path the notification's Stop action takes: it
    // powers the engine down and the engine's power callback stops this
    // service through powerControl(false), while the process still holds the
    // foreground service and may legally receive the STOP intent.
    @Override
    public void onTaskRemoved(Intent rootIntent) {
        if (!notificationVisible()) MainActivity.powerOff();
        super.onTaskRemoved(rootIntent);
    }

    /** The launcher artwork as a bitmap. Adaptive-icon XMLs need to be DRAWN
     *  into one - BitmapFactory.decodeResource returns null for them. */
    private android.graphics.Bitmap launcherBitmap() {
        android.graphics.drawable.Drawable d =
                androidx.core.content.ContextCompat.getDrawable(this, R.mipmap.ic_launcher);
        if (d == null) return null;
        int w = d.getIntrinsicWidth()  > 0 ? d.getIntrinsicWidth()  : 128;
        int h = d.getIntrinsicHeight() > 0 ? d.getIntrinsicHeight() : 128;
        android.graphics.Bitmap b =
                android.graphics.Bitmap.createBitmap(w, h, android.graphics.Bitmap.Config.ARGB_8888);
        android.graphics.Canvas c = new android.graphics.Canvas(b);
        d.setBounds(0, 0, w, h);
        d.draw(c);
        return b;
    }

    /** Whether our foreground notification can actually be seen: app-level
     *  notifications enabled AND (from O up) our channel not blocked. */
    private boolean notificationVisible() {
        androidx.core.app.NotificationManagerCompat nm =
                androidx.core.app.NotificationManagerCompat.from(this);
        if (!nm.areNotificationsEnabled()) return false;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            android.app.NotificationChannel ch =
                    nm.getNotificationChannel(MyApplication.CHANNEL_ID);
            if (ch != null && ch.getImportance()
                    == android.app.NotificationManager.IMPORTANCE_NONE) return false;
        }
        return true;
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
