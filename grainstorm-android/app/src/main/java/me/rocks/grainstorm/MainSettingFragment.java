package me.rocks.grainstorm;

import android.Manifest;
import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AlertDialog;
import androidx.core.app.ActivityCompat;
import androidx.documentfile.provider.DocumentFile;
import androidx.fragment.app.FragmentActivity;
import androidx.preference.CheckBoxPreference;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import java.io.InputStream;
import java.io.OutputStream;
import static me.rocks.grainstorm.Activities.getDefaultsString;
import static me.rocks.grainstorm.Activities.setDefaultsString;
import static me.rocks.grainstorm.MainActivity.REQUEST_DIRECTORY_PRESET;
import static me.rocks.grainstorm.MainActivity.REQUEST_DIRECTORY_REC;
import static me.rocks.grainstorm.MainActivity.showToast;
import static me.rocks.grainstorm.MainActivity.url_eula;
import static me.rocks.grainstorm.MainActivity.url_oss;
import static me.rocks.grainstorm.MainActivity.url_changes;
import static me.rocks.grainstorm.MyAdapter.paths;
import static me.rocks.grainstorm.MyApplication.java_save_audio;
import static me.rocks.grainstorm.MyApplication.java_set_micrec_format;
import static me.rocks.grainstorm.MyApplication.java_set_output_format;
import static me.rocks.grainstorm.MyApplication.java_set_pauseplayback;
import static me.rocks.grainstorm.MyApplication.showToast2;

public class MainSettingFragment extends PreferenceFragmentCompat {

    static class CopyPresets extends Activities.QueueTask{
        private final Activity act;
        private final Uri uri;
        CopyPresets(Activity activity, Uri uriTree){
            act = activity;
            uri = uriTree;
        }
        @Override
        void func(){
            String s = getDefaultsString(act.getString(R.string.presetdirectory), null, act);
            if (s==null || !s.equals(uri.toString())) {

                DocumentFile dest = DocumentFile.fromTreeUri(act, uri);

                if (dest != null && dest.canWrite()) {
                    for (String f : paths) {
                        DocumentFile outfolder = dest.findFile(f);
                        try {
                            if (outfolder == null)
                                outfolder = dest.createDirectory(f);
                            if (outfolder != null && outfolder.canWrite()) {
                                DocumentFile home = MainActivity.getHome(f);
                                if (home != null) {
                                    DocumentFile[] infiles = home.listFiles();
                                    for (DocumentFile infile : infiles) {
                                        DocumentFile outfile = outfolder.findFile(infile.getName() != null ? infile.getName() : String.valueOf(System.currentTimeMillis()));
                                        if (outfile == null)
                                            outfile = outfolder.createFile(f, infile.getName() != null ? infile.getName() : String.valueOf(System.currentTimeMillis()));
                                        if (outfile != null) {
                                            OutputStream outputStream = act.getContentResolver().openOutputStream(outfile.getUri());
                                            InputStream inputStream = act.getContentResolver().openInputStream(infile.getUri());
                                            if(inputStream == null || outputStream == null){
                                                showToast("Something went wrong.");
                                                return;
                                            }
                                            byte[] buffer = new byte[1024];
                                            int length;

                                            while ((length = inputStream.read(buffer)) > 0) {
                                                outputStream.write(buffer, 0, length);
                                            }

                                            outputStream.flush();
                                            inputStream.close();
                                            outputStream.close();
                                        }
                                    }
                                }
                            }
                        } catch (Exception e) {
                            showToast(e.toString());
                        }

                    }

                }
            }
            setDefaultsString(act.getString(R.string.presetdirectory), uri.toString(), act);
            String outdir = uri.getPath();

            if (outdir != null) {
                String output = ("Preset Folder set to: ") + outdir;
                showToast(output);
            } else
                showToast("Preset Folder changed.");
        }
    }

