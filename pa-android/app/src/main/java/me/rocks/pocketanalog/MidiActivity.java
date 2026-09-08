package me.rocks.pocketanalog;


import android.app.Activity;
import android.app.Fragment;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.media.midi.MidiDevice.MidiConnection;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.lang.ref.WeakReference;

/**
 * Simple synthesizer as a MIDI Device.
 */
public class MidiActivity extends Activity {
    static final String TAG = "Grainstorm";

    private MidiManager mMidiManager;
    private MidiOutputPortConnectionSelector mPortSelector;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.midi_activity);

        // Is Android MIDI supported?
        if (getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI)) {
            setupMidi();
        } else {
            Toast.makeText(MidiActivity.this,
                    "MIDI not supported!", Toast.LENGTH_LONG)
                    .show();
        }
    }

    private void setupMidi() {
        // Setup MIDI
        mMidiManager = (MidiManager) getSystemService(MIDI_SERVICE);

        MidiDeviceInfo synthInfo =  MidiTools.findDevice(mMidiManager, "The Secret Laboratory",
                "Grainstorm");
        int portIndex = 0;
        mPortSelector = new MidiOutputPortConnectionSelector(mMidiManager, this,
                R.id.spinner_synth_sender, synthInfo, portIndex);
        mPortSelector.setConnectedListener(new MyPortsConnectedListener());
    }

    private void closeSynthResources() {
        if (mPortSelector != null) {
            mPortSelector.close();
            mPortSelector.onDestroy();
        }
    }

    // TODO Listen to the synth server
    // for open/close events and then disable/enable the spinner.
    private class MyPortsConnectedListener
            implements MidiPortConnector.OnPortsConnectedListener {
        @Override
        public void onPortsConnected(final MidiConnection connection) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (connection == null) {
                        Toast.makeText(MidiActivity.this,
                                R.string.error_port_busy, Toast.LENGTH_LONG)
                                .show();
                        mPortSelector.clearSelection();
                    } else {
                        Toast.makeText(MidiActivity.this,
                                R.string.port_open_ok, Toast.LENGTH_LONG)
                                .show();
                    }
                }
            });
        }
    }

    @Override
    public void onDestroy() {
        closeSynthResources();
        super.onDestroy();
    }

}