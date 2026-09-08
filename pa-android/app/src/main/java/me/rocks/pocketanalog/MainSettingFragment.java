package me.rocks.pocketanalog;

import android.Manifest;
import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.core.app.ActivityCompat;
import androidx.fragment.app.FragmentActivity;
import androidx.annotation.NonNull;
import androidx.preference.CheckBoxPreference;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import static me.rocks.pocketanalog.Activities.setDefaultsString;
import static me.rocks.pocketanalog.MainActivity.showToast;
import static me.rocks.pocketanalog.MyApplication.java_set_hqresampling;
import static me.rocks.pocketanalog.MyApplication.java_set_showfactory;
import static me.rocks.pocketanalog.MyApplication.java_set_ignore_second_midievent;
import static me.rocks.pocketanalog.MyApplication.java_set_micrec_format;
import static me.rocks.pocketanalog.MyApplication.java_set_output_format;
import static me.rocks.pocketanalog.MyApplication.java_set_pauseplayback;
import static me.rocks.pocketanalog.MyApplication.showToast2;
import static me.rocks.pocketanalog.MainActivity.url_changes;
import static me.rocks.pocketanalog.MainActivity.url_eula;
import static me.rocks.pocketanalog.MainActivity.url_instructions;
import static me.rocks.pocketanalog.MainActivity.url_oss;
import static me.rocks.pocketanalog.MainActivity.url_policy;

