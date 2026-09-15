package me.rocks.pocketanalog;

import android.app.Activity;
import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.os.Build;
import android.os.Debug;
import android.os.Environment;
import android.util.Log;
import android.view.ViewConfiguration;
import android.widget.Toast;


import androidx.preference.PreferenceManager;

//import com.crashlytics.android.Crashlytics;
//import com.crashlytics.android.ndk.CrashlyticsNdk;
//import io.fabric.sdk.android.Fabric;

import java.io.File;

import static me.rocks.pocketanalog.Activities.LOG_TAG;

import com.google.android.datatransport.backend.cct.BuildConfig;

public class MyApplication extends Application {

    final static String url_eula = "file:///android_asset/eula.html";
    final static String url_oss = "file:///android_asset/oss.html";
    final static String url_instructions = "https://pocketanalog.rocks.me/instructions.html";
    final static String url_changes = "https://pocketanalog.rocks.me/changes.html";

    /**
     * The engine's sample rate. A constant, not the device's.
     *
     * <p>Native sets up every table and filter for this rate during
     * {@code tsl::app::setup()}, which runs before any audio stream exists, so
     * the engine can never be told what the hardware chose — the stream is
     * asked for this rate instead and Oboe converts where the device differs.
     * Must stay equal to {@code tsl::kEngineSampleRate} in player.h.
     *
     * <p>Until this existed, OpenSL ES opened at Oboe's placeholder default of
     * 48000 while the engine ran at PROPERTY_OUTPUT_SAMPLE_RATE, so 44.1 kHz
     * devices played about two semitones sharp with no setting to escape it.
     */
    final static int ENGINE_SAMPLERATE = 48000;

    static int maxTextureSize = 2048;

    static int maxZoomFactor = 1;

    static MyApplication thiz = null;

    static MyApplication getInstance() {
        return thiz;
    }

    static Object getInstance2() {
        return thiz;
    }

    static {
        System.loadLibrary("nativelib");
    }

    native static void java_set_micrec_format(int format);

    native static void java_control(int c);

    native static String java_ofl();

    native static void guiSetup(boolean df, int width, int height);

    //native static int java_recorder_control(int action);

 //   native static int java_recorder_init(int sr);

    native static int java_recorder_callback(short[] array, int bufsize);

    native static int java_record_live(int fd);

    native static int java_record_loop(int fd, long track);

    native static void java_set_track_source(int track);

    native static void java_set_output_format(int format);

    native static void java_receive_midievent(byte one, byte two, byte three);

    native static int save_midimapping_callback(String name);

    native static void java_set_ignore_second_midievent(boolean ignore);
    native static void java_set_pauseplayback(boolean pauseplayback);
    native static void java_set_hqresampling(boolean hqresampling);
    native static void java_set_showfactory(boolean show);
    // Re-reads the preset directory into the engine after the preset folder moved.
    native static void java_reload_presets();
    // Trial gate verdict: 0 none/capped, 1 trial active, 2 expired. Pushed from
    // TrialGate once resolved, which may be after the UI is already up.
    native static void java_set_trial_state(int state);
    // A folder row's truth changed (a picker returned, a preset move
    // committed): the toolkit settings page re-reads its lines.
    native static void java_settings_changed();

    //native static byte[] java_sig();


    /**
     * Index into the buffer size list. 0 is Auto. Never a size.
     *
     * <p>Read by name from Player::BuildStream, which owns the table this
     * indexes ({@code kBufferChoiceFrames} in player.cpp) and resolves it to
     * frames at its own engine rate. Java stores the index and nothing else, so
     * there is no frame count or sample rate on this side to drift out of step
     * with the audio code.
     *
     * <p>Deliberately a new field and a new preference key: the old
     * savedBufSize held device-derived frame counts that meant a different
     * latency on every phone, and reusing either name would have read those
     * saved values as indices into a list they know nothing about.
     */
    static int audioBufIndex;

