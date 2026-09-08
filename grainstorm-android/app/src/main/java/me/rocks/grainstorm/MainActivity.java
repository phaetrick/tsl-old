package me.rocks.grainstorm;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentResolver;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.ref.WeakReference;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.DecimalFormatSymbols;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicReference;

import android.content.pm.PackageManager;
import android.database.Cursor;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.app.NativeActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.net.Uri;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.util.Base64;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import android.window.SplashScreen;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatDialog;
import androidx.core.app.ActivityCompat;
import androidx.documentfile.provider.DocumentFile;

import static android.content.Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION;
import static android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION;
import static me.rocks.grainstorm.Activities.getDefaultsString;
import static me.rocks.grainstorm.Activities.setDefaultsString;
import static me.rocks.grainstorm.MyAdapter.getFileMetaData;
import static me.rocks.grainstorm.MyApplication.filebrowsercallback;
import static me.rocks.grainstorm.MyApplication.java_load_preset;
import static me.rocks.grainstorm.MyApplication.java_record_live;
import static me.rocks.grainstorm.MyApplication.java_record_loop;
import static me.rocks.grainstorm.MyApplication.java_save_loop;
import static me.rocks.grainstorm.PresetActivity.PRESET_EXTRA_MESSAGE;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.Purchase;

public class MainActivity extends NativeActivity implements BillingManager.BillingUpdatesListener {


    static boolean firstrundone = false;

    static final String PRODUCT_ID = "pro_upgrade";
    /*
    private static final String LICENSE_KEY =
            "KjsoKycZNSEwCgoBGCsOKlgeXGwiPiI0ICghMDU+SiwsOTkCJAoqKi1/Ji4UQCUQWkIbGgchVzcz" +
                    "az0ZACohfQkGHwIERitKQxs3LFI+FCcPCFZcKBk7PjMnVBEnPE0OGRxTFRMWAy8xUSJoKl1QCwdb" +
                    "VjwTXCM8NUIYDjMIDSFDYDoYXxE2MC85MgsrGlkaGxI1XBcZLhoHCA4KEAMtAyIXRA8gRScJKgk2" +
                    "Dh5IEBcXXRs7Agk9ViUqUgMEDik1DSo8WFcODig7WQ8yFiMcPRkeICERJzYfFQUFCjRKMQsBKgw8" +
                    "QCUnER01UQkqPhhNIAMWHjQsB0sOKBg0AiU8DAoUDiBeWDoICwFOIDgaFjlAJFUkFjEFKwQwCmhX" +
                    "Wy1EEkIXMiYoCEYwAyIiN1tTKwt9OiIoAFMrCh83GAFdLEc+Nx0DV1oDVyocCgIiACwWIT0XKDk5" +
                    "IzVXGgwKP0kEXxcfNwE8GhcYMDxYEh0QCAQwRidiJV0tEBIIISElBQM3KQc5BCY8ICs=";
    static String MERCHANT_ID = "V0ZYW1lDR1xHWFRAR3NWVVdaVBs=";
    private static final String LICENSE_KEY2 = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAs2Dy41ouuL6GC+ZtaCMSjixpe/E97tEA3Ndghe75D7XQTU5xIO9akq2ecVdBP8NFI27yf28Og3QQT2hNTelH/NYw8cWYAJFdYw8jkRR1vpB4dgixqjCpVx6bA5WIMdWgrfsxp/zRlzI9WG3stNNXlCPv4aiZZ0aAbLnPxnPavJWvy+feS8PboYxS2HFamu6dKWtcClqlUEi8zGjYcULLmyoI2vYgls/IVibV2I4TfqbFeYfF44J6s+yARGz+QsRbP62BgSYMOr2BdlCws0M7Nwzn63oyIsmpCiBeUReEXISu0wmcSgg0pmVhRicwBQ9bmPoiQ/KLF2JbsaORQjqZHwIDAQAB"; // PUT YOUR MERCHANT KEY HERE;
    // put your Google merchant id here (as stated in public profile of your Payments Merchant Center)
    // if filled library will provide protection against Freedom alike Play Market simulators
    private final static String key = "grainstormapp@gmail.com";
*/

    static private final String[] strings = {
            "ANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAs2Dy41ouuL6GC+ZtaCMSjixpe/E97tEA3Ndghe75D7XQTU5xIO9akq2ecVdBP8NFI27yf28Og3QQT2hNTelH/NYw8cWYAJFdYw8jkRR1vpB4dgixqjCpVx6bA5WIMdWgrfsxp/zRlzI9WG3stNNXlCPv4aiZZ0aAbLnPxnPavJWvy+feS8PboYxS2HFamu6dKWtcClqlUEi8zGjYcULLmyoI2vYgls/IVibV2I4TfqbFeYfF44J6s+yARGz+QsRbP62BgSYMOr2BdlCws0M7Nwzn63oyIsmpCiBeUReEXISu0wmcSgg0pmVhRicwBQ9bmPoiQ/KLF2JbsaORQjqZHwIDAQAB",
            "MMjhgIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAs2Dy41ouuL6GC+ZtaCMSjixpe/E97tEA3Ndghe75D7XQTU5xIO9akq2ecVdBP8NFI27yf28Og3QQT2hNTelH/NYw8cWYAJFdYw8jkRR1vpB4dgixqjCpVx6bA5WIMdWgrfsxp/zRlzI9WG3stNNXlCPv4aiZZ0aAbLnPxnPavJWvy+feS8PboYxS2HFamu6dKWtcClqlUEi8zGjYcULLmyoI2vYgls/IVibV2I4TfqbFeYfF44J6s+yARGz+QsRbP62BgSYMOr2BdlCws0M7Nwzn63oyIsmpCiBeUReEXISu0wmcSgg0pmVhRicwBQ9bmPoiQ/KLF2JbsaORQjqZHwIDAQAB"};
    final private static AtomicReference<String> val = new AtomicReference<>(strings[0]);
    final private static AtomicReference<String> lca = new AtomicReference<>(strings[1]);

    static final String PROVIDER_NAME = "me.rocks.grainstormunlocker";
    static final String URL = "content://" + PROVIDER_NAME + "/data";
    static final Uri CONTENT_URI = Uri.parse(URL);

    final static String url_eula = "file:///android_asset/eula.html";
    final static String url_oss = "file:///android_asset/oss.html";
    final static String url_instructions = "https://grainstorm.rocks.me/instructions.html";
    //final static String url_changes = "https://grainstorm.rocks.me/changes.html";
    final static String url_changes = "file:///android_asset/changes.html";

    final static int PERMISSION_ALL = 1;
    //String[] PERMISSIONS = {Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.MODIFY_AUDIO_SETTINGS};
    AppCompatDialog fileNameDialog;

    static final int REQUEST_FILE_SAF = 42;
    static final int REQUEST_FILE_INTERNAL = 43;
    static final int REQUEST_DIRECTORY_REC = 44;
    static final int REQUEST_DIRECTORY_PRESET = 45;
    static final int REQUEST_SAVE_PRESET = 46;
    static final int REQUEST_SAVE_MIDI = 48;

    final static String TAG = "MainActivity";
    static String poweroffString = "Power Off";
    static String poweronString = "Power On";
    static String micoffString = "Stop";
    static String foregroundString = "Foreground";

    static boolean nativeCrash = false;

    static WeakReference<AlertDialog> itemDialog = null;
    String[] PERMISSIONS = {Manifest.permission.MODIFY_AUDIO_SETTINGS};

    private static WeakReference<MainActivity> weakReference;

    static MainActivity getInstance() {
        if (weakReference != null) {
            MainActivity grainstorm = weakReference.get();
            if (grainstorm != null && !grainstorm.isFinishing())
                return grainstorm;
        }
        return null;
    }

    public static boolean nativeEngineReady = false;

