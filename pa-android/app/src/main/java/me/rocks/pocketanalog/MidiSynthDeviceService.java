package me.rocks.pocketanalog;

import android.media.midi.MidiDeviceService;
import android.media.midi.MidiDeviceStatus;
import android.media.midi.MidiReceiver;


public class MidiSynthDeviceService extends MidiDeviceService {
    private static MidiEngine mMidiEngine = new MidiEngine();
    private boolean midiStarted = false;
    private static MidiSynthDeviceService mInstance;

    @Override
    public void onCreate() {
        super.onCreate();
        mInstance = this;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    @Override
    public MidiReceiver[] onGetInputPortReceivers() {
        return new MidiReceiver[] { mMidiEngine };
    }

    /**
     * This will get called when clients connect or disconnect.
     */
    @Override
    public void onDeviceStatusChanged(MidiDeviceStatus status) {
        if (status.isInputPortOpen(0) && !midiStarted) {
            midiStarted = true;
        } else if (!status.isInputPortOpen(0) && midiStarted){
            midiStarted = false;
        }
    }

    public static int getMidiByteCount() {
        return mMidiEngine.getMidiByteCount();
    }
}
