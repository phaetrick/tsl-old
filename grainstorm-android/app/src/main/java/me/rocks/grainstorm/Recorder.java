package me.rocks.grainstorm;

import android.Manifest;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.TextView;

import androidx.core.app.ActivityCompat;
import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.text.DecimalFormatSymbols;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.Objects;
import android.os.Build;
import static me.rocks.grainstorm.MainActivity.showToast;
import static me.rocks.grainstorm.MyApplication.java_recorder_callback;
import static me.rocks.grainstorm.MainActivity.PERMISSION_ALL;

class Recorder  {
    // private static int bufSize = 1920;
    // private static int sampleRate = 48000;
    // private static int audioSource = MediaRecorder.AudioSource.MIC;
    private static String firstname = null;
    private static String secondname = null;
    private static boolean writefile = false;
    private static String outputFolder = null;
    private static DocumentFile outputfile = null;
    private final static String[] PERMISSIONS = {Manifest.permission.RECORD_AUDIO};

    private static String getRecordingTimeString(int sr, long samples) {
        long seconds = (long) Math.floor(samples / (float) sr);
        long minutes = (long) Math.floor(seconds / (float) 60);
        seconds = seconds % 60;
        return String.format(Locale.US, "%02d : %02d", minutes, seconds);
    }

    static long Record(long track) {
        writefile = false;
        MyApplication myApplication = MyApplication.getInstance();
        if (myApplication == null) {
            Log.e("Record", "MyApplication instance is null. Cannot proceed.");
            return 0;
        }

        try {
            // --- PRE-CHECKS (Excellent, no changes needed here) ---
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && !MyApplication.isInForeground()) {
                Log.e("Record", "App not in foreground - cannot start service on Android 12+");
                MyApplication.showToast("Cannot start recording from background");
                return 0;
            }

            // --- PERMISSION HANDLING ---
            // Get MainActivity instance ONCE for permission dialogs.
            MainActivity grainstorm = MainActivity.getInstance();

            if (ActivityCompat.checkSelfPermission(myApplication, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                Log.d("Record", "Requesting RECORD_AUDIO permission...");
                if (grainstorm == null || grainstorm.isFinishing()) {
                    Log.e("Record", "MainActivity is not available to request RECORD_AUDIO permission.");
                    return 0;
                }
                boolean granted = PermissionHandler.requestPermissionsBlocking(grainstorm, new String[]{Manifest.permission.RECORD_AUDIO}, 60);
                if (!granted) {
                    Log.e("Record", "RECORD_AUDIO permission denied.");
                    MyApplication.showToast("Record audio permission is required for recording.");
                    return 0;
                }
                Log.d("Record", "RECORD_AUDIO permission granted.");
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                if (ActivityCompat.checkSelfPermission(myApplication, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                    Log.d("Record", "Requesting POST_NOTIFICATIONS permission...");
                    if (grainstorm == null || grainstorm.isFinishing()) {
                        Log.e("Record", "MainActivity is not available to request POST_NOTIFICATIONS permission.");
                        return 0;
                    }
                    boolean granted = PermissionHandler.requestPermissionsBlocking(grainstorm, new String[]{Manifest.permission.POST_NOTIFICATIONS}, 60);
                    if (!granted) {
                        Log.e("Record", "Notification permission denied.");
                        MyApplication.showToast("Notification permission is required for recording status.");
                        return 0;
                    }
                    Log.d("Record", "Notification permission granted.");
                }
            }

            // --- SERVICE MANAGEMENT ---
            Intent startIntent = new Intent(myApplication, MicrophoneService.class);
            startIntent.setAction(MicrophoneService.ACTION_START_FOREGROUND_SERVICE);
            MicrophoneService.error = true; // Reset state before starting

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                myApplication.startForegroundService(startIntent);
            } else {
                myApplication.startService(startIntent);
            }

            // Wait for service to confirm startup
            synchronized (MicrophoneService.mutex) {
                MicrophoneService.mutex.wait(1000);
            }

            if (MicrophoneService.error) {
                Log.e("Record", "MicrophoneService failed to start.");
                MyApplication.showToast("Failed to start recording service.");
                return 0;
            }

            // --- RECORDING ---
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO);
            // Sized against the engine's rate, not the device's — native opens
            // the input stream at ENGINE_SAMPLERATE and lets Oboe convert.
            //
            // NOTE: getMinInputFrameSize returns AudioRecord.getMinBufferSize,
            // which is a count of BYTES, and this is passed on as a count of
            // FRAMES. At 16-bit mono that makes the requested callback twice
            // the intended size. Left as it was rather than halved as part of a
            // sample-rate change, but it is wrong and worth fixing on purpose.
            long result = java_recorder_callback(track, MyApplication.getMinInputFrameSize(MyApplication.ENGINE_SAMPLERATE, 1));

            // --- CLEANUP ---
            // Always stop the service using the application context, which is guaranteed to be non-null here.
            Log.d("Record", "Stopping microphone service.");
            Intent stopIntent = new Intent(myApplication, MicrophoneService.class);
            stopIntent.setAction(MicrophoneService.ACTION_STOP_FOREGROUND_SERVICE);
            myApplication.startService(stopIntent); // startService() is safe for stopping, even on API 26+
// Wait for it to fully destroy
            MicrophoneService.waitForDestroy();
            return result;

        } catch (InterruptedException e) {
            Log.e("Record", "Interrupted while waiting for service", e);
            Thread.currentThread().interrupt();
            MyApplication.showToast("Recording was interrupted.");
            return 0;
        } catch (Exception e) {
            Log.e("Record", "An unexpected error occurred in the Record method", e);
            MyApplication.showToast("A recording error occurred: " + e.getMessage());
            return 0;
        }
    }

    static int getFD() {
        try {
            final MainActivity grainstorm = MainActivity.getInstance();
            if (grainstorm == null)
                return 0;
            final String format = Activities.recordingExtensions[Activities.getDefaultsInt(grainstorm.getString(R.string.recordingformat), 0, grainstorm)];
            final String extension = "." + format;
            writefile = Activities.getDefaultsBoolean(grainstorm.getString(R.string.writemic), false, grainstorm);
            if (!writefile)
                return 0;

            SimpleDateFormat formatter = new SimpleDateFormat("yyyy_MM_dd_HH_mm_ss", Locale.US);
            Date now = new Date();
            firstname = secondname = "grainstorm-mic-" + formatter.format(now) + extension;

            String uristring = Activities.getDefaultsString(grainstorm.getString(R.string.recordingdirectory), null, grainstorm);
            if (uristring != null) {
                Uri uri = Uri.parse(uristring);
                DocumentFile documentFile = DocumentFile.fromTreeUri(grainstorm, uri);
                assert documentFile != null;
                outputfile = documentFile.createFile("audio/" + format, firstname);
                outputFolder = uri.getPath();
                return Objects.requireNonNull(grainstorm.getContentResolver().openFileDescriptor(outputfile.getUri(), "w")).detachFd();
            } else {
                MainActivity.setOutputDirectory(MainActivity.REQUEST_DIRECTORY_REC);
                return -1;
            }

        } catch (
                Exception e) {
            showToast(e.toString());
            return 0;
        }

    }

    static void closeFD(final boolean delete) {
        try {
            final boolean[] del = {false};
            final MainActivity grainstorm = MainActivity.getInstance();
            if (!delete && grainstorm != null) {
                final String format = Activities.recordingExtensions[Activities.getDefaultsInt(grainstorm.getString(R.string.recordingformat), 0, grainstorm)];
                final String extension = "." + format;
                if (Activities.getDefaultsBoolean(grainstorm.getString(R.string.askname), false, grainstorm)) {
                    final Object lock = new Object();

                    grainstorm.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            try {
                                LayoutInflater adbInflater = LayoutInflater.from(grainstorm);
                                final AlertDialog.Builder adb = new AlertDialog.Builder(grainstorm, R.style.MyAlertdialogtheme);
                                final View rootView = adbInflater.inflate(R.layout.askname, null);
                                final EditText mEdit = rootView.findViewById(R.id.askName);
                                final TextView mTopic = rootView.findViewById(R.id.paramText);
                                //myFormatter.setMaximumFractionDigits(digits);
                                DecimalFormatSymbols otherSymbols = new DecimalFormatSymbols(Locale.getDefault());
                                otherSymbols.setDecimalSeparator('.');
                                mTopic.setText("Save as");
                                adb.setView(rootView);
                                adb.setCancelable(false);
                                adb.setPositiveButton("Save", new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        dialog.dismiss();
                                    }
                                });
                                adb.setNegativeButton("Delete", new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        del[0] = true;
                                        dialog.dismiss();
                                    }
                                });
                                adb.setOnDismissListener(new DialogInterface.OnDismissListener() {
                                    @Override
                                    public void onDismiss(DialogInterface dialogInterface) {
                                        String text = mEdit.getText().toString();
                                        if (!text.isEmpty())
                                            secondname = text;
                                        synchronized (lock) {
                                            lock.notifyAll();
                                        }
                                    }
                                });
                                final Dialog dialog = adb.show();

                                Window w = dialog.getWindow();

                                if (w != null) {
                                    w.clearFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM);
                                    w.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                                }
                                //adb.setTitle("Import Preset:");
                                mEdit.setText(firstname);
                                mEdit.requestFocus();
                                mEdit.setSelection(0, firstname.length() - (format.equals("flac") ? 5 : 4));
                                mEdit.setOnKeyListener((v, keyCode, event) -> {

                                    if (event.getAction() == KeyEvent.ACTION_DOWN
                                            && event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
                                        dialog.dismiss();
                                        return true;
                                    }
                                    return false;
                                });
                            } catch (Exception e) {

                            }
                        }
                    });
                    synchronized (lock) {
                        lock.wait();
                    }

                }

            }

            if (delete || del[0]) {
                outputfile.delete();
                if (del[0])
                    showToast("File Deleted.");
            } else {
                if (!firstname.equals(secondname))
                    outputfile.renameTo(secondname);
                String output;
                if (outputFolder != null)
                    output = "Saved as: " + outputFolder + File.separator + secondname;
                else
                    output = "Saved as: " + secondname;
                showToast(output);
            }
        } catch (Exception e) {
            showToast(e.toString());
        }
    }
}

