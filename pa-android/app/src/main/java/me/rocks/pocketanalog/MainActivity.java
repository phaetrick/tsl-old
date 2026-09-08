package me.rocks.pocketanalog;

import android.Manifest;
import android.app.Activity;
import android.app.NotificationManager;
import android.app.PendingIntent;
import androidx.core.app.ActivityCompat;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.SharedPreferences;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.ref.WeakReference;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicReference;

import android.content.pm.PackageManager;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.app.NativeActivity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.net.Uri;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.SystemClock;
import android.util.Base64;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.TextView;
import android.widget.Toast;


import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatDialog;
import androidx.documentfile.provider.DocumentFile;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.BillingResult;

import static me.rocks.pocketanalog.Activities.getDefaultsString;
import static me.rocks.pocketanalog.Activities.setDefaultsString;
import static me.rocks.pocketanalog.MyApplication.java_record_live;
import static me.rocks.pocketanalog.MyApplication.java_record_loop;
//import static me.rocks.pocketanalog.MyApplication.java_sig;

public class MainActivity extends NativeActivity implements BillingManager.BillingUpdatesListener{

    final static int PERMISSION_ALL = 1;

    final static String url_eula = "file:///android_asset/eula.html";
    final static String url_oss = "https://thesecretlaboratory.com/apps/pocketanalog/oss";
    final static String url_instructions = "https://thesecretlaboratory.com/apps/pocketanalog/instructions";
    final static String url_policy = "https://thesecretlaboratory.com/apps/pocketanalog/policy";
    final static String url_changes = "https://pocketanalog.rocks.me/changes.html";

    static final int REQUEST_FILE_SAF = 42;
    static final int REQUEST_DIRECTORY_REC = 44;
    static final int REQUEST_DIRECTORY_PRESET = 45;
final static String TAG = "MainActivity";
    static boolean nativeCrash = false;

    static String homeDirectory = "";
    static String presetDirectory = "";
    static String midimappingDirectory = "";
    String[] PERMISSIONS = {Manifest.permission.MODIFY_AUDIO_SETTINGS};

    final static Object mutex = new Object();

    static void wait2(int timeout) {
        try {
            synchronized (mutex) {
                mutex.wait(timeout);
            }
        } catch (InterruptedException e) {
            ;
        }
    }

    static void rel() {
        synchronized (mutex) {
            mutex.notify();
        }
    }

    private static WeakReference<MainActivity> weakReference;

    static MainActivity getInstance() {
        if (weakReference != null) {
            MainActivity pocketanalog = weakReference.get();
            if (pocketanalog != null && !pocketanalog.isFinishing())
                return pocketanalog;
        }
        return null;
    }

    static Object getInstance2() {
        return getInstance();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        weakReference = new WeakReference<>(this);
        super.onCreate(savedInstanceState);
        if (!hasPermissions(PERMISSIONS)) {
            requestPermissions(PERMISSIONS, PERMISSION_ALL);
        }
        checkLicense2();
        // Before anything reads the bank: an interrupted move is finished here.
        resumePendingPresetMove();
    }

    public void showSoftInput(final int type)
    {
        InputMethodManager imm = ( InputMethodManager )getSystemService( Context.INPUT_METHOD_SERVICE );
        imm.showSoftInput( this.getWindow().getDecorView(), InputMethodManager.SHOW_FORCED );
    }

    public void hideKeyboard()
    {
        InputMethodManager imm = ( InputMethodManager )getSystemService( Context.INPUT_METHOD_SERVICE );
        imm.hideSoftInputFromWindow( this.getWindow().getDecorView().getWindowToken(), 0 );
    }

    @Override
    protected void onResume() {
        super.onResume();
        setScreenOnFlag(MyApplication.screenOn);
        // Cheap retry for the device that launched with no signal: coming back to
        // the app is the moment they are most likely to have some. Costs nothing
        // when a token is already held -- resolve() only reaches the network when
        // it has no usable one -- and the gate ignores overlapping calls.
        if (!isOpen()) {
            TrialGate.refresh(this);
            TrialGate.start(this);   // no-op until Play has confirmed not-owned
        }
    }

