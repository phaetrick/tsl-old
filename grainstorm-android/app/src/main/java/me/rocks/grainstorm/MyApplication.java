package me.rocks.grainstorm;

import android.app.Activity;
import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.Environment;
import android.util.Log;
import android.view.ViewConfiguration;
import android.widget.Toast;
import androidx.preference.PreferenceManager;
import java.io.File;
import static me.rocks.grainstorm.Activities.LOG_TAG;
import com.google.android.datatransport.backend.cct.BuildConfig;

public class MyApplication extends Application {

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

    native static void guiSetup(boolean df, int width, int height);

    native static void java_set_micrec_format(int format);

    native static int filebrowsercallback(String name);

    native static int save_preset_callback(String presetname);
    native static void java_load_preset(String uriString);

    native static int read_header(String uri, Object obj, boolean isProject);

    native static int save_midimapping_callback(String presetname);

    native static int read_midiheader(String uri, Object obj);

    native static void java_control(int c);

    //native static int java_recorder_control(int action);

 //   native static int java_recorder_init(int sr);

    native static long java_recorder_callback(long track, int bufsize);

    native static int java_record_live(int fd);

    native static int java_record_loop(int fd, long track);
    native static int java_save_loop(int fd, long track);

    native static void java_set_output_format(int format);

    native static void java_receive_midievent(byte one, byte two, byte three);
    native static void java_set_pauseplayback(boolean pauseplayback);
    //native static byte[] java_sig();

    native static void java_save_audio(boolean what);
    native static String java_ofl();

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

    /**
     * The largest callback the engine must be able to absorb, never -1.
     *
     * <p>Separate from {@link #audioBufIndex} because allocation happens in
     * setup(), before a stream exists, while Auto does not choose a block size
     * until the stream opens. Native sizes its buffers against this and checks
     * every callback against it.
     */
    static int maxBufSize;

    static int savedChannels;
    static boolean pausePlayback;
    static boolean useAAudio;
    static boolean screenOn;
    static boolean startPoweredOn;
    static boolean runsAsForeground;
    static boolean saveAudioWithPreset;
    static boolean systemFolderForPresets;
    static boolean hasMic;
    static int outputFormat;
    static int micRecFormat;

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

    /**
     * Ceiling for {@link #maxBufSize}. Covers the largest offered choice and
     * leaves room for Auto, whose block size the device picks at open time.
     */
    final static int MAX_BUFSIZE = 16384;
    private static boolean isInForeground = false;
    private int activityReferences = 0;
    private boolean isActivityChangingConfigurations = false;
    static {
        System.loadLibrary("grainstorm");
    }
    // Called when the application is starting, before any other application objects have been created.
    // Overriding this method is totally optional!
    static float dpi, mMinimumFlingVelocity,  mMaximumFlingVelocity;

    @Override
    public void onCreate() {
        if(BuildConfig.DEBUG || Debug.isDebuggerConnected())
           System.exit(0);
        thiz = this;
        super.onCreate();
        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {

            @Override
            public void onActivityStarted(Activity activity) {
                if (++activityReferences == 1 && !isActivityChangingConfigurations) {
                    isInForeground = true;
                }
            }

            @Override
            public void onActivityStopped(Activity activity) {
                isActivityChangingConfigurations = activity.isChangingConfigurations();
                if (--activityReferences == 0 && !isActivityChangingConfigurations) {
                    isInForeground = false;
                }
            }

            // Other lifecycle methods...
            @Override
            public void onActivityCreated(Activity activity, Bundle savedInstanceState) {}
            @Override
            public void onActivityResumed(Activity activity) {}
            @Override
            public void onActivityPaused(Activity activity) {}
            @Override
            public void onActivitySaveInstanceState(Activity activity, Bundle outState) {}
            @Override
            public void onActivityDestroyed(Activity activity) {}
        });
        dpi = getResources().getDisplayMetrics().density;
        mMinimumFlingVelocity =  ViewConfiguration.get(this).getScaledMinimumFlingVelocity();
        mMaximumFlingVelocity =  ViewConfiguration.get(this).getScaledMaximumFlingVelocity();
        deviceBurst = getDeviceBurst();
       // inBufSize = getMinInputFrameSize(inSampleRate, 1);
        // cpuIds = getExclusiveCores();
        PreferenceManager.setDefaultValues(this, R.xml.preferences_main, false);

        // No "== 0 means unset" repair here: 0 is Auto, a choice someone may
        // have made on purpose. An out-of-range index is native's to reject.
        audioBufIndex = Activities.getDefaultsInt(getString(R.string.bufsize3), DEFAULT_BUF_INDEX, this);
        // Auto (-1) has no block size to allocate against, and neither does a
        // stored value from a build that offered something no longer listed, so
        // the ceiling is a constant rather than a function of the choice.
        maxBufSize = MAX_BUFSIZE;
        Log.i(LOG_TAG, "Audio: engine " + ENGINE_SAMPLERATE + " Hz, burst " + deviceBurst
                + ", buffer index " + audioBufIndex);
        savedChannels = Activities.getDefaultsInt(getString(R.string.channels), 1, this);
        pausePlayback = Activities.getDefaultsBoolean(getString(R.string.pauseplayback), true, this);
        useAAudio = Activities.getDefaultsBoolean(getString(R.string.useaaudio), false, this) && Build.VERSION.SDK_INT >= 27;
        screenOn = Activities.getDefaultsBoolean(getString(R.string.screenonflag), false, this);
        copyFiles = Activities.getDefaultsBoolean(getString(R.string.filepicker), false, this);
        startPoweredOn = Activities.getDefaultsBoolean(getString(R.string.startPoweredOn), true, this);
        runsAsForeground = Activities.getDefaultsBoolean(getString(R.string.runAsForeground), true, this);
        saveAudioWithPreset = Activities.getDefaultsBoolean(getString(R.string.saveaudio), false, this);
        systemFolderForPresets = Activities.getDefaultsBoolean(getString(R.string.systemFolderForPresets), true, this);
        hasMic = getPackageManager().hasSystemFeature("android.hardware.microphone");
        outputFormat = Activities.getDefaultsInt(getString(R.string.recordingformat), 0, this);
        micRecFormat =  Activities.getDefaultsInt(getString(R.string.audiosource), 1, this);

        createNotificationChannel();
    }
    public static boolean isInForeground() {
        return isInForeground;
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
     * <p>Two things this deliberately no longer does. It does not multiply the
     * burst up to a usable buffer size: that number is now a user setting in
     * milliseconds, and mixing the two roles is what made the old value
     * impossible to read. And it is not converted to the engine's rate — the
     * property is in device-rate frames and native converts it where needed.
     *
     * <p>The fallback is a plausible burst, not a plausible buffer. The old
     * DEFAULT_OUT_BUFSIZE of 1920 was a multiplied value and would be wrong here
     * by roughly a factor of ten.
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
                Log.e("MainActivity ", e.getMessage());
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
            CharSequence name = "Grainstorm";
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
