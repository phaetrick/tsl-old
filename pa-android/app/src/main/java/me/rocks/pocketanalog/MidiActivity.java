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
import android.os.Looper;
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

    /** How often the byte counter is re-read while this dialog is on screen. */
    private static final long STATUS_POLL_MS = 500;

    private MidiManager mMidiManager;
    private MidiOutputPortConnectionSelector mPortSelector;
    private TextView mStatusView;
    private Spinner mSpinner;
    /**
     * True while the client holding our input port is the one this dialog made. Static
     * because the connection deliberately survives the dialog closing: an instance
     * field would come back false on reopen and label our own source as another app.
     */
    private static boolean sConnectedByUs;
    private final Handler mStatusPoll = new Handler(Looper.getMainLooper());

    private final MidiSynthDeviceService.StatusListener mStatusListener =
            new MidiSynthDeviceService.StatusListener() {
                @Override
                public void onInputPortOpenChanged(final boolean open) {
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            if (!open) {
                                sConnectedByUs = false;
                            }
                            updateStatus();
                        }
                    });
                }
            };

    private final Runnable mStatusTick = new Runnable() {
        @Override
        public void run() {
            updateStatus();
            mStatusPoll.postDelayed(this, STATUS_POLL_MS);
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.midi_activity);
        mStatusView = (TextView) findViewById(R.id.text_midi_status);
        mSpinner = (Spinner) findViewById(R.id.spinner_synth_sender);

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

        // Must match res/xml/grainstorm_device_info.xml exactly. This was left at
        // "Grainstorm" through the rename, so findDevice() returned null, the
        // destination stayed null, and every source the user picked in the spinner
        // was connected to nothing at all.
        MidiDeviceInfo synthInfo = MidiTools.findDevice(mMidiManager, "The Secret Laboratory",
                "Voltaic");
        if (synthInfo == null) {
            // Never fail silently here again: without a destination there is no MIDI
            // input, however healthy the spinner looks.
            Log.e(MidiConstants.TAG, "own MIDI device service not found, no MIDI input");
            Toast.makeText(MidiActivity.this,
                    "MIDI unavailable!", Toast.LENGTH_LONG)
                    .show();
            return;
        }
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

    @Override
    protected void onResume() {
        super.onResume();
        MidiSynthDeviceService.setStatusListener(mStatusListener);
        updateStatus();
        mStatusPoll.postDelayed(mStatusTick, STATUS_POLL_MS);
    }

    @Override
    protected void onPause() {
        mStatusPoll.removeCallbacks(mStatusTick);
        MidiSynthDeviceService.setStatusListener(null);
        super.onPause();
    }

    /**
     * The dialog used to look identical whether MIDI was flowing or not, which is
     * exactly how a destination that connected to nothing stayed invisible. Two things
     * are worth saying out loud: that something holds the port at all, and whether any
     * bytes have actually arrived through it.
     */
    private void updateStatus() {
        if (mStatusView == null) {
            return;
        }
        boolean open = MidiSynthDeviceService.isInputPortOpen();
        if (!open) {
            // Self-heal: the port can close while this dialog is not listening, and a
            // stale "ours" would then mislabel the next client as ours.
            sConnectedByUs = false;
        }
        int bytes = MidiSynthDeviceService.getMidiByteCount();
        String name = sConnectedByUs ? MidiPortSelector.getCurrentDeviceName() : null;
        if (!open) {
            mStatusView.setText(R.string.midi_status_idle);
        } else if (name != null) {
            mStatusView.setText(getString(R.string.midi_status_self_named, name, bytes));
        } else if (sConnectedByUs) {
            mStatusView.setText(getString(R.string.midi_status_self, bytes));
        } else {
            // Android tells a MidiDeviceService THAT its input port is open, never
            // which client opened it -- MidiDeviceStatus carries our own device info
            // and the per-port flags, and nothing about the other side.
            mStatusView.setText(getString(R.string.midi_status_external, bytes));
        }
        // One input port, opened exclusively: while another app holds it there is
        // nothing for the spinner to connect, so say so by disabling it rather than
        // letting a selection fail with a toast.
        if (mSpinner != null) {
            mSpinner.setEnabled(!open || sConnectedByUs);
        }
    }

    private class MyPortsConnectedListener
            implements MidiPortConnector.OnPortsConnectedListener {
        @Override
        public void onPortsConnected(final MidiConnection connection) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    sConnectedByUs = connection != null;
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
                    updateStatus();
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