    void setScreenOnFlag(boolean set) {
        if (set) {
            getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } else {
            getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (nativeCrash) {
            finish();
            System.exit(0);
        }
    }

    protected boolean hasPermissions(String... permissions) {
        for (String permission : permissions) {
            if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        if (requestCode == PERMISSION_ALL) {
            for (int i = 0; i < permissions.length; i++) {
                if (permissions[i].equals(Manifest.permission.POST_NOTIFICATIONS)) {
                    // Start the service regardless of the user's choice (Grant or Deny)
                    // because "service should always start". 
                    // If granted, it will have a notification. If denied, it won't.
                    if (synthIsOn) {
                        MyApplication myApplication = MyApplication.getInstance();
                        if (myApplication != null) {
                            startSynthService(myApplication, new Intent(myApplication, SynthService.class));
                        }
                    }
                }
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    private void hideSystemUI() {
        // Enables regular immersive mode.
        // For "lean back" mode, remove SYSTEM_UI_FLAG_IMMERSIVE.
        // Or for "sticky immersive," replace it with SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        // Set the content to appear under the system bars so that the
                        // content doesn't resize when the system bars hide and show.
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        // Hide the nav bar and status bar
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN);
    }

    private void showSystemUI() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );
    }

    static int trackindex2 = 0;

    static void filebrowser(int index) {
        final MainActivity pocketanalog = getInstance();
        if (pocketanalog != null) {
            Intent intent;
            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            intent.setType("audio/*");
            intent.putExtra("Trackindex", index);
            //intent.setType("*/*");
            //String[] mimetypes = {"audio/*", "video/*"};
            //intent.putExtra(Intent.EXTRA_MIME_TYPES, mimetypes);
            trackindex2 = index;
            try {
                pocketanalog.startActivityForResult(intent, REQUEST_FILE_SAF);
            } catch (Exception e) {
                showToast("Error: " + e.toString());
            }
        }
    }


    @Override
    public void onActivityResult(int requestCode, int resultCode,
                                 Intent resultData) {
       if (requestCode == REQUEST_DIRECTORY_REC && resultCode == Activity.RESULT_OK) {
            ContentResolver contentResolver = getContentResolver();
            if (contentResolver == null) {
                Log.e("MainActivity", "Something went wrong");
                showToast("Something went wrong.");
                return;
            }

            Uri uriTree = resultData.getData();
            if (uriTree != null) {
                int takeFlags = resultData.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
                grantUriPermission(getPackageName(), uriTree, takeFlags);

                contentResolver.takePersistableUriPermission(uriTree, takeFlags);
                setDefaultsString(getString(R.string.recordingdirectory), uriTree.toString(), this);
                String outdir = Activities.describeTree(this, uriTree);
                if (outdir != null) {
                    String output = "Output Folder set to: " + outdir;
                    showToast(output);
                } else showToast("Output Folder changed.");
            }
        } else if (requestCode == REQUEST_DIRECTORY_PRESET && resultCode == Activity.RESULT_OK) {
            ContentResolver contentResolver = getContentResolver();
            if (contentResolver == null) {
                Log.e("MainActivity", "Something went wrong");
                showToast("Something went wrong.");
                return;
            }

            Uri uriTree = resultData.getData();
            if (uriTree != null) {
                int takeFlags = resultData.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
                grantUriPermission(getPackageName(), uriTree, takeFlags);
                // Persistable, so the grant survives restart and reboot. It does NOT
                // survive an uninstall -- the folder and its files do, and picking the
                // same folder again on the new install brings the library back.
                contentResolver.takePersistableUriPermission(uriTree, takeFlags);
                adoptPresetFolder(uriTree);   // moves first, then stores the pref
            }
        }
    }


    static int getFd(String uristring) {
        MainActivity pocketanalog = getInstance();
        if (pocketanalog == null)
            return 0;
        Uri uri = Uri.parse(uristring);
        if (uri != null) {
            Log.i("MainActivity", "Uri: " + uri.toString());
            try {
                ParcelFileDescriptor parcelFileDescriptor =
                        pocketanalog.getContentResolver().openFileDescriptor(uri, "r");
                if (parcelFileDescriptor == null) {
                    showToast("File error.");
                    return 0;
                }
                return parcelFileDescriptor.getFd();
            } catch (FileNotFoundException e) {
                Log.e("MainActivity", "Could not open uri as fd");
                showToast("File error.");
                return 0;
            }
        } else return 0;
    }


    static String getPathfromUri(String uristring) {
        MainActivity pocketanalog = getInstance();
        if (pocketanalog == null)
            return null;
        try {
            String path;
            ParcelFileDescriptor parcelFileDescriptor =
                    pocketanalog.getContentResolver().openFileDescriptor(Uri.parse(uristring), "r");
            if (parcelFileDescriptor == null) {
                showToast("File error.");
                return null;
            }
            if (MyApplication.copyFiles) {
                File outputDir = pocketanalog.getCacheDir(); // context being the Activity pointer
                File outputFile = File.createTempFile("pocketanalogtmpdecoding", "tmp", outputDir);
                InputStream fileStream = new FileInputStream(parcelFileDescriptor.getFileDescriptor());
                OutputStream newDatabase = new FileOutputStream(outputFile);

                byte[] buffer = new byte[1024];
                int length;

                while ((length = fileStream.read(buffer)) > 0) {
                    newDatabase.write(buffer, 0, length);
                }

                newDatabase.flush();
                fileStream.close();
                newDatabase.close();
                parcelFileDescriptor.close();
                parcelFileDescriptor = null;
                path = "file:" + outputFile.getAbsolutePath();
            } else {
                path = String.format(Locale.US, "pipe:%d", parcelFileDescriptor.detachFd());
            }
            return path;
        } catch (Exception e) {
            showToast("Error: " + e.toString());
            return null;
        }
    }

    static String getStorageDirRec() {
        String path = Environment.getExternalStorageState().equals(
                Environment.MEDIA_MOUNTED) ? Environment.getExternalStorageDirectory().getAbsolutePath() + "/MainActivity" : "";
        if (path.isEmpty()) {
            return null;
        }
        File folder = new File(path);
        if (!folder.exists()) {
            showToast("No home directory present which should have been created on first launch.");
            return null;
        }
        return path;
    }

    static final int RECORD_LIVE = 0;
    static final int RECORD_LOOP = 1;

    static void recFile(final int mode, final long track) {
        final boolean[] delete = {false};
        final String[] firstname = {null};
        final String[] secondname = {null};

        String outputFolder;
        ParcelFileDescriptor parcelFileDescriptor;
        DocumentFile outputfile;
        try {
            final MainActivity pocketanalog = MainActivity.getInstance();
            if (pocketanalog == null)
                return;
            final String format = MainActivity.isOpen() ? Activities.recordingExtensions[Activities.getDefaultsInt(pocketanalog.getString(R.string.recordingformat), 0, pocketanalog)] : "mp3";
            final String extension = "." + format;
            SimpleDateFormat formatter = new SimpleDateFormat("yyyy_MM_dd_HH_mm_ss", Locale.US);
            Date now = new Date();
            // Filename prefix only, so it follows the rename; existing recordings keep
            // the names they were written with. This is the Android path - the desktop
            // builds carry their own prefix in va/callbacks_loop_controls.cpp.
            if (mode == RECORD_LIVE)
                secondname[0] = firstname[0] = "voltaic-" + formatter.format(now) + extension;
            else
                secondname[0] = firstname[0] = "voltaic-loop-" + formatter.format(now) + extension;

            String uristring = getDefaultsString(pocketanalog.getString(R.string.recordingdirectory), null, pocketanalog);
            if (uristring != null) {
                try {
                    Uri uri = Uri.parse(uristring);
                    DocumentFile documentFile = DocumentFile.fromTreeUri(pocketanalog, uri);
                    outputfile = documentFile.createFile("audio/" + format, (String) firstname[0]);
                    outputFolder = Activities.describeTree(pocketanalog, uri);
                    parcelFileDescriptor = pocketanalog.getContentResolver().openFileDescriptor(((DocumentFile) outputfile).getUri(), "w");
                } catch (Exception e) {
                    showToast(e.toString());

                    return;
                }
            } else {
                pocketanalog.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        new AlertDialog.Builder(pocketanalog, R.style.MyAlertdialogtheme)
                                .setTitle("No recording directory set.")
                                .setPositiveButton("Set now", new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        dialog.dismiss();
                                        try {
                                            Intent intent;
                                            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);

                                            //intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                                            intent.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);

                                            //intent.putExtra(Intent.EXTRA_LOCAL_ONLY, true);
                                            intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                                            // ACTION_OPEN_DOCUMENT is the intent to choose a file via the system's file
                                            // browser.
                                            //Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);

                                            // Filter to only show results that can be "opened", such as a
                                            // file (as opposed to a list of contacts or timezones)

                                            // Filter to show only images, using the image MIME data type.
                                            // If one wanted to search for ogg vorbis files, the type would be "audio/ogg".
                                            // To search for all documents available via installed storage providers,
                                            // it would be "*/*".

                                            pocketanalog.startActivityForResult(intent, REQUEST_DIRECTORY_REC);
                                        } catch (Exception e) {
                                            MainActivity.showToast(e.toString());
                                        }
                                    }
                                })
                                .show();
                    }
                });
                return;
            }

            int result;
            if (mode == RECORD_LIVE)
                result = java_record_live(((ParcelFileDescriptor) parcelFileDescriptor).detachFd());
            else
                result = java_record_loop(((ParcelFileDescriptor) parcelFileDescriptor).detachFd(), track);

            if (parcelFileDescriptor != null) {
                ((ParcelFileDescriptor) parcelFileDescriptor).close();
            }

            if (result == 0) {
                if (!pocketanalog.isFinishing() && Activities.getDefaultsBoolean(pocketanalog.getString(R.string.askname), false, pocketanalog)) {
                    final Object lock = new Object();

                    pocketanalog.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {


                            LayoutInflater adbInflater = LayoutInflater.from(pocketanalog);
                            final AlertDialog.Builder adb = new AlertDialog.Builder(pocketanalog, R.style.MyAlertdialogtheme);
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
                                    delete[0] = true;
                                    dialog.dismiss();
                                }
                            });
                            adb.setOnDismissListener(new DialogInterface.OnDismissListener() {
                                @Override
                                public void onDismiss(DialogInterface dialogInterface) {
                                    String text = mEdit.getText().toString();
                                    if (!text.isEmpty()) {
                                        secondname[0] = text;
                                    }
                                    synchronized (lock) {
                                        lock.notifyAll();
                                    }
                                }
                            });
                            final AppCompatDialog dialog = adb.show();

                            Window w = dialog.getWindow();

                            if (w != null) {
                                w.clearFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM);
                                w.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                            }

                            //adb.setTitle("Import Preset:");
                            mEdit.setText((String) firstname[0]);
                            mEdit.requestFocus();
                            mEdit.setSelection(0, ((String) firstname[0]).length() - (format.equals("flac") ? 5 : 4));
                            mEdit.setOnKeyListener(new View.OnKeyListener() {

                                public boolean onKey(View v, int keyCode, KeyEvent event) {

                                    if (event.getAction() == KeyEvent.ACTION_DOWN
                                            && event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
                                        mEdit.clearFocus();
                                        InputMethodManager imm = (InputMethodManager) pocketanalog.getSystemService(Context.INPUT_METHOD_SERVICE);
                                        if (imm != null)
                                            imm.hideSoftInputFromWindow(rootView.getWindowToken(), 0);
                                        dialog.dismiss();
                                        return true;
                                    }
                                    return false;
                                }

                            });
                        }
                    });

                    synchronized (lock) {
                        lock.wait();
                    }
                }
                String output;
                if (delete[0] && outputfile != null) {
                    boolean success = outputfile.delete();
                    showToast("File Deleted.");
                } else {
                    if (!firstname[0].equals(secondname[0]))
                        outputfile.renameTo(secondname[0]);
                    if (outputFolder != null)
                        output = "Saved as: " + outputFolder + File.separator + secondname[0];
                    else
                        output = "Saved as: " + secondname[0];
                    showToast(output);
                }
            } else {
                if (outputfile != null) {
                    outputfile.delete();
                }
            }

        } catch (Exception e) {
            showToast(e.toString());
        }
    }


    static String getStorageDirPreset() {
        if (!Activities.isExternalStorageWritable())
            return null;
        if (presetDirectory == null || presetDirectory.isEmpty()) {
            MainActivity pocketanalog = getInstance();
            if (pocketanalog == null || Activities.prepareHome(pocketanalog))
                return null;
        }
        return presetDirectory;
    }

    static String getStorageDirMidiMapping() {
        if (!Activities.isExternalStorageWritable())
            return null;
        if (midimappingDirectory == null || midimappingDirectory.isEmpty()) {
            MainActivity pocketanalog = getInstance();
            if (pocketanalog == null || Activities.prepareHome(pocketanalog))
                return null;
        }
        return midimappingDirectory;
    }

    static String getHomeDirectory() {
        if (!Activities.isExternalStorageWritable())
            return null;
        if (homeDirectory == null || homeDirectory.isEmpty()) {
            MainActivity pocketanalog = getInstance();
            if (pocketanalog == null || Activities.prepareHome(pocketanalog))
                return null;
        }
        return homeDirectory;
    }

    static int getff(){
        final MainActivity pocketanalog = MainActivity.getInstance();
        if (pocketanalog == null)
            return 0 ;

        String uristring = getDefaultsString(pocketanalog.getString(R.string.recordingdirectory), null, pocketanalog);
        if (uristring != null) {
            try {
                Uri uri = Uri.parse(uristring);
                DocumentFile documentFile = DocumentFile.fromTreeUri(pocketanalog, uri);
                DocumentFile outputfile = documentFile.createFile("image/png", "banner.png");
                ParcelFileDescriptor parcelFileDescriptor = pocketanalog.getContentResolver().openFileDescriptor(((DocumentFile) outputfile).getUri(), "w");
                return parcelFileDescriptor.detachFd();
            } catch (Exception e) {
                showToast(e.toString());

                return 0;
            }
        }
        return 0;
    }

    static DocumentFile getHome(String subDir) {
        MainActivity context = getInstance();
        if (context == null) return null;
        DocumentFile home = null;
        String uristring = getDefaultsString(context.getString(R.string.presetdirectory), null, context);
        if (uristring != null) {
            Uri uri = Uri.parse(uristring);
            home = DocumentFile.fromTreeUri(context, uri);

        } else {
            File externalfilesdir = Activities.isExternalStorageWritable() ? context.getExternalFilesDir(null) : null;
            if (externalfilesdir != null) {
                String homeStr = externalfilesdir.getAbsolutePath();
                home = DocumentFile.fromFile(new File(homeStr));
            }
        }
        if (home != null) {
            if (subDir != null) {
                DocumentFile sub = home.findFile(subDir);
                if (sub == null)
                    sub = home.createDirectory(subDir);
                return sub;
            }
        } else {
            return home;
        }
        return null;
    }
    static String getStoragePath(String subDir) {
        MainActivity grainstorm = getInstance();
        return grainstorm != null ? Activities.getHomePath(grainstorm, subDir) : null;
    }
    static DocumentFile[] getDirContent2(String subDir) {
        DocumentFile home = getHome(subDir);
        if (home != null)
            return home.listFiles();
        else return null;
    }
    static  Object[] getDirContent(String subDir) {
        try {
            MainActivity context = getInstance();
            DocumentFile[] home = getDirContent2(subDir);
            if(context != null && home != null && home.length>0){
                String[] strings = new String[home.length];
                int index = 0;
                for (DocumentFile f : home) {
                    strings[index++] = f.getUri().toString();
                }
                return strings;
            }
            return null;
        }
        catch (Exception e){
            showToast(e.toString());
            return null;
        }
    }
    static int getFdFromUriString(String uriString, String mode) {
        try {
            MainActivity context = getInstance();
            // ContentResolver takes r|w|rw|wa|rwt only; native passes stdio modes
            // like "rb"/"wb", which threw IllegalArgumentException before this.
            String m = mode == null ? "r" : mode;
            if (m.contains("r") && m.contains("w")) m = "rw";
            else if (m.contains("w") || m.contains("a")) m = "w";
            else m = "r";
            ParcelFileDescriptor pfd = context.getContentResolver().openFileDescriptor(Uri.parse(uriString), m);
            if (pfd == null) return 0;
            return pfd.detachFd();
        }
        catch (Exception e){
            showToast(e.toString());
            return 0;
        }
    }

    static boolean deleteFromUri(String uriString) {
        try {
            MainActivity context = getInstance();
            DocumentFile f = DocumentFile.fromSingleUri(context, Uri.parse(uriString));
            return f != null && f.delete();
        }
        catch (Exception e){
            showToast(e.toString());
            return false;
        }
    }

    static void logString(final String string) {
        int maxLogSize = 1000;
        for (int i = 0; i <= string.length() / maxLogSize; i++) {
            int start = i * maxLogSize;
            int end = (i + 1) * maxLogSize;
            end = end > string.length() ? string.length() : end;
            Log.e(TAG, string.substring(start, end));
        }
    }

    static void showToast(final String string) {
        final MainActivity pocketanalog = getInstance();
        if (pocketanalog != null && !pocketanalog.isFinishing()) {
            pocketanalog.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    Toast.makeText(pocketanalog, string, Toast.LENGTH_LONG).show();

                }
            });
        }
    }

    static void reallyQuit() {
        final MainActivity pocketanalog = getInstance();
        if (pocketanalog != null && !pocketanalog.isFinishing()) {
            final SharedPreferences mSharedPreferences = pocketanalog.getPreferences(Activity.MODE_PRIVATE);

            int tries = mSharedPreferences.getInt(pocketanalog.getString(R.string.usageamount), 0);
            if (tries == 20) {
                pocketanalog.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        new AlertDialog.Builder(pocketanalog, R.style.MyAlertdialogtheme)
                                .setTitle("Feedback needed.").setMessage("If not already done so, please rate MainActivity. It takes only a minute and positive feedback encourages further development.").setPositiveButton("Rate now.", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                dialog.dismiss();
                                SharedPreferences.Editor editor = mSharedPreferences.edit();
                                editor.putInt(pocketanalog.getString(R.string.usageamount), 100);
                                editor.apply();
                                Uri uri = Uri.parse("market://details?id=" + pocketanalog.getPackageName());
                                Intent goToMarket = new Intent(Intent.ACTION_VIEW, uri);
                                // To count with Play market backstack, After pressing back button,
                                // to taken back to our application, we need to add following flags to intent.
                                goToMarket.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY | Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
                                try {
                                    pocketanalog.startActivity(goToMarket);
                                } catch (ActivityNotFoundException e) {
                                    pocketanalog.startActivity(new Intent(Intent.ACTION_VIEW,
                                            Uri.parse("http://play.google.com/store/apps/details?id=" + pocketanalog.getPackageName())));
                                }
                            }
                        }).setNegativeButton("Never ask again.", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                dialog.dismiss();
                                SharedPreferences.Editor editor = mSharedPreferences.edit();
                                editor.putInt(pocketanalog.getString(R.string.usageamount), 100);
                                editor.apply();
                                pocketanalog.finish();
                            }
                        }).setNeutralButton("Remind me later.", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                dialog.dismiss();
                                SharedPreferences.Editor editor = mSharedPreferences.edit();
                                editor.putInt(pocketanalog.getString(R.string.usageamount), 0);
                                editor.apply();
                                pocketanalog.finish();
                            }
                        }).show();
                    }
                });
            }
        }
    }

    static boolean waiting = true;

    static void outOfMemory() {
        final MainActivity pocketanalog = getInstance();
        if (pocketanalog != null && !pocketanalog.isFinishing()) {
            waiting = true;
            pocketanalog.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    new AlertDialog.Builder(pocketanalog, R.style.MyAlertdialogtheme)
                            .setTitle("Out Of Memory").setMessage("Please close other applications to free resources then try again or quit.").setNegativeButton("Quit", new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            dialog.dismiss();
                            pocketanalog.finish();
                            System.exit(0);
                        }
                    }).setPositiveButton("Try Again", new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialogInterface, int i) {
                            waiting = false;
                            dialogInterface.dismiss();
                        }
                    }).setCancelable(false)
                            .show();
                }
            });
            while (waiting) {
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    System.exit(0);
                }
            }
        } else {
            System.exit(0);
        }
    }

    static void initFailed(final String errorStr) {
        nativeCrash = true;
        final String message = "Something went wrong during setup. (" + errorStr + ") Most likely because the system is low on memory. Please close other applications to free resources.";
        final MainActivity pocketanalog = getInstance();
        if (pocketanalog != null && !pocketanalog.isFinishing()) {
            pocketanalog.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    new AlertDialog.Builder(pocketanalog, R.style.MyAlertdialogtheme)
                            .setTitle("Initialization Error.").setMessage(message).setNegativeButton("Quit", new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            dialog.dismiss();
                            pocketanalog.finish();
                            System.exit(0);
                        }
                    }).setCancelable(false)
                            .show();
                }
            });
        } else {
            System.exit(0);
        }
    }

    static boolean crashHandler(final String error) {
        nativeCrash = true;
        final MainActivity pocketanalog = getInstance();
        if (pocketanalog != null && !pocketanalog.isFinishing()) {
            pocketanalog.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    new AlertDialog.Builder(pocketanalog, R.style.MyAlertdialogtheme)
                            .setTitle("That's a crash (" + error + "). Sorry.").setNegativeButton("Quit", new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            dialog.dismiss();
                        }
                    })/*.setPositiveButton("Quit and send report", new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialogInterface, int u) {
                            Intent intent = new Intent(Intent.ACTION_SENDTO);
                            intent.setData(Uri.parse("mailto:"));
                            intent.putExtra(Intent.EXTRA_EMAIL, new String[]{"pocketanalog@rocks.me"});
                            intent.putExtra(Intent.EXTRA_SUBJECT, "Crash report (" + error + ")");
                            intent.putExtra(Intent.EXTRA_TEXT, "Please provide a short information on what happened.");
                            try {
                                pocketanalog.startActivity(Intent.createChooser(intent, "Send crash report..."));
                            } catch (android.content.ActivityNotFoundException ex) {
                                Toast.makeText(pocketanalog, "There are no email clients installed.", Toast.LENGTH_SHORT).show();
                            }
                        }
                    })*/.setOnDismissListener(new DialogInterface.OnDismissListener() {
                        @Override
                        public void onDismiss(DialogInterface dialogInterface) {
                            pocketanalog.finish();
                            System.exit(0);
                        }
                    }).setOnCancelListener(new DialogInterface.OnCancelListener() {
                        @Override
                        public void onCancel(DialogInterface dialogInterface) {
                            pocketanalog.finish();
                            System.exit(0);
                        }
                    })
                            .setCancelable(false)
                            .show();
                }
            });
            return true;
        } else {
            return false;
        }
    }

    boolean hasMic() {
        return getPackageManager().hasSystemFeature("android.hardware.microphone");
    }


    int getOutputFormat() {
        return Activities.getDefaultsInt(getString(R.string.recordingformat), 0, this);
    }

    int getMicRecFormat() {
        return Activities.getDefaultsInt(getString(R.string.recordingformat), 0, this);
    }

    static void invokeSettings() {
        MainActivity pocketanalog = getInstance();
        if (pocketanalog != null) {
            Intent intent = new Intent(pocketanalog, MainSettings2.class);
            pocketanalog.startActivity(intent);
        }
    }

    /** Called from native (session wall's UNLOCK button): opens the purchase screen. */
    static void invokeUpgrade() {
        MainActivity pocketanalog = getInstance();
        if (pocketanalog != null) {
            Intent intent = new Intent(pocketanalog, InAppPurchases.class);
            pocketanalog.startActivity(intent);
        }
    }

    /** Called from native once per launch on free installs. Reports whether the
     *  session-cap model notice was ever shown, and marks it shown on the first
     *  ask — so native shows that dialog exactly once per install. */
    static boolean capNoticeSeen() {
        MainActivity pocketanalog = getInstance();
        if (pocketanalog == null) return true;
        boolean seen = Activities.getDefaultsBoolean("cap_notice_seen", false, pocketanalog);
        if (!seen) Activities.setDefaultsBoolean("cap_notice_seen", true, pocketanalog);
        return seen;
    }


    // ── Preset folder ────────────────────────────────────────────────────────
    // The chosen SAF tree is stored under presetdirectory and read by getHome().
    // With nothing stored, getHome() falls back to getExternalFilesDir and the
    // native side keeps using its own plain POSIX path -- so a user who never
    // opens this setting runs exactly the code that shipped.
    //
    // Presets MOVE when the folder changes rather than being copied. They are
    // small here (parameter dumps, no embedded audio unlike grainstorm's), and
    // two live locations would only raise "which copy is the real one".

    static String presetFolderUri() {
        MainActivity a = getInstance();
        if (a == null) return null;
        return getDefaultsString(a.getString(R.string.presetdirectory), null, a);
    }

    /** Native asks this to choose between the SAF path and its own POSIX path. */
    static boolean presetFolderIsCustom() {
        return presetFolderUri() != null;
    }

    /** Short, factual "where are my presets" line. No advice, no warnings --
     *  the settings row adds those, the storage dialog wants just the answer. */
    static String presetFolderLabel() {
        MainActivity a = getInstance();
        String s = presetFolderUri();
        if (a == null) return "";
        if (s == null) return "The app's default folder";
        String label = Activities.describeTree(a, Uri.parse(s));
        return label != null ? label + "/presets" : "Your chosen folder";
    }

    /**
     * Creates one file in the chosen preset folder and hands native an fd:
     * "pipe:/<fd>|<uri>". Native fdopen()s the fd and keeps the uri as the
     * preset's path, which is what deleteFromUri() later needs. Null when no
     * folder is set -- native then writes through its own POSIX path.
     */
    static String createStorageFile(String subDir, String name) {
        try {
            MainActivity a = getInstance();
            if (a == null || presetFolderUri() == null) return null;
            DocumentFile home = getHome(subDir);
            if (home == null || !home.canWrite()) return null;
            DocumentFile f = home.createFile("application/octet-stream", name);
            if (f == null) return null;
            ParcelFileDescriptor pfd = a.getContentResolver().openFileDescriptor(f.getUri(), "rw");
            if (pfd == null) return null;
            return "pipe:/" + pfd.detachFd() + "|" + f.getUri();
        } catch (Exception e) {
            showToast(e.toString());
            return null;
        }
    }

    /**
     * Opens a NAMED file in the chosen folder, for the small sidecar files that
     * are rewritten in place rather than accumulated (presetorder.conf). mode
     * "w" truncates, creating the file if needed; "r" returns null when it does
     * not exist. Same "pipe:/<fd>|<uri>" shape as createStorageFile.
     */
    static String openStorageFile(String subDir, String name, String mode) {
        try {
            MainActivity a = getInstance();
            if (a == null || presetFolderUri() == null) return null;
            DocumentFile home = getHome(subDir);
            if (home == null) return null;
            boolean write = mode != null && mode.contains("w");
            // findFile(name) is not enough: createDocument appends the MIME
            // type's extension whenever the requested name's own extension does
            // not map to it, so "presetorder.conf" is stored as
            // "presetorder.conf.bin". Without matching that back, every write
            // would create yet another copy and no read would ever find one.
            DocumentFile f = null;
            DocumentFile[] all = home.listFiles();
            if (all != null)
                for (DocumentFile c : all) {
                    String n = c.getName();
                    if (c.isFile() && n != null && (n.equals(name) || n.startsWith(name + "."))) {
                        f = c;
                        break;
                    }
                }
            if (f == null) {
                if (!write) return null;
                f = home.createFile("application/octet-stream", name);
                if (f == null) return null;
            }
            ParcelFileDescriptor pfd = a.getContentResolver()
                    .openFileDescriptor(f.getUri(), write ? "rwt" : "r");
            if (pfd == null) return null;
            return "pipe:/" + pfd.detachFd() + "|" + f.getUri();
        } catch (Exception e) {
            return null;
        }
    }

    /** Whether the chosen preset folder can actually be reached right now.
     *  False after the grant is revoked, the card is ejected, or the folder is
     *  deleted -- native uses it to say so instead of showing an empty list. */
    static boolean presetFolderReachable() {
        if (presetFolderUri() == null) return true;   // app storage
        DocumentFile home = getHome("presets");
        return home != null && home.canRead();
    }

    /** Name without its extension. A DocumentsProvider is free to append one
     *  (application/octet-stream commonly becomes ".bin"), so a move that is
     *  re-run must match on the stem or it copies everything a second time. */
    private static String stemOf(String name) {
        int dot = name.lastIndexOf('.');
        return dot > 0 ? name.substring(0, dot) : name;
    }

    /**
     * Moves every preset from wherever they are now into `dest`.
     *
     * Copy, verify, then delete: a source file is removed only once its
     * replacement exists at the same length, so an interrupted move leaves a
     * consistent split that simply re-running completes. Nothing is deleted on
     * any failure path -- the worst outcome is a duplicate, never a loss.
     */
    private static int movePresetsInto(MainActivity a, DocumentFile dest) {
        DocumentFile src = getHome("presets");
        if (src == null || dest == null) return 0;
        // Source and destination being the same folder is NOT a no-op by
        // accident: every file would find itself in `there`, match its own
        // length, and be deleted as a redundant leftover -- wiping the bank.
        // Re-picking the folder you are already using is an ordinary thing for
        // a user to do (and is exactly how you recover after a reinstall), so
        // this guard is load-bearing, not defensive.
        if (src.getUri().equals(dest.getUri())) return 0;
        DocumentFile[] files = src.listFiles();
        if (files == null || files.length == 0) return 0;

        java.util.HashMap<String, DocumentFile> there = new java.util.HashMap<>();
        DocumentFile[] existing = dest.listFiles();
        if (existing != null)
            for (DocumentFile f : existing)
                if (f.isFile() && f.getName() != null) there.put(stemOf(f.getName()), f);

        int moved = 0;
        for (DocumentFile in : files) {
            if (!in.isFile() || in.getName() == null) continue;
            final String name = in.getName();
            if (in.getUri().equals(dest.getUri())) continue;

            DocumentFile already = there.get(stemOf(name));
            if (already != null && already.getUri().equals(in.getUri())) continue;
            if (already != null) {
                // Left behind by an interrupted run. Same length means the copy
                // completed, so the source is now redundant; a different length
                // is not ours to resolve, so both stay.
                if (already.length() == in.length() && in.delete()) moved++;
                continue;
            }

            // Create under the STEM, not the name as found. Both DocumentsProvider
            // and RawDocumentFile append the MIME type's extension, so a file that
            // has already made one trip is called "1234567.bin"; passing that back
            // in would yield "1234567.bin.bin" and the stem match would then miss
            // on the trip after that. Using the stem makes round trips idempotent.
            DocumentFile out = dest.createFile("application/octet-stream", stemOf(name));
            if (out == null) continue;
            boolean ok = false;
            try (InputStream is = a.getContentResolver().openInputStream(in.getUri());
                 OutputStream os = a.getContentResolver().openOutputStream(out.getUri())) {
                if (is != null && os != null) {
                    byte[] buf = new byte[1 << 15];
                    int n;
                    while ((n = is.read(buf)) > 0) os.write(buf, 0, n);
                    os.flush();
                    ok = true;
                }
            } catch (Exception e) {
                Log.e(TAG, "movePresets: " + name + ": " + e);
            }
            if (ok && out.length() == in.length()) {
                if (in.delete()) moved++;
            } else {
                out.delete();   // incomplete copy: drop it, keep the original
            }
        }
        return moved;
    }

    // ── The move is journalled ───────────────────────────────────────────────
    // A move that is interrupted -- process killed, battery flat, the user swipes
    // the app away mid-copy -- must never leave the library half in one place and
    // half in the other with nothing recording the fact. So the DESTINATION is
    // written to presetMovePending BEFORE any file is touched, and the setting
    // that actually decides where presets are read from (presetdirectory) is only
    // written once every file has arrived.
    //
    // That gives three states and no others:
    //   pending unset                  -- presetdirectory is the truth, nothing in flight
    //   pending set, files not all over -- resumePendingMove() finishes the job
    //   pending set, all files over     -- resume runs, moves nothing, commits
    // The move itself is idempotent (copy, verify by length, then delete the
    // source), so resuming is always safe and can be repeated any number of times.
    //
    // PENDING_APP means "back to the app's own folder"; anything else is a tree URI.
    private static final String PENDING_APP = "app";
    private static final String PENDING_KEY = "presetMovePending";
    // Two moves at once would have them fighting over the same source files --
    // reachable by double-tapping the picker, or by a resume racing a fresh
    // pick. Whoever holds this finishes; the other returns and the journal
    // guarantees the work is not lost.
    private static final java.util.concurrent.atomic.AtomicBoolean moveRunning =
            new java.util.concurrent.atomic.AtomicBoolean(false);

    /** The presets folder inside the app's own storage, created if needed. */
    private static DocumentFile appPresetsDir(MainActivity a) {
        File ext = Activities.isExternalStorageWritable() ? a.getExternalFilesDir(null) : null;
        if (ext == null) return null;
        File presets = new File(ext, "presets");
        if (!presets.exists() && !presets.mkdirs()) return null;
        return DocumentFile.fromFile(presets);
    }

    /**
     * Performs (or completes) the move recorded in presetMovePending.
     * `announce` is false when resuming at startup: the user did not just ask for
     * this and does not need a toast about work they thought was already done.
     */
    private static void runPendingMove(final MainActivity a, boolean announce) {
        if (!moveRunning.compareAndSet(false, true)) return;
        try {
            runPendingMoveLocked(a, announce);
        } finally {
            moveRunning.set(false);
        }
    }

    private static void runPendingMoveLocked(final MainActivity a, boolean announce) {
        final String target = Activities.getDefaultsString(PENDING_KEY, null, a);
        if (target == null) return;

        DocumentFile dest;
        String where;
        if (PENDING_APP.equals(target)) {
            dest = appPresetsDir(a);
            where = "the app's default folder";
            if (dest == null) {
                if (announce) showToast("The app's default folder isn't available right now.");
                return;
            }
        } else {
            Uri tree = Uri.parse(target);
            DocumentFile root = DocumentFile.fromTreeUri(a, tree);
            if (root == null || !root.canWrite()) {
                // Reachable again later, most likely -- an ejected card, a folder
                // renamed, a grant not yet restored. Keep the journal entry so the
                // move can still finish; presetdirectory is untouched, so the
                // presets that have not moved yet are all still in use.
                if (announce)
                    showToast("Voltaic can't save into that folder. Please pick another one.");
                return;
            }
            dest = root.findFile("presets");
            if (dest == null) dest = root.createDirectory("presets");
            if (dest == null) {
                if (announce)
                    showToast("Voltaic can't create a presets folder there. Please pick another one.");
                return;
            }
            String label = Activities.describeTree(a, tree);
            where = label != null ? label : "your folder";
        }

        final int moved = movePresetsInto(a, dest);

        // Commit: the destination becomes the truth, and only then is the journal
        // cleared. Killed between these two, the next launch resumes and finds
        // nothing left to move -- which is exactly what it should find.
        Activities.setDefaultsStringSync(a.getString(R.string.presetdirectory),
                                         PENDING_APP.equals(target) ? null : target, a);
        Activities.setDefaultsStringSync(PENDING_KEY, null, a);
        MyApplication.java_reload_presets();

        if (!announce) {
            if (moved > 0)
                Log.i(TAG, "resumed preset move: " + moved + " file(s) -> " + where);
            return;
        }
        showToast(moved > 0
                ? (moved == 1 ? "1 preset moved to " : moved + " presets moved to ") + where
                : "Presets will now be saved in " + where);
    }

    /**
     * Opens the journal and runs the move. The journal write is synchronous and
     * happens before any file is touched -- but on the worker, not the caller:
     * both entry points are the UI thread and commit() is real disk I/O.
     */
    private static void startMove(final MainActivity a, final String target,
                                  final boolean announce, String threadName) {
        new Thread(() -> {
            Activities.setDefaultsStringSync(PENDING_KEY, target, a);
            runPendingMove(a, announce);
        }, threadName).start();
    }

    /** Called after the picker returns. */
    static void adoptPresetFolder(final Uri tree) {
        final MainActivity a = getInstance();
        if (a == null) return;
        startMove(a, tree.toString(), true, "preset-move");
    }

    /** "Use the App's Default Folder": moves everything back, then forgets the tree. */
    static void releasePresetFolder() {
        final MainActivity a = getInstance();
        if (a == null || presetFolderUri() == null) return;
        startMove(a, PENDING_APP, false, "preset-move-back");
    }

    /**
     * Finishes a move that a previous run started and did not complete. Called
     * once from onCreate. Does nothing in the normal case, which is why it can
     * run unconditionally.
     */
    static void resumePendingPresetMove() {
        final MainActivity a = getInstance();
        if (a == null) return;
        if (Activities.getDefaultsString(PENDING_KEY, null, a) == null) return;
        Log.i(TAG, "preset move was interrupted - finishing it");
        new Thread(() -> runPendingMove(a, false), "preset-move-resume").start();
    }


    static boolean synthIsOn = false;

    /** Whether powerControl actually started SynthService, so the stop path does
     *  not create one just to shut it down. */
    private static boolean serviceStarted = false;

    /** Stop only — never starts. Used from the notification's Stop action. */
    static void powerOff() {
        if (synthIsOn) MyApplication.java_control(0);
    }

    static void powerControl(boolean power) {
        final MyApplication myApplication = MyApplication.getInstance();
        if (myApplication == null) return;

        synthIsOn = power;
        final Intent intent = new Intent(myApplication, SynthService.class);

        if (!power) {
            // Only stop a service we actually started. Sending STOP otherwise
            // creates the service just to tear it down, and startService from
            // the background is itself restricted on Android 8+ — so the tidying
            // up could throw where doing nothing cannot.
            if (serviceStarted) {
                serviceStarted = false;
                intent.setAction("STOP");
                myApplication.startService(intent);
            }
            return;
        }

        // The service is not optional any more (targetSdk 37): Android 17's
        // background-audio hardening silences playback that has no foreground
        // service, so every power-on is protected from the start. This costs
        // nothing visible on a fresh install — with POST_NOTIFICATIONS
        // ungranted the service runs with its notification hidden — and
        // SynthService.onTaskRemoved makes a swipe-away STOP the synth in that
        // state, so nothing drones on without a handle. No permission is ever
        // requested here: power-on happens at first launch and must not open
        // with a prompt. The "Show notification when backgrounded" setting
        // asks, over in MainSettingFragment.
        //
        // (The service used to be behind a "Run as Foreground Service" option;
        // with the option off, backgrounded audio survived only until the OS
        // reaped the process — and on Android 17 would have stopped outright.)
        startSynthService(myApplication, intent);
    }

    /**
     * Re-post the foreground notification after POST_NOTIFICATIONS is granted.
     *
     * <p>A notification posted while the permission was missing never appears
     * retroactively, so the settings screen calls this once the grant lands.
     * Starting an already-running foreground service from the foreground is
     * legal and just re-runs startForeground with the same notification —
     * which now shows.
     *
     * <p>Does nothing while the synth is off: powerControl posts it at the
     * next power-on anyway.
     */
    static void refreshServiceNotification() {
        final MyApplication myApplication = MyApplication.getInstance();
        if (myApplication == null || !synthIsOn || !serviceStarted) return;
        startSynthService(myApplication, new Intent(myApplication, SynthService.class));
    }

    private static void startSynthService(MyApplication myApplication, Intent intent) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                myApplication.startForegroundService(intent);
            } else {
                myApplication.startService(intent);
            }
            serviceStarted = true;
        } catch (IllegalStateException | SecurityException e) {
            // Android 12+ refuses a foreground-service start from the background
            // and throws here rather than inside the service. The synth is
            // running either way; this only costs the protection from being
            // killed, so there is nothing to report and nothing to retry.
            Log.w(TAG, "Could not start synth service: " + e);
        }
    }

    static boolean firstrundone = false;

    public static void onNativeReady() {}

    static final String PRODUCT_ID = "upgrade1";

    static private final String[] strings = {
            "ANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAs2Dy41ouuL6GC+ZtaCMSjixpe/E97tEA3Ndghe75D7XQTU5xIO9akq2ecVdBP8NFI27yf28Og3QQT2hNTelH/NYw8cWYAJFdYw8jkRR1vpB4dgixqjCpVx6bA5WIMdWgrfsxp/zRlzI9WG3stNNXlCPv4aiZZ0aAbLnPxnPavJWvy+feS8PboYxS2HFamu6dKWtcClqlUEi8zGjYcULLmyoI2vYgls/IVibV2I4TfqbFeYfF44J6s+yARGz+QsRbP62BgSYMOr2BdlCws0M7Nwzn63oyIsmpCiBeUReEXISu0wmcSgg0pmVhRicwBQ9bmPoiQ/KLF2JbsaORQjqZHwIDAQAB",
            "MMjhgIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAs2Dy41ouuL6GC+ZtaCMSjixpe/E97tEA3Ndghe75D7XQTU5xIO9akq2ecVdBP8NFI27yf28Og3QQT2hNTelH/NYw8cWYAJFdYw8jkRR1vpB4dgixqjCpVx6bA5WIMdWgrfsxp/zRlzI9WG3stNNXlCPv4aiZZ0aAbLnPxnPavJWvy+feS8PboYxS2HFamu6dKWtcClqlUEi8zGjYcULLmyoI2vYgls/IVibV2I4TfqbFeYfF44J6s+yARGz+QsRbP62BgSYMOr2BdlCws0M7Nwzn63oyIsmpCiBeUReEXISu0wmcSgg0pmVhRicwBQ9bmPoiQ/KLF2JbsaORQjqZHwIDAQAB"};
    final private static AtomicReference<String> val = new AtomicReference<>(strings[0]);
    final private static AtomicReference<String> lca = new AtomicReference<>(strings[1]);
    void checkLicense2() {
        if (firstrundone)
            return;

        // A purchase Play confirmed on an earlier launch stands until Play says
        // otherwise. The entitlement used to live only in this process, so a paid
        // user whose Play connection was down started as unpurchased -- and under
        // the session cap that stops their instrument after ten minutes, mid-play.
        final boolean cached = cachedEntitlement(this);
        if (cached)
            iabset(true);
        // Deliberately NOT reporting the trial state here. A stored verdict can be
        // "expired", and acting on that before Play has answered walls a device
        // that trialled, expired, and then BOUGHT -- the wall is not dismissable,
        // so the purchase looks like it killed the app. The state is pushed once
        // the purchase question is settled, in onPurchasesUpdated/onBillingError.

        // Ask Play regardless of the cache. The cache only bridges the gap until
        // the real answer lands, or stands in for one that never does; Play stays
        // the authority, so a refund still takes the grant back (onPurchasesUpdated
        // clears the cache on an authoritative "no").
        if (billingManager != null)
            billingManager.destroy();   // holds a strong ref to the previous Activity
        billingManager = new BillingManager(this, this);
        billingManager.queryPurchases();

        // With the grant already in hand there is nothing to wait for. Native
        // blocks on readyForUiSetup for ten seconds, so without this the UI would
        // sit there until the query answered -- or the whole ten, if it never did.
        if (cached)
            MyApplication.guiSetup(isOpen(), getWindow().getDecorView().getWidth(), getWindow().getDecorView().getHeight());
    }

    // The cached entitlement. Stored as a token rather than a bare boolean, and
    // bound to this install's firstInstallTime so a token pasted from elsewhere
    // does not verify. Not a security boundary -- it never grants anything Play
    // has denied, it only carries a past YES across a launch Play cannot answer.
    private static final String ENTITLE_PREF = "iabc";

    private static String entitleToken(Context c) {
        long installed = 0L;
        try {
            installed = c.getPackageManager().getPackageInfo(c.getPackageName(), 0).firstInstallTime;
        } catch (Exception e) {
            // No install time to bind to. Fall through with 0: the token still
            // works, it just stops being install-specific. Failing shut here
            // would cap a paying user, which is the bug this exists to prevent.
        }
        return xorEncrypt(PRODUCT_ID + "|" + installed, "vLt/2026");
    }

    private static void cacheEntitlement(Context c, boolean owned) {
        if (c != null)
            Activities.setDefaultsString(ENTITLE_PREF, owned ? entitleToken(c) : "", c);
    }

    private static boolean cachedEntitlement(Context c) {
        return c != null && entitleToken(c).equals(Activities.getDefaultsString(ENTITLE_PREF, "", c));
    }

    @Override
    public void onSubscriptionsUpdated(List<Purchase> subscriptions) {}

    @Override
    public void onPurchaseConsumed(String purchaseToken) {}

    @Override
    public void onBillingError(BillingResult billingResult) {
        firstrundone = true;
        // Play could not answer. The cache is left exactly as it is -- an
        // unanswered question is not a "no" -- but the UI must stop waiting:
        // native blocks on readyForUiSetup for ten seconds, and nothing else
        // on this path ever released it.
        //
        // The cache has had its say by now, so an unpurchased device may be told
        // what its stored token says. No mint: nothing here is authoritative.
        if (!isOpen())
            TrialGate.refresh(this);
        MyApplication.guiSetup(isOpen(), getWindow().getDecorView().getWidth(), getWindow().getDecorView().getHeight());
    }

    static String xorEncrypt(String input, String key) {
        byte[] inputBytes = input.getBytes();
        int inputSize = inputBytes.length;

        byte[] keyBytes = key.getBytes();
        int keySize = keyBytes.length - 1;

        byte[] outBytes = new byte[inputSize];
        for (int i = 0; i < inputSize; i++) {
            outBytes[i] = (byte) (inputBytes[i] ^ keyBytes[i % keySize]);
        }

        return new String(Base64.encode(outBytes, Base64.DEFAULT));
    }

    static String xorDecrypt(String input, String key) {
        byte[] inputBytes = Base64.decode(input, Base64.DEFAULT);
        int inputSize = inputBytes.length;

        byte[] keyBytes = key.getBytes();
        int keySize = keyBytes.length - 1;

        byte[] outBytes = new byte[inputSize];
        for (int i = 0; i < inputSize; i++) {
            outBytes[i] = (byte) (inputBytes[i] ^ keyBytes[i % keySize]);
        }

        return new String(outBytes);
    }

    static boolean isOpen() {
        return val.get().equals(lca.get());
    }
    private static void iabset(boolean result) {
        if (result) {
            val.set(strings[0]);
            lca.set(strings[0]);
        }
    }
    private BillingManager billingManager;

    @Override
    public void onBillingClientSetupFinished() {
        if (billingManager != null && billingManager.getBillingClientResponseCode() != BillingClient.BillingResponseCode.OK) {
            MyApplication.guiSetup(isOpen(), getWindow().getDecorView().getWidth(), getWindow().getDecorView().getHeight());
        }
    }

    @Override
    public void onPurchasesUpdated(List<Purchase> purchaseList, boolean fromQuery) {
        boolean owned = false;
        for (Purchase purchase : purchaseList) {
            if (purchase.getProducts().contains(PRODUCT_ID.toLowerCase(Locale.ROOT))) {
                if (purchase.getPurchaseState() == Purchase.PurchaseState.PURCHASED) {
                    if (!purchase.isAcknowledged() && billingManager != null) {
                        billingManager.acknowledgePurchase(purchase, null);
                    }
                    iabset(true);
                    owned = true;
                } else if (purchase.getPurchaseState() == Purchase.PurchaseState.PENDING && !isFinishing()) {
                    new AlertDialog.Builder(this, R.style.MyAlertdialogtheme)
                            .setMessage("Purchase pending. Once it completes, restart Voltaic.")
                            .setPositiveButton("Restart",
                                    (dialog, which) -> {
                                        PackageManager packageManager = getPackageManager();
                                        Intent intent = packageManager.getLaunchIntentForPackage(getPackageName());
                                        if (intent != null) {
                                            ComponentName componentName = intent.getComponent();
                                            Intent mainIntent = Intent.makeRestartActivityTask(componentName);
                                            startActivity(mainIntent);
                                            finish();
                                            System.exit(0);
                                        }
                                    })
                            .setNegativeButton("Cancel", (dialogInterface, i) -> dialogInterface.dismiss())
                            .show();
                }

            }

        }

        // Only an explicit query that Play answered OK is authoritative in both
        // directions; that is the one place absence means "not owned", and it is
        // what lets a refund take the grant back instead of the cache outliving
        // it. The purchase-flow notification arrives with whatever the manager
        // happens to hold -- on a cancelled dialog, possibly nothing -- so
        // reading absence as a negative there would wipe a live entitlement.
        //
        // Clearing the cache is as far as this goes: iabset has no false branch
        // and deliberately gains none here. A revoked order costs the rest of
        // this session and is capped from the next launch, which is exactly what
        // shipped before the cache existed (entitlement was resolved once, at
        // launch). Revoking mid-session would mean one spurious empty-but-OK
        // response could wall a paying customer mid-playing -- the failure this
        // whole change exists to prevent.
        if (fromQuery || owned)
            cacheEntitlement(this, owned);

        // The purchase question is settled, so a trial verdict can finally be
        // acted on. Owned devices are never told anything about trials.
        if (!owned && !isOpen()) {
            TrialGate.refresh(this);
            // Authoritative "you do not own this" -- the only point at which
            // starting a trial clock is correct.
            if (fromQuery)
                TrialGate.arm(this);
        }

        MyApplication.guiSetup(isOpen(), getWindow().getDecorView().getWidth(), getWindow().getDecorView().getHeight());
    }

}

