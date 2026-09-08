package me.rocks.grainstorm;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Looper;
import android.text.format.DateUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.TextView;

import androidx.activity.ComponentActivity;
import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.recyclerview.widget.LinearLayoutManager;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.Comparator;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicReference;

import static me.rocks.grainstorm.MyApplication.read_header;
import static me.rocks.grainstorm.MyApplication.read_midiheader;
import static me.rocks.grainstorm.MyApplication.save_midimapping_callback;
import static me.rocks.grainstorm.MyApplication.save_preset_callback;

public class PresetActivity extends AppCompatActivity {
    final static int SAVE_PRESET = 0;
    final static int SAVE_MAPPING = 1;
    final static int SAVE_PROJECT = 2;
    final static String[] defaultName = {"My Preset", "My Mapping", "My Project"};
    final static String[] titles = {"SAVE PRESET", "SAVE MIDI MAPPING", "SAVE PROJECT"};
    final static String[] emptyString = {"NO PRESETS FOUND", "NO MAPPINGS FOUND", "NO PROJECTS FOUND"};
    final static String PRESET_EXTRA_MESSAGE = "Preset Extra Message";
    int type = SAVE_PRESET;
    MyRecyclerView myRecyclerView;
    MyAdapter myAdapter;
    TextView myEmptyView, myTitleView;
    String finalString;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        EdgeToEdge.enable(this);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.presetactivity);
        myRecyclerView = findViewById(R.id.preset_recyclerview);
        myEmptyView = findViewById(R.id.preset_empty_view);
        myTitleView = findViewById(R.id.presettitle);
        myRecyclerView.setHasFixedSize(false);
        // use a linear layout manager
        LinearLayoutManager myLayoutManager = new LinearLayoutManager(this);
        myRecyclerView.setLayoutManager(myLayoutManager);
        // Get the Intent that started this activity and extract the string
        Intent intent = getIntent();
        type = intent.getIntExtra(PRESET_EXTRA_MESSAGE, 0);
        myTitleView.setText(titles[type]);
        myAdapter = new MyAdapter(this);
        myRecyclerView.setAdapter(myAdapter);

        Activities.queue.push(new LoadPresets(this));
        //nameEdit.setText(new SimpleDateFormat("yyyy_MM-dd_HH-mm-ss", Locale.US).format(Calendar.getInstance().getTime()));
        findViewById(R.id.nameeditlayout).setVisibility(View.VISIBLE);
        final EditText nameEdit = findViewById(R.id.nameedit);
        nameEdit.setVisibility(View.VISIBLE);
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null)
            imm.showSoftInput(nameEdit, InputMethodManager.SHOW_IMPLICIT);
        findViewById(R.id.presetok).setVisibility(View.VISIBLE);
        findViewById(R.id.presetok).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                processEditText(nameEdit);
            }
        });

        //dialog.getWindow().setLayout(width, dheight);
        //dialog.getWindow().setGravity(Gravity.CENTER);
        nameEdit.setText(defaultName[type]);
        nameEdit.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                nameEdit.setText("");
                nameEdit.setOnClickListener(null);
            }
        });

        nameEdit.setOnKeyListener(new View.OnKeyListener() {

            public boolean onKey(View v, int keyCode, KeyEvent event) {

                if (event.getAction() == KeyEvent.ACTION_DOWN
                        && event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
                    processEditText(nameEdit);
                }
                return false;

            }

        });
        View view = getWindow().getDecorView();

        ViewCompat.setOnApplyWindowInsetsListener(view, (v, windowInsets) -> {
            // Apply the insets as padding to the view. Here, set all the dimensions
            // as appropriate to your layout. You can also update the view's margin if
            // more appropriate.
            Insets systemBarsInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
            Insets imeInsets = windowInsets.getInsets(WindowInsetsCompat.Type.ime());
            Insets gestureInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemGestures());

            int left = Math.max(systemBarsInsets.left, gestureInsets.left);
            int top = systemBarsInsets.top;
            int right = Math.max(systemBarsInsets.right, gestureInsets.right);
            int bottom = Math.max(Math.max(systemBarsInsets.bottom, gestureInsets.bottom), imeInsets.bottom);

            view.setPadding(left, top, right, bottom);
            return WindowInsetsCompat.CONSUMED;
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    private void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.show(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            // For API levels below 30
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_VISIBLE
            );
        }

    }


    @Override
    protected void onDestroy() {
        if (type == SAVE_MAPPING)
            save_midimapping_callback(finalString);
        else
            save_preset_callback(finalString);
        super.onDestroy();
    }

    private void processEditText(EditText nameEdit) {
        String str = nameEdit.getText().toString();
        if (!str.isEmpty()) {
            finalString = str;
            finish();
        }
    }

    private static class LoadPresets extends Activities.QueueTask {
        WeakReference<PresetActivity> reference;

        LoadPresets(PresetActivity obj) {
            reference = new WeakReference<>(obj);
        }

        @Override
        void func() {
            final PresetActivity presetActivity = reference.get();
            if (presetActivity == null || presetActivity.isFinishing())
                return;
            int result = presetActivity.myAdapter.getPresets();
            if (result == -1) {
                presetActivity.finish();
            } else if (result == 0) {
                presetActivity.runOnUiThread(() -> {
                    presetActivity.myRecyclerView.setVisibility(View.GONE);
                    presetActivity.myEmptyView.setText(emptyString[presetActivity.type]);
                    presetActivity.myEmptyView.setVisibility(View.VISIBLE);

                });

            } else {
                presetActivity.runOnUiThread(() -> {
                    presetActivity.myRecyclerView.setVisibility(View.VISIBLE);
                    presetActivity.myEmptyView.setVisibility(View.GONE);
                    presetActivity.myAdapter.notifyDataSetChanged();
                });
            }
        }
    }
}
