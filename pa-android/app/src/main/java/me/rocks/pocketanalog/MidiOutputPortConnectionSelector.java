package me.rocks.pocketanalog;

import android.app.Activity;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.util.Log;
import android.widget.Spinner;

import java.io.IOException;

/**
 * Select an output port and connect it to a destination input port.
 */
public class MidiOutputPortConnectionSelector extends MidiPortSelector {
    public final static String TAG = "MidiOutputPortConnectionSelector";
    private static MidiPortConnector mSynthConnector;
    private MidiDeviceInfo mDestinationDeviceInfo;
    private int mDestinationPortIndex;
    private MidiPortWrapper mLastWrapper;
    private MidiPortConnector.OnPortsConnectedListener mConnectedListener;

    /**
     * Create a selector for connecting to the destination input port.
     *
     * @param midiManager
     * @param activity
     * @param spinnerId
     * @param destinationDeviceInfo
     * @param destinationPortIndex
     */
     MidiOutputPortConnectionSelector(MidiManager midiManager,
                                            Activity activity, int spinnerId,
                                            MidiDeviceInfo destinationDeviceInfo, int destinationPortIndex) {
        super(midiManager, activity, spinnerId,
                MidiDeviceInfo.PortInfo.TYPE_OUTPUT);
        mDestinationDeviceInfo = destinationDeviceInfo;
        mDestinationPortIndex = destinationPortIndex;
    }

    @Override
    public void onPortSelected(final MidiPortWrapper wrapper) {
        // onNothingSelected() sends null, and equals() below would throw on it. This
        // is what the blanket catch was really covering for.
        if (wrapper == null || wrapper.equals(mLastWrapper)) {
            return;
        }
        try {
            if (mSynthConnector != null) {
                mSynthConnector.close();
                mSynthConnector = null;
            }
        } catch (IOException e) {
            Log.e(MidiConstants.TAG, "Exception in closeSynthResources()", e);
        }
        onClose();
        if (wrapper.getDeviceInfo() != null) {
            try {
                mSynthConnector = new MidiPortConnector(mMidiManager);
                mSynthConnector.connectToDevicePort(wrapper.getDeviceInfo(),
                        wrapper.getPortIndex(), mDestinationDeviceInfo,
                        mDestinationPortIndex,
                        // not safe on UI thread
                        mConnectedListener, null);
            } catch (Exception e) {
                // openDevice() throws for a bad destination. Swallowed, that read as a
                // successful connection: no toast, no events, nothing to go on.
                Log.e(MidiConstants.TAG, "could not connect " + wrapper
                        + " to destination", e);
                mSynthConnector = null;
                if (mConnectedListener != null) {
                    mConnectedListener.onPortsConnected(null);
                }
            }
        }
        mLastWrapper = wrapper;
    }

    @Override
    public void onClose() {
         /*
        try {
            if (mSynthConnector != null) {
                mSynthConnector.close();
                mSynthConnector = null;
            }
        } catch (IOException e) {
            Log.e(MidiConstants.TAG, "Exception in closeSynthResources()", e);
        }

          */
        super.onClose();
    }

    /**
     * @param connectedListener
     */
    public void setConnectedListener(
            MidiPortConnector.OnPortsConnectedListener connectedListener) {
        mConnectedListener = connectedListener;
    }
}