    public static void onNativeReady() {
        nativeEngineReady = true;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        androidx.core.splashscreen.SplashScreen splashScreen =
                androidx.core.splashscreen.SplashScreen.installSplashScreen(this);

        super.onCreate(savedInstanceState);

        // 2. Now you can use 'splashScreen'
        splashScreen.setKeepOnScreenCondition(() -> !nativeEngineReady);
        weakReference = new WeakReference<>(this);
        if (!hasPermissions(PERMISSIONS)) {
            requestPermissions(PERMISSIONS, PERMISSION_ALL);
        }
        checkLicense2();
        // AI models verify and install themselves when their download lands
        // (aiVerifyPending): DownloadManager's completion broadcast while the
        // app runs, and this call for a download that finished while it did
        // not.
        aiRegisterDownloadReceiver();
        aiVerifyPending();
        Intent intent = getIntent();
        if (intent != null) {
            String action = intent.getAction();
            if (action == null) ;
            else if (Intent.ACTION_VIEW.equals(action))
                onNewIntent(intent);
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
        // 1. Handle the Notch/Cutout (Crucial for 9:16/16:9 alignment)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            // This allows drawing behind the camera "hole"
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }

        // 2. Handle the Bars (Sticky Immersive)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                // Hide both bars
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                // This is the equivalent of "IMMERSIVE_STICKY" (Auto-hide after swipe)
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            // Legacy support for API 24-29
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_FULLSCREEN);
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (!Intent.ACTION_VIEW.equals(intent.getAction())) return;
        String type = intent.getType();
        Uri uri = intent.getData();
        if (uri == null) {
            return;
        }

