# Pocket Analog ProGuard Rules

# Keep the Application class and its members accessed via JNI
-keep class me.rocks.pocketanalog.MyApplication {
    static me.rocks.pocketanalog.MyApplication thiz;
    static float dpi;
    static float mMaximumFlingVelocity;
    static float mMinimumFlingVelocity;
    static int[] cpuIds;
    static int androidApi;
    # Read from native by NAME (Player::BuildStream), never from Java, so R8
    # has no reference to keep them by and will rename them unless listed here.
    # A renamed field is silent: GetStaticFieldID just fails and the native
    # fallback takes over, which once turned the buffer setting into "Auto" and
    # the burst into Oboe's placeholder 192 on every minified build.
    static int audioBufIndex;
    static int deviceBurst;
    static int savedChannels;
    static boolean ignoreSecondMidiEvent;
    static boolean pausePlayback;
    static boolean hqResampling;
    static boolean startPoweredOn;
    static boolean runAsForeGround;
    static boolean useAAudio;
    # Read by name in va/setup.cpp at startup. Left out until 2026-08-20, so on
    # every minified build GetStaticFieldID failed and the toggle silently
    # reverted to its default on each launch.
    static boolean showFactoryPresets;

    static me.rocks.pocketanalog.MyApplication getInstance();
    static java.lang.Object getInstance2();
    static java.lang.String getHomeDirectory();
    static boolean setprio(int, int);
    static void showToast(java.lang.String);
    boolean hasMic();
    java.lang.Object getAssetManager();
    native <methods>;
}

# Keep the MainActivity and its members accessed via JNI
-keep class me.rocks.pocketanalog.MainActivity {
    static me.rocks.pocketanalog.MainActivity getInstance();
    static java.lang.Object getInstance2();
    static void onNativeReady();
    static boolean crashHandler(java.lang.String);
    static void outOfMemory();
    static java.lang.String getStoragePath(java.lang.String);
    static java.lang.Object[] getDirContent(java.lang.String);
    static int getFdFromUriString(java.lang.String, java.lang.String);
    static boolean deleteFromUri(java.lang.String);
    static void showToast(java.lang.String);
    static void logString(java.lang.String);
    static void filebrowser(int);
    static void powerControl(boolean);

    static void invokeSettings();
    # Session-cap bridge, called from native only (va/gui.cpp startSessionCap):
    # unlisted, R8 renames them and the JNI lookup fails silently — the model
    # notice never shows and the wall's UNLOCK button does nothing.
    static void invokeUpgrade();
    static boolean capNoticeSeen();
    # The toolkit settings page's rows (va2/src/settings.cpp), by name.
    static int getBufferIndex();
    static void setBufferIndex(int);
    static boolean aaudioAvailable();
    static boolean getUseAAudio();
    static void setUseAAudio(boolean);
    static boolean getKeepScreenOn();
    static void setKeepScreenOn(boolean);
    static boolean getStartPoweredOn();
    static void setStartPoweredOn(boolean);
    static void chooseRecordingFolder();
    static void choosePresetFolder();
    static java.lang.String recordingFolderLabel();
    static java.lang.String trialStatusSummary();
    static boolean isUnlocked();
    static boolean getShowNotification();
    static void setShowNotification(boolean);
    static void openMidiSettings();
    static void releasePresetFolder();
    static java.lang.String presetFolderLabel();
    static boolean presetFolderIsCustom();
    # Preset-folder bridge, called from native only (va/preset.cpp).
    static boolean presetFolderIsCustom();
    static boolean presetFolderReachable();
    static java.lang.String createStorageFile(java.lang.String, java.lang.String);
    static java.lang.String openStorageFile(java.lang.String, java.lang.String, java.lang.String);
    static void recFile(int, long);
    static int getFd(java.lang.String);
    int getOutputFormat();
    int getMicRecFormat();
    boolean hasMic();
    native <methods>;
}

# Keep the Presetitem class as it is used in Object arrays passed to/from JNI
-keep class me.rocks.pocketanalog.Presetitem {
    *;
}

# General JNI keep rules
-keep class me.rocks.pocketanalog.MyBroadcastReceiver { *; }

-keepclasseswithmembernames class * {
    native <methods>;
}

-keepclassmembers class * {
    @androidx.annotation.Keep *;
}

-dontwarn android.support.**
-dontwarn com.google.android.material.**
-dontwarn androidx.**

# Keep models/pojos that might be serialized or accessed via JNI
-keep class me.rocks.pocketanalog.models.** { *; }

# Ed25519 for the trial token (TrialGate). Used through the low-level crypto API
# only -- never by registering the JCE provider -- so the rest of BouncyCastle is
# free to be shrunk away. These two are named so the classes survive under a name
# the dex can be grepped for: if R8 ever did drop them, verify() would return
# false for every token, every device would resolve to "no trial", and the whole
# feature would fail silently as capped-sessions-forever.
-keep class org.bouncycastle.crypto.signers.Ed25519Signer { *; }
-keep class org.bouncycastle.crypto.params.Ed25519PublicKeyParameters { *; }
-dontwarn org.bouncycastle.**
# BC ships providers for JVM APIs that do not exist on Android; nothing here
# touches them, and without this R8 warns on every build.
-dontwarn javax.naming.**

-obfuscationdictionary compact.txt
