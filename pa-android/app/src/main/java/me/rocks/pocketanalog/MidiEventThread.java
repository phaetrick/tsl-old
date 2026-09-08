package me.rocks.pocketanalog;

import android.media.midi.MidiSender;
import android.util.Log;

import java.io.IOException;

public class MidiEventThread extends MidiEventScheduler {

    private EventThread mEventThread;
    MidiDispatcher mDispatcher = new MidiDispatcher();

    class EventThread extends Thread {
        private boolean go = true;

        @Override
        public void run() {
            while (go) {
                try {
                    MidiEvent event = (MidiEvent) waitNextEvent();
                    try {
                        Log.i(MidiConstants.TAG, "Fire event " + event.data[0] + " at "
                                + event.getTimestamp());
                        mDispatcher.send(event.data, 0,
                                event.count, event.getTimestamp());
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                    // Put event back in the pool for future use.
                    addEventToPool(event);
                } catch (InterruptedException e) {
                    // OK, this is how we stop the thread.
                }
            }
        }

        /**
         * Asynchronously tell the thread to stop.
         */
        public void requestStop() {
            go = false;
            interrupt();
        }
    }

    public void start() {
        stop();
        mEventThread = new EventThread();
        mEventThread.start();
    }

    /**
     * Asks the thread to stop then waits for it to stop.
     */
    public void stop() {
        if (mEventThread != null) {
            mEventThread.requestStop();
            try {
                mEventThread.join(500);
            } catch (InterruptedException e) {
                Log.e(MidiConstants.TAG,
                        "Interrupted while waiting for MIDI EventScheduler thread to stop.");
            } finally {
                mEventThread = null;
            }
        }
    }

    public MidiSender getSender() {
        return mDispatcher.getSender();
    }

}
