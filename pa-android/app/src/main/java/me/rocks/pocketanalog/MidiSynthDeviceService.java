package me.rocks.pocketanalog;

import android.media.midi.MidiDeviceService;
import android.media.midi.MidiDeviceStatus;
import android.media.midi.MidiReceiver;


public class MidiSynthDeviceService extends MidiDeviceService {
    private static MidiEngine mMidiEngine = new MidiEngine();
    // Static, and not tied to a service instance: the settings dialog asks for this
    // before any status callback of its own has arrived, and the service instance
    // comes and goes underneath it.
    private static volatile boolean sInputPortOpen;
    private static volatile StatusListener sStatusListener;

    /** Told when a client opens or closes the input port this app exposes. */
    interface StatusListener {
        void onInputPortOpenChanged(boolean open);
    }

    /**
     * Listen for clients of the exposed device. An external synth or sequencer that
     * picked this app as its MIDI destination arrives here exactly like the settings
     * dialog's own connection does -- the dialog tells the two apart itself, since it
     * knows which one it made. Pass null to stop listening.
     */
    static void setStatusListener(StatusListener listener) {
        sStatusListener = listener;
    }

    static boolean isInputPortOpen() {
        return sInputPortOpen;
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
        boolean open = status.isInputPortOpen(0);
        if (open == sInputPortOpen) {
            return;
        }
        sInputPortOpen = open;
        StatusListener listener = sStatusListener;
        if (listener != null) {
            listener.onInputPortOpenChanged(open);
        }
    }

    public static int getMidiByteCount() {
        return mMidiEngine.getMidiByteCount();
    }
}
