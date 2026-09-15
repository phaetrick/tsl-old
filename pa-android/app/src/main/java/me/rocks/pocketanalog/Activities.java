package me.rocks.pocketanalog;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Environment;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.preference.PreferenceManager;
import android.provider.DocumentsContract;
import android.util.TypedValue;

import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;


public class Activities {
    final static String APP_NAME = "MainActivity";
    //    final static String DEFAULT_HOME_PATH = String.format("%s%s%s", Environment.getExternalStorageDirectory(), File.separator, APP_NAME);
    final static String LOG_TAG = APP_NAME;

    // indices match C++ format codes: 0=WAV16, 1=MP3, 10=FLAC16, 11=FLAC24, 12=WAV32
    final static String[] recordingExtensions = {"wav", "mp3", "mp3", "mp3", "mp3", "mp3", "mp3", "flac", "flac", "flac", "flac", "flac", "wav"};
/*
    final static String[] decodingErrors = {
            "OK", //0
            "Error opening file." //1
            , "No Audiostreams Found" //2
            , "Codec not supported."//3
            , "Could not allocate decoding context."//4
            , "Error setting codec context parameters.",//5
            "Unable to allocate resampler context.",//6
            "Something went wrong. Unable to init resampler context.",//7
            "Something went wrong. av_frame_alloc() == nullptr",//8
            "Something went wrong. av_frame_alloc() == nullptr",//9
            "Out of memory error.", //10
            "Out of memory error.", //11

            "",//12
            "", //13
            ""//14
    };
*/
    /* Checks if external storage is available to at least read */
    public static boolean isExternalStorageReadable() {
        String state = Environment.getExternalStorageState();
        return (Environment.MEDIA_MOUNTED.equals(state) ||
                Environment.MEDIA_MOUNTED_READ_ONLY.equals(state));
    }

    static void setDefaultsString(String key, String value, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        SharedPreferences.Editor editor = preferences.edit();
        editor.putString(key, value);
        editor.apply();
    }

    /**
     * Like setDefaultsString but SYNCHRONOUS. apply() only guarantees the value
     * in memory and schedules the disk write; an abrupt kill can lose it. That
     * is fine for a preference, and not fine for a journal entry whose whole
     * purpose is surviving exactly that kill -- so those use commit().
     */
    static boolean setDefaultsStringSync(String key, String value, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        return preferences.edit().putString(key, value).commit();
    }

    static String getDefaultsString(String key, String defaultvalue, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        return preferences.getString(key, defaultvalue);
    }

    static void setDefaultsInt(String key, int value, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        SharedPreferences.Editor editor = preferences.edit();
        editor.putString(key, Integer.toString(value));
        editor.apply();
    }

    static int getDefaultsInt(String key, int defaultvalue, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        return Integer.parseInt(preferences.getString(key, Integer.toString(defaultvalue)));
    }

    // Stored as a real long, not the stringified form setDefaultsInt uses: these
    // are never rendered by a PreferenceFragment, so there is no string-typed
    // widget on the other side that would choke on the type.
    static long getDefaultsLong(String key, long defaultvalue, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        return preferences.getLong(key, defaultvalue);
    }

    static void setDefaultsLong(String key, long value, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        preferences.edit().putLong(key, value).apply();
    }

    static boolean getDefaultsBoolean(String key, boolean defaultvalue, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        return preferences.getBoolean(key, defaultvalue);
    }

    static void setDefaultsBoolean(String key, boolean value, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        preferences.edit().putBoolean(key, value).apply();
    }

    static boolean isExternalStorageWritable() {
        return Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState());
    }

    static String getHomePath(Context context){
        File externalfilesdir = Activities.isExternalStorageWritable() ? context.getExternalFilesDir(null) : null;
        if (externalfilesdir == null) {
            return null;
        }
        return externalfilesdir.getAbsolutePath();
    }

    static String getHomePath(Context context, String subDir) {
        File externalfilesdir = Activities.isExternalStorageWritable() ? context.getExternalFilesDir(null) : null;
        if (externalfilesdir == null) {
            return null;
        }
        String home = externalfilesdir.getAbsolutePath();
        if(subDir != null){
            File sub = new File(home + "/" + subDir);
            boolean success = true;

            if (!sub.exists())
                success = sub.mkdirs();
            if (!success) {
                return null;
            }
            else return sub.getAbsolutePath();
        }
        else return home;
    }


    static boolean prepareHome(Context context) {
        File externalfilesdir = Activities.isExternalStorageWritable() ? context.getExternalFilesDir(null) : null;
        if (externalfilesdir == null) {
            return true;
        }

        File presetdir = new File(externalfilesdir.getAbsolutePath() + "/presets");
        File midimappingsdir = new File(externalfilesdir.getAbsolutePath() + "/midimappings");

        boolean success = true;

        if (!presetdir.exists())
            success = presetdir.mkdir();

        if (success) {
            if (!midimappingsdir.exists())
                success = midimappingsdir.mkdir();
        }
        if (!success) {
            return true;
        } else {
            MainActivity.homeDirectory = externalfilesdir.toString();
            MainActivity.presetDirectory = presetdir.toString();
            MainActivity.midimappingDirectory = midimappingsdir.toString();
            return false;
        }

    }

    /**
     * Human-readable label for a SAF tree, e.g. "Internal storage/Documents/Voltaic".
     *
     * NOT a filesystem path, deliberately. FileUtil.getFullPathFromTreeUri tries to
     * build one by reflecting on the hidden StorageManager.getVolumeList() and
     * StorageVolume internals; Android 9 blocked non-SDK interfaces, so that returns
     * null on any current device and the helper falls through to returning "/". That
     * is what the settings screen has been showing.
     *
     * And a real path may not exist at all: a tree on Drive or a NAS is served by a
     * DocumentsProvider with nothing behind it on the filesystem. So this decodes the
     * tree's document id -- "primary:Documents/Voltaic" -- and names the volume via
     * the PUBLIC StorageVolume.getDescription() (API 24). Providers whose ids carry
     * no volume fall back to the folder's own display name.
     */
    static String describeTree(Context context, Uri treeUri) {
        if (context == null || treeUri == null) return null;
        try {
            String docId = DocumentsContract.getTreeDocumentId(treeUri);
            int sep = docId.indexOf(':');
            if (sep < 0) {
                DocumentFile d = DocumentFile.fromTreeUri(context, treeUri);
                String name = d != null ? d.getName() : null;
                return name != null ? name : "Selected folder";
            }
            String volId = docId.substring(0, sep);
            String rel = docId.substring(sep + 1);
            String vol = "Internal storage";
            if (!"primary".equalsIgnoreCase(volId)) {
                vol = volId;
                StorageManager sm = (StorageManager) context.getSystemService(Context.STORAGE_SERVICE);
                if (sm != null) {
                    for (StorageVolume v : sm.getStorageVolumes()) {
                        String uuid = v.getUuid();
                        if (uuid != null && uuid.equalsIgnoreCase(volId)) {
                            String desc = v.getDescription(context);
                            if (desc != null) vol = desc;
                            break;
                        }
                    }
                }
            }
            return rel.isEmpty() ? vol : vol + " > " + rel;   // "Internal storage > Music/Voltaic"
        } catch (Exception e) {
            return null;
        }
    }

    static int dpTopx(Context context, int dp){
        Resources r = context.getResources();

        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP,
                dp,
                r.getDisplayMetrics());
    }

}
