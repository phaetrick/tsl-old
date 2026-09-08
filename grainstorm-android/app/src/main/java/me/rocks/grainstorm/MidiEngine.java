package me.rocks.grainstorm;

import android.media.midi.MidiReceiver;
import android.util.Log;
import android.widget.TextView;

import java.io.IOException;

import static me.rocks.grainstorm.MyApplication.java_receive_midievent;

/**
 * Very simple polyphonic, single channel synthesizer. It runs a background
 * thread that processes MIDI events and synthesizes audio.
 */
public class MidiEngine extends MidiReceiver {

    private int mMidiByteCount;
    private MidiFramer mFramer;
    private MidiReceiver mReceiver;

     MidiEngine() {
        mReceiver = new MyReceiver();
        mFramer = new MidiFramer(mReceiver);
    }


    /* This will be called when MIDI data arrives. */
    @Override
    public void onSend(byte[] data, int offset, int count, long timestamp)
            throws IOException {
        if (!MidiConstants.isAllActiveSensing(data, offset, count))
            mFramer.send(data, offset, count, timestamp);
        mMidiByteCount += count;
    }

    private class MyReceiver extends MidiReceiver {
        @Override
        public void onSend(byte[] data, int offset, int count, long timestamp)
                throws IOException {
            java_receive_midievent(data[0], data[1], data[2]);
        }

    }

     int getMidiByteCount() {
        return mMidiByteCount;
    }
}

