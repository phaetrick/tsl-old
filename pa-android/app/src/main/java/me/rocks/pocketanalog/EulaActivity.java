package me.rocks.pocketanalog;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.webkit.WebSettings;
import android.webkit.WebView;

import androidx.appcompat.app.AppCompatActivity;

public class EulaActivity extends AppCompatActivity {
    private static String url = MainActivity.url_eula;
    WebView wv;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_eula);
        wv = (WebView) findViewById(R.id.wv);
        Intent intent = getIntent();
        if(intent.getStringExtra("url") != null)
            url = intent.getStringExtra("url");
        wv.getSettings().setBuiltInZoomControls(true);
        boolean isWebsite = url.startsWith("https://") || url.startsWith("http://");
        wv.getSettings().setUseWideViewPort(isWebsite);
        wv.getSettings().setLoadWithOverviewMode(isWebsite);
        wv.getSettings().setJavaScriptEnabled(isWebsite);

        //wv.getSettings().setDefaultZoom(WebSettings.ZoomDensity.CLOSE);
       // wv.addJavascriptInterface(new JavaBridge(this), "Android");

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
}