    static final String FRAGMENT_TAG = "my_preference_fragment";
    private ActivityResultLauncher<Intent> someActivityResultLauncher;
    int code = 0;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
// You can do the assignment inside onAttach or onCreate, i.e, before the activity is displayed
        someActivityResultLauncher = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                result -> {
                    if (result.getResultCode() == Activity.RESULT_OK) {
                        // There are no request codes
                        try {
                            Activity activity = getActivity();
                            if (activity == null)
                                return;
                            ContentResolver contentResolver = activity.getContentResolver();
                            if (contentResolver == null) {
                                Log.e("MainActivity", "Something went wrong");
                                showToast("Something went wrong.");
                                return;
                            }
                            Intent resultData = result.getData();
                            if (resultData == null) {
                                Log.e("MainActivity", "Something went wrong");
                                showToast("Something went wrong.");
                                return;
                            }
                            Uri uriTree = resultData.getData();
                            if (uriTree != null) {
                                //int requestCode = code;//resultData.getIntExtra("TYPEINT", MainActivity.REQUEST_DIRECTORY_REC);
                                int takeFlags = resultData.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
                                activity.grantUriPermission(activity.getPackageName(), uriTree, takeFlags);
                                contentResolver.takePersistableUriPermission(uriTree, takeFlags);
                                if (code == REQUEST_DIRECTORY_PRESET) {
                                    setDefaultsString(getString(R.string.presetdirectory), uriTree.toString(), activity);
                                    String outdir = uriTree.getPath();

                                    if (outdir != null) {
                                        String output = ("Preset Folder set to: ") + outdir;
                                        showToast(output);
                                    } else
                                        showToast("Preset Folder changed.");
                                    //Activities.queue.push(new CopyPresets(activity, uriTree));
                                }
                                else {
                                    setDefaultsString(getString(code == MainActivity.REQUEST_DIRECTORY_REC ? R.string.recordingdirectory : R.string.presetdirectory), uriTree.toString(), activity);
                                    String outdir = uriTree.getPath();

                                    if (outdir != null) {
                                        String output = (code == MainActivity.REQUEST_DIRECTORY_REC ? "Output Folder set to: " : "Preset Folder set to: ") + outdir;
                                        showToast(output);
                                    } else
                                        showToast(code == MainActivity.REQUEST_DIRECTORY_REC ? "Output Folder changed." : "Preset Folder changed.");
                                }
                            }
                        } catch (Exception e) {
                            showToast(e.toString());
                        }
                    }
                });

    }

    // Live state line under the AI model entries; the static XML summary stays
    // as the explainer (with the download size) until a download exists.
    // While a download or verification is running the summary re-polls itself,
    // otherwise it freezes at "Verifying..." after the background thread has
    // long finished (user-reported).
    private final android.os.Handler aiHandler =
            new android.os.Handler(android.os.Looper.getMainLooper());
    private final java.util.HashMap<String, Runnable> aiPollers = new java.util.HashMap<>();

    private void refreshAiSummary(Preference p, String key) {
        String state = MainActivity.aiModelState(key);
        boolean active = false;
        if (state.equals("installed")) p.setSummary("Installed (1.7 GB) - tap to delete.");
        else if (state.equals("verifying")) { p.setSummary("Verifying..."); active = true; }
        // A finished download verifies itself (MainActivity.aiVerifyPending);
        // "success" is the frame between the transfer ending and that starting.
        else if (state.equals("success")) { p.setSummary("Downloaded - verifying..."); active = true; }
        else if (state.startsWith("running|")) {
            String line = "Downloading...";
            try {
                String[] parts = state.split("\\|");
                long soFar = Long.parseLong(parts[1]);
                long total = Long.parseLong(parts[2]);
                if (total > 0) line = "Downloading... " + (soFar * 100 / total) + "%";
            } catch (Exception ignored) {}
            p.setSummary(line);
            active = true;
        }
        else if (state.startsWith("failed|")) p.setSummary("Download failed - tap to retry.");

        Runnable old = aiPollers.get(key);
        if (old != null) aiHandler.removeCallbacks(old);
        if (active) {
            Runnable next = () -> refreshAiSummary(p, key);
            aiPollers.put(key, next);
            aiHandler.postDelayed(next, 1500);
        }
    }

    @Override
    public void onDestroy() {
        aiHandler.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.preferences_main, rootKey);

        CheckBoxPreference useAAudio = findPreference(getString(R.string.useaaudio));
        PreferenceCategory preferenceCategory = findPreference("audio");

        if (Build.VERSION.SDK_INT < 27 && preferenceCategory != null && useAAudio != null) {
            preferenceCategory.removePreference(useAAudio);
        }

        //if(Startup.isOpen()) {
        Preference chooseOutputDir = findPreference(getString(R.string.recordingdirectory));
        if (chooseOutputDir != null)
            chooseOutputDir.setOnPreferenceClickListener(preference -> {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                    intent.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                    intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                    code = REQUEST_DIRECTORY_REC;
                    intent.putExtra("TYPEINT", MainActivity.REQUEST_DIRECTORY_REC);
                    someActivityResultLauncher.launch(intent);
                } catch (Exception e) {
                    MainActivity.showToast(e.toString());
                }
                return true;
            });

        Preference midisource = findPreference(getString(R.string.midisender));
        if (midisource != null)
            midisource.setOnPreferenceClickListener(preference -> {
                Intent intent = new Intent(getActivity(), MidiActivity.class);
                startActivity(intent);
                return false;
            });

        // RECORD AI->TRACK model downloads. All logic lives in
        // MainActivity.aiModelClick (DownloadManager) and aiVerifyPending
        // (sha256 verify + install, unasked, when the download lands); here
        // we only wire the taps and keep the summaries honest.
        MainActivity.aiRefreshManifest();
        for (String aiKey : new String[]{"sfx", "music"}) {
            Preference p = findPreference("ai_model_" + aiKey);
            if (p == null) continue;
            refreshAiSummary(p, aiKey);
            p.setOnPreferenceClickListener(preference -> {
                if (MainActivity.aiModelState(aiKey).equals("installed")) {
                    // Installed: the tap offers deletion (the model is 1.7 GB
                    // and some phones need the space back).
                    new android.app.AlertDialog.Builder(requireContext())
                            .setTitle("Delete this model?")
                            .setMessage("Frees about 1.7 GB. You can download it again at any time.")
                            .setPositiveButton("Delete", (d, w) -> {
                                MainActivity.aiModelDelete(aiKey);
                                refreshAiSummary(preference, aiKey);
                            })
                            .setNegativeButton("Cancel", null)
                            .show();
                } else {
                    MainActivity.aiModelClick(aiKey);
                    refreshAiSummary(preference, aiKey);
                }
                return true;
            });
        }

        ListPreference recf = findPreference(getString(R.string.recordingformat));

        Preference pro = findPreference(getString(R.string.upgrade));

        PreferenceScreen presets = findPreference("preset screen");

        if (MainActivity.isOpen()) {
            Preference choosePresetDir = findPreference(getString(R.string.presetdirectory));
            if (choosePresetDir != null)
                choosePresetDir.setOnPreferenceClickListener(preference -> {
                    try {
                        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                        intent.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                        intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                        intent.putExtra("TYPEINT2", MainActivity.REQUEST_DIRECTORY_PRESET);
                        code = REQUEST_DIRECTORY_PRESET;
                        someActivityResultLauncher.launch(intent);
                    } catch (Exception e) {
                        MainActivity.showToast(e.toString());
                    }
                    return true;
                });
            Preference saveAudioWithPreset = findPreference(getString(R.string.saveaudio));
            if(saveAudioWithPreset != null){
                saveAudioWithPreset.setOnPreferenceChangeListener((preference, newValue) -> {
                    boolean set = (boolean) newValue;
                    MyApplication.saveAudioWithPreset = set;
                        java_save_audio(set);
                    return true;
                });
            }
            Preference systemFolderForPresets = findPreference(getString(R.string.systemFolderForPresets));
            if(systemFolderForPresets != null){
                systemFolderForPresets.setOnPreferenceChangeListener((preference, newValue) -> {
                    MyApplication.systemFolderForPresets = (boolean) newValue;
                    return true;
                });
            }
            if (pro != null) pro.setOnPreferenceClickListener(preference -> {
                Activity ctx = getActivity();
                if (ctx != null && !ctx.isFinishing())
                    showToast2(ctx, ctx.getString(R.string.owned));
                return false;
            });
        } else {
            if (presets != null) presets.setVisible(false);
            if (pro != null) pro.setOnPreferenceClickListener(preference -> {
                Intent intent = new Intent(getActivity(), InAppPurchases.class);
                startActivity(intent);
                return false;
            });
        }

        if (recf != null) {
            recf.setVisible(true);
            recf.setOnPreferenceChangeListener((preference, newValue) -> {
                int newVal = Integer.parseInt((String) newValue);
                    java_set_output_format(newVal);
                    MyApplication.outputFormat = newVal;
                Log.d("MainActivity", "Audioformat: " + newVal);

                return true;
            });
        }

        ListPreference micf = findPreference(getString(R.string.audiosource));
        if (micf != null) {
            micf.setOnPreferenceChangeListener((preference, newValue) -> {
                int newVal = Integer.parseInt((String) newValue);
                    java_set_micrec_format(newVal);
                    MyApplication.micRecFormat = newVal;
                Log.d("MainActivity", "Micrecformat: " + newVal);

                return true;
            });
        }


        final FragmentActivity activity = getActivity();
        Preference chans = findPreference(getString(R.string.channels));
        if (chans != null)
            chans.setOnPreferenceChangeListener((preference, newValue) -> {
                if (getActivity() != null && !getActivity().isFinishing())
                    new AlertDialog.Builder(getActivity(), R.style.MyAlertdialogtheme)
                            .setCancelable(true)
                            .setTitle("Restart required.")
                            //.setMessage("MainActivity needs to be restarted.")
                            .setPositiveButton("Restart",
                                    (dialog, which) -> new Thread(new Runnable() {
                                        @Override
                                        public void run() {
                                            if (activity == null)
                                                return;
                                            PackageManager packageManager = activity.getPackageManager();
                                            if (packageManager == null)
                                                return;
                                            Intent intent = packageManager.getLaunchIntentForPackage(activity.getPackageName());
                                            if (intent != null) {
                                                ComponentName componentName = intent.getComponent();
                                                Intent mainIntent = Intent.makeRestartActivityTask(componentName);
                                                startActivity(mainIntent);
                                                activity.finish();
                                                System.exit(0);
                                            }
                                        }
                                    }).start())
                            .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> dialogInterface.dismiss())
                            .show();
                else
                    MyApplication.savedChannels = Integer.parseInt((String) newValue);
                Log.d("MainActivity", "Channels changed to: " + MyApplication.savedChannels);

                return true;
            });


        Preference bufs = findPreference(getString(R.string.bufsize3));
        if (bufs != null)
            bufs.setOnPreferenceChangeListener((preference, newValue) -> {
                if (getActivity() != null && !getActivity().isFinishing())
                    new AlertDialog.Builder(getActivity(), R.style.MyAlertdialogtheme)
                            .setCancelable(true)
                            .setTitle("Restart required.")
                            //.setMessage("MainActivity needs to be restarted.")
                            .setPositiveButton("Restart",
                                    (dialog, which) -> new Thread(new Runnable() {
                                        @Override
                                        public void run() {
                                            if (activity == null)
                                                return;
                                            PackageManager packageManager = activity.getPackageManager();
                                            if (packageManager == null)
                                                return;
                                            Intent intent = packageManager.getLaunchIntentForPackage(activity.getPackageName());
                                            if (intent != null) {
                                                ComponentName componentName = intent.getComponent();
                                                Intent mainIntent = Intent.makeRestartActivityTask(componentName);
                                                startActivity(mainIntent);
                                                activity.finish();
                                                System.exit(0);
                                            }
                                        }
                                    }).start())
                            .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> dialogInterface.dismiss())
                            .show();
                else MyApplication.audioBufIndex = Integer.parseInt((String) newValue);
                Log.d("MainActivity", "Bufsize index changed to: " + MyApplication.audioBufIndex);
                return true;
            });

        // Apply the foreground-service switch immediately. powerControl re-reads
        // the preference on every power-on, so without this the switch only took
        // effect at the next start — while the synth was already playing, the
        // notification and the switch disagreed until playback stopped.
        Preference runAsForeGround = findPreference(getString(R.string.runAsForeground));
        if (runAsForeGround != null)
            runAsForeGround.setOnPreferenceChangeListener((preference, newValue) -> {
                final boolean on = (boolean) newValue;
                if (on && Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                        && getActivity() != null
                        && ActivityCompat.checkSelfPermission(getActivity(),
                        Manifest.permission.POST_NOTIFICATIONS)
                        != PackageManager.PERMISSION_GRANTED) {
                    requestPermissions(
                            new String[]{Manifest.permission.POST_NOTIFICATIONS},
                            MainActivity.PERMISSION_ALL);
                }
                MainActivity.applyForegroundSetting(on);
                return true;
            });

        Preference useaa = findPreference(getString(R.string.useaaudio));
        if (useaa != null) useaa.setOnPreferenceChangeListener((preference, newValue) -> {

            if (getActivity() != null && !getActivity().isFinishing())
                new AlertDialog.Builder(getActivity(), R.style.MyAlertdialogtheme)
                        .setCancelable(true)
                        .setTitle("Restart required.")
                        //.setMessage("MainActivity needs to be restarted.")
                        .setPositiveButton("Restart",
                                (dialog, which) -> new Thread(new Runnable() {
                                    @Override
                                    public void run() {
                                        if (activity == null)
                                            return;
                                        PackageManager packageManager = activity.getPackageManager();
                                        if (packageManager == null)
                                            return;
                                        Intent intent = packageManager.getLaunchIntentForPackage(activity.getPackageName());
                                        if (intent != null) {
                                            ComponentName componentName = intent.getComponent();
                                            Intent mainIntent = Intent.makeRestartActivityTask(componentName);
                                            startActivity(mainIntent);
                                            activity.finish();
                                            System.exit(0);
                                        }
                                    }
                                }).start())
                        .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> dialogInterface.dismiss())
                        .show();
            else
                MyApplication.useAAudio = (boolean) newValue;
            Log.d("MainActivity", "UseAAudio: " + (MyApplication.useAAudio ? "TRUE" : "FALSE"));

            return true;
        });

        Preference pausep = findPreference(getString(R.string.pauseplayback));
        if (pausep != null) pausep.setOnPreferenceChangeListener((preference, newValue) -> {
            boolean set = (boolean) newValue;
            MyApplication.pausePlayback = set;
                java_set_pauseplayback(set);
            Log.d("MainActivity", "Pauseplayback: " + (MyApplication.pausePlayback ? "TRUE" : "FALSE"));

            return true;
        });

        Preference screenon = findPreference(getString(R.string.screenonflag));
        if (screenon != null) screenon.setOnPreferenceChangeListener((preference, newValue) -> {
            MyApplication.screenOn = (boolean) newValue;
            // MainActivity grainstorm = MainActivity.getInstance();
            // if(grainstorm != null)
            // grainstorm.setScreenOnFlag(set);
            return true;
        });

        Preference copy = findPreference(getString(R.string.filepicker));
        if (copy != null) copy.setOnPreferenceChangeListener((preference, newValue) -> {
            MyApplication.copyFiles = (boolean) newValue;
            return true;
        });

        Preference inst = findPreference(getString(R.string.instructions));
        if (inst != null) inst.setOnPreferenceClickListener(preference -> {
            Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://thesecretlaboratory.com/apps/grainstorm/instructions"));
            startActivity(browserIntent);
            return false;
        });
        Preference change = findPreference(getString(R.string.changelog));
        if (change != null) change.setOnPreferenceClickListener(preference -> {
            Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://thesecretlaboratory.com/forum/grainstorm/changelog.29"));
            startActivity(browserIntent);
            return false;
        });

        Preference eula = findPreference(getString(R.string.eula));
        if (eula != null) eula.setOnPreferenceClickListener(preference -> {
            Intent intent = new Intent(getActivity(), EulaActivity.class);
            intent.putExtra("url", url_eula);
            startActivity(intent);
            return false;
        });

        Preference oss = findPreference(getString(R.string.oss));
        if (oss != null) oss.setOnPreferenceClickListener(preference -> {
            Intent intent = new Intent(getActivity(), EulaActivity.class);
            intent.putExtra("url", url_oss);
            startActivity(intent);
            return false;
        });

    }

}
