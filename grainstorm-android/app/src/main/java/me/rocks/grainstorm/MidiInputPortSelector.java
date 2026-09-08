package me.rocks.grainstorm;

import android.app.Activity;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiReceiver;
import android.util.Log;

import java.io.IOException;

/**
 * Manages a Spinner for selecting a MidiInputPort.
 */
public class MidiInputPortSelector extends MidiPortSelector {

    private MidiInputPort mInputPort;
    private MidiDevice mOpenDevice;

    /**
     * @param midiManager
     * @param activity
     * @param spinnerId ID from the layout resource
     */
    public MidiInputPortSelector(MidiManager midiManager, Activity activity,
                                 int spinnerId) {
        super(midiManager, activity, spinnerId, MidiDeviceInfo.PortInfo.TYPE_INPUT);
    }

    @Override
    public void onPortSelected(final MidiPortWrapper wrapper) {
        close();
        final MidiDeviceInfo info = wrapper.getDeviceInfo();
        if (info != null) {
            mMidiManager.openDevice(info, new MidiManager.OnDeviceOpenedListener() {
                @Override
                public void onDeviceOpened(MidiDevice device) {
                    if (device == null) {
                        Log.e(MidiConstants.TAG, "could not open " + info);
                    } else {
                        mOpenDevice = device;
                        mInputPort = mOpenDevice.openInputPort(
                                wrapper.getPortIndex());
                        if (mInputPort == null) {
                            Log.e(MidiConstants.TAG, "could not open input port on " + info);
                        }
                    }
                }
            }, null);
            // Don't run the callback on the UI thread because openInputPort might take a while.
        }
    }

    public MidiReceiver getReceiver() {
        return mInputPort;
    }

    @Override
    public void onClose() {
        try {
            if (mInputPort != null) {
                Log.i(MidiConstants.TAG, "MidiInputPortSelector.onClose() - close port");
                mInputPort.close();
            }
            mInputPort = null;
            if (mOpenDevice != null) {
                mOpenDevice.close();
            }
            mOpenDevice = null;
        } catch (IOException e) {
            Log.e(MidiConstants.TAG, "cleanup failed", e);
        }
        super.onClose();
    }
}
