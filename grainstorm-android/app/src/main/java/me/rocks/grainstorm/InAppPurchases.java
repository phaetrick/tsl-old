package me.rocks.grainstorm;

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

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.ProductDetailsResponseListener;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.QueryProductDetailsParams;

import com.google.common.collect.ImmutableList;

import org.jetbrains.annotations.NotNull;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicReference;

import static me.rocks.grainstorm.MainActivity.PRODUCT_ID;

public class InAppPurchases extends AppCompatActivity implements View.OnClickListener, BillingManager.BillingUpdatesListener{

    final static String url_pro = "file:///android_asset/upgrade.html";

    private BillingManager mBillingManager;

    private Button mPurchaseButton;

    private TextView mPriceView;

    private ProductDetails productDetails = null;
    private boolean purchaseFlowInitiated = false;


    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        EdgeToEdge.enable(this);
        super.onCreate(savedInstanceState);

        setContentView(R.layout.inappbilling);
        TextView title = findViewById(R.id.titleTextView);
        WebView wv = findViewById(R.id.iabwebview);
        wv.getSettings().setUseWideViewPort(false);
        wv.getSettings().setBuiltInZoomControls(true);
        wv.getSettings().setJavaScriptEnabled(true);

        wv.loadUrl(url_pro);
        title.setText(getString(R.string.titleiap));
        mPurchaseButton = findViewById(R.id.purchaseButton);
        mPriceView = findViewById(R.id.priceView);
        // mPriceView.setText("Retrieving price...");
        if (mBillingManager == null)
            mBillingManager = new BillingManager(this, this);
        mBillingManager.queryPurchases();
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
                showToast("Billing not initialized.");
                return;
            }
            if (mBillingManager == null)
                mBillingManager = new BillingManager(this, this);
            mBillingManager.initiatePurchaseFlow(productDetails);
            purchaseFlowInitiated = true;
        }
    }

    private void displayAnErrorIfNeeded(BillingManager mBillingManager) {
        if (isFinishing()) {
            return;
        }

        int billingResponseCode = mBillingManager.getBillingClientResponseCode();

        switch (billingResponseCode) {
            case BillingClient.BillingResponseCode.OK:
                // If manager was connected successfully, then show no SKUs error
                showToast("Failed to retrieve Products");
                break;
            case BillingClient.BillingResponseCode.BILLING_UNAVAILABLE:
                showToast("Billing unavailiable.");
                break;
            default:
                showToast("Api error.");
        }
        updatePriceView("Failed to retrieve price.");

    }

    private void updatePriceView(final String text){
        new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
            @Override
            public void run() {
                mPriceView.setText(text);
            }
        }, 100);
    }

    @Override
    public void onBillingClientSetupFinished() {

        if (mBillingManager != null && mBillingManager.getBillingClientResponseCode() != BillingClient.BillingResponseCode.OK) {
            updatePriceView("Failed to retrieve price");
        }

    }

    @Override
    public void onPurchasesUpdated(List<Purchase> purchaseList) {
        if (productDetails == null) {
            QueryProductDetailsParams params = QueryProductDetailsParams.newBuilder()
                    .setProductList(ImmutableList.of(QueryProductDetailsParams.Product.newBuilder()
                            .setProductId(PRODUCT_ID)
                            .setProductType(BillingClient.ProductType.INAPP)
                            .build()))
                    .build();

            mBillingManager.queryProductDetailsAsync(params, (billingResult, detailsList) -> {
                if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                    List<ProductDetails> pList = detailsList.getProductDetailsList();
                    for (ProductDetails details : pList) {
                        if (details.getProductId().equals(PRODUCT_ID)) {
                            productDetails = details;
                            ProductDetails.OneTimePurchaseOfferDetails offer = details.getOneTimePurchaseOfferDetails();
                            if (offer != null) {
                                String finalPrice = offer.getFormattedPrice();
                                runOnUiThread(() -> {
                                    mPriceView.setText(getString(R.string.price_label, finalPrice));                                    mPurchaseButton.setVisibility(View.VISIBLE);
                                    mPurchaseButton.setOnClickListener(this);
                                });
                            }
                        }
                    }
                } else {
                    updatePriceView("Failed to retrieve price.");
                }
            });
        }
        if(purchaseFlowInitiated && !MainActivity.isOpen()) {
            for (Purchase purchase : purchaseList) {
                if (purchase.getProducts().contains(PRODUCT_ID.toLowerCase(Locale.ROOT))) {
                    if (purchase.getPurchaseState() == Purchase.PurchaseState.PURCHASED) {
                        if(!isFinishing())
                            new AlertDialog.Builder(this, R.style.MyAlertdialogtheme)
                                    .setCancelable(false)
                                    .setMessage("Thank you very much. A restart is required now.")
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
                                    .show();
                    } else if (purchase.getPurchaseState() == Purchase.PurchaseState.PENDING) {
                        if (isFinishing())
                            return;
                        new AlertDialog.Builder(this, R.style.MyAlertdialogtheme)
                                .setMessage("Pending purchase. Please complete your purchase and then do a full restart.")
                                .setPositiveButton("Restart",
                                        new DialogInterface.OnClickListener() {
                                            public void onClick(DialogInterface dialog,
                                                                int which) {
                                                PackageManager packageManager = getPackageManager();
                                                Intent intent = packageManager.getLaunchIntentForPackage(getPackageName());
                                                if (intent != null) {
                                                    ComponentName componentName = intent.getComponent();
                                                    Intent mainIntent = Intent.makeRestartActivityTask(componentName);
                                                    startActivity(mainIntent);
                                                    finish();
                                                    System.exit(0);
                                                }
                                            }
                                        })
                                .setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialogInterface, int i) {
                                        dialogInterface.dismiss();
                                    }
                                })
                                .show();
                    }
                    return;
                }
            }
        }
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
}