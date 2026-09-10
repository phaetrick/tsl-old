package me.rocks.pocketanalog;

import android.media.midi.MidiReceiver;
import android.util.Log;
import android.widget.TextView;

import java.io.IOException;

import static me.rocks.pocketanalog.MyApplication.java_receive_midievent;

/**
 * Very simple polyphonic, single channel synthesizer. It runs a background
 * thread that processes MIDI events and synthesizes audio.
 */
public class MidiEngine extends MidiReceiver {

    // Written on the MIDI thread, read by the settings dialog on the UI thread.
    private volatile int mMidiByteCount;
    private final MidiFramer mFramer;

    MidiEngine() {
        MidiReceiver mReceiver = new MyReceiver();
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

    private static class MyReceiver extends MidiReceiver {
        @Override
        public void onSend(byte[] data, int offset, int count, long timestamp)
                throws IOException {
            if (count < 1) {
                return;
            }
            // offset is NOT always 0. MidiFramer hands channel messages over from its
            // own 3-byte buffer, which does start at 0 -- but it forwards real-time
            // bytes and SysEx straight out of the caller's array, at the offset they
            // actually sit at. Reading data[0..2] there re-sent the first three bytes
            // of the packet instead: 90 3C 64 FE delivered the Note On, then delivered
            // it AGAIN for the trailing Active Sensing byte.
            //
            // count matters for the same reason -- a one- or two-byte message has no
            // third byte to read, and past the end of a short array that threw, which
            // MidiFramer's catch-all swallowed along with the rest of the packet.
            byte status = data[offset];
            byte data1 = count > 1 ? data[offset + 1] : 0;
            byte data2 = count > 2 ? data[offset + 2] : 0;
            java_receive_midievent(status, data1, data2);
        }

    }

     int getMidiByteCount() {
        return mMidiByteCount;
    }
}

