# To enable ProGuard in your project, edit project.properties
# to define the proguard.config property as described in that file.
#
# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in ${sdk.dir}/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the ProGuard
# include property in project.properties.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Add any project specific keep options here:

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#
#-keep class me.rocks.grainstorm.Grainstorm {
#    public <methods>;
#}

-keep class me.rocks.grainstorm.MyApplication{
    int outputFormat;
    int micRecFormat;
    boolean hasMic;
    static int maxBufSize;
    void startService(...);
    static boolean saveAudioWithPreset;
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
    static boolean pausePlayback;
    static boolean startPoweredOn;
        static boolean useAAudio;
    me.rocks.grainstorm.MyApplication getInstance();
    java.lang.Object getInstance2();
    java.lang.String getHomeDirectory();
    boolean setprio(...);
    boolean hasMic();
    void java_control(...);
    java.lang.Object getAssetManager();
     void java_set_pauseplayback(...);
        void java_receive_midievent(...);
        int save_midimapping_callback(...);
        int read_midiheader(...);
        void showToast(...);
            void java_set_micrec_format(...);
                long java_recorder_callback(...);
                    void java_save_audio(...);
    void java_load_preset(...);
}

-keep class me.rocks.grainstorm.MainActivity {
    java.lang.String getFilePath(...);
    java.lang.String getDecodingPath(...);
    java.lang.String getStorageDirRec();
    java.lang.String getStoragePath(...);
    java.lang.Object[] getDirContent(...);
    int getFdFromUriString(...);
    boolean deleteFromUri(...);
    void powerControl(...);
    void showToast(...);
    void filebrowser(...);
    void savePreset(...);
    void recFile(...);
    void invokeSettings(...);
    void logString(...);
    void onNativeReady(...);
    # Called only from aigen.cpp via GetStaticMethodID, so R8 has no Java-side
    # reference and would rename it. Silent failure, exactly as described above:
    # the lookup returns null, modelsRoot() comes back empty, and the feature
    # reports "AI models not found" on every minified build.
    java.lang.String getAiModelsDir();
    long startAiModelDownload(...);
    java.lang.String queryAiModelDownload(...);
    # The fd-based model API grainstorm2/src/aigen.cpp looks up by name the
    # same way (aiStatic -> GetStaticMethodID). aiModelPresent survives
    # shrinking because Java calls it too, but obfuscation RENAMES it; the
    # other two have no Java caller and are removed outright. Either way the
    # native lookup returns null, the code falls back to the OLD plain path
    # under getAiModelsDir() -- which the Downloads-based flow never writes
    # -- and FROM AI reports "NO MODEL INSTALLED" on every minified build
    # while the settings page, calling Java directly, says installed.
    boolean aiModelPresent(...);
    int aiModelOpen(...);
    java.lang.String getAiLinkDir();
}


-keep class me.rocks.grainstorm.Recorder {
    long Record(...);
    int getFD(...);
    void closeFD(...);
}

 -keep class me.rocks.grainstorm.Presetitem**
 -keepclassmembers class me.rocks.grainstorm.Presetitem** {
    *;
 }
# Also protect the signature classes just in case
# 2. Prevent renaming of Android framework classes used in the signature check
-keep class android.content.pm.PackageManager { *; }
-keep class android.content.pm.PackageInfo { *; }
-keep class android.content.pm.Signature { *; }
-keep class android.content.pm.SigningInfo { *; }

-dontwarn android.support.**
-obfuscationdictionary compact.txt
#-repackageclasses ' '
#-optimizationpasses 5
#-allowaccessmodification