package me.rocks.grainstorm;

import static android.app.Notification.FOREGROUND_SERVICE_IMMEDIATE;
import static android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;

import android.Manifest;
import android.app.ActivityManager;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.IBinder;
import android.app.PendingIntent;
import android.util.Log;

import androidx.core.app.ActivityCompat;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

import java.util.List;

public class MicrophoneService extends Service {

    private static final String TAG_FOREGROUND_SERVICE = "MIC_FOREGROUND_SERVICE";
    public static final int NOTIFICATION_ID = 102;

    public static final String ACTION_START_FOREGROUND_SERVICE = "ACTION_START_FOREGROUND_SERVICE";
    public static final String ACTION_STOP_FOREGROUND_SERVICE = "ACTION_STOP_FOREGROUND_SERVICE";
    public static final String ACTION_PAUSE = "ACTION_PAUSE";
    public static final String ACTION_PLAY = "ACTION_PLAY";

    private long notificationStartTime = 0;
    final static Object mutex = new Object();
    final static Object destroyMutex = new Object();
    static boolean error = false;
    static boolean isDestroyed = true; // Start as destroyed

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {


        if (intent == null || intent.getAction() == null) {
            Log.w(TAG_FOREGROUND_SERVICE, "Service restarted with null intent. Stopping self.");
            stopForegroundService();
            return START_NOT_STICKY;
        }

        String action = intent.getAction();
        Log.d(TAG_FOREGROUND_SERVICE, "onStartCommand action: " + action);

        switch (action) {
            case ACTION_START_FOREGROUND_SERVICE:
                startForegroundService();
                break;
            case ACTION_STOP_FOREGROUND_SERVICE:
                stopForegroundService();
                break;
            default:
                Log.w(TAG_FOREGROUND_SERVICE, "Unknown action received: " + action);
                break;
        }

        return START_STICKY;
    }

    private void startForegroundService() {
        try {
            notificationStartTime = System.currentTimeMillis();
            Intent resultIntent = new Intent(this, MainActivity.class);
            resultIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
            PendingIntent resultPendingIntent = PendingIntent.getActivity(
                    this,
                    0,
                    resultIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
            );

            Intent powerIntent = new Intent(this, MyBroadcastReceiver.class);
            powerIntent.setAction(MainActivity.micoffString);

            PendingIntent pe2 = PendingIntent.getBroadcast(
                    this,
                    2,
                    powerIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
            );

            NotificationCompat.Builder mBuilder = new NotificationCompat.Builder(this, MyApplication.CHANNEL_ID)
                    .setSmallIcon(R.mipmap.ic_launcher_foreground)
                    .setContentTitle("Microphone Recording")
                    .setContentText("Recording in progress")
                    .setWhen(notificationStartTime)
                    .setShowWhen(true)
                    .setUsesChronometer(true)
                    .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                    .setContentIntent(resultPendingIntent)
                    .setAutoCancel(false)
                    .addAction(
                            R.drawable.ic_mic_white_24dp,
                            MainActivity.micoffString,
                            pe2
                    )
                    .setOngoing(true);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                mBuilder.setForegroundServiceBehavior(FOREGROUND_SERVICE_IMMEDIATE);
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                startForeground(
                        NOTIFICATION_ID,
                        mBuilder.build(),
                        FOREGROUND_SERVICE_TYPE_MICROPHONE
                );
            } else {
                startForeground(NOTIFICATION_ID, mBuilder.build());
            }

            Log.d(TAG_FOREGROUND_SERVICE, "Foreground service started successfully");

            synchronized (mutex) {
                error = false;
                mutex.notify();
            }

        } catch (Exception e) {
            Log.e(TAG_FOREGROUND_SERVICE, "Error starting foreground service", e);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                String exceptionName = e.getClass().getName();
                if (exceptionName.contains("ForegroundServiceStartNotAllowedException")) {
                    Log.e(TAG_FOREGROUND_SERVICE, "Cannot start from background - Android 12+ restriction");
                    MyApplication.showToast("Cannot start recording from background");
                }
            }

            if (e instanceof SecurityException) {
                Log.e(TAG_FOREGROUND_SERVICE, "Security exception - likely notification permission issue");
                MyApplication.showToast("Please enable notifications to use microphone recording");
            } else {
                MyApplication.showToast("Failed to start microphone service: " + e.getMessage());
            }
            stopForegroundService();
            synchronized (mutex) {
                error = true;
                mutex.notify();
            }
        }
    }

    private void stopForegroundService() {
        Log.d(TAG_FOREGROUND_SERVICE, "Stopping foreground service");

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(STOP_FOREGROUND_REMOVE);
        } else {
            stopForeground(true);
        }

        stopSelf();
        notificationStartTime = 0;
    }

    @Override
    public IBinder onBind(Intent intent) {
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG_FOREGROUND_SERVICE, "MicrophoneService onCreate");
        createNotificationChannel();

        synchronized (destroyMutex) {
            isDestroyed = false;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        notificationStartTime = 0;
        Log.d(TAG_FOREGROUND_SERVICE, "MicrophoneService onDestroy");

        synchronized (destroyMutex) {
            isDestroyed = true;
            destroyMutex.notifyAll(); // Wake up anyone waiting for destruction
        }
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    MyApplication.CHANNEL_ID,
                    "Microphone Service Channel",
                    NotificationManager.IMPORTANCE_DEFAULT
            );
            channel.setDescription("Microphone recording service notification");

            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }

    // Call this from MainActivity before starting service
    public static void waitForDestroy() {
        synchronized (destroyMutex) {
            while (!isDestroyed) {
                try {
                    Log.d(TAG_FOREGROUND_SERVICE, "Waiting for service to fully destroy...");
                    destroyMutex.wait(100); // Wait max 100ms
                } catch (InterruptedException e) {
                    break;
                }
            }
        }
    }
}