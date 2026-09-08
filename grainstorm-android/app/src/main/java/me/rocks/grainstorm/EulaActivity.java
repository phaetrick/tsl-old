package me.rocks.grainstorm;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.webkit.WebSettings;
import android.webkit.WebView;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

public class EulaActivity extends AppCompatActivity {
    private static String url = MainActivity.url_eula;
    WebView wv;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        EdgeToEdge.enable(this);

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_eula);

        wv = (WebView) findViewById(R.id.wv);
        Intent intent = getIntent();
        if(intent.getStringExtra("url") != null)
            url = intent.getStringExtra("url");
        wv.getSettings().setBuiltInZoomControls(true);
        //wv.getSettings().setBuiltInZoomControls(false);
        //wv.getSettings().setLoadWithOverviewMode(false);
        wv.getSettings().setUseWideViewPort(url.equals(MainActivity.url_oss));

        //wv.getSettings().setDefaultZoom(WebSettings.ZoomDensity.CLOSE);
       // wv.getSettings().setJavaScriptEnabled(true);
       // wv.addJavascriptInterface(new JavaBridge(this), "Android");
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

        wv.loadUrl(url);
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

}
