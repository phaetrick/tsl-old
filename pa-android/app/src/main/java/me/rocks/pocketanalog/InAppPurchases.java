package me.rocks.pocketanalog;

/**
 * Created by pr on 12.02.18.
 */

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentName;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.webkit.WebView;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.QueryProductDetailsParams;

import com.google.common.collect.ImmutableList;

import java.util.List;
import java.util.Locale;

import static me.rocks.pocketanalog.MainActivity.PRODUCT_ID;

public class InAppPurchases extends AppCompatActivity implements View.OnClickListener, BillingManager.BillingUpdatesListener {

    final static String url_pro = "file:///android_asset/upgrade.html";

    private BillingManager mBillingManager;
    private Button mPurchaseButton;
    private TextView mPriceView;
    private ProductDetails productDetails = null;
    private boolean purchaseFlowInitiated = false;

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.inappbilling);
        TextView title = findViewById(R.id.titleTextView);
        WebView wv = findViewById(R.id.iabwebview);
        wv.getSettings().setUseWideViewPort(false);
        wv.getSettings().setBuiltInZoomControls(true);
        wv.getSettings().setJavaScriptEnabled(true);

        wv.loadUrl(url_pro);
        // The page draws its own title so the V can be the logo mark instead of a
        // letter. Two titles would show otherwise.
        title.setVisibility(android.view.View.GONE);
        mPurchaseButton = findViewById(R.id.purchaseButton);
        mPriceView = findViewById(R.id.priceView);
        if (mBillingManager == null)
            mBillingManager = new BillingManager(this, this);
        mBillingManager.queryPurchases();
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
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
        }
    }

    @Override
    public void onDestroy() {
        if (mBillingManager != null)
            mBillingManager.destroy();
        mBillingManager = null;
        super.onDestroy();
    }

    void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    public void onClick(View v) {
        if (v.getId() == R.id.purchaseButton) {
            if (productDetails == null) {
                showToast("Google Play billing is unavailable.");
                return;
            }
            if (mBillingManager == null)
                mBillingManager = new BillingManager(this, this);
            mBillingManager.initiatePurchaseFlow(productDetails);
            purchaseFlowInitiated = true;
        }
    }

    private void updatePriceView(final String text) {
        new Handler(Looper.getMainLooper()).postDelayed(() -> mPriceView.setText(text), 100);
    }

    @Override
    public void onBillingClientSetupFinished() {
        if (mBillingManager != null && mBillingManager.getBillingClientResponseCode() != BillingClient.BillingResponseCode.OK) {
            updatePriceView("Could not load the price.");
        }
    }

    @Override
    public void onPurchasesUpdated(List<Purchase> purchaseList, boolean fromQuery) {
        if (productDetails == null) {
            QueryProductDetailsParams params = QueryProductDetailsParams.newBuilder()
                    .setProductList(ImmutableList.of(QueryProductDetailsParams.Product.newBuilder()
                            .setProductId(PRODUCT_ID)
                            .setProductType(BillingClient.ProductType.INAPP)
                            .build()))
                    .build();

            mBillingManager.queryProductDetailsAsync(params, (billingResult, detailsList) -> {
                if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                    for (ProductDetails details : detailsList.getProductDetailsList()) {
                        if (details.getProductId().equals(PRODUCT_ID)) {
                            productDetails = details;
                            ProductDetails.OneTimePurchaseOfferDetails offer = details.getOneTimePurchaseOfferDetails();
                            if (offer != null) {
                                String finalPrice = offer.getFormattedPrice();
                                runOnUiThread(() -> {
                                    mPriceView.setText(getString(R.string.price) + finalPrice);
                                    mPurchaseButton.setVisibility(View.VISIBLE);
                                    mPurchaseButton.setOnClickListener(this);
                                });
                            }
                        }
                    }
                } else {
                    updatePriceView("Could not load the price.");
                }
            });
        }
        if (purchaseFlowInitiated && !MainActivity.isOpen()) {
            for (Purchase purchase : purchaseList) {
                if (purchase.getProducts().contains(PRODUCT_ID.toLowerCase(Locale.ROOT))) {
                    if (purchase.getPurchaseState() == Purchase.PurchaseState.PURCHASED) {
                        if (!isFinishing())
                            new AlertDialog.Builder(this, R.style.MyAlertdialogtheme)
                                    .setCancelable(false)
                                    .setMessage("Thank you! Restart to unlock the full version.")
                                    .setPositiveButton("Restart", (dialog, which) -> {
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
                                    .show();
                    } else if (purchase.getPurchaseState() == Purchase.PurchaseState.PENDING) {
                        if (isFinishing())
                            return;
                        new AlertDialog.Builder(this, R.style.MyAlertdialogtheme)
                                .setMessage("Purchase pending. Once it completes, restart Voltaic.")
                                .setPositiveButton("Restart", (dialog, which) -> {
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
                                .setNegativeButton("Cancel", (dialog, i) -> dialog.dismiss())
                                .show();
                    }
                    return;
                }
            }
        }
    }

    @Override
    public void onSubscriptionsUpdated(List<Purchase> subscriptions) {}

    @Override
    public void onPurchaseConsumed(String purchaseToken) {}

    @Override
    public void onBillingError(BillingResult billingResult) {}
}