public class MainSettingFragment extends PreferenceFragmentCompat {
    static final String FRAGMENT_TAG = "my_preference_fragment";

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
            chooseOutputDir.setSummary(recordingFolderSummary());
        if (chooseOutputDir != null)
            chooseOutputDir.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
                @Override
                public boolean onPreferenceClick(Preference preference) {
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

                        startActivityForResult(intent, MainActivity.REQUEST_DIRECTORY_REC);
                    } catch (Exception e) {
                        MainActivity.showToast(e.toString());
                    }
                    return true;
                }
            });

        final Preference presetDir = findPreference(getString(R.string.presetdirectory));
        final Preference presetReset = findPreference(getString(R.string.presetfolderreset));
        if (presetDir != null) {
            presetDir.setSummary(presetFolderSummary());
            presetDir.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
                @Override
                public boolean onPreferenceClick(Preference preference) {
                    try {
                        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                        intent.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                        intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                        startActivityForResult(intent, MainActivity.REQUEST_DIRECTORY_PRESET);
                    } catch (Exception e) {
                        MainActivity.showToast(e.toString());
                    }
                    return true;
                }
            });
        }
        if (presetReset != null) {
            // Only meaningful once a folder has been chosen.
            presetReset.setVisible(MainActivity.presetFolderIsCustom());
            presetReset.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
                @Override
                public boolean onPreferenceClick(Preference preference) {
                    MainActivity.releasePresetFolder();
                    preference.setVisible(false);
                    if (presetDir != null) presetDir.setSummary(presetFolderSummary());
                    return true;
                }
            });
        }

        Preference midisource = findPreference(getString(R.string.midisender));
        if(midisource != null)
        midisource.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                Intent intent = new Intent(getActivity(), MidiActivity.class);
                startActivity(intent);
                return false;
            }
        });


        Preference showhome = findPreference(getString(R.string.homedirectory));
        ListPreference recf = findPreference(getString(R.string.recordingformat));

        Preference pro = findPreference(getString(R.string.upgrade));

        // Say where this device actually stands. Without it a trial is invisible:
        // it runs for fifteen days and then simply stops.
        //
        // A purchased device keeps the row and is told it owns the thing, rather
        // than having the row disappear. A row that vanishes reads as a bug -- it
        // is indistinguishable from a setting that broke -- and this one going
        // missing is exactly what it looked like in testing.
        if (pro != null) {
            if (MainActivity.isOpen()) {
                pro.setTitle("Full Version");
                pro.setSummary("Unlocked on this device");
                pro.setSelectable(false);
            } else {
                String status = TrialGate.statusSummary(getContext());
                if (status != null) pro.setSummary(status);
            }
        }

        // No purchase gate on either of these. Under the trial model the free
        // period IS the full instrument, and recording already works for everyone
        // -- hiding the rows that say where the files go, and in what format, left
        // a trial user able to record but unable to see or change either. It also
        // made the upgrade page's "the limit goes away, nothing else does" untrue.
        {
            if(showhome != null) {
                showhome.setVisible(true);
                showhome.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
                    @Override
                    public boolean onPreferenceClick(Preference preference) {
                        final String homedir = Activities.getHomePath(getContext());
                        String rec = Activities.getDefaultsString(
                                getString(R.string.recordingdirectory), null, getContext());
                        String sb = "Presets\n" + MainActivity.presetFolderLabel() + "\n\n"
                                  + "Recordings\n"
                                  + (rec != null
                                     ? Activities.describeTree(getContext(), Uri.parse(rec))
                                     : "No folder chosen yet")
                                  + "\n\n"
                                  + "Default folder location\n"
                                  + (homedir != null ? homedir : "Not available right now");
                        new AlertDialog.Builder(getContext(), R.style.MyAlertdialogtheme)
                                .setCancelable(true)
                                .setTitle("Storage Locations")
                                .setMessage(sb)
                                .show();
                        return false;
                    }
                });
            }
            if(pro != null) pro.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
                @Override
                public boolean onPreferenceClick(Preference preference) {
                    Intent intent = new Intent(getActivity(), InAppPurchases.class);
                    startActivity(intent);
                    return false;
                }
            });
        }

        if (recf != null) {
            recf.setVisible(true);
            recf.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                @Override
                public boolean onPreferenceChange(Preference preference, Object newValue) {
                        java_set_output_format(Integer.parseInt((String) newValue));
                    Log.d("MainActivity", "Audioformat: " + Integer.parseInt((String) newValue));

                    return true;
                }
            });
        }

        final FragmentActivity activity = getActivity();
        Preference chans = findPreference(getString(R.string.channels));
       if(chans!= null) {
           chans.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
               @Override
               public boolean onPreferenceChange(Preference preference, Object newValue) {
                   if (getActivity() != null && !getActivity().isFinishing())
                       new AlertDialog.Builder(getActivity(), R.style.MyAlertdialogtheme)
                               .setCancelable(true)
                               .setTitle("Restart required.")
                               //.setMessage("MainActivity needs to be restarted.")
                               .setPositiveButton("Restart",
                                       new DialogInterface.OnClickListener() {
                                           public void onClick(DialogInterface dialog,
                                                               int which) {
                                               new Thread(new Runnable() {
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
                                               }).start();
                                           }
                                       })
                               .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                                   @Override
                                   public void onClick(DialogInterface dialogInterface, int i) {
                                       dialogInterface.dismiss();
                                   }
                               })
                               .show();
                   else
                       MyApplication.savedChannels = Integer.parseInt((String) newValue);
                   Log.d("MainActivity", "Channels changed to: " + MyApplication.savedChannels);

                   return true;
               }
           });
           chans.setVisible(false);
       }

       // Recording is not a paid feature -- the RECORD button works for everyone,
       // so its settings screen has to be reachable by everyone too.
       PreferenceScreen rec = findPreference("rec screen");


       Preference bufs = findPreference(getString(R.string.bufsize3));
       if(bufs != null)
        bufs.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                if(getActivity() != null && !getActivity().isFinishing())
                new AlertDialog.Builder(getActivity(), R.style.MyAlertdialogtheme)
                        .setCancelable(true)
                        .setTitle("Restart required.")
                        //.setMessage("MainActivity needs to be restarted.")
                        .setPositiveButton("Restart",
                                new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog,
                                                        int which) {
                                        new Thread(new Runnable() {
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
                                        }).start();
                                    }
                                })
                        .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialogInterface, int i) {
                                dialogInterface.dismiss();
                            }
                        })
                        .show();
                else MyApplication.audioBufIndex = Integer.parseInt((String) newValue);
                Log.d("MainActivity", "Bufsize index changed to: " + MyApplication.audioBufIndex);
                return true;
            }
        });


       Preference hqr = findPreference(getString(R.string.hqresampling));
       if(hqr != null) {
           hqr.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
               @Override
               public boolean onPreferenceChange(Preference preference, Object newValue) {
                   boolean set = (boolean) newValue;
                   MyApplication.hqResampling = set;
                       java_set_hqresampling(set);
                   Log.d("MainActivity", "hqresampling: " + (MyApplication.hqResampling ? "TRUE" : "FALSE"));

                   return true;
               }
           });
           hqr.setVisible(false);
       }

       Preference showFactory = findPreference(getString(R.string.showfactory));
       if(showFactory != null) {
           showFactory.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
               @Override
               public boolean onPreferenceChange(Preference preference, Object newValue) {
                   boolean set = (boolean) newValue;
                   MyApplication.showFactoryPresets = set;
                   java_set_showfactory(set);
                   return true;
               }
           });
       }

       Preference useaa = findPreference(getString(R.string.useaaudio));
       if(useaa != null) useaa.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {

                if(getActivity() != null && !getActivity().isFinishing())
                new AlertDialog.Builder(getActivity(), R.style.MyAlertdialogtheme)
                        .setCancelable(true)
                        .setTitle("Restart required.")
                        //.setMessage("MainActivity needs to be restarted.")
                        .setPositiveButton("Restart",
                                new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog,
                                                        int which) {
                                        new Thread(new Runnable() {
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
                                        }).start();
                                    }
                                })
                        .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialogInterface, int i) {
                                dialogInterface.dismiss();
                            }
                        })
                        .show();
                else
                    MyApplication.useAAudio = (boolean) newValue;
                Log.d("MainActivity", "UseAAudio: " + (MyApplication.useAAudio  ? "TRUE" : "FALSE"));

                return true;
            }
        });


        Preference ign = findPreference(getString(R.string.ignoresecondnoteonevent));
        if(ign != null) {
            ign.setVisible(false);
            ign.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                @Override
                public boolean onPreferenceChange(Preference preference, Object newValue) {
                    boolean set = (boolean) newValue;
                    MyApplication.ignoreSecondMidiEvent = set;
                        java_set_ignore_second_midievent(set);
                    Log.d("MainActivity", "IgnoreMidi: " + (MyApplication.ignoreSecondMidiEvent ? "TRUE" : "FALSE"));
                    return true;
                }
            });
        }



        Preference pausep = findPreference(getString(R.string.pauseplayback));
        if(pausep != null) {
            pausep.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                @Override
                public boolean onPreferenceChange(Preference preference, Object newValue) {
                    boolean set = (boolean) newValue;
                    MyApplication.pausePlayback = set;
                        java_set_pauseplayback(set);
                    Log.d("MainActivity", "Pauseplayback: " + (MyApplication.pausePlayback ? "TRUE" : "FALSE"));

                    return true;
                }

            });
            pausep.setVisible(false);
        }

        Preference copyAudio = findPreference(getString(R.string.pauseplayback));
        if(copyAudio != null)
            copyAudio.setVisible(false);

        Preference screenon = findPreference(getString(R.string.screenonflag));
        if(screenon != null) screenon.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                MyApplication.screenOn = (boolean) newValue;
               // MainActivity pocketanalog = MainActivity.getInstance();
               // if(pocketanalog != null)
               // pocketanalog.setScreenOnFlag(set);
                return true;
            }
        });

        // "Show notification when backgrounded". The foreground service itself
        // always runs with the synth (powerControl, targetSdk 37) — this switch
        // only governs whether its notification is VISIBLE, which on 13+ is the
        // POST_NOTIFICATIONS permission. Enabling without the grant asks
        // IMMEDIATELY and the switch STAYS OFF (return false) until the system
        // says yes — onRequestPermissionsResult below flips it on. So the
        // checkbox never claims a notification the OS will not show.
        Preference foreground = findPreference(getString(R.string.runAsForeGround));
        if (foreground != null) foreground.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                final boolean on = (boolean) newValue;
                if (on && Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                        && getActivity() != null
                        && ActivityCompat.checkSelfPermission(getActivity(),
                        Manifest.permission.POST_NOTIFICATIONS)
                        != PackageManager.PERMISSION_GRANTED) {
                    requestPermissions(
                            new String[]{Manifest.permission.POST_NOTIFICATIONS},
                            MainActivity.PERMISSION_ALL);
                    return false;   // stays disabled unless the grant lands
                }
                if (on) MainActivity.refreshServiceNotification();
                return true;
            }
        });

        Preference copy = findPreference(getString(R.string.filepicker));
        if(copy != null) {
            copy.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                @Override
                public boolean onPreferenceChange(Preference preference, Object newValue) {
                    MyApplication.copyFiles = (boolean) newValue;
                    return true;
                }
            });
            copy.setVisible(false);
        }

        Preference inst = findPreference(getString(R.string.instructions));
        if(inst != null) {
            inst.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
                @Override
                public boolean onPreferenceClick(Preference preference) {
                    Intent intent = new Intent(getActivity(), EulaActivity.class);
                    intent.putExtra("url", url_instructions);
                    startActivity(intent);
                    return false;
                }

            });
        }

        Preference change = findPreference(getString(R.string.changelog));
        if(change != null) {
            change.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
                @Override
                public boolean onPreferenceClick(Preference preference) {
                    Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://pocketanalog.rocks.me/changes.html"));
                    startActivity(browserIntent);
                    return false;
                }
            });
            change.setVisible(false);
        }

        Preference eula = findPreference(getString(R.string.eula));
        if(eula != null) eula.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                Intent intent = new Intent(getActivity(), EulaActivity.class);
                intent.putExtra("url", url_eula);
                startActivity(intent);
                return false;
            }
        });

        Preference oss = findPreference(getString(R.string.oss));
        if(oss != null) oss.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                Intent intent = new Intent(getActivity(), EulaActivity.class);
                intent.putExtra("url", url_oss);
                startActivity(intent);
                return false;
            }
        });

        Preference policy = findPreference(getString(R.string.policy));
        if(policy != null) policy.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                Intent intent = new Intent(getActivity(), EulaActivity.class);
                intent.putExtra("url", url_policy);
                startActivity(intent);
                return false;
            }
        });

    }


    /** The other half of the "Show notification when backgrounded" flow: the
     *  switch returned false while the system prompt was up, so on a grant it
     *  is flipped on HERE (setChecked persists), and the already-running
     *  service re-posts its notification so it appears right away. On a denial
     *  nothing happens and the switch simply stays off. */
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        if (requestCode == MainActivity.PERMISSION_ALL
                && grantResults.length > 0
                && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            Preference p = findPreference(getString(R.string.runAsForeGround));
            if (p instanceof CheckBoxPreference) ((CheckBoxPreference) p).setChecked(true);
            MainActivity.refreshServiceNotification();
        }
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    /** The recording folder, or why there isn't one yet. */
    private String recordingFolderSummary() {
        String rec = Activities.getDefaultsString(
                getString(R.string.recordingdirectory), null, getContext());
        if (rec == null)
            return "No folder chosen yet. Voltaic asks for one the first time you record.";
        String where = Activities.describeTree(getContext(), Uri.parse(rec));
        return where != null ? where : "Your chosen folder";
    }

    /** Where the presets are, and -- while they are still inside the app --
     *  why you might want to change that. Once a folder is chosen the path
     *  alone is the answer, so the explanation drops away. */
    private String presetFolderSummary() {
        if (MainActivity.presetFolderIsCustom())
            return MainActivity.presetFolderLabel();
        return "Presets are currently saved in the app's default folder and will "
             + "be deleted when you uninstall Voltaic.\n"
             + "Tap to choose a folder of your own instead.";
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode,
                                 Intent resultData) {
        try {
            if (resultCode == Activity.RESULT_OK) {
                Activity activity = getActivity();
                if(activity == null)
                    return;
                ContentResolver contentResolver = activity.getContentResolver();
                if (contentResolver == null) {
                    Log.e("MainActivity", "Something went wrong");
                    showToast("Something went wrong.");
                    return;
                }

                Uri uriTree = resultData.getData();
                if (uriTree != null) {
                    int takeFlags = resultData.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
                    activity.grantUriPermission(activity.getPackageName(), uriTree, takeFlags);

                    contentResolver.takePersistableUriPermission(uriTree, takeFlags);
                    if (requestCode == MainActivity.REQUEST_DIRECTORY_PRESET) {
                        // adoptPresetFolder moves the presets first and only then
                        // stores the pref -- getHome() has to keep resolving to
                        // where they currently are while the move runs.
                        MainActivity.adoptPresetFolder(uriTree);
                        Preference dir = findPreference(getString(R.string.presetdirectory));
                        Preference reset = findPreference(getString(R.string.presetfolderreset));
                        if (dir != null) dir.setSummary(Activities.describeTree(activity, uriTree) + "/presets");
                        // (the move runs in the background; the row already shows where they are going)
                        if (reset != null) reset.setVisible(true);
                    } else {
                        setDefaultsString(getString(R.string.recordingdirectory), uriTree.toString(), activity);
                        String outdir = Activities.describeTree(activity, uriTree);
                        Preference out = findPreference(getString(R.string.recordingdirectory));
                        if (out != null) out.setSummary(recordingFolderSummary());
                        if (outdir != null) showToast("Recordings will be saved in " + outdir);
                        else showToast("Recording folder changed.");
                    }
                }
            }
        } catch (Exception e) {
            showToast(e.toString());
        }
    }

}