    static int savedChannels;
    static boolean ignoreSecondMidiEvent;
    static boolean pausePlayback;
    static boolean hqResampling;
    static boolean useAAudio;
    static boolean screenOn;
    static boolean startPoweredOn;
    // Read by native setup (GetStaticBooleanField, like startPoweredOn); live
    // changes go through java_set_showfactory.
    static boolean showFactoryPresets;


    static int[] cpuIds;
    final static int androidApi = Build.VERSION.SDK_INT;
    static boolean copyFiles = false;


    /** Raw hardware burst, device-rate frames. Read from native by name. */
    static int deviceBurst;

    /** Fallback burst when the device reports none. A burst, not a buffer. */
    final static int DEFAULT_BURST = 192;

    /**
     * Default: index 4, 40 ms. Chosen to land on the old default rather than to
     * be round — that multiplied the device burst up past 2000 frames, which
     * came out between 1920 and 2160 frames, i.e. 40 to 45 ms, on the phones it
     * was tested against.
     */
    final static int DEFAULT_BUF_INDEX = 4;

    // Called when the application is starting, before any other application objects have been created.
    // Overriding this method is totally optional!
    static float dpi, mMinimumFlingVelocity,  mMaximumFlingVelocity;


    @Override
    public void onCreate() {
        if(BuildConfig.DEBUG || Debug.isDebuggerConnected())
            ;//System.exit(0);
        thiz = this;
        super.onCreate();
        dpi = getResources().getDisplayMetrics().density;
        mMinimumFlingVelocity =  ViewConfiguration.get(this).getScaledMinimumFlingVelocity();
        mMaximumFlingVelocity =  ViewConfiguration.get(this).getScaledMaximumFlingVelocity();
        deviceBurst = getDeviceBurst();
       // inBufSize = getMinInputFrameSize(inSampleRate, 1);
        // cpuIds = getExclusiveCores();

//        PreferenceManager.setDefaultValues(this, R.xml.preferences_main, false);

        // No "== 0 means unset" repair here: 0 is Auto, a choice someone may
        // have made on purpose. An out-of-range index is native's to reject.
        audioBufIndex = Activities.getDefaultsInt(getString(R.string.bufsize3), DEFAULT_BUF_INDEX, this);
        Log.i(LOG_TAG, "Audio: engine " + ENGINE_SAMPLERATE + " Hz, burst " + deviceBurst
                + ", buffer index " + audioBufIndex);
        savedChannels = Activities.getDefaultsInt(getString(R.string.channels), 1, this);
        ignoreSecondMidiEvent = Activities.getDefaultsBoolean(getString(R.string.ignoresecondnoteonevent), false, this);
        pausePlayback = Activities.getDefaultsBoolean(getString(R.string.pauseplayback), true, this);
        hqResampling = Activities.getDefaultsBoolean(getString(R.string.hqresampling), false, this);
        useAAudio = Activities.getDefaultsBoolean(getString(R.string.useaaudio), false, this) && Build.VERSION.SDK_INT >= 27;
        screenOn = Activities.getDefaultsBoolean(getString(R.string.screenonflag), false, this);
        copyFiles = Activities.getDefaultsBoolean(getString(R.string.filepicker), false, this);
        startPoweredOn = Activities.getDefaultsBoolean(getString(R.string.startPoweredOn), true, this);
        showFactoryPresets = Activities.getDefaultsBoolean(getString(R.string.showfactory), true, this);


        createNotificationChannel();
    }


    public void startService(String action, Class clazz) {
        Intent intent = new Intent(this, clazz);
        if (action != null)
            intent.setAction(action);
        startService(intent);
    }