        if (type != null) {
            if (type.startsWith("audio/")) {
                try {
                    grantUriPermission(getPackageName(), uri, FLAG_GRANT_READ_URI_PERMISSION);
                    try {
                        getContentResolver().takePersistableUriPermission(uri, FLAG_GRANT_READ_URI_PERMISSION);
                    } catch (Exception ignored) {}
                    filebrowsercallback(uri.toString());
                } catch (Exception e) {
                    showToast(e.toString());
                }

            } else if (type.startsWith("application/octet-stream") || uri.toString().endsWith(".gsp") && isOpen()) {
                try {
                    //  int takeFlags = intent.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION;
                    //        contentResolver.takePersistableUriPermission(uri, takeFlags);
                    Log.e(Activities.LOG_TAG, uri.toString());
                    grantUriPermission(getPackageName(), uri, FLAG_GRANT_READ_URI_PERMISSION);
                    java_load_preset(uri.toString());
                } catch (Exception e) {
                    showToast(e.toString());
                    Log.e(Activities.LOG_TAG, e.toString());
                }
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        setScreenOnFlag(MyApplication.screenOn);
    }

    @Override
    protected void onDestroy() {
        if (fileNameDialog != null && fileNameDialog.isShowing())
            fileNameDialog.dismiss();
        aiUnregisterDownloadReceiver();
        super.onDestroy();
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
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        // Forward to your class (replace YourClassName with actual class name)
        PermissionHandler.onPermissionResult(requestCode, permissions, grantResults);

        if (requestCode == PERMISSION_ALL) {
            for (String permission : permissions) {
                if (permission.equals(Manifest.permission.POST_NOTIFICATIONS)) {
                    // Start the service regardless of the user's choice (Grant or
                    // Deny): since Android 13 a denied permission only hides the
                    // notification — the foreground service still starts and
                    // still holds the process up.
                    if (synthIsOn && MyApplication.runsAsForeground && !serviceStarted) {
                        MyApplication myApplication = MyApplication.getInstance();
                        if (myApplication != null) {
                            startSynthService(myApplication, new Intent(myApplication, SynthService.class));
                        }
                    }
                }
            }
        }
    }


    static int trackindex2 = 0;

    static void filebrowser(int index) {
        final MainActivity grainstorm = getInstance();
        if (grainstorm != null) {
            Intent intent;
            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.addFlags(FLAG_GRANT_READ_URI_PERMISSION
                    | FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            intent.setType("audio/*");
            intent.putExtra("Trackindex", index);
            trackindex2 = index;
            try {
                grainstorm.startActivityForResult(intent, REQUEST_FILE_SAF);
            } catch (Exception e) {
                showToast("Error: " + e.toString());
            }
        }
    }

    static void savePreset(int type) {

        final MainActivity grainstorm = getInstance();
        if (grainstorm == null || !Activities.isExternalStorageWritable())
            return;
        Intent intent = new Intent(grainstorm, PresetActivity.class);
        intent.putExtra(PRESET_EXTRA_MESSAGE, type);
        grainstorm.startActivity(intent);
    }

    static void setOutputDirectory(final int requestCode) {
        final MainActivity grainstorm = getInstance();
        if (grainstorm != null)
            grainstorm.runOnUiThread(() -> new AlertDialog.Builder(grainstorm, R.style.MyAlertdialogtheme)
                    .setTitle(requestCode == REQUEST_DIRECTORY_REC ? "No recording directory set." : "No home directory set.")
                    .setMessage(requestCode == REQUEST_DIRECTORY_REC ? "Recordings will be saved to that directory." : "Presets and Midimappings will be saved to that directory. There will be subfolders created in that directory, it is recommended you create a new directory.")

                    .setPositiveButton("Set now", (dialog, which) -> {
                        dialog.dismiss();
                        try {
                            Intent intent;
                            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);

                            //intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                            intent.addFlags(FLAG_GRANT_PERSISTABLE_URI_PERMISSION);

                            //intent.putExtra(Intent.EXTRA_LOCAL_ONLY, true);
                            intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

                            grainstorm.startActivityForResult(intent, requestCode);
                        } catch (Exception e) {
                            MainActivity.showToast(e.toString());
                        }
                    })
                    .show());
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode,
                                 Intent resultData) {
        if (requestCode == REQUEST_FILE_SAF && resultCode == Activity.RESULT_OK) {
            Uri uri;
            if (resultData != null) {
                ContentResolver contentResolver = null;
                contentResolver = getContentResolver();
                if (contentResolver == null) {
                    showToast("Something went wrong.");
                    return;
                }
                uri = resultData.getData();
                if (uri != null) {
                    Log.i("MainActivity", "Uri: " + uri.toString());
                    int takeFlags = resultData.getFlags() & FLAG_GRANT_READ_URI_PERMISSION;
                    try {
                        contentResolver.takePersistableUriPermission(uri, takeFlags);
                    } catch (Exception e) {
                        showToast(e.toString());
                    } finally {
                        filebrowsercallback(uri.toString());
                    }
                }
            }
        } else if (requestCode == REQUEST_DIRECTORY_REC && resultCode == Activity.RESULT_OK) {
            ContentResolver contentResolver = getContentResolver();
            if (contentResolver == null) {
                Log.e("MainActivity", "Something went wrong");
                showToast("Something went wrong.");
                return;
            }

            Uri uriTree = resultData.getData();
            if (uriTree != null) {
                int takeFlags = resultData.getFlags() & FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
                grantUriPermission(getPackageName(), uriTree, takeFlags);

                contentResolver.takePersistableUriPermission(uriTree, takeFlags);
                setDefaultsString(getString(R.string.recordingdirectory), uriTree.toString(), this);
                String outdir = FileUtil.getFullPathFromTreeUri(uriTree, this);
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
                int takeFlags = resultData.getFlags() & FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
                grantUriPermission(getPackageName(), uriTree, takeFlags);

                contentResolver.takePersistableUriPermission(uriTree, takeFlags);
                setDefaultsString(getString(R.string.homedirectory), uriTree.toString(), this);
                String outdir = FileUtil.getFullPathFromTreeUri(uriTree, this);
                if (outdir != null) {
                    String output = "Preset directory set to: " + outdir;
                    showToast(output);
                } else showToast("Preset directory changed.");
            }
        }
    }


    static int getFd(String uristring) {
        MainActivity grainstorm = getInstance();
        if (grainstorm == null)
            return 0;
        Uri uri = Uri.parse(uristring);
        if (uri != null) {
            Log.i("MainActivity", "Uri: " + uri.toString());
            try {
                ParcelFileDescriptor parcelFileDescriptor =
                        grainstorm.getContentResolver().openFileDescriptor(uri, "r");
                if (parcelFileDescriptor == null) {
                    showToast("File error.");
                    return 0;
                }
                return parcelFileDescriptor.detachFd();
            } catch (FileNotFoundException e) {
                Log.e("MainActivity", "Could not open uri as fd");
                showToast("File error.");
                return 0;
            }
        } else return 0;
    }

    static String getDecodingPath(String uristring) {
        MainActivity grainstorm = getInstance();
        if (grainstorm == null)
            return null;
        try {
            String path;
            ParcelFileDescriptor parcelFileDescriptor =
                    grainstorm.getContentResolver().openFileDescriptor(Uri.parse(uristring), "r");
            if (parcelFileDescriptor == null) {
                showToast("File error.");
                return null;
            }
            if (MyApplication.copyFiles) {
                File outputDir = grainstorm.getCacheDir(); // context being the Activity pointer
                File outputFile = File.createTempFile("grainstormtmpdecoding", "tmp", outputDir);
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
                path = "file:" + outputFile.getAbsolutePath();
            } else {
                path = String.format(Locale.US, "pipe:%d", parcelFileDescriptor.detachFd());
                //path = String.format(Locale.US, "file:/proc/self/fd/%d", parcelFileDescriptor.detachFd());

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
            final MainActivity grainstorm = MainActivity.getInstance();
            if (grainstorm == null)
                return;
            final String format = Activities.recordingExtensions[Activities.getDefaultsInt(grainstorm.getString(R.string.recordingformat), 0, grainstorm)];
            final String extension = "." + format;
            SimpleDateFormat formatter = new SimpleDateFormat("yyyy_MM_dd_HH_mm_ss", Locale.US);
            Date now = new Date();
            if (mode == RECORD_LIVE)
                secondname[0] = firstname[0] = "grainstorm-" + formatter.format(now) + extension;
            else if (mode == RECORD_LOOP)
                secondname[0] = firstname[0] = "grainstorm-loop-" + formatter.format(now) + extension;
            else
                secondname[0] = firstname[0] = "grainstorm-loop-unprocessed-" + formatter.format(now) + extension;


            String uristring = getDefaultsString(grainstorm.getString(R.string.recordingdirectory), null, grainstorm);
            if (uristring != null) {
                try {
                    Uri uri = Uri.parse(uristring);
                    DocumentFile documentFile = DocumentFile.fromTreeUri(grainstorm, uri);
                    assert documentFile != null;
                    outputfile = documentFile.createFile("audio/" + format, firstname[0]);
                    outputFolder = uri.getPath();
                    parcelFileDescriptor = grainstorm.getContentResolver().openFileDescriptor(((DocumentFile) outputfile).getUri(), "w");
                } catch (Exception e) {
                    showToast(e.toString());
                    return;
                }
            } else {
                grainstorm.runOnUiThread(() -> new AlertDialog.Builder(grainstorm, R.style.MyAlertdialogtheme)
                        .setTitle("No recording directory set.")
                        .setPositiveButton("Set now", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                dialog.dismiss();
                                try {
                                    Intent intent;
                                    intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);

                                    //intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                                    intent.addFlags(FLAG_GRANT_PERSISTABLE_URI_PERMISSION);

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

                                    grainstorm.startActivityForResult(intent, REQUEST_DIRECTORY_REC);
                                } catch (Exception e) {
                                    MainActivity.showToast(e.toString());
                                }
                            }
                        })
                        .show());
                return;
            }

            int result;
            if (mode == RECORD_LIVE) {
                assert parcelFileDescriptor != null;
                result = java_record_live(parcelFileDescriptor.detachFd());
            }
            else if (mode == RECORD_LOOP) {
                assert parcelFileDescriptor != null;
                result = java_record_loop(parcelFileDescriptor.detachFd(), track);
            }
            else {
                assert parcelFileDescriptor != null;
                result = java_save_loop(parcelFileDescriptor.detachFd(), track);
            }

            parcelFileDescriptor.close();

            if (result == 0) {
                if (!grainstorm.isFinishing() && Activities.getDefaultsBoolean(grainstorm.getString(R.string.askname), false, grainstorm)) {
                    final Object lock = new Object();

                    grainstorm.runOnUiThread(() -> {


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
                        grainstorm.fileNameDialog = adb.show();

                        Window w = grainstorm.fileNameDialog.getWindow();

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
                                    InputMethodManager imm = (InputMethodManager) grainstorm.getSystemService(Context.INPUT_METHOD_SERVICE);
                                    if (imm != null)
                                        imm.hideSoftInputFromWindow(rootView.getWindowToken(), 0);
                                    grainstorm.fileNameDialog.dismiss();
                                    return true;
                                }
                                return false;
                            }

                        });
                    });

                    synchronized (lock) {
                        lock.wait();
                    }
                }
                String output;
                if (delete[0]) {
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
                outputfile.delete();
            }

        } catch (Exception e) {
            showToast(e.toString());
        }
    }


    static DocumentFile getHome(String subDir) {
        MainActivity context = getInstance();
        if (context == null) return null;
        DocumentFile home = null;
        String uristring = getDefaultsString(context.getString(R.string.presetdirectory), null, context);
        if (uristring != null && !MyApplication.systemFolderForPresets) {
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

    static DocumentFile[] getDirContent2(String subDir) {
        DocumentFile home = getHome(subDir);
        if (home != null)
            return home.listFiles();
        else return null;
    }

    // WHERE THE AI MODELS LIVE: Downloads/Grainstorm, the public Downloads
    // folder, where the system DownloadManager puts them -- and where they
    // STAY. Not this app's own storage (Patrick: "The download should never
    // go into the apps folder (too big)"), and not a folder anyone has to
    // choose ("put into downloads folder Grainstorm/ and leave it there?
    // easier no asking"). The download's record is the model's record: it
    // says the file is there, and it hands out the descriptor the native
    // engine reads the model through (aiModelOpen), so no path is assumed
    // -- a file in shared storage is not always one this app may open by
    // name. Deleting the record deletes the file.
    //
    // The old app-storage location, kept only for the grainstorm (gs1)
    // build's native side, which still asks for a path.
    static String getAiModelsDir() {
        MainActivity context = getInstance();
        if (context == null) return null;
        File base = Activities.isExternalStorageWritable() ? context.getExternalFilesDir(null) : null;
        if (base == null) base = context.getFilesDir();
        File dir = new File(base, "aigen-models");
        //noinspection ResultOfMethodCallIgnored
        dir.mkdirs();
        return dir.getAbsolutePath();
    }

    private static long aiDownloadId(String fileName) {
        MainActivity context = getInstance();
        if (context == null) return -1;
        return context.getSharedPreferences("aigen", MODE_PRIVATE).getLong("dl_" + fileName, -1);
    }

    // Verified once, by size and sha256, when the download finished
    // (aiVerifyPending); from then on the record plus this flag is "installed".
    private static boolean aiVerified(String fileName) {
        MainActivity context = getInstance();
        return context != null && context.getSharedPreferences("aigen", MODE_PRIVATE)
                .getBoolean("ok_" + fileName, false);
    }

    private static void aiSetVerified(String fileName, boolean ok) {
        MainActivity context = getInstance();
        if (context == null) return;
        context.getSharedPreferences("aigen", MODE_PRIVATE).edit()
                .putBoolean("ok_" + fileName, ok).apply();
    }

    // THE FILE'S OWN ENTRY, kept beside the download's record from the moment
    // the model verifies: MediaStore's row for it, which this app owns because
    // it asked for the download. The DownloadManager record can go -- the user
    // clears the Downloads app, the system prunes old entries -- while the
    // file stays in Downloads/Grainstorm; the row is how the model is still
    // found and opened then, instead of being fetched again. Android 10 and
    // up, where DownloadManager reports the row.
    private static String aiMediaUri(String fileName) {
        MainActivity context = getInstance();
        if (context == null) return null;
        return context.getSharedPreferences("aigen", MODE_PRIVATE).getString("ms_" + fileName, null);
    }

    private static void aiSetMediaUri(String fileName, String uri) {
        MainActivity context = getInstance();
        if (context == null) return;
        android.content.SharedPreferences.Editor e =
                context.getSharedPreferences("aigen", MODE_PRIVATE).edit();
        if (uri == null) e.remove("ms_" + fileName); else e.putString("ms_" + fileName, uri);
        e.apply();
    }

    // The row for a finished download: what DownloadManager reports for it,
    // and failing that the row named like it in Download/Grainstorm among
    // what this app owns.
    private static String aiQueryMediaUri(android.app.DownloadManager dm, long id, String fileName) {
        if (android.os.Build.VERSION.SDK_INT < 29) return null;
        MainActivity context = getInstance();
        if (context == null) return null;
        String uri = null;
        // The download provider's own column for it, by name: DownloadManager
        // has no public constant for it, and a cursor without the column
        // answers -1 rather than throwing.
        try (Cursor c = dm.query(new android.app.DownloadManager.Query().setFilterById(id))) {
            if (c != null && c.moveToFirst()) {
                int col = c.getColumnIndex("mediastore_uri");
                if (col >= 0) uri = c.getString(col);
            }
        } catch (Exception ignored) {}
        if (uri == null) {
            try {
                Uri coll = android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI;
                try (Cursor c = context.getContentResolver().query(coll,
                        new String[]{ android.provider.MediaStore.Downloads._ID },
                        android.provider.MediaStore.Downloads.DISPLAY_NAME + "=? AND " +
                                android.provider.MediaStore.Downloads.RELATIVE_PATH + "=?",
                        new String[]{ fileName, "Download/Grainstorm/" }, null)) {
                    if (c != null && c.moveToFirst())
                        uri = android.content.ContentUris.withAppendedId(coll, c.getLong(0)).toString();
                }
            } catch (Exception ignored) {}
        }
        return uri;
    }

    // The model through its MediaStore row; null when the row, or the file
    // behind it, is gone.
    private static ParcelFileDescriptor aiOpenMedia(String fileName) {
        String u = aiMediaUri(fileName);
        MainActivity context = getInstance();
        if (u == null || context == null) return null;
        try {
            return context.getContentResolver().openFileDescriptor(Uri.parse(u), "r");
        } catch (Exception e) {
            return null;
        }
    }

    private static boolean aiRecordSays(String file) {
        return aiDownloadId(file) >= 0 && "success".equals(queryAiModelDownload(file));
    }

    // Called from native (aigen.cpp): whether the model is there to load --
    // by the download's record, or by the file's own row when the record is
    // gone. Verified once either way.
    static boolean aiModelPresent(String dir, String file) {
        if (!aiVerified(file)) return false;
        if (aiRecordSays(file)) return true;
        try (ParcelFileDescriptor pfd = aiOpenMedia(file)) {
            return pfd != null && pfd.getStatSize() > 0;
        } catch (Exception e) {
            return false;
        }
    }

    // Called from native (aigen.cpp): the model file opened for reading, as a
    // detached descriptor the native side keeps for as long as the engine may
    // have the model resident -- through DownloadManager while its record is
    // there, through the file's MediaStore row after. 0 when there is none.
    static int aiModelOpen(String dir, String file) {
        try {
            MainActivity context = getInstance();
            if (context == null || !aiModelPresent(dir, file)) return 0;
            if (aiRecordSays(file)) {
                android.app.DownloadManager dm = (android.app.DownloadManager)
                        context.getSystemService(Context.DOWNLOAD_SERVICE);
                if (dm != null) {
                    ParcelFileDescriptor pfd = dm.openDownloadedFile(aiDownloadId(file));
                    if (pfd != null) return pfd.detachFd();
                }
            }
            ParcelFileDescriptor pfd = aiOpenMedia(file);
            return pfd == null ? 0 : pfd.detachFd();
        } catch (Exception e) {
            return 0;
        }
    }

    // Called from native (aigen.cpp): a private place for the symlinks that
    // let the engine open a model directory by path. Kilobytes, not models.
    static String getAiLinkDir() {
        MainActivity context = getInstance();
        if (context == null) return null;
        File d = new File(context.getFilesDir(), "aigen-link");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();
        return d.getAbsolutePath();
    }

    // Called from native (aigen.cpp). Hands the model transfer to the system
    // DownloadManager: it shows the progress notification, resumes after
    // network loss, and keeps going if the app dies. Writes to
    // aigen-models/.dl/<fileName>; native verifies sha256 there and renames
    // into the model dir, so a partial file is never loadable.
    static long startAiModelDownload(String url, String fileName, String title) {
        MainActivity context = getInstance();
        if (context == null) return -1;
        try {
            android.app.DownloadManager dm =
                    (android.app.DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
            if (dm == null) return -1;
            // A stale download from an earlier attempt would make DownloadManager
            // append or fail oddly; start clean -- through DownloadManager,
            // which owns the file and deletes it with the record.
            aiClearDownloadRecord(fileName);
            android.app.DownloadManager.Request request =
                    new android.app.DownloadManager.Request(Uri.parse(url));
            request.setTitle(title);
            request.setNotificationVisibility(
                    android.app.DownloadManager.Request.VISIBILITY_VISIBLE);
            // INTO THE PUBLIC DOWNLOADS FOLDER, where it stays: the one place
            // outside this app DownloadManager can write, and no permission
            // is needed for it.
            request.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS,
                    "Grainstorm/" + fileName);
            long id = dm.enqueue(request);
            context.getSharedPreferences("aigen", MODE_PRIVATE)
                    .edit().putLong("dl_" + fileName, id).apply();
            return id;
        } catch (Exception e) {
            return -1;
        }
    }

    // RECORD AI->TRACK model registry. Mirrors kModels in aigen.cpp — keep the
    // two in sync. URLs are pinned to a Hugging Face commit sha; the sha256
    // values match both a local computation and HF's x-linked-etag for these
    // exact URLs, so what we verified is what users download.
    static class AiModel {
        final String key, dir, file, url, sha256, title;
        final long bytes;
        AiModel(String key, String dir, String file, String url, String sha256,
                long bytes, String title) {
            this.key = key; this.dir = dir; this.file = file; this.url = url;
            this.sha256 = sha256; this.bytes = bytes; this.title = title;
        }
    }

    static final AiModel[] AI_MODELS = {
            new AiModel("sfx", "stable-audio-3-small-sfx",
                    "stable-audio-3-small-sfx-q8_0.gguf",
                    "https://huggingface.co/TheSecretLaboratory/grainstorm-models/resolve/45b75f8786b3ca53ad24dc2fd57e57872c271e82/stable-audio-3-small-sfx-q8_0.gguf",
                    "5bb1ec653134e63dac46264e336ae198a2590195fc996fcd0605021215e9b26f",
                    1683570688L, "AI SFX Model"),
            new AiModel("music", "stable-audio-3-small-music",
                    "stable-audio-3-small-music-q8_0.gguf",
                    "https://huggingface.co/TheSecretLaboratory/grainstorm-models/resolve/45b75f8786b3ca53ad24dc2fd57e57872c271e82/stable-audio-3-small-music-q8_0.gguf",
                    "89bb22db5fa68ab8f1d90af0a9f88121977c945bdb99a57c96b1e88fa34bf1c8",
                    1683570752L, "AI Music Model"),
    };

    // Server-side manifest: lets us re-point hosting, ship new quantizations,
    // or serve device/app-version-specific files without an app update. The
    // baked AI_MODELS constants above are the fallback, so offline users and
    // server outages degrade to "still works" — never "can't download".
    private static volatile java.util.Map<String, AiModel> aiManifestOverrides = null;

    static final String AI_MANIFEST_URL =
            "https://thesecretlaboratory.com/grainstorm/aimodels.json";

    // Fire-and-forget refresh; called when the settings screen opens, so the
    // manifest is fresh by the time a download could start.
    static void aiRefreshManifest() {
        new Thread(() -> {
            try {
                MainActivity context = getInstance();
                if (context == null) return;
                String gs = "0";
                try {
                    gs = String.valueOf(context.getPackageManager()
                            .getPackageInfo(context.getPackageName(), 0).versionCode);
                } catch (Exception ignored) {}
                java.net.HttpURLConnection conn = (java.net.HttpURLConnection)
                        new java.net.URL(AI_MANIFEST_URL + "?gs=" + gs + "&os=android&abi=arm64-v8a")
                                .openConnection();
                conn.setConnectTimeout(4000);
                conn.setReadTimeout(4000);
                if (conn.getResponseCode() != 200) return;
                java.io.ByteArrayOutputStream bo = new java.io.ByteArrayOutputStream();
                try (InputStream in = conn.getInputStream()) {
                    byte[] buf = new byte[4096];
                    int n;
                    while ((n = in.read(buf)) > 0 && bo.size() < 65536) bo.write(buf, 0, n);
                }
                applyAiManifest(bo.toString("UTF-8"), true);
            } catch (Exception ignored) {
                // Baked constants cover every failure mode.
            }
        }, "aigen-manifest").start();
    }

    private static void applyAiManifest(String json, boolean persist) {
        try {
            org.json.JSONArray arr = new org.json.JSONObject(json).getJSONArray("models");
            java.util.HashMap<String, AiModel> map = new java.util.HashMap<>();
            for (int i = 0; i < arr.length(); i++) {
                org.json.JSONObject o = arr.getJSONObject(i);
                AiModel base = null;
                for (AiModel m : AI_MODELS) if (m.key.equals(o.getString("key"))) base = m;
                if (base == null) continue;  // unknown keys are for future app versions
                String url = o.getString("url");
                if (!url.startsWith("https://")) continue;
                map.put(base.key, new AiModel(base.key,
                        o.optString("dir", base.dir), o.optString("file", base.file),
                        url, o.getString("sha256"), o.getLong("bytes"), base.title));
            }
            if (map.isEmpty()) return;
            aiManifestOverrides = map;
            if (persist) {
                MainActivity context = getInstance();
                if (context != null)
                    context.getSharedPreferences("aigen", MODE_PRIVATE)
                            .edit().putString("manifest", json).apply();
            }
        } catch (Exception ignored) {
            // Malformed manifest: keep whatever we had.
        }
    }

    static AiModel aiModelByKey(String key) {
        java.util.Map<String, AiModel> overrides = aiManifestOverrides;
        if (overrides == null) {
            // First use this process: apply the last-fetched manifest, if any.
            MainActivity context = getInstance();
            if (context != null) {
                String cached = context.getSharedPreferences("aigen", MODE_PRIVATE)
                        .getString("manifest", null);
                if (cached != null) applyAiManifest(cached, false);
                overrides = aiManifestOverrides;
            }
        }
        if (overrides != null && overrides.containsKey(key)) return overrides.get(key);
        for (AiModel m : AI_MODELS) if (m.key.equals(key)) return m;
        return null;
    }

    static boolean aiModelInstalled(AiModel m) {
        return aiModelPresent(m.dir, m.file);
    }

    private static final java.util.concurrent.atomic.AtomicBoolean aiVerifying =
            new java.util.concurrent.atomic.AtomicBoolean(false);

    private static void aiToast(final String text) {
        MainActivity context = getInstance();
        if (context == null) return;
        context.runOnUiThread(() -> android.widget.Toast.makeText(
                context, text, android.widget.Toast.LENGTH_LONG).show());
    }

    // One state string for the settings UI:
    // "installed" | "running|soFar|total" | "success" | "failed|reason" |
    // "verifying" | "none"
    static String aiModelState(String key) {
        AiModel m = aiModelByKey(key);
        if (m == null) return "none";
        if (aiModelInstalled(m)) return "installed";
        if (aiVerifying.get()) return "verifying";
        String state = queryAiModelDownload(m.file);
        // A finished download is never left waiting for a tap: it verifies
        // and installs itself, as grainstorm's own download thread does on
        // the desktop the moment the transfer ends (aigen.cpp).
        if (state.equals("success")) {
            aiVerifyPending();
            if (aiVerifying.get()) return "verifying";
        }
        return state;
    }

    // VERIFY AND INSTALL, UNASKED. grainstorm's desktop download thread checks
    // size and sha256 the moment the transfer ends and installs on a match
    // (aigen.cpp, modelDownloadThread); on Android the transfer is the system
    // DownloadManager's, which only hands back a completion broadcast, so the
    // same check runs from that broadcast (aiDownloadReceiver), from onCreate
    // for a download that finished while the app was not running, and from
    // the first state query after either -- whichever comes first, once. The
    // tap that used to be needed ("Downloaded - tap to verify and install")
    // is gone; a tap now only reports.
    //
    // One pass over every model with a SUCCESSFUL record and no verified
    // flag, on one thread, guarded by aiVerifying so two triggers cannot
    // hash the same 1.7 GB twice.
    static void aiVerifyPending() {
        final MainActivity context = getInstance();
        if (context == null) return;
        final java.util.ArrayList<AiModel> pending = new java.util.ArrayList<>();
        for (AiModel base : AI_MODELS) {
            AiModel m = aiModelByKey(base.key);
            if (m == null || aiVerified(m.file)) continue;
            if (aiDownloadId(m.file) >= 0 && "success".equals(queryAiModelDownload(m.file)))
                pending.add(m);
        }
        if (pending.isEmpty()) return;
        if (!aiVerifying.compareAndSet(false, true)) return;   // one already running
        new Thread(() -> {
            try {
                for (AiModel m : pending) aiVerifyOne(m);
            } finally {
                aiVerifying.set(false);
            }
        }, "aigen-verify").start();
    }

    // The check itself, on the verify thread: size, then sha256 over the file
    // DownloadManager holds. A match records the file's MediaStore row and the
    // verified flag -- that pair IS "installed"; the file stays where it is.
    // A mismatch clears the record and its file, or the next look would
    // re-verify the void forever instead of downloading again.
    private static void aiVerifyOne(AiModel m) {
        aiToast("Verifying " + m.title + "...");
        try {
            MainActivity context = getInstance();
            android.app.DownloadManager dm = context == null ? null
                    : (android.app.DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
            long id = aiDownloadId(m.file);
            if (dm == null || id < 0) {
                aiToast(m.title + ": download record missing - tap to retry.");
                return;
            }
            long size;
            try (ParcelFileDescriptor pfd = dm.openDownloadedFile(id)) {
                size = pfd.getStatSize();
            }
            if (size != m.bytes || !sha256Matches(dm, id, m.sha256)) {
                aiClearDownloadRecord(m.file);
                aiToast(m.title + ": download was corrupt - deleted, tap to retry.");
                return;
            }
            aiSetMediaUri(m.file, aiQueryMediaUri(dm, id, m.file));
            aiSetVerified(m.file, true);
            aiToast(m.title + " installed.");
        } catch (Exception e) {
            aiToast(m.title + ": verification failed - " + e.getMessage());
        }
    }

    // DownloadManager's completion broadcast, registered while the activity
    // lives. Exported, because the sender is the system's download provider
    // and not this app; the action is a protected broadcast, so nothing else
    // can send it. On Android 14 and up a context-registered receiver has to
    // say which it is.
    private android.content.BroadcastReceiver aiDownloadReceiver = null;

    private void aiRegisterDownloadReceiver() {
        if (aiDownloadReceiver != null) return;
        aiDownloadReceiver = new android.content.BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                if (intent == null ||
                    !android.app.DownloadManager.ACTION_DOWNLOAD_COMPLETE.equals(intent.getAction()))
                    return;
                aiVerifyPending();
            }
        };
        android.content.IntentFilter filter =
                new android.content.IntentFilter(android.app.DownloadManager.ACTION_DOWNLOAD_COMPLETE);
        try {
            if (android.os.Build.VERSION.SDK_INT >= 33)
                registerReceiver(aiDownloadReceiver, filter, Context.RECEIVER_EXPORTED);
            else
                registerReceiver(aiDownloadReceiver, filter);
        } catch (Exception e) {
            aiDownloadReceiver = null;
        }
    }

    private void aiUnregisterDownloadReceiver() {
        if (aiDownloadReceiver == null) return;
        try {
            unregisterReceiver(aiDownloadReceiver);
        } catch (Exception ignored) {}
        aiDownloadReceiver = null;
    }

    // How much can be written to the models volume right now. Android 8+
    // getAllocatableBytes also counts space the system can reclaim from app
    // caches, so it will not under-report a device that could actually hold
    // the file; StatFs is the fallback. Any failure returns MAX_VALUE - the
    // caller treats "unknown" as "do not block".
    static long aiAvailableBytes() {
        File dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        if (dir == null) return Long.MAX_VALUE;
        if (android.os.Build.VERSION.SDK_INT >= 26) {
            try {
                android.os.storage.StorageManager sm =
                        getInstance().getSystemService(android.os.storage.StorageManager.class);
                if (sm != null)
                    return sm.getAllocatableBytes(sm.getUuidForPath(dir));
            } catch (Exception ignored) {}
        }
        try {
            return new android.os.StatFs(dir.getAbsolutePath()).getAvailableBytes();
        } catch (Exception ignored) {}
        return Long.MAX_VALUE;
    }

    static String aiHumanBytes(long b) {
        if (b >= 1024L * 1024 * 1024)
            return String.format(java.util.Locale.US, "%.1f GB", b / (1024.0 * 1024 * 1024));
        return (b / (1024 * 1024)) + " MB";
    }

    // Settings tap: starts the download, or reports on one. Verification is
    // not the tap's any more -- see aiVerifyPending -- and a torn download
    // still never installs: only a size AND sha256 match sets the flag.
    static void aiModelClick(String key) {
        final AiModel m = aiModelByKey(key);
        if (m == null) return;
        if (aiModelInstalled(m)) {
            aiToast(m.title + " is installed.");
            return;
        }
        String state = queryAiModelDownload(m.file);
        if (state.startsWith("running|")) {
            try {
                String[] parts = state.split("\\|");
                long soFar = Long.parseLong(parts[1]);
                long total = Long.parseLong(parts[2]);
                aiToast(m.title + ": downloading... " +
                        (total > 0 ? (soFar * 100 / total) + "%" : "starting"));
            } catch (Exception e) {
                aiToast(m.title + ": downloading...");
            }
            return;
        }
        if (state.equals("success")) {
            // Finished: it verifies itself (aiVerifyPending). A tap here only
            // starts that if nothing has yet, and says so either way.
            aiVerifyPending();
            aiToast(aiVerifying.get() ? "Still verifying, one moment."
                                      : m.title + ": verifying...");
            return;
        }
        // Space pre-flight. Blocks only when the system POSITIVELY reports too
        // little room; anything uncertain lets the download proceed (fail-open),
        // because a genuinely full disk still fails cleanly through the
        // DownloadManager error path this flow already handles.
        long available = aiAvailableBytes();
        long needed = m.bytes + 16L * 1024 * 1024;
        if (available < needed) {
            aiToast("Not enough space for " + m.title + ": needs " + aiHumanBytes(needed)
                    + ", " + aiHumanBytes(available) + " available. Free some space and retry.");
            return;
        }
        // "none" or "failed|...": (re)start. The DownloadManager notification
        // is the progress UI.
        if (startAiModelDownload(m.url, m.file, m.title) >= 0)
            aiToast(m.title + ": download started (1.7 GB) - progress in the " +
                    "notification. It installs itself when the download finishes.");
        else
            aiToast("Could not start the download.");
    }

    // Forget a finished/stale DownloadManager record. Without this, a deleted
    // staged file with a surviving SUCCESS record makes every tap re-"verify"
    // nothing instead of restarting the download — a stuck loop (found by the
    // delete-then-redownload test on device).
    private static void aiClearDownloadRecord(String fileName) {
        MainActivity context = getInstance();
        if (context == null) return;
        boolean removed = false;
        try {
            long id = context.getSharedPreferences("aigen", MODE_PRIVATE)
                    .getLong("dl_" + fileName, -1);
            if (id >= 0) {
                android.app.DownloadManager dm = (android.app.DownloadManager)
                        context.getSystemService(Context.DOWNLOAD_SERVICE);
                if (dm != null) removed = dm.remove(id) > 0;   // the file goes with it
            }
        } catch (Exception ignored) {}
        // The record was already gone: the file may still be there under its
        // own row, and that row is this app's to delete.
        if (!removed) {
            try {
                String u = aiMediaUri(fileName);
                if (u != null) context.getContentResolver().delete(Uri.parse(u), null, null);
            } catch (Exception ignored) {}
        }
        context.getSharedPreferences("aigen", MODE_PRIVATE)
                .edit().remove("dl_" + fileName).remove("ok_" + fileName)
                .remove("ms_" + fileName).apply();
    }

    // Settings "tap to delete" — frees the ~1.7 GB when the user needs space.
    // Also clears any staged download and the DM record so a later
    // re-download starts clean.
    static void aiModelDelete(String key) {
        AiModel m = aiModelByKey(key);
        if (m == null) return;
        try {
            aiClearDownloadRecord(m.file);   // DownloadManager deletes the file with it
            aiToast(m.title + " deleted.");
        } catch (Exception e) {
            aiToast("Could not delete: " + e.getMessage());
        }
    }

    private static boolean sha256Matches(android.app.DownloadManager dm, long id, String expectedHex) {
        try (ParcelFileDescriptor pfd = dm.openDownloadedFile(id);
             InputStream in = new FileInputStream(pfd.getFileDescriptor())) {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] buf = new byte[1 << 20];
            int n;
            while ((n = in.read(buf)) > 0) md.update(buf, 0, n);
            StringBuilder hex = new StringBuilder();
            for (byte b : md.digest()) hex.append(String.format(Locale.US, "%02x", b));
            return hex.toString().equalsIgnoreCase(expectedHex);
        } catch (Exception e) {
            return false;
        }
    }

    // Called from native (aigen.cpp).
    // Returns "none" | "running|<soFar>|<total>" | "success" | "failed|<code>".
    static String queryAiModelDownload(String fileName) {
        MainActivity context = getInstance();
        if (context == null) return "none";
        long id = context.getSharedPreferences("aigen", MODE_PRIVATE)
                .getLong("dl_" + fileName, -1);
        if (id < 0) return "none";
        try {
            android.app.DownloadManager dm =
                    (android.app.DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
            if (dm == null) return "none";
            Cursor c = dm.query(new android.app.DownloadManager.Query().setFilterById(id));
            if (c == null) return "none";
            try {
                if (!c.moveToFirst()) return "none"; // cleared from the downloads list
                int status = c.getInt(c.getColumnIndexOrThrow(
                        android.app.DownloadManager.COLUMN_STATUS));
                if (status == android.app.DownloadManager.STATUS_SUCCESSFUL) return "success";
                if (status == android.app.DownloadManager.STATUS_FAILED) {
                    int reason = c.getInt(c.getColumnIndexOrThrow(
                            android.app.DownloadManager.COLUMN_REASON));
                    return "failed|" + reason;
                }
                long soFar = c.getLong(c.getColumnIndexOrThrow(
                        android.app.DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR));
                long total = c.getLong(c.getColumnIndexOrThrow(
                        android.app.DownloadManager.COLUMN_TOTAL_SIZE_BYTES));
                return "running|" + soFar + "|" + total;
            } finally {
                c.close();
            }
        } catch (Exception e) {
            return "none";
        }
    }

    static String getStoragePath(String subDir) {
        DocumentFile home = getHome(subDir);
        if (home != null) {
            try {
                String mimetype = "application/octet-stream";
                String name = String.format(Locale.US, "%s%s", String.valueOf(System.currentTimeMillis()), subDir == "midimappings" ? ".gsm" : ".gsp");
                DocumentFile file = home.createFile(mimetype, name);
                if (file != null) {
                    ParcelFileDescriptor parcelFileDescriptor = Objects.requireNonNull(getInstance()).getContentResolver().openFileDescriptor(file.getUri(), "rw");
                    return "pipe:/" + Objects.requireNonNull(parcelFileDescriptor).detachFd() + "|" + file.getUri().toString();
                }
            } catch (Exception e) {
                showToast(e.toString());
                return null;
            }

        }
        return null;
    }

    static Object[] getDirContent(String subDir) {
        try {
            MainActivity context = getInstance();
            DocumentFile[] home = getDirContent2(subDir);
            if (context != null && home != null && home.length > 0) {
                String[] strings = new String[home.length];
                int index = 0;
                for (DocumentFile f : home) {
                    strings[index++] = f.getUri().toString();
                }
                return strings;
            }
            return null;
        } catch (Exception e) {
            showToast(e.toString());
            return null;
        }
    }

    static int getFdFromUriString(String uriString, String mode) {
        try {
            MainActivity context = getInstance();
            if (context == null || context.getContentResolver() == null) return 0;
            if (mode.contains("r") && mode.contains("w")) mode = "rw";
            else if (mode.contains("r")) mode = "r";
            else if (mode.contains("w")) mode = "w";
            return Objects.requireNonNull(context.getContentResolver().openFileDescriptor(Uri.parse(uriString), mode)).detachFd();
        } catch (Exception e) {
            showToast(e.toString());
            return 0;
        }
    }


    static boolean deleteFromUri(String uriString) {
        try {
            MainActivity context = getInstance();
            if (context == null) return false;
            Uri uri = Uri.parse(uriString);
            if ("file".equals(uri.getScheme())) {
                return new File(uri.getPath()).delete();
            }
            return DocumentsContract.deleteDocument(context.getContentResolver(), uri);
        } catch (Exception e) {
            showToast("deleteFromUri: " + e.toString());
            return false;
        }
    }

    static void logString(final String string) {
        int maxLogSize = 1000;
        for (int i = 0; i <= string.length() / maxLogSize; i++) {
            int start = i * maxLogSize;
            int end = (i + 1) * maxLogSize;
            end = Math.min(end, string.length());
            Log.e(TAG, string.substring(start, end));
        }
    }

    static void showToast(final String string) {
        final MainActivity grainstorm = getInstance();
        if (grainstorm != null && !grainstorm.isFinishing()) {
            grainstorm.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    Toast.makeText(grainstorm, string, Toast.LENGTH_LONG).show();

                }
            });
        }
    }

    static boolean waiting = true;

    static void outOfMemory() {
        final MainActivity grainstorm = getInstance();
        if (grainstorm != null && !grainstorm.isFinishing()) {
            waiting = true;
            grainstorm.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    new AlertDialog.Builder(grainstorm, R.style.MyAlertdialogtheme)
                            .setTitle("Out Of Memory").setMessage("Please close other applications to free resources then try again or quit.").setNegativeButton("Quit", new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    dialog.dismiss();
                                    grainstorm.finish();
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
        final MainActivity grainstorm = getInstance();
        if (grainstorm != null && !grainstorm.isFinishing()) {
            grainstorm.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    new AlertDialog.Builder(grainstorm, R.style.MyAlertdialogtheme)
                            .setTitle("Initialization Error.").setMessage(message).setNegativeButton("Quit", new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    dialog.dismiss();
                                    grainstorm.finish();
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
        final MainActivity grainstorm = getInstance();
        if (grainstorm != null && !grainstorm.isFinishing()) {
            grainstorm.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    new AlertDialog.Builder(grainstorm, R.style.MyAlertdialogtheme)
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
                            intent.putExtra(Intent.EXTRA_EMAIL, new String[]{"grainstorm@rocks.me"});
                            intent.putExtra(Intent.EXTRA_SUBJECT, "Crash report (" + error + ")");
                            intent.putExtra(Intent.EXTRA_TEXT, "Please provide a short information on what happened.");
                            try {
                                grainstorm.startActivity(Intent.createChooser(intent, "Send crash report..."));
                            } catch (android.content.ActivityNotFoundException ex) {
                                Toast.makeText(grainstorm, "There are no email clients installed.", Toast.LENGTH_SHORT).show();
                            }
                        }
                    })*/.setOnDismissListener(new DialogInterface.OnDismissListener() {
                                @Override
                                public void onDismiss(DialogInterface dialogInterface) {
                                    grainstorm.finish();
                                    System.exit(0);
                                }
                            }).setOnCancelListener(new DialogInterface.OnCancelListener() {
                                @Override
                                public void onCancel(DialogInterface dialogInterface) {
                                    grainstorm.finish();
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
        return Activities.getDefaultsInt(getString(R.string.audiosource), 1, this);
    }

    static void invokeSettings() {
        MainActivity grainstorm = getInstance();
        if (grainstorm != null) {
            Intent intent = new Intent(grainstorm, MainSettings2.class);
            grainstorm.startActivity(intent);
        }
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
                intent.setAction(SynthService.ACTION_STOP_FOREGROUND_SERVICE);
                myApplication.startService(intent);
            }
            return;
        }

        MyApplication.runsAsForeground = Activities.getDefaultsBoolean(
                myApplication.getString(R.string.runAsForeground), true, myApplication);

        // Option off: no service at all. Without a started service nothing holds
        // the process up, so swiping the task out of recents ends the synth with
        // the app — which is what switching the option off asks for.
        if (!MyApplication.runsAsForeground) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                && ActivityCompat.checkSelfPermission(myApplication, Manifest.permission.POST_NOTIFICATIONS)
                != PackageManager.PERMISSION_GRANTED) {

            final MainActivity activity = getInstance();
            if (activity != null && !activity.isFinishing()) {
                activity.runOnUiThread(() -> {
                    new AlertDialog.Builder(activity, R.style.MyAlertdialogtheme)
                            .setTitle("Notifications Required")
                            .setMessage("You have enabled 'Run as Foreground Service', which requires notification permission to keep the synth running in the background. Please grant notification access.")
                            .setPositiveButton("Grant", (dialog, which) ->
                                activity.requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, PERMISSION_ALL))
                            .setNegativeButton("Later", (dialog, which) ->
                                startSynthService(myApplication, intent))
                            .show();
                });
            }
            return;
        }

        startSynthService(myApplication, intent);
    }

    /**
     * Apply a change to the "Run as Foreground Service" setting straight away.
     *
     * <p>Called from the settings screen. powerControl re-reads the preference
     * on every power-on, so without this the switch only took effect at the next
     * start — leaving the notification and the switch disagreeing for as long as
     * the synth kept playing.
     *
     * <p>Does nothing while the synth is off: there is no service either way,
     * and powerControl will read the preference when it next starts.
     */
    static void applyForegroundSetting(boolean on) {
        final MyApplication myApplication = MyApplication.getInstance();
        if (myApplication == null) return;

        MyApplication.runsAsForeground = on;
        if (!synthIsOn) return;

        final Intent intent = new Intent(myApplication, SynthService.class);
        if (on) {
            if (!serviceStarted) startSynthService(myApplication, intent);
        } else if (serviceStarted) {
            serviceStarted = false;
            intent.setAction(SynthService.ACTION_STOP_FOREGROUND_SERVICE);
            myApplication.startService(intent);
        }
    }

    private static void startSynthService(MyApplication myApplication, Intent intent) {
        intent.setAction(SynthService.ACTION_START_FOREGROUND_SERVICE);
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
            Log.w(Activities.LOG_TAG, "Could not start synth service: " + e);
        }
    }

    boolean doImport;

    void setDoImport(boolean what) {
        doImport = what;
    }

    static String getFilePath(final String filepath, boolean isProject) {
        final MainActivity grainstorm = getInstance();
        if (grainstorm == null || grainstorm.isFinishing())
            return null;
        grainstorm.setDoImport(false);
        if (filepath.startsWith("content://") || filepath.startsWith("presethasaudio")) {
            String path = "Unknown";
            if (filepath.startsWith("content://")) {
                MyAdapter.FileMetaData fileMetaData = getFileMetaData(grainstorm, Uri.parse(filepath));
                if (fileMetaData != null && fileMetaData.displayName != null)
                    path = fileMetaData.displayName;
            } else {
                path = "Included in Preset";
            }
            final String finalpath = path;
            if (!isProject) {
                final Object syncToken = new Object();

                grainstorm.runOnUiThread(() -> new AlertDialog.Builder(grainstorm, R.style.MyAlertdialogtheme)
                        .setTitle("IMPORT AUDIO?")
                        .setMessage(finalpath)
                        .setOnDismissListener(new DialogInterface.OnDismissListener() {
                            @Override
                            public void onDismiss(DialogInterface dialog) {
                                synchronized (syncToken) {
                                    syncToken.notify();
                                }
                            }
                        })
                        .setOnCancelListener(new DialogInterface.OnCancelListener() {
                            @Override
                            public void onCancel(DialogInterface dialog) {
                                synchronized (syncToken) {
                                    syncToken.notify();
                                }
                            }
                        })
                        .setPositiveButton("Import",
                                new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog,
                                                        int which) {
                                        synchronized (syncToken) {
                                            getInstance().setDoImport(true);
                                            syncToken.notify();
                                        }
                                    }
                                })
                        .setNegativeButton("Don't Import", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                synchronized (syncToken) {
                                    getInstance().setDoImport(false);
                                    syncToken.notify();
                                }

                            }
                        }).show());

                synchronized (syncToken) {
                    try {
                        syncToken.wait();
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        e.printStackTrace();
                        return null;
                    }
                }
            } else grainstorm.doImport = true;
            return grainstorm.doImport ? filepath : null;
        }
        return null;
    }

    private final static AtomicReference<WeakReference<BillingManager>> mBillingManager = new AtomicReference<>(null);

    private BillingManager billingManager;

    void checkLicense2() {
        if (firstrundone)
            return;

        String s = sSH("dide");
        if (s != null) {
            String s1 = gC(s);
            if (s1 != null) {
                String s2 = sSH("lca");
                if (s2 != null) {
                    val.set(s1);
                    lca.set(s2);
                }
            }
        }

        if (!lca.get().equals(val.get())) {
            billingManager = new BillingManager(this, this);
            billingManager.queryPurchases();
        } else
            MyApplication.guiSetup(isOpen(), getWindow().getDecorView().getWidth(), getWindow().getDecorView().getHeight());
        firstrundone = true;
    }

    @SuppressLint({"Range"})
    private String gC(String name) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                getPackageManager()
                        .getPackageInfo("me.rocks.grainstormunlocker", PackageManager.PackageInfoFlags.of(0));

            } else {
                getPackageManager()
                        .getPackageInfo("me.rocks.grainstormunlocker", 0);

            }

        } catch (PackageManager.NameNotFoundException e) {
            // Log.e("MainActivity", "Nothing installed");
            return null;
        }

        Cursor c;

        try {
            c = getContentResolver().query(CONTENT_URI, null, null, null, null);
        } catch (SecurityException e) {
            //Log.e("MainActivity", "This should not happen: " + e.getMessage());
            return null;
        }

        if (c == null) {
            //Log.e("MainActivity", "DB Empty.");
            return null;
        } else if (c.moveToFirst()) {
            do {
                if (name.matches(c.getString(c.getColumnIndex("name")))) {
                    String temp = c.getString(c.getColumnIndex("data"));
                    c.close();
                    return temp;
                }

            } while (c.moveToNext());
        }
        // Log.e("MainActivity", "DB empty");
        c.close();
        return null;
    }

    private String sSH(String input) {
        String deviceId = Settings.Secure.getString(getContentResolver(),
                Settings.Secure.ANDROID_ID);

        String temp;
        try {
            temp = makeSHA1Hash(String.format("%s%s", deviceId, input));
        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            return null;
        }
        return temp;
    }

    private static String makeSHA1Hash(String input) throws NoSuchAlgorithmException {
        MessageDigest md = MessageDigest.getInstance("SHA1");
        md.reset();
        byte[] buffer = input.getBytes();
        md.update(buffer);
        byte[] digest = md.digest();

        StringBuilder hexStr = new StringBuilder("");

        for (byte b : digest) {
            hexStr.append(Integer.toString((b & 0xff) + 0x100, 16)
                    .substring(1));
        }
        return hexStr.toString();
    }

    private static void iabset(boolean result) {
        if (result) {
            val.set(strings[0]);
            lca.set(strings[0]);
        }
    }

    @Override
    public void onBillingClientSetupFinished() {
        if (billingManager != null && billingManager.getBillingClientResponseCode() != BillingClient.BillingResponseCode.OK) {
            MyApplication.guiSetup(isOpen(), getWindow().getDecorView().getWidth(), getWindow().getDecorView().getHeight());
        }
    }

    @Override
    public void onPurchasesUpdated(List<Purchase> purchaseList) {
        for (Purchase purchase : purchaseList) {
            if (purchase.getProducts().contains(PRODUCT_ID.toLowerCase(Locale.ROOT))) {
                if (purchase.getPurchaseState() == Purchase.PurchaseState.PURCHASED) {
                    if (!purchase.isAcknowledged() && billingManager != null) {
                        billingManager.acknowledgePurchase(purchase, null);
                    }
                    iabset(true);
                } else if (purchase.getPurchaseState() == Purchase.PurchaseState.PENDING && !isFinishing()) {
                    new AlertDialog.Builder(this, R.style.MyAlertdialogtheme)
                            .setMessage("Pending purchase detected. Please complete your purchase and then do a full restart.")
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
        MyApplication.guiSetup(isOpen(), getWindow().getDecorView().getWidth(), getWindow().getDecorView().getHeight());
    }

    @Override
    public void onSubscriptionsUpdated(List<Purchase> subscriptions) {

    }

    @Override
    public void onPurchaseConsumed(String purchaseToken) {

    }

    @Override
    public void onBillingError(BillingResult billingResult) {

    }

    static boolean isOpen() {

        return val.get().equals(lca.get());
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


}





