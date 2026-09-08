package me.rocks.grainstorm;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.preference.PreferenceManager;
import android.text.format.DateUtils;
import android.util.Log;
import android.util.TypedValue;

import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.io.IOException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Objects;
import java.util.TimeZone;
import java.util.concurrent.ConcurrentLinkedQueue;

import static me.rocks.grainstorm.MainActivity.getHome;
import static me.rocks.grainstorm.MainActivity.showToast;
import static me.rocks.grainstorm.MyApplication.read_header;
import static me.rocks.grainstorm.MyApplication.read_midiheader;
import static me.rocks.grainstorm.PresetActivity.SAVE_PRESET;
import static me.rocks.grainstorm.PresetActivity.SAVE_PROJECT;


public class Activities {
    static class QueueTask {
        void func(){};
    };

    static class Queue extends Thread {
        Queue(){
            start();
        }
        void push(QueueTask task){
            queue.add(task);
            synchronized (lock) {
                lock.notify();
            }
        }
        @Override
        public void run() {
            while(true) {
                synchronized (lock) {
                    try {
                        lock.wait();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    for (QueueTask obj = queue.poll(); obj != null; obj = queue.poll()) {
                        obj.func();
                    }
                }
            }
        }
        private final Object lock = new Object();
        private final ConcurrentLinkedQueue<QueueTask> queue = new ConcurrentLinkedQueue<>();
    };

    static Queue queue = new Queue();

    final static String APP_NAME = "Grainstorm";
    //    final static String DEFAULT_HOME_PATH = String.format("%s%s%s", Environment.getExternalStorageDirectory(), File.separator, APP_NAME);
    final static String LOG_TAG = APP_NAME;

    final static String[] recordingExtensions = {"wav", "mp3", "mp3", "mp3", "mp3", "mp3", "mp3", "flac", "flac", "flac", "flac", "flac", "wav", "flac"};

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

    static boolean getDefaultsBoolean(String key, boolean defaultvalue, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        return preferences.getBoolean(key, defaultvalue);
    }

    static boolean isExternalStorageWritable() {
        return Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState());
    }

    static int dpTopx(Context context, int dp) {
        Resources r = context.getResources();

        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP,
                dp,
                r.getDisplayMetrics());
    }

}
