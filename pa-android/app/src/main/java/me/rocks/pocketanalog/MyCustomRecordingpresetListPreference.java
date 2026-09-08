package me.rocks.pocketanalog;

import android.content.Context;
import android.media.MediaRecorder;
import android.util.AttributeSet;

import androidx.preference.ListPreference;

public class MyCustomRecordingpresetListPreference extends ListPreference {
    private static final String[] presetValuesNormal = {Integer.toString(MediaRecorder.AudioSource.MIC), Integer.toString(MediaRecorder.AudioSource.VOICE_RECOGNITION)};
    private static final String[] presetValues24 = {Integer.toString(MediaRecorder.AudioSource.MIC), Integer.toString(MediaRecorder.AudioSource.VOICE_RECOGNITION), Integer.toString(MediaRecorder.AudioSource.UNPROCESSED)};
    private static final String[] presetEntries24 = {"Microphone", "Voice Recognition", "Unprocessed"};
    private static final String[] presetEntriesNormal = {"Microphone", "Voice Recognition"};

    // ...
    private Context mContext;
    private String[] mValues;
    private String[] mEntries;

    public MyCustomRecordingpresetListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            mEntries = presetEntries24;
            mValues = presetValues24;
        } else {
            mEntries = presetEntriesNormal;
            mValues = presetValuesNormal;
        }
        setEntries(mEntries);
        setEntryValues(mValues);
        String audioSource = Activities.getDefaultsString(context.getString(R.string.audiosource), Integer.toString(MediaRecorder.AudioSource.MIC), context);
        for (int i = 0; i < mValues.length; i++) {
            if (mValues[i].equals(audioSource)) {
                setValueIndex(i);
                break;
            }
        }
    }

    public MyCustomRecordingpresetListPreference(Context context) {
        this(context, null);
        mContext = context;
    }
}