    /**
     * The device's hardware burst, raw and unmultiplied.
     *
     * <p>Passed to native for {@code oboe::DefaultStreamValues::FramesPerBurst},
     * which Oboe ships as a placeholder 192 and documents as the app's job to
     * fill in over JNI. Only the OpenSL ES path reads it — AAudio takes the
     * burst from the HAL — and it is what OpenSL ES divides the requested
     * capacity by to choose its queue length, so an honest value here matters.
     *
     * <p>It deliberately no longer multiplies the burst up to a usable buffer
     * size: that number is a user setting in milliseconds now, and mixing the
     * two roles is what made the old value impossible to read. The fallback is
     * a plausible burst, not a plausible buffer — the old DEFAULT_OUT_BUFSIZE
     * of 1920 was a multiplied value and would be wrong here by roughly 10x.
     */
    private int getDeviceBurst() {
        int framesPerBuffer = 0;
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (audioManager != null) {
            String bufsize = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            if (bufsize != null) {
                try {
                    framesPerBuffer = Integer.parseInt(bufsize);
                } catch (NumberFormatException ignored) {
                }
            }
        }
        if (framesPerBuffer <= 0) {
            Log.w(LOG_TAG, "No native burst size; using " + DEFAULT_BURST);
            framesPerBuffer = DEFAULT_BURST;
        }
        return framesPerBuffer;
    }

    static int getMinInputFrameSize(int sampleRate, int channels) {
        int channelConfig;
        if (channels == 1) {
            channelConfig = AudioFormat.CHANNEL_IN_MONO;
        } else if (channels == 2) {
            channelConfig = AudioFormat.CHANNEL_IN_STEREO;
        } else {
            return -1;
        }
        return AudioRecord.getMinBufferSize(sampleRate, channelConfig,
                AudioFormat.ENCODING_PCM_16BIT);
    }

    static String getHomeDirectory() {
        if (!Activities.isExternalStorageWritable())
            return null;
        String path = Environment.getExternalStorageDirectory() + File.separator + Activities.APP_NAME + File.separator + Activities.APP_NAME;
        Log.i(LOG_TAG, path);
        File file = new File(path);
        if (!file.exists()) {
            boolean success = file.mkdir();
            if (success) {
                return path;
            } else {
                return null;
            }
        } else if (file.canRead() && file.canWrite() && file.canExecute()) {
            return path;
        } else {
            return null;
        }
    }

    static boolean setprio(int tid, int prio) {
        try {
            android.os.Process.setThreadPriority(tid, prio);
        } catch (final SecurityException e) {
            if (BuildConfig.DEBUG)
                Log.i("MainActivity", e.getMessage());
            return false;
        }
        return true;
    }

    public boolean hasMic() {
        return getPackageManager().hasSystemFeature("android.hardware.microphone");
    }

    Object getAssetManager() {
        return getAssets();
    }


    private int[] getExclusiveCores() {
        int exclusiveCores[] = {};

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            Log.w("MainActivity", "getExclusiveCores() not supported. Only available on API " +
                    Build.VERSION_CODES.N + "+");
        } else {
            try {
                exclusiveCores = android.os.Process.getExclusiveCores();
            } catch (RuntimeException e) {
                Log.w("MainActivity", "getExclusiveCores() is not supported on this device.");
            }
        }
        return exclusiveCores;
    }

    final static String CHANNEL_ID = "Engine";

    private void createNotificationChannel() {
        // Create the NotificationChannel, but only on API 26+ because
        // the NotificationChannel class is new and not in the support library
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            CharSequence name = getString(R.string.app_name);
            String description = "Engine on.";
            int importance = NotificationManager.IMPORTANCE_LOW;
            NotificationChannel channel = new NotificationChannel(CHANNEL_ID, name, importance);
            channel.setDescription(description);
            channel.setSound(null,null);// <---- ignore sound
            // Register the channel with the system; you can't change the importance
            // or other notification behaviors after this
            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }
    }

    static void showToast(String toast){
        MainActivity.showToast(toast);
    }
    static void showToast2(Context ctx, String toast){
        if(ctx != null)
            Toast.makeText(ctx, toast, Toast.LENGTH_LONG).show();
        }
}